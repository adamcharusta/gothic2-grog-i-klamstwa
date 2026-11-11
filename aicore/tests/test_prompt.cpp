#include <doctest/doctest.h>

#include <algorithm>

#include <grog/ai/cards.h>
#include <grog/ai/prompt.h>

#include "test_helpers.h"

using namespace grog::ai;
using nlohmann::json;

namespace {
Cast testCast() {
  return loadCast(json::parse(testutil::repoFile("mod/data/cast.json")),
                  json::parse(testutil::repoFile("mod/data/places.json")))
      .value();
}
NpcCard card(const std::string& id) {
  auto c = loadCard(json::parse(testutil::repoFile("mod/data/npc/" + id + ".json")));
  REQUIRE(c.ok());
  return c.value();
}
Lore lore() {
  auto l = loadLore(json::parse(testutil::repoFile("mod/data/lore/camp.json")));
  REQUIRE(l.ok());
  return l.value();
}
std::string rules() { return testutil::repoFile("mod/data/rules/actor.md"); }

// Card for an arbitrary npc id, built from Skip's card (actor prompts only need a card).
NpcCard cardAs(const std::string& id) {
  NpcCard c = card("skip");
  c.id = id;
  c.name = id;
  return c;
}

std::string truthfulFactsRendered(const Scenario& s, const Cast& cast, Language lang) {
  std::string all;
  for(auto& c : s.clues)
    if(c.truthful)
      all += renderFact(c.fact, cast, lang) + "\n";
  return all;
}
}  // namespace

TEST_CASE("cards load and reject missing required texts") {
  CHECK(card("greg").name == "Greg");
  CHECK(card("matt").items == std::vector<std::string>{"tankard_lookalike"});
  json j = json::parse(testutil::repoFile("mod/data/npc/skip.json"));
  j["fallback_lines"].erase("pl");
  CHECK_FALSE(loadCard(j).ok());
}

TEST_CASE("cards load the voice") {
  NpcCard greg = card("greg");
  CHECK(greg.voice.name == "onyx");
  CHECK(greg.voice.original == 1);
  CHECK_FALSE(greg.voice.style["en"].empty());
  CHECK_FALSE(greg.voice.style["pl"].empty());
}

TEST_CASE("voice name is validated") {
  json j = json::parse(testutil::repoFile("mod/data/npc/skip.json"));
  j["voice"] = json{{"name", 5}};
  CHECK_FALSE(loadCard(j).ok());
  j["voice"] = json{{"name", "ash"}, {"style", json{{"en", 7}}}};
  CHECK_FALSE(loadCard(j).ok());
  j["voice"] = "ash";
  CHECK_FALSE(loadCard(j).ok());
  j["voice"] = json{{"name", "ash"}, {"original", "eight"}};
  CHECK_FALSE(loadCard(j).ok());
  j["voice"] = json{{"name", "ash"}, {"original", -1}};
  CHECK_FALSE(loadCard(j).ok());
  j.erase("voice");
  auto c = loadCard(j);
  REQUIRE(c.ok());
  CHECK(c.value().voice.name.empty());
  CHECK(c.value().voice.original == 0);
  CHECK(c.value().voice.style.empty());
}

TEST_CASE("renderFact replaces ids with names and place names") {
  Cast cast = testCast();
  CHECK(renderFact("{alligator_jack} saw {greg} near {place:campfire}.", cast, Language::Pl) ==
        "Alligator Jack saw Greg near ognisko w obozie.");
  CHECK(renderFact("{place:campfire}", cast, Language::En) == "the camp fire");
}

TEST_CASE("an unaware NPC never sees the truth") {
  Cast cast = testCast();
  for(uint64_t seed = 1; seed <= 100; ++seed) {
    WorldState w = WorldState::fromScenario(rollScenario(seed, cast), cast);
    for(auto& [npc, role] : w.scenario.roles) {
      bool holdsClue = false;
      for(auto& c : w.scenario.clues)
        holdsClue |= (c.holderNpc == npc);
      if(role != Role::Unaware || holdsClue)
        continue;
      auto req = buildActorRequest(cardAs(npc), w, cast, lore(), Language::Pl, "Gdzie jest kufel?", rules());
      CAPTURE(seed);
      CAPTURE(npc);
      CHECK(req.system.find("culprit") == std::string::npos);
      CHECK(req.system.find(w.scenario.tankardLocation) == std::string::npos);
      for(auto& c : w.scenario.clues)
        if(c.truthful)
          CHECK(req.system.find(renderFact(c.fact, cast, Language::Pl)) == std::string::npos);
    }
  }
}

