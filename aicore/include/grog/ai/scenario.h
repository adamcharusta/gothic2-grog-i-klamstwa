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

struct CastMember {
  std::string id;
  std::string name;
};

struct Cast {
  std::string questGiver;
  std::vector<CastMember> npcs;
  std::vector<std::string> places;
  std::map<std::string, std::map<std::string, std::string>> placeNames;  // id -> {pl,en}
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
