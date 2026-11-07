#include <doctest/doctest.h>

#include <grog/ai/world.h>

#include "test_helpers.h"

using namespace grog::ai;
using nlohmann::json;

namespace {
Cast testCast() {
  return loadCast(json::parse(testutil::repoFile("mod/data/cast.json")),
                  json::parse(testutil::repoFile("mod/data/places.json")))
      .value();
}
}  // namespace

TEST_CASE("world starts with the tankard where the scenario put it") {
  Cast cast = testCast();
  for(uint64_t seed = 1; seed <= 50; ++seed) {
    Scenario s = rollScenario(seed, cast);
    WorldState w = WorldState::fromScenario(s, cast);
    CHECK(w.itemHolder.at(kTankard) == s.tankardLocation);
    CHECK(w.npcs.size() == cast.npcs.size());
    CHECK(w.npcs.at("greg").attitude == 0);
  }
}

TEST_CASE("facts are filtered by who knows them") {
  WorldState w = WorldState::fromScenario(rollScenario(3, testCast()), testCast());
  w.addFact({10, "Gracz pobił Matta przy ognisku.", {"matt", "skip"}});
  w.addFact({20, "Greg krzyczał na cały obóz.", {}});  // empty = everyone
  CHECK(w.factsKnownBy("matt").size() == 2);
  CHECK(w.factsKnownBy("garett").size() == 1);
}

TEST_CASE("NPCs with very low attitude count as burned") {
  WorldState w = WorldState::fromScenario(rollScenario(3, testCast()), testCast());
  w.npcs["matt"].attitude = -3;
  w.npcs["skip"].attitude = -2;
  CHECK(w.burnedNpcs() == std::set<std::string>{"matt"});
}

TEST_CASE("world JSON round-trip is lossless, including Polish text") {
  Cast cast = testCast();
  WorldState w = WorldState::fromScenario(rollScenario(11, cast), cast);
  w.npcs["skip"].memory.push_back({ChatMessage::User, "Gdzie jest kufel? Żółć!"});
  w.npcs["skip"].memory.push_back({ChatMessage::Assistant, "Nie wiem, ąę."});
  w.npcs["skip"].summary = "Gracz pytał o kufel.";
  w.npcs["skip"].knows.push_back("player_is_asking_about_tankard");
  w.addFact({42, "Łódź odpłynęła.", {"skip"}});
  w.quests.push_back({"tankard", "Porozmawiaj ze Skipem.", 42});
  w.itemHolder["greg_tankard"] = "player";
  w.eventsThisHour = 3;
  w.activeMonster = "swampshark";

  json j = toJson(w);
  CHECK(j["version"] == kWorldSaveVersion);
  auto back = worldFromJson(j);
  REQUIRE(back.ok());
  CHECK(toJson(back.value()).dump() == j.dump());
  CHECK(back.value().npcs.at("skip").memory[0].text == "Gdzie jest kufel? Żółć!");
}

TEST_CASE("world JSON with a different version is rejected") {
  Cast cast = testCast();
  json j = toJson(WorldState::fromScenario(rollScenario(1, cast), cast));
  j["version"] = kWorldSaveVersion + 1;
  auto r = worldFromJson(j);
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().kind == ErrorKind::Io);
}
