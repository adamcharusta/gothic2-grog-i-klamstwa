#pragma once
#include <grog/ai/result.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace grog::ai {

enum class ScenarioType { LostAtDice, Lost, Stolen, InHisHut };
enum class Role { Culprit, Witness, Liar, Gossip, Unaware };

const char* scenarioTypeName(ScenarioType t);
const char* roleName(Role r);

struct Clue {
  std::string id;
  std::string holderNpc;               // who knows this
  std::string fact;                    // English, uses NPC ids/place ids in {braces}: "{skip} saw ..."
  std::vector<std::string> pointsTo;   // npc ids or "place:<id>"
  bool truthful = true;                // false for liars' and gossips' stories (never lead to the truth)
};

struct Scenario {
  uint64_t seed = 0;
  std::string questGiver;       // root of the clue graph ("greg")
  ScenarioType type = ScenarioType::Stolen;
  std::string culprit;          // npc id; "greg" for InHisHut; "" when the tankard just lies somewhere
  std::string motive;
  std::string tankardLocation;  // npc id or "place:<id>"
  std::map<std::string, Role> roles;
  std::vector<Clue> clues;
};

// Daily routine the engine forces on a new game, for cast members whose original routine keeps them
// away from the camp. Either a script routine function, or one state at one waypoint all day.
struct CastRoutine {
  std::string function;  // e.g. RTN_PRESTART_1368
  std::string state;     // e.g. ZS_SMALLTALK
  std::string waypoint;  // used with `state`
  bool empty() const { return function.empty() && state.empty(); }
};

struct CastMember {
  std::string id;
  std::string name;
  std::string instance;  // Gothic script instance, e.g. PIR_1320_ADDON_GREG
  std::map<std::string, std::string> hangout;  // {pl,en}: where this NPC usually is (shown to the AI)
  CastRoutine routine;
};

struct Cast {
  std::string questGiver;
  std::vector<CastMember> npcs;
  std::vector<std::string> places;
  std::map<std::string, std::map<std::string, std::string>> placeNames;  // id -> {pl,en}
  std::map<std::string, std::string> placeWaypoints;                    // id -> engine waypoint
  std::map<std::string, std::string> itemAiName;                        // story item id -> neutral handle the AI sees
  std::map<std::string, std::string> itemDescription;                   // story item id -> description for the AI

  std::string aiNameOf(const std::string& itemId) const;
  std::string itemIdForAiName(const std::string& aiName) const;  // "" if unknown
  std::vector<std::string> motives;

  std::string displayName(const std::string& npcId) const;
  bool hasNpc(const std::string& npcId) const;
  bool hasPlace(const std::string& placeId) const;
};

Result<Cast> loadCast(const nlohmann::json& cast, const nlohmann::json& places);

// Deterministic for a given seed and cast.
Scenario rollScenario(uint64_t seed, const Cast& cast);

// True if the truth (tankardLocation) is reachable from the quest giver through clues
// whose holders are not in `burnedNpcs`.
bool isSolvable(const Scenario& s, const std::set<std::string>& burnedNpcs);

nlohmann::json toJson(const Scenario& s);
Result<Scenario> scenarioFromJson(const nlohmann::json& j);

}  // namespace grog::ai
