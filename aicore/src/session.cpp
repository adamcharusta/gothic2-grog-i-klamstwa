#include <grog/ai/io.h>
#include <grog/ai/session.h>

#include <algorithm>

namespace grog::ai {
using nlohmann::json;
namespace fs = std::filesystem;

namespace {
inline constexpr size_t kMaxMemory = 40;

Result<json> readJson(const fs::path& p) {
  auto t = readTextFile(p);
  if(!t.ok())
    return t.error();
  json j = json::parse(t.value(), nullptr, false);
  if(j.is_discarded())
    return Error{ErrorKind::Io, "invalid JSON in " + pathToUtf8(p)};
  return j;
}
}  // namespace

Result<GameData> loadGameData(const fs::path& dir) {
  GameData d;
  auto cast = readJson(dir / "cast.json");
  auto places = readJson(dir / "places.json");
  if(!cast.ok())
    return cast.error();
  if(!places.ok())
    return places.error();
  auto c = loadCast(cast.value(), places.value());
  if(!c.ok())
    return c.error();
  d.cast = c.value();

  std::error_code ec;
  std::vector<fs::path> files;
  for(auto& e : fs::directory_iterator(dir / "npc", ec))
    if(e.path().extension() == ".json")
      files.push_back(e.path());
  std::sort(files.begin(), files.end());
  for(auto& f : files) {
    auto j = readJson(f);
    if(!j.ok())
      return j.error();
    auto card = loadCard(j.value());
    if(!card.ok())
      return card.error();
    if(!d.cast.hasNpc(card.value().id))
      return Error{ErrorKind::Io, "npc card " + card.value().id + " is not in cast.json"};
    d.cards[card.value().id] = card.value();
  }

  files.clear();
  for(auto& e : fs::directory_iterator(dir / "lore", ec))
    if(e.path().extension() == ".json")
      files.push_back(e.path());
  std::sort(files.begin(), files.end());
  for(auto& f : files) {
    auto j = readJson(f);
    if(!j.ok())
      return j.error();
    auto l = loadLore(j.value());
    if(!l.ok())
      return l.error();
    for(auto& e : l.value().entries)
      d.lore.entries.push_back(e);
  }

  auto ar = readTextFile(dir / "rules" / "actor.md");
  auto dr = readTextFile(dir / "rules" / "director.md");
  if(!ar.ok())
    return ar.error();
  if(!dr.ok())
    return dr.error();
  d.actorRules = ar.value();
  d.directorRules = dr.value();

  auto items = readJson(dir / "items.json");
  if(!items.ok())
    return items.error();
  if(!items.value().contains("items") || !items.value()["items"].is_object())
    return Error{ErrorKind::Io, "items.json: 'items' object missing"};
  for(auto& [id, inst] : items.value()["items"].items())
    if(inst.is_string())
      d.items[id] = inst.get<std::string>();
  for(const char* key : {"ai_names", "descriptions"}) {
    if(!items.value().contains(key) || !items.value()[key].is_object())
      continue;
    auto& target = std::string(key) == "ai_names" ? d.cast.itemAiName : d.cast.itemDescription;
    for(auto& [id, v] : items.value()[key].items())
      if(v.is_string())
        target[id] = v.get<std::string>();
  }

  auto start = readJson(dir / "start.json");
  if(!start.ok())
    return start.error();
  const json& sj = start.value();
  if(!sj.contains("world") || !sj["world"].is_string() || !sj.contains("waypoint") || !sj["waypoint"].is_string() ||
     !sj.contains("armor") || !sj["armor"].is_string() || !sj.contains("level") || !sj["level"].is_number_integer())
    return Error{ErrorKind::Io, "start.json: world, waypoint, armor and level are required"};
  d.start = {sj["world"].get<std::string>(), sj["waypoint"].get<std::string>(), sj["armor"].get<std::string>(),
             sj["level"].get<int>()};

  if(fs::exists(dir / "pricing.json")) {
    auto pj = readJson(dir / "pricing.json");
    if(!pj.ok())
      return pj.error();
    auto p = loadPricing(pj.value());
    if(!p.ok())
      return p.error();
    d.pricing = p.value();
  }
  return d;
}

GameSession::GameSession(AiConfig config, GameData data, ILlmProvider& actor, ILlmProvider& director,
                         Telemetry& telemetry, uint64_t seed)
    : config_(std::move(config)),
      data_(std::move(data)),
      actor_(actor),
      director_(director),
      telemetry_(telemetry),
      world_(WorldState::fromScenario(rollScenario(seed, data_.cast), data_.cast)),
      rng_(seed ^ 0x5eed5eed5eed5eedULL) {
  for(auto& [id, card] : data_.cards)
    for(auto& item : card.items)
      if(!world_.itemHolder.count(item))
        world_.itemHolder[item] = id;
  telemetry_.session(world_.scenario, config_);
}

// Caller holds mu_.
std::string GameSession::fallbackLine(const NpcCard& card) {
  auto& lines = card.fallbackLines.at(config_.language);
  return rng_.pick(lines);
}

WorldState GameSession::worldSnapshot() {
  std::lock_guard<std::mutex> lock(mu_);
  return world_;
}

TurnResult GameSession::talk(std::string_view npcView, std::string_view playerLine, int64_t gameTimeS) {
  std::string npc(npcView);
  TurnResult out;
  LlmRequest req;
  const NpcCard* card = nullptr;
  uint64_t gen = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = data_.cards.find(npc);
    if(it == data_.cards.end()) {
      out.error = Error{ErrorKind::Validation, "no character card for '" + npc + "'"};
      out.usedFallback = true;
      out.speech = "...";
      return out;
    }
    card = &it->second;  // data_ is never modified after construction
    gen = generation_;
    req = buildActorRequest(*card, world_, data_.cast, data_.lore, config_.language, playerLine, data_.actorRules);
  }

