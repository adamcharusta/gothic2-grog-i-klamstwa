#include <grog/ai/io.h>
#include <grog/ai/session.h>

#include <grog/ai/hints.h>

#include <algorithm>
#include <atomic>

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

  for(const char* key : {"game_names", "game_texts"}) {
    if(!items.value().contains(key) || !items.value()[key].is_object())
      continue;
    auto& target = std::string(key) == "game_names" ? d.itemGameNames : d.itemGameTexts;
    for(auto& [id, v] : items.value()[key].items()) {
      if(!v.is_object())
        return Error{ErrorKind::Io, std::string("items.json: ") + key + "." + id + " must be {\"pl\":..,\"en\":..}"};
      for(auto [lang, name] : {std::pair{Language::Pl, "pl"}, std::pair{Language::En, "en"}})
        if(v.contains(name) && v[name].is_string())
          target[id][lang] = v[name].get<std::string>();
    }
  }

  auto start = readJson(dir / "start.json");
  if(!start.ok())
    return start.error();
  const json& sj = start.value();
  if(!sj.contains("world") || !sj["world"].is_string() || !sj.contains("waypoint") || !sj["waypoint"].is_string() ||
     !sj.contains("armor") || !sj["armor"].is_string() || !sj.contains("level") || !sj["level"].is_number_integer())
    return Error{ErrorKind::Io, "start.json: world, waypoint, armor and level are required"};
  d.start = {sj["world"].get<std::string>(), sj["waypoint"].get<std::string>(), sj["armor"].get<std::string>(),
             sj["level"].get<int>(), {}};
  if(sj.contains("items")) {
    if(!sj["items"].is_object())
      return Error{ErrorKind::Io, "start.json: 'items' must be an object {instance: count}"};
    for(auto& [inst, n] : sj["items"].items()) {
      if(!n.is_number_integer() || n.get<int>() <= 0)
        return Error{ErrorKind::Io, "start.json: item " + inst + " needs a positive count"};
      d.start.items.push_back({inst, n.get<int>()});
    }
  }
  if(sj.contains("first_quest")) {
    if(!sj["first_quest"].is_object())
      return Error{ErrorKind::Io, "start.json: 'first_quest' must be an object {pl: text, en: text}"};
    for(auto& [lang, text] : sj["first_quest"].items())
      if(text.is_string() && !text.get<std::string>().empty())
        d.start.firstQuest[lang] = text.get<std::string>();
  }

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
  // New game: tell the player where to start. A loaded save replaces the whole world state.
  if(auto it = data_.start.firstQuest.find(languageName(config_.language)); it != data_.start.firstQuest.end())
    world_.quests.push_back({"tankard", it->second, 0});
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
  uint64_t gen = 0;
  bool voiceable = false;
  TurnResult out = answer(npc, playerLine, gameTimeS, gen, voiceable);
  if(!voiceable || tts_.load() == nullptr)
    return out;
  // The reply is already applied; synthesis runs without the lock, like the LLM call.
  Voiced v = voice(npc, out.speech, false);
  std::lock_guard<std::mutex> lock(mu_);
  if(gen != generation_) {  // a save was loaded while the audio was made: drop it with the turn's effects
    out.error = Error{ErrorKind::Validation, "answer discarded: the game state was reloaded"};
    out.verdicts.clear();
    out.endDialog = false;
    return out;
  }
  out.audioWav = std::move(v.wav);
  if(out.hint.empty())  // LLM hints come first; an unshown voice hint waits for a later turn
    out.hint = takeVoiceHint(v.error);
  return out;
}

TurnResult GameSession::answer(std::string_view npcView, std::string_view playerLine, int64_t gameTimeS,
                               uint64_t& gen, bool& voiceable) {
  std::string npc(npcView);
  TurnResult out;
  LlmRequest req;
  const NpcCard* card = nullptr;
  voiceable = false;
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
  bool retriedTransport = false;
  for(int attempt = 0; attempt < 2 && !reply; ++attempt) {
    auto res = actor_.complete(req);
    record("actor", npc, actor_, res, playerLine);
    if(!res.ok()) {
      out.error = res.error();
      // One retry for passing trouble; key, quota and config errors would only fail again (and may cost money).
      const ErrorKind k = res.error().kind;
      const bool transient = k == ErrorKind::Timeout || k == ErrorKind::Server || k == ErrorKind::Network;
      if(transient && !retriedTransport) {
        retriedTransport = true;
        --attempt;  // does not use up the bad-reply retry
        continue;
      }
      break;
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
      auto verdicts = validate(reply->actions, npc, world_, data_.cast, gameTimeS);
      for(auto& v : verdicts)
        stats_.rejected += v.accepted ? 0 : 1;
      stats_.verdicts += static_cast<int>(verdicts.size());
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
      voiceable = true;
      return out;
    } catch(const std::exception& e) {
      out.error = Error{ErrorKind::BadResponse, std::string("reply could not be applied: ") + e.what()};
      out.verdicts.clear();
    }
  }
  ++stats_.fallbacks;
  out.usedFallback = true;
  if(out.error && !hinted_.count(out.error->kind)) {
    out.hint = playerHint(out.error->kind, config_.language);
    if(!out.hint.empty())
      hinted_.insert(out.error->kind);
  }
  out.speech = fallbackLine(*card);
  out.gesture = "think";
  telemetry_.event("fallback", {{"npc", npc}, {"reason", out.error ? errorKindName(out.error->kind) : "unknown"}});
  voiceable = true;
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
    req = buildDirectorRequest(world_, data_.cast, data_.lore, trigger, data_.directorRules, config_.language);
  }
  auto res = director_.complete(req);
  record("director", "", director_, res, trigger);
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
    WorldState next = world_;
    next.rollEventHour(gameTimeS);
    auto verdicts = validate(actions, kDirector, next, data_.cast, gameTimeS);
    for(auto& v : verdicts)
      stats_.rejected += v.accepted ? 0 : 1;
    stats_.verdicts += static_cast<int>(verdicts.size());
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

