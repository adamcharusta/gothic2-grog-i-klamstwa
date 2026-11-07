#include <doctest/doctest.h>

#include <grog/ai/scenario.h>

#include "test_helpers.h"

#include <set>

using namespace grog::ai;
using nlohmann::json;

namespace {
Cast loadTestCast() {
  auto c = loadCast(json::parse(testutil::repoFile("mod/data/cast.json")),
                    json::parse(testutil::repoFile("mod/data/places.json")));
  REQUIRE(c.ok());
  return c.value();
}

// Every NPC that holds a clue on some path (candidates for "burning").
std::set<std::string> clueHolders(const Scenario& s) {
  std::set<std::string> out;
  for(auto& c : s.clues)
    if(c.holderNpc != "greg")
      out.insert(c.holderNpc);
  return out;
}
}  // namespace

TEST_CASE("cast loads from mod data") {
  Cast c = loadTestCast();
  CHECK(c.questGiver == "greg");
  CHECK(c.npcs.size() == 16);
  CHECK(c.displayName("alligator_jack") == "Alligator Jack");
  CHECK(c.places.size() == 8);
  CHECK(c.motives.size() == 5);
}

TEST_CASE("cast rejects data without the quest giver") {
  auto c = loadCast(json::parse(R"({"schema":1,"quest_giver":"greg","npcs":[{"id":"skip","name":"Skip"}],"motives":["x"]})"),
                    json::parse(R"({"schema":1,"places":[{"id":"greg_hut","name":{"pl":"a","en":"b"}}]})"));
  CHECK_FALSE(c.ok());
}

TEST_CASE("same seed gives the identical scenario") {
  Cast cast = loadTestCast();
  CHECK(toJson(rollScenario(2026, cast)).dump() == toJson(rollScenario(2026, cast)).dump());
  CHECK(toJson(rollScenario(2026, cast)).dump() != toJson(rollScenario(2027, cast)).dump());
}

TEST_CASE("500 seeds: rules hold and the truth survives losing any single witness") {
  Cast cast = loadTestCast();
  std::set<ScenarioType> types;
  for(uint64_t seed = 1; seed <= 500; ++seed) {
    Scenario s = rollScenario(seed, cast);
    types.insert(s.type);
    CAPTURE(seed);
    CAPTURE(toJson(s).dump());

    if(s.type == ScenarioType::InHisHut) {
      CHECK(s.culprit == "greg");
      CHECK(s.tankardLocation == "place:greg_hut");
    } else {
      CHECK(s.culprit != "greg");
    }
    if(s.type == ScenarioType::LostAtDice || s.type == ScenarioType::Stolen) {
      CHECK_FALSE(s.culprit.empty());
      CHECK(s.tankardLocation == s.culprit);
    }
    if(s.type == ScenarioType::Lost) {
      if(s.culprit.empty())
        CHECK(s.tankardLocation.rfind("place:", 0) == 0);
      else
        CHECK(s.tankardLocation == s.culprit);
    }
    CHECK(s.roles.at("greg") == Role::Unaware);
    if(!s.culprit.empty() && s.culprit != "greg")
      CHECK(s.roles.at(s.culprit) == Role::Culprit);

    CHECK(isSolvable(s, {}));
    for(auto& x : clueHolders(s))
      CHECK_MESSAGE(isSolvable(s, {x}), "burning " << x);
  }
  CHECK(types.size() == 4);
}

TEST_CASE("burning both paths makes it unsolvable") {
  Cast cast = loadTestCast();
  Scenario s = rollScenario(5, cast);
  CHECK_FALSE(isSolvable(s, clueHolders(s)));
}

TEST_CASE("scenario round-trips through JSON") {
  Cast cast = loadTestCast();
  Scenario s = rollScenario(99, cast);
  auto back = scenarioFromJson(toJson(s));
  REQUIRE(back.ok());
  CHECK(toJson(back.value()).dump() == toJson(s).dump());
}