  // Provider call without holding the lock.
  std::optional<ActorReply> reply;
  for(int attempt = 0; attempt < 2 && !reply; ++attempt) {
    auto res = actor_.complete(req);
    telemetry_.request("actor", npc, actor_.id(), res, playerLine);
    if(!res.ok()) {
      out.error = res.error();
      break;  // provider errors are not retried here (cost, rate limits)
    }
    auto parsed = parseActorReply(res.value().text);
    if(!parsed.ok()) {
      out.error = parsed.error();
      continue;
    }
    if(breaksCharacter(parsed.value().speech)) {
      out.error = Error{ErrorKind::BadResponse, "reply broke character"};
      continue;
    }
    reply = parsed.value();
    out.error.reset();
  }

  std::lock_guard<std::mutex> lock(mu_);
  if(gen != generation_) {  // a save was loaded meanwhile: this answer belongs to another timeline
    out.speech = reply ? reply->speech : fallbackLine(*card);
    out.error = Error{ErrorKind::Validation, "answer discarded: the game state was reloaded"};
    return out;
  }
  ++stats_.turns;
  lastGameT_ = std::max(lastGameT_, gameTimeS);
  if(reply) {
    try {
      // Validate the whole batch, apply it to a copy, then commit: all or nothing.
      translateItems(reply->actions);
      auto verdicts = validate(reply->actions, npc, world_, data_.cast);
      for(auto& v : verdicts)
        stats_.rejected += v.accepted ? 0 : 1;
      WorldState next = world_;
      apply(verdicts, npc, next, gameTimeS);
      auto& mem = next.npcs[npc].memory;
      mem.push_back({ChatMessage::User, std::string(playerLine)});
      mem.push_back({ChatMessage::Assistant, reply->speech});
      while(mem.size() > kMaxMemory)
        mem.pop_front();
      world_ = std::move(next);
      out.speech = reply->speech;
      out.gesture = reply->gesture;
      out.endDialog = reply->endDialog;
      out.verdicts = std::move(verdicts);
      telemetry_.verdicts(npc, out.verdicts);
      return out;
    } catch(const std::exception& e) {
      out.error = Error{ErrorKind::BadResponse, std::string("reply could not be applied: ") + e.what()};
      out.verdicts.clear();
    }
  }
  ++stats_.fallbacks;
  out.usedFallback = true;
  out.speech = fallbackLine(*card);
  out.gesture = "think";
  telemetry_.event("fallback", {{"npc", npc}, {"reason", out.error ? errorKindName(out.error->kind) : "unknown"}});
  return out;
}

