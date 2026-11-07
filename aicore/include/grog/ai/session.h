#pragma once
#include <grog/ai/cards.h>
#include <grog/ai/commands.h>
#include <grog/ai/config.h>
#include <grog/ai/io.h>
#include <grog/ai/llm.h>
#include <grog/ai/prompt.h>
#include <grog/ai/rng.h>
#include <grog/ai/scenario.h>
#include <grog/ai/telemetry.h>
#include <grog/ai/world.h>

#include <filesystem>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace grog::ai {

struct StartSetup {
  std::string world;     // e.g. ADDONWORLD.ZEN
  std::string waypoint;  // where the hero starts
  std::string armor;     // item instance equipped at start
  int level = 1;
};

// Everything loaded from mod/data.
struct GameData {
  Cast cast;
  std::map<std::string, NpcCard> cards;
  Lore lore;
  std::string actorRules;
  std::string directorRules;
  Pricing pricing;
  std::map<std::string, std::string> items;  // story item id -> engine item instance (items.json)
  StartSetup start;                          // start.json
};

// Loads cast.json, places.json, npc/*.json, lore/*.json, rules/actor.md, rules/director.md, pricing.json.
Result<GameData> loadGameData(const std::filesystem::path& dataDir);

struct TurnResult {
  std::string speech;
  std::string gesture = "neutral";
  bool endDialog = false;
  std::vector<Verdict> verdicts;
  std::optional<Error> error;  // set when the provider failed or the reply was unusable
  bool usedFallback = false;
};

struct DirectorResult {
  std::vector<Verdict> verdicts;
  std::string thought;
  std::optional<Error> error;
};

// One playthrough.
// Threading: the world is locked only while a prompt is built and while a reply is applied, never during the
// provider call, so farewell()/worldSnapshot() stay responsive while a talk is in flight.
// Lifetime: the session (and the providers/telemetry it references) must outlive every future from talkAsync;
// wait for pending futures before destroying it. Keep the returned future: discarding it blocks until done.
class GameSession {
 public:
  GameSession(AiConfig config, GameData data, ILlmProvider& actor, ILlmProvider& director, Telemetry& telemetry,
              uint64_t seed);

  TurnResult talk(std::string_view npc, std::string_view playerLine, int64_t gameTimeS);
  std::future<TurnResult> talkAsync(std::string npc, std::string playerLine, int64_t gameTimeS);
  DirectorResult direct(std::string_view trigger, int64_t gameTimeS);
  std::string farewell(std::string_view npc);

  // Locked copy, safe from any thread.
  WorldState worldSnapshot();
  // Direct access: only when no talk/direct call is in flight (tests, single-threaded tools).
  WorldState& world() { return world_; }
  const GameData& data() const { return data_; }
  const AiConfig& config() const { return config_; }

 private:
  std::string fallbackLine(const NpcCard& card);

  AiConfig config_;
  GameData data_;
  ILlmProvider& actor_;
  ILlmProvider& director_;
  Telemetry& telemetry_;
  WorldState world_;
  Rng rng_;
  std::mutex mu_;
};

}  // namespace grog::ai
