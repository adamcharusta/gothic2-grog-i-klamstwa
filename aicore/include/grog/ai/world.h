#pragma once
#include <grog/ai/llm.h>
#include <grog/ai/result.h>
#include <grog/ai/scenario.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace grog::ai {

inline constexpr const char* kTankard = "greg_tankard";
inline constexpr int kWorldSaveVersion = 2;
inline constexpr const char* kRewardKey = "key_reward_chest";

struct Fact {
  int64_t gameTimeS = 0;
  std::string text;
  std::vector<std::string> knownBy;  // empty = common knowledge in the camp
};

struct NpcState {
  int attitude = 0;                  // towards the player, -5..5; <= -3 means "won't help any more"
  std::vector<std::string> knows;    // extra facts/rumours learned during play (ids or short text)
  std::deque<ChatMessage> memory;    // recent exchanges with the player
  std::string summary;               // older exchanges, summarised
  bool knockedOutByPlayer = false;
};

struct QuestEntry {
  std::string quest;
  std::string text;
  int64_t gameTimeS = 0;
};

class WorldState {
 public:
  Scenario scenario;
  std::map<std::string, NpcState> npcs;
  std::map<std::string, std::string> itemHolder;  // item id -> npc id | "place:<id>" | "player"
  std::vector<Fact> facts;
  std::vector<QuestEntry> quests;
  int eventsThisHour = 0;
  std::string activeMonster;  // "" when none
  std::string ending;         // "" while playing; returned | fooled | caught | gave_up

  static WorldState fromScenario(const Scenario& s, const Cast& cast);

  void addFact(Fact f) { facts.push_back(std::move(f)); }
  std::vector<Fact> factsKnownBy(std::string_view npc) const;
  std::set<std::string> burnedNpcs() const;
};

nlohmann::json toJson(const WorldState& w);
Result<WorldState> worldFromJson(const nlohmann::json& j);

}  // namespace grog::ai