std::future<TurnResult> GameSession::talkAsync(std::string npc, std::string playerLine, int64_t gameTimeS) {
  return std::async(std::launch::async, [this, npc = std::move(npc), line = std::move(playerLine), gameTimeS]() {
    return talk(npc, line, gameTimeS);
  });
}

DirectorResult GameSession::direct(std::string_view trigger, int64_t gameTimeS) {
  DirectorResult out;
  LlmRequest req;
  uint64_t gen = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    gen = generation_;
    ++stats_.directorCalls;
    req = buildDirectorRequest(world_, data_.cast, data_.lore, trigger, data_.directorRules);
  }
  auto res = director_.complete(req);
  telemetry_.request("director", "", director_.id(), res, trigger);
  if(!res.ok()) {
    out.error = res.error();
    return out;
  }
  auto parsed = parseDirectorReply(res.value().text);
  if(!parsed.ok()) {
    out.error = parsed.error();
    return out;
  }
  std::lock_guard<std::mutex> lock(mu_);
  if(gen != generation_) {
    out.error = Error{ErrorKind::Validation, "director answer discarded: the game state was reloaded"};
    return out;
  }
  try {
    out.thought = parsed.value().thought;
    auto actions = parsed.value().actions;
    translateItems(actions);
    auto verdicts = validate(actions, kDirector, world_, data_.cast);
    WorldState next = world_;
    apply(verdicts, kDirector, next, gameTimeS);
    world_ = std::move(next);
    out.verdicts = std::move(verdicts);
    telemetry_.verdicts(kDirector, out.verdicts);
  } catch(const std::exception& e) {
    out.verdicts.clear();
    out.error = Error{ErrorKind::BadResponse, std::string("director reply could not be applied: ") + e.what()};
  }
  return out;
}

void GameSession::translateItems(std::vector<Command>& actions) const {
  for(auto& c : actions) {
    if(!c.args.is_object() || !c.args.contains("item") || !c.args["item"].is_string())
      continue;
    std::string id = data_.cast.itemIdForAiName(c.args["item"].get<std::string>());
    // The AI only knows neutral handles; anything else (incl. internal ids) is made invalid.
    c.args["item"] = id.empty() ? "unknown:" + c.args["item"].get<std::string>() : id;
  }
}

void GameSession::syncPlayerItems(const std::set<std::string>& onPlayer) {
  std::lock_guard<std::mutex> lock(mu_);
  bool moved = false;
  for(auto& [item, holder] : world_.itemHolder) {
    bool has = onPlayer.count(item) > 0;
    if(has && holder != "player") {
      holder = "player";
      moved = true;
    } else if(!has && holder == "player") {
      holder = "lost";
      moved = true;
    }
  }
  for(auto& item : onPlayer)
    if(!world_.itemHolder.count(item) && data_.items.count(item)) {
      world_.itemHolder[item] = "player";
      moved = true;
    }
  if(moved)
    triggers_.insert("item_moved");
}

void GameSession::reportFight(std::string_view npcView, bool playerWon, int64_t gameTimeS) {
  std::lock_guard<std::mutex> lock(mu_);
  std::string npc(npcView);
  if(!world_.npcs.count(npc))
    return;
  auto& st = world_.npcs[npc];
  std::string name = data_.cast.displayName(npc);
  if(playerWon) {
    st.knockedOutByPlayer = true;
    // Losing hurts pride, but a culprit is never "burned" by fights alone: they hold the truth.
    int floor = world_.scenario.culprit == npc ? -2 : -5;
    st.attitude = std::max(floor, st.attitude - 1);
    world_.addFact({gameTimeS, "The newcomer beat " + name + " in a fist fight.", {}});
  } else {
    st.attitude = std::min(5, st.attitude + 1);
    world_.addFact({gameTimeS, name + " beat the newcomer in a fist fight.", {}});
  }
  telemetry_.event("fight", {{"npc", npc}, {"player_won", playerWon}});
  triggers_.insert("fight");
  lastGameT_ = std::max(lastGameT_, gameTimeS);
}

