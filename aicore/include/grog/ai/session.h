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
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace grog::ai {

struct StartSetup {
  std::string world;     // e.g. ADDONWORLD.ZEN
  std::string waypoint;  // where the hero starts
  std::string armor;     // item instance equipped at start
  int level = 1;
  std::vector<std::pair<std::string, int>> items;  // item instance -> count given at start (start.json "items")
  std::map<std::string, std::string> firstQuest;    // {pl,en}: first quest log entry (start.json "first_quest")
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
  std::map<std::string, LangText> itemGameNames;  // story item id -> in-game name (items.json "game_names")
  std::map<std::string, LangText> itemGameTexts;  // story item id -> in-game description ("game_texts")
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
  std::string hint;            // UTF-8 player-facing hint, set the first time an error kind occurs (else empty)
};

struct Ending {
  std::string outcome;  // returned | fooled | caught | gave_up
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

  // Story items the engine found in the hero's inventory (ids from items.json).
  void syncPlayerItems(const std::set<std::string>& storyItemsOnPlayer);
  // A fist fight between the player and an NPC ended.
  void reportFight(std::string_view npc, bool playerWon, int64_t gameTimeS);

  // Game Master clock: due on the interval or after a trigger (min gap), never while a call is in flight.
  bool directorDue(int64_t gameTimeS);
  void trigger(std::string reason);
  std::future<DirectorResult> directAsync(std::string trigger, int64_t gameTimeS);

  std::optional<Ending> ending();
  nlohmann::json summary();

  // Save/load of the whole AI state. Loading discards answers still in flight.
  nlohmann::json saveState();
  Result<bool> loadState(const nlohmann::json& j);

  // Locked copy, safe from any thread.
  WorldState worldSnapshot();
  // Direct access: only when no talk/direct call is in flight (tests, single-threaded tools).
  WorldState& world() { return world_; }
  const GameData& data() const { return data_; }
  const AiConfig& config() const { return config_; }

 private:
  std::string fallbackLine(const NpcCard& card);
  void translateItems(std::vector<Command>& actions) const;  // AI item handles -> internal ids

  struct Stats {
    int turns = 0, fallbacks = 0, directorCalls = 0, rejected = 0, verdicts = 0, requests = 0;
    long long inputTokens = 0, outputTokens = 0;
    double costUsd = 0.0, latencySumMs = 0.0, latencyMaxMs = 0.0;
  };
  // Logs the request and adds it to the stats. Takes mu_ itself.
  void record(std::string_view role, std::string_view npc, const ILlmProvider& p, const Result<LlmReply>& res,
              std::string_view requestText);
  std::set<ErrorKind> hinted_;  // error kinds already explained to the player

  AiConfig config_;
  GameData data_;
  ILlmProvider& actor_;
  ILlmProvider& director_;
  Telemetry& telemetry_;
  WorldState world_;
  Rng rng_;
  std::mutex mu_;
  uint64_t generation_ = 0;  // bumped by loadState: older in-flight answers are discarded
  int64_t lastDirectorT_ = 0;
  bool directorInFlight_ = false;
  std::set<std::string> triggers_;
  int64_t lastGameT_ = 0;
  Stats stats_;
};

}  // namespace grog::ai