TEST_CASE("a witness gets exactly its own clues, not the others") {
  Cast cast = testCast();
  WorldState w = WorldState::fromScenario(rollScenario(17, cast), cast);
  const Clue* mine = nullptr;
  for(auto& c : w.scenario.clues)
    if(c.truthful && w.scenario.roles.at(c.holderNpc) == Role::Witness) {
      mine = &c;
      break;
    }
  REQUIRE(mine != nullptr);
  auto req = buildActorRequest(cardAs(mine->holderNpc), w, cast, lore(), Language::En, "Hi", rules());
  CHECK(req.system.find(renderFact(mine->fact, cast, Language::En)) != std::string::npos);
  for(auto& c : w.scenario.clues)
    if(c.holderNpc != mine->holderNpc && c.truthful)
      CHECK(req.system.find(renderFact(c.fact, cast, Language::En)) == std::string::npos);
}

TEST_CASE("director sees the full truth") {
  Cast cast = testCast();
  WorldState w = WorldState::fromScenario(rollScenario(23, cast), cast);
  auto req = buildDirectorRequest(w, cast, lore(), "player_talked:skip", testutil::repoFile("mod/data/rules/director.md"));
  CHECK(req.system.find(w.scenario.tankardLocation) != std::string::npos);
  CHECK(req.system.find(truthfulFactsRendered(w.scenario, cast, Language::En).substr(0, 20)) != std::string::npos);
  CHECK(req.system.find("solvable") != std::string::npos);
  REQUIRE(req.messages.size() == 1);
  CHECK(req.messages[0].text.find("player_talked:skip") != std::string::npos);
  CHECK(req.replySchema.contains("properties"));
}

TEST_CASE("language instruction, memory window and Polish bytes") {
  Cast cast = testCast();
  WorldState w = WorldState::fromScenario(rollScenario(4, cast), cast);
  for(int i = 0; i < 20; ++i)
    w.npcs["greg"].memory.push_back({i % 2 == 0 ? ChatMessage::User : ChatMessage::Assistant, "turn " + std::to_string(i)});
  auto req = buildActorRequest(card("greg"), w, cast, lore(), Language::Pl, "Gdzie jest mój kufel? Żółć!", rules());
  CHECK(req.system.find("Always reply in Polish") != std::string::npos);
  CHECK(req.system.find("Twardy, głośny i porywczy") != std::string::npos);  // Polish personality, byte-exact
  REQUIRE(req.messages.size() == 13);  // 12 most recent + new line
  CHECK(req.messages.front().text == "turn 8");
  CHECK(req.messages.back().text == "Gdzie jest mój kufel? Żółć!");
  CHECK(req.messages.back().role == ChatMessage::User);
  CHECK(req.replySchema["properties"].contains("speech"));

  auto en = buildActorRequest(card("greg"), w, cast, lore(), Language::En, "Hi", rules());
  CHECK(en.system.find("Always reply in English") != std::string::npos);
}

TEST_CASE("playtest 2: everyone knows where the camp members usually are") {
  Cast cast = testCast();
  for(auto& m : cast.npcs) {
    INFO(m.id);
    CHECK_FALSE(m.hangout["pl"].empty());
    CHECK_FALSE(m.hangout["en"].empty());
  }
  auto malcolm = std::find_if(cast.npcs.begin(), cast.npcs.end(), [](auto& m) { return m.id == "malcolm"; });
  REQUIRE(malcolm != cast.npcs.end());
  WorldState w = WorldState::fromScenario(rollScenario(4, cast), cast);
  auto pl = buildActorRequest(card("greg"), w, cast, lore(), Language::Pl, "Gdzie jest Malcolm?", rules());
  CHECK(pl.system.find("# Where people usually are") != std::string::npos);
  CHECK(pl.system.find("- Malcolm: " + malcolm->hangout.at("pl")) != std::string::npos);
  auto dir = buildDirectorRequest(w, cast, lore(), "tick", testutil::repoFile("mod/data/rules/director.md"));
  CHECK(dir.system.find("- malcolm: " + malcolm->hangout.at("en")) != std::string::npos);
}