bool GameSession::directorDue(int64_t t) {
  std::lock_guard<std::mutex> lock(mu_);
  if(directorInFlight_ || !world_.ending.empty())
    return false;
  if(!isSolvable(world_.scenario, world_.burnedNpcs()))
    triggers_.insert("unsolvable");
  int64_t since = t - lastDirectorT_;
  if(since < config_.directorMinGapS)
    return false;
  return since >= config_.directorIntervalS || !triggers_.empty();
}

void GameSession::trigger(std::string reason) {
  std::lock_guard<std::mutex> lock(mu_);
  triggers_.insert(std::move(reason));
}

std::future<DirectorResult> GameSession::directAsync(std::string trig, int64_t t) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    directorInFlight_ = true;
    lastDirectorT_ = t;
    for(auto& r : triggers_)
      trig += "," + r;
    triggers_.clear();
  }
  return std::async(std::launch::async, [this, trig = std::move(trig), t]() {
    DirectorResult r = direct(trig, t);
    std::lock_guard<std::mutex> lock(mu_);
    directorInFlight_ = false;
    return r;
  });
}

std::optional<Ending> GameSession::ending() {
  std::lock_guard<std::mutex> lock(mu_);
  if(world_.ending.empty())
    return std::nullopt;
  return Ending{world_.ending};
}

nlohmann::json GameSession::summary() {
  std::lock_guard<std::mutex> lock(mu_);
  const Scenario& sc = world_.scenario;
  return {{"seed", sc.seed},
          {"scenario", scenarioTypeName(sc.type)},
          {"culprit", sc.culprit},
          {"motive", sc.motive},
          {"tankard_location", sc.tankardLocation},
          {"outcome", world_.ending.empty() ? std::string("playing") : world_.ending},
          {"play_time_s", lastGameT_},
          {"turns", stats_.turns},
          {"fallbacks", stats_.fallbacks},
          {"director_calls", stats_.directorCalls},
          {"rejected_commands", stats_.rejected},
          {"quest_entries", world_.quests.size()}};
}

nlohmann::json GameSession::saveState() {
  std::lock_guard<std::mutex> lock(mu_);
  return {{"version", kWorldSaveVersion},
          {"world", toJson(world_)},
          {"rng", rng_.state()},
          {"director", {{"last_t", lastDirectorT_}, {"triggers", triggers_}}},
          {"stats",
           {{"turns", stats_.turns},
            {"fallbacks", stats_.fallbacks},
            {"director_calls", stats_.directorCalls},
            {"rejected", stats_.rejected},
            {"play_time_s", lastGameT_}}}};
}

Result<bool> GameSession::loadState(const nlohmann::json& j) {
  if(!j.is_object() || !j.contains("version") || j["version"] != kWorldSaveVersion)
    return Error{ErrorKind::Io, "AI save data is missing or from an incompatible mod version"};
  try {
    auto w = worldFromJson(j.at("world"));
    if(!w.ok())
      return w.error();
    std::lock_guard<std::mutex> lock(mu_);
    world_ = std::move(w.value());
    rng_.setState(j.at("rng").get<std::vector<uint64_t>>());
    lastDirectorT_ = j.at("director").at("last_t").get<int64_t>();
    triggers_ = j.at("director").at("triggers").get<std::set<std::string>>();
    const auto& st = j.at("stats");
    stats_ = Stats{};
    stats_.turns = st.at("turns").get<int>();
    stats_.fallbacks = st.at("fallbacks").get<int>();
    stats_.directorCalls = st.at("director_calls").get<int>();
    stats_.rejected = st.at("rejected").get<int>();
    lastGameT_ = st.at("play_time_s").get<int64_t>();
    directorInFlight_ = false;
    ++generation_;
  } catch(const nlohmann::json::exception& e) {
    return Error{ErrorKind::Io, std::string("AI save data: ") + e.what()};
  }
  return true;
}

std::string GameSession::farewell(std::string_view npc) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = data_.cards.find(std::string(npc));
  if(it == data_.cards.end())
    return "...";
  return rng_.pick(it->second.farewells.at(config_.language));
}

}  // namespace grog::ai
