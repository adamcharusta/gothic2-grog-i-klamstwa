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
#include <grog/ai/tts.h>
#include <grog/ai/world.h>

#include <atomic>
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
  std::string hint;            // UTF-8 player-facing hint, set the first time an error kind occurs (else empty);
                               // also set (when the LLM gave none) for a voice error, even if the LLM call succeeded
  std::string audioWav;        // WAV of `speech`; empty when voice is off or synthesis failed
};

// A voiced line outside a talk turn (director openings, farewells). Ambient lines stay silent.
struct Speech {
  std::string text;      // UTF-8
  std::string audioWav;  // empty when voice is off or synthesis failed
  std::string hint;      // UTF-8 voice hint, set the first time a voice error kind occurs (else empty)
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
// Lifetime: the session (and the providers/TTS/telemetry it references) must outlive every future from talkAsync,
// speakAsync, farewellAsync and directAsync;
// wait for pending futures before destroying it. Keep the returned future: discarding it blocks until done.
class GameSession {
 public:
  GameSession(AiConfig config, GameData data, ILlmProvider& actor, ILlmProvider& director, Telemetry& telemetry,
              uint64_t seed);

  TurnResult talk(std::string_view npc, std::string_view playerLine, int64_t gameTimeS);
  std::future<TurnResult> talkAsync(std::string npc, std::string playerLine, int64_t gameTimeS);
  DirectorResult direct(std::string_view trigger, int64_t gameTimeS);
  std::string farewell(std::string_view npc);

  // Voice (TTS). nullptr = voice off. The provider must outlive every future from talkAsync/speakAsync/
  // farewellAsync. Synthesis runs without holding the world lock. Results are discarded (empty Speech) when a
  // save is loaded while they are in flight.
  // Repeated failures turn the voice off for the session (until loadState): see ttsOffReason_.
  void setTts(ITtsProvider* tts);
  // `cacheable` lines are kept as <cacheDir>/<hash>.wav and synthesized only once (fixed texts like farewells).
  std::future<Speech> speakAsync(std::string npc, std::string textUtf8, bool cacheable);
  std::future<Speech> farewellAsync(std::string npc);  // farewell() + speak, cacheable

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
    int ttsRequests = 0, ttsErrors = 0;  // provider calls (cache hits are not requests)
    long long ttsAudioMs = 0;
    double ttsCostUsd = 0.0, ttsLatencySumMs = 0.0;
  };
  // Logs the request and adds it to the stats. Takes mu_ itself.
  void record(std::string_view role, std::string_view npc, const ILlmProvider& p, const Result<LlmReply>& res,
              std::string_view requestText);
  std::set<ErrorKind> hinted_;     // error kinds already explained to the player
  std::set<ErrorKind> hintedTts_;  // voice error kinds already explained to the player
  // Voice backoff (under mu_): a non-transient error, or kTtsMaxTransientFails transient ones in a row, turns the
  // voice off until loadState. While off, voice() makes no requests and reports `ttsOffReason_` as its error.
  std::optional<ErrorKind> ttsOffReason_;
  int ttsTransientFails_ = 0;

  struct Voiced {
    std::string wav;
    std::optional<ErrorKind> error;
  };
  // Synthesizes `text` for `npc` (cache first when `cacheable`), logs it and adds it to the stats.
  // Must be called without holding mu_. Returns an empty wav when voice is off, turned off by the backoff, or
  // synthesis failed.
  Voiced voice(const std::string& npc, const std::string& text, bool cacheable);
  // Voice hint for `error` the first time this kind occurs, else "". Caller holds mu_.
  std::string takeVoiceHint(const std::optional<ErrorKind>& error);
  Speech speak(const std::string& npc, std::string text, bool cacheable, uint64_t gen);
  // The text part of talk(). `gen` is the generation the turn started in; `voiceable` is false when the answer
  // was discarded or the NPC is unknown (nothing to voice).
  TurnResult answer(std::string_view npc, std::string_view playerLine, int64_t gameTimeS, uint64_t& gen,
                    bool& voiceable);

  AiConfig config_;
  GameData data_;
  ILlmProvider& actor_;
  ILlmProvider& director_;
  Telemetry& telemetry_;
  std::atomic<ITtsProvider*> tts_{nullptr};
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