void GameSession::record(std::string_view role, std::string_view npc, const ILlmProvider& p,
                         const Result<LlmReply>& res, std::string_view requestText) {
  double cost = telemetry_.request(role, npc, p.id(), res, requestText);
  std::lock_guard<std::mutex> lock(mu_);
  ++stats_.requests;
  if(!res.ok())
    return;
  stats_.costUsd += cost;
  stats_.inputTokens += res.value().inputTokens;
  stats_.outputTokens += res.value().outputTokens;
  stats_.latencySumMs += res.value().latencyMs;
  stats_.latencyMaxMs = std::max(stats_.latencyMaxMs, res.value().latencyMs);
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
          {"rejected_rate", stats_.verdicts ? double(stats_.rejected) / stats_.verdicts : 0.0},
          {"requests", stats_.requests},
          {"input_tokens", stats_.inputTokens},
          {"output_tokens", stats_.outputTokens},
          {"cost_usd", stats_.costUsd},
          {"latency_ms_avg", stats_.requests ? stats_.latencySumMs / stats_.requests : 0.0},
          {"latency_ms_max", stats_.latencyMaxMs},
          {"provider", providerKindName(config_.actor.kind)},
          {"model", config_.actor.model},
          {"director_model", config_.director.model},
          {"quest_entries", world_.quests.size()},
          {"tts_requests", stats_.ttsRequests},
          {"tts_errors", stats_.ttsErrors},
          {"tts_audio_s", stats_.ttsAudioMs / 1000.0},
          {"tts_cost_usd", stats_.ttsCostUsd},
          {"tts_latency_avg_ms", stats_.ttsRequests > stats_.ttsErrors
                                     ? stats_.ttsLatencySumMs / (stats_.ttsRequests - stats_.ttsErrors)
                                     : 0.0}};
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
            {"verdicts", stats_.verdicts},
            {"requests", stats_.requests},
            {"input_tokens", stats_.inputTokens},
            {"output_tokens", stats_.outputTokens},
            {"cost_usd", stats_.costUsd},
            {"latency_sum_ms", stats_.latencySumMs},
            {"latency_max_ms", stats_.latencyMaxMs},
            {"tts_requests", stats_.ttsRequests},
            {"tts_errors", stats_.ttsErrors},
            {"tts_audio_ms", stats_.ttsAudioMs},
            {"tts_cost_usd", stats_.ttsCostUsd},
            {"tts_latency_sum_ms", stats_.ttsLatencySumMs},
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
    // Added in Plan 5; saves from beta.5/6 lack them.
    stats_.verdicts = st.value("verdicts", 0);
    stats_.requests = st.value("requests", 0);
    stats_.inputTokens = st.value("input_tokens", 0LL);
    stats_.outputTokens = st.value("output_tokens", 0LL);
    stats_.costUsd = st.value("cost_usd", 0.0);
    stats_.latencySumMs = st.value("latency_sum_ms", 0.0);
    stats_.latencyMaxMs = st.value("latency_max_ms", 0.0);
    // Added in Plan 6.
    stats_.ttsRequests = st.value("tts_requests", 0);
    stats_.ttsErrors = st.value("tts_errors", 0);
    stats_.ttsAudioMs = st.value("tts_audio_ms", 0LL);
    stats_.ttsCostUsd = st.value("tts_cost_usd", 0.0);
    stats_.ttsLatencySumMs = st.value("tts_latency_sum_ms", 0.0);
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

void GameSession::setTts(ITtsProvider* tts) { tts_.store(tts); }

