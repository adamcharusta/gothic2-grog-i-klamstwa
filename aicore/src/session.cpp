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

std::string GameSession::fallbackLine(const NpcCard& card) {
  auto& lines = card.fallbackLines.at(config_.language);
  return rng_.pick(lines);
}

TurnResult GameSession::talk(std::string_view npcView, std::string_view playerLine, int64_t gameTimeS) {
  std::lock_guard<std::mutex> lock(mu_);
  std::string npc(npcView);
  TurnResult out;
  auto it = data_.cards.find(npc);
  if(it == data_.cards.end()) {
    out.error = Error{ErrorKind::Validation, "no character card for '" + npc + "'"};
    out.usedFallback = true;
    out.speech = "...";
    return out;
  }
  const NpcCard& card = it->second;
  LlmRequest req = buildActorRequest(card, world_, data_.cast, data_.lore, config_.language, playerLine,
                                     data_.actorRules);

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

  if(!reply) {
    out.usedFallback = true;
    out.speech = fallbackLine(card);
    out.gesture = "think";
    telemetry_.event("fallback", {{"npc", npc}, {"reason", out.error ? errorKindName(out.error->kind) : "unknown"}});
    return out;
  }

  out.speech = reply->speech;
  out.gesture = reply->gesture;
  out.endDialog = reply->endDialog;
  out.verdicts = validate(reply->actions, npc, world_, data_.cast);
  apply(out.verdicts, npc, world_, gameTimeS);
  telemetry_.verdicts(npc, out.verdicts);

  auto& mem = world_.npcs[npc].memory;
  mem.push_back({ChatMessage::User, std::string(playerLine)});
  mem.push_back({ChatMessage::Assistant, out.speech});
  while(mem.size() > kMaxMemory)
    mem.pop_front();
  return out;
}

std::future<TurnResult> GameSession::talkAsync(std::string npc, std::string playerLine, int64_t gameTimeS) {
  return std::async(std::launch::async, [this, npc = std::move(npc), line = std::move(playerLine), gameTimeS]() {
    return talk(npc, line, gameTimeS);
  });
}

DirectorResult GameSession::direct(std::string_view trigger, int64_t gameTimeS) {
  std::lock_guard<std::mutex> lock(mu_);
  DirectorResult out;
  LlmRequest req = buildDirectorRequest(world_, data_.cast, data_.lore, trigger, data_.directorRules);
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
  out.thought = parsed.value().thought;
  out.verdicts = validate(parsed.value().actions, kDirector, world_, data_.cast);
  apply(out.verdicts, kDirector, world_, gameTimeS);
  telemetry_.verdicts(kDirector, out.verdicts);
  return out;
}

std::string GameSession::farewell(std::string_view npc) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = data_.cards.find(std::string(npc));
  if(it == data_.cards.end())
    return "...";
  return rng_.pick(it->second.farewells.at(config_.language));
}

}  // namespace grog::ai