std::future<Speech> GameSession::speakAsync(std::string npc, std::string textUtf8, bool cacheable) {
  uint64_t gen = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    gen = generation_;
  }
  return std::async(std::launch::async, [this, npc = std::move(npc), text = std::move(textUtf8), cacheable, gen]() {
    return speak(npc, text, cacheable, gen);
  });
}

std::future<Speech> GameSession::farewellAsync(std::string npc) {
  uint64_t gen = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    gen = generation_;
  }
  return std::async(std::launch::async, [this, npc = std::move(npc), gen]() {
    return speak(npc, farewell(npc), true, gen);
  });
}

Speech GameSession::speak(const std::string& npc, std::string text, bool cacheable, uint64_t gen) {
  Voiced v = voice(npc, text, cacheable);
  std::lock_guard<std::mutex> lock(mu_);
  if(gen != generation_)
    return {};  // a save was loaded meanwhile: this line belongs to another timeline
  return Speech{std::move(text), std::move(v.wav), takeVoiceHint(v.error)};
}

std::string GameSession::takeVoiceHint(const std::optional<ErrorKind>& error) {
  if(!error || hintedTts_.count(*error))
    return {};
  std::string h = voiceHint(*error, config_.language);
  if(!h.empty())
    hintedTts_.insert(*error);
  return h;
}

namespace {
uint64_t fnv1a64(std::string_view s, uint64_t h = 0xcbf29ce484222325ULL) {
  for(unsigned char c : s) {
    h ^= c;
    h *= 0x100000001b3ULL;
  }
  return h;
}

std::string hex64(uint64_t v) {
  static const char* digits = "0123456789abcdef";
  std::string out(16, '0');
  for(int i = 15; i >= 0; --i, v >>= 4)
    out[i] = digits[v & 0xF];
  return out;
}

// Temp file + rename, so a reader never sees half a file. Failures only cost a later re-synthesis.
void writeCacheFile(const fs::path& file, const std::string& wav) {
  static std::atomic<unsigned> counter{0};
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  fs::path tmp = file;
  tmp += pathFromUtf8(".tmp" + std::to_string(counter++));
  if(!writeTextFile(tmp, wav).ok()) {
    fs::remove(tmp, ec);
    return;
  }
  fs::rename(tmp, file, ec);
  if(ec)
    fs::remove(tmp, ec);
}
}  // namespace

GameSession::Voiced GameSession::voice(const std::string& npc, const std::string& text, bool cacheable) {
  ITtsProvider* tts = tts_.load();
  if(tts == nullptr || text.find_first_not_of(" .") == std::string::npos)
    return {};
  // data_ and config_ never change after construction: no lock needed.
  TtsRequest req;
  req.text = text;
  req.voice = "ash";
  if(auto it = data_.cards.find(npc); it != data_.cards.end()) {
    const NpcVoice& nv = it->second.voice;
    if(!nv.name.empty())
      req.voice = nv.name;
    if(auto st = nv.style.find(languageName(config_.language)); st != nv.style.end())
      req.instructions = st->second;
  }
  const std::string model(tts->model());
  const size_t chars = utf8Length(text);

  fs::path file;
  if(cacheable && !config_.tts.cacheDir.empty()) {
    // The key covers everything that changes the audio, never the API key.
    uint64_t h = fnv1a64(model);
    for(const std::string* part : {&req.voice, &req.instructions, &req.text})
      h = fnv1a64(*part, fnv1a64(std::string_view("\n", 1), h));
    file = pathFromUtf8(config_.tts.cacheDir) / pathFromUtf8(hex64(h) + ".wav");
    std::error_code ec;
    if(fs::is_regular_file(file, ec)) {
      auto cached = readTextFile(file);
      if(cached.ok()) {
        if(auto ms = wavDurationMs(cached.value())) {  // anything else is a miss
          TtsReply r;
          r.wav = std::move(cached.value());
          r.audioMs = *ms;
          r.model = model;
          Result<TtsReply> res(std::move(r));
          telemetry_.tts(npc, model, res, chars, true);
          return {std::move(res.value().wav), std::nullopt};
        }
      }
    }
  }

  auto res = tts->synthesize(req);
  double cost = telemetry_.tts(npc, model, res, chars, false);
  {
    std::lock_guard<std::mutex> lock(mu_);
    ++stats_.ttsRequests;
    if(res.ok()) {
      stats_.ttsAudioMs += res.value().audioMs;
      stats_.ttsCostUsd += cost;
      stats_.ttsLatencySumMs += res.value().latencyMs;
    } else {
      ++stats_.ttsErrors;
    }
  }
  if(!res.ok())
    return {"", res.error().kind};
  if(!file.empty() && !res.value().wav.empty())
    writeCacheFile(file, res.value().wav);
  return {std::move(res.value().wav), std::nullopt};
}

}  // namespace grog::ai
