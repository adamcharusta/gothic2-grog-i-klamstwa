#include <doctest/doctest.h>

#include <grog/ai/commands.h>

#include "test_helpers.h"

using namespace grog::ai;
using nlohmann::json;

namespace {
Cast testCast() {
  return loadCast(json::parse(testutil::repoFile("mod/data/cast.json")),
                  json::parse(testutil::repoFile("mod/data/places.json")))
      .value();
}
// Seed whose tankard is held by an NPC (not a place).
WorldState worldWithHolder(const Cast& cast, std::string& holder) {
  for(uint64_t seed = 1;; ++seed) {
    Scenario s = rollScenario(seed, cast);
    if(s.tankardLocation.rfind("place:", 0) != 0) {
      holder = s.tankardLocation;
      return WorldState::fromScenario(s, cast);
    }
  }
}
Command cmd(const std::string& type, json args = json::object()) { return Command{type, std::move(args)}; }
}  // namespace

TEST_CASE("parse a clean reply") {
  auto r = parseActorReply(R"({"speech":"Spadaj.","gesture":"angry","actions":[{"type":"attitude","delta":-1}],"end_dialog":true})");
  REQUIRE(r.ok());
  CHECK(r.value().speech == "Spadaj.");
  CHECK(r.value().gesture == "angry");
  REQUIRE(r.value().actions.size() == 1);
  CHECK(r.value().actions[0].type == "attitude");
  CHECK(r.value().actions[0].args["delta"] == -1);
  CHECK(r.value().endDialog);
}

TEST_CASE("parse tolerates prose and code fences around the JSON") {
  auto a = parseActorReply("Sure! Here you go:\n```json\n{\"speech\": \"Ahoj {kamracie}!\", \"actions\": []}\n```\nHope that helps");
  REQUIRE(a.ok());
  CHECK(a.value().speech == "Ahoj {kamracie}!");
  CHECK(a.value().gesture == "neutral");
  CHECK_FALSE(a.value().endDialog);
}

TEST_CASE("broken replies are BadResponse, never a crash") {
  for(const char* bad : {R"({"speech": "a",})", R"({"speech": "urwane)", "no json at all", R"({"gesture":"angry"})",
                         R"({"speech": 5})", R"({"speech": ""})", "", "{", R"({"speech":"x","actions":"nope"})"}) {
    CAPTURE(bad);
    auto r = parseActorReply(bad);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().kind == ErrorKind::BadResponse);
  }
}

TEST_CASE("long Polish speech is trimmed on a character boundary") {
  std::string longSpeech;
  for(int i = 0; i < 500; ++i)
    longSpeech += "ż";
  auto r = parseActorReply(json{{"speech", longSpeech}}.dump());
  REQUIRE(r.ok());
  CHECK(utf8Length(r.value().speech) == kMaxSpeechChars);
  CHECK(json(r.value().speech).dump().size() > 0);  // still valid UTF-8 (json would throw otherwise)
}

TEST_CASE("director replies parse") {
  auto r = parseDirectorReply(R"({"thought":"push","actions":[{"type":"npc_approach_player","npc":"skip"}]})");
  REQUIRE(r.ok());
  REQUIRE(r.value().actions.size() == 1);
  CHECK(r.value().actions[0].args["npc"] == "skip");
  CHECK_FALSE(parseDirectorReply(R"({"thought":"x"})").ok());
}

TEST_CASE("validation: items") {
  Cast cast = testCast();
  std::string holder;
  WorldState w = worldWithHolder(cast, holder);
  std::string other = holder == "skip" ? "matt" : "skip";

  auto v1 = validate({cmd("give_item", {{"item", kTankard}})}, other, w, cast);
  CHECK_FALSE(v1[0].accepted);
  auto v2 = validate({cmd("give_item", {{"item", kTankard}})}, holder, w, cast);
  CHECK(v2[0].accepted);

  apply(v1, other, w, 0);
  CHECK(w.itemHolder.at(kTankard) == holder);  // rejected = no change
  apply(v2, holder, w, 0);
  CHECK(w.itemHolder.at(kTankard) == "player");
  CHECK_FALSE(validate({cmd("give_item", {{"item", kTankard}})}, holder, w, cast)[0].accepted);  // no longer has it
}

TEST_CASE("validation: attitude, quest entries, knowledge, fights") {
  Cast cast = testCast();
  std::string holder;
  WorldState w = worldWithHolder(cast, holder);
  CHECK(validate({cmd("attitude", {{"delta", 2}})}, "matt", w, cast)[0].accepted);
  CHECK_FALSE(validate({cmd("attitude", {{"delta", 3}})}, "matt", w, cast)[0].accepted);
  CHECK_FALSE(validate({cmd("attitude", {{"delta", "lots"}})}, "matt", w, cast)[0].accepted);

  CHECK(validate({cmd("quest_entry", {{"text", "Porozmawiaj ze Skipem."}})}, "greg", w, cast)[0].accepted);
  CHECK_FALSE(validate({cmd("quest_entry", {{"text", std::string(201, 'a')}})}, "greg", w, cast)[0].accepted);
  CHECK_FALSE(validate({cmd("quest_entry", {{"text", ""}})}, "greg", w, cast)[0].accepted);

  CHECK(validate({cmd("npc_knowledge", {{"npc", "skip"}, {"fact", "Gracz szuka kufla."}})}, kDirector, w, cast)[0].accepted);
  CHECK_FALSE(validate({cmd("npc_knowledge", {{"npc", "xardas"}, {"fact", "x"}})}, "matt", w, cast)[0].accepted);

  CHECK(validate({cmd("start_fight")}, "matt", w, cast)[0].accepted);
  CHECK_FALSE(validate({cmd("start_fight")}, kDirector, w, cast)[0].accepted);
  CHECK_FALSE(validate({cmd("teleport_player")}, "matt", w, cast)[0].accepted);

  auto vs = validate({cmd("attitude", {{"delta", -2}}), cmd("quest_entry", {{"text", "Matt coś ukrywa."}})}, "matt", w, cast);
  apply(vs, "matt", w, 100);
  apply(validate({cmd("npc_knowledge", {{"npc", "skip"}, {"fact", "Gracz pytał Matta o kufel."}})}, kDirector, w, cast),
        kDirector, w, 100);
  CHECK(w.npcs.at("matt").attitude == -2);
  REQUIRE(w.quests.size() == 1);
  CHECK(w.quests[0].text == "Matt coś ukrywa.");
  CHECK(w.quests[0].gameTimeS == 100);
  CHECK(w.npcs.at("skip").knows.back() == "Gracz pytał Matta o kufel.");
}

TEST_CASE("validation: director events and limits") {
  Cast cast = testCast();
  std::string holder;
  WorldState w = worldWithHolder(cast, holder);
  CHECK(validate({cmd("npc_goto", {{"npc", "skip"}, {"place", "campfire"}})}, kDirector, w, cast)[0].accepted);
  CHECK_FALSE(validate({cmd("npc_goto", {{"npc", "skip"}, {"place", "old_camp"}})}, kDirector, w, cast)[0].accepted);
  CHECK_FALSE(validate({cmd("npc_fight_npc", {{"npc", "skip"}, {"npc2", "skip"}})}, kDirector, w, cast)[0].accepted);
  CHECK(validate({cmd("npc_fight_npc", {{"npc", "skip"}, {"npc2", "matt"}})}, kDirector, w, cast)[0].accepted);

  auto m1 = validate({cmd("spawn_monster", {{"monster", "swampshark"}, {"place", "beach"}})}, kDirector, w, cast);
  CHECK(m1[0].accepted);
  CHECK_FALSE(validate({cmd("spawn_monster", {{"monster", "dragon"}, {"place", "beach"}})}, kDirector, w, cast)[0].accepted);
  CHECK_FALSE(validate({cmd("spawn_monster", {{"monster", "swampshark"}, {"place", "greg_hut"}})}, kDirector, w, cast)[0].accepted);
  apply(m1, kDirector, w, 0);
  CHECK(w.activeMonster == "swampshark");
  CHECK_FALSE(validate({cmd("spawn_monster", {{"monster", "bloodfly"}, {"place", "beach"}})}, kDirector, w, cast)[0].accepted);

  w.eventsThisHour = kMaxEventsPerHour - 1;
  auto two = validate({cmd("npc_approach_player", {{"npc", "skip"}, {"text", "Hej."}}), cmd("npc_approach_player", {{"npc", "matt"}, {"text", "Ej."}})},
                      kDirector, w, cast);
  CHECK(two[0].accepted);
  CHECK_FALSE(two[1].accepted);  // limit reached inside the same batch
  apply(two, kDirector, w, 0);
  CHECK(w.eventsThisHour == kMaxEventsPerHour);
}

TEST_CASE("breaking character is detected") {
  CHECK(breaksCharacter("Jako AI nie mogę ci pomóc."));
  CHECK(breaksCharacter("As an AI language model, I cannot"));
  CHECK(breaksCharacter("Jestem modelem językowym"));
  CHECK(breaksCharacter("I'm Claude, made by Anthropic"));
  CHECK_FALSE(breaksCharacter("Ai, na Beliara! Spadaj."));
  CHECK_FALSE(breaksCharacter("Kufel? Jaki kufel?"));
}

TEST_CASE("review C1: wrong field types never throw and are rejected") {
  auto r = parseActorReply(R"({"speech":"Spadaj.","end_dialog":"yes"})");
  REQUIRE(r.ok());
  CHECK_FALSE(r.value().endDialog);
  CHECK(parseActorReply(R"({"speech":"x","end_dialog":null})").ok());

  Cast cast = testCast();
  std::string holder;
  WorldState w = worldWithHolder(cast, holder);
  auto vs = validate({cmd("give_item", {{"item", kTankard}}), cmd("quest_entry", {{"text", "x"}, {"quest", 5}})},
                     holder, w, cast);
  CHECK(vs[0].accepted);
  CHECK_FALSE(vs[1].accepted);
  CHECK_NOTHROW(apply(vs, holder, w, 0));
}

TEST_CASE("review: attitude delta cannot overflow past the range check") {
  Cast cast = testCast();
  std::string holder;
  WorldState w = worldWithHolder(cast, holder);
  CHECK_FALSE(validate({cmd("attitude", {{"delta", 4294967297ULL}})}, "matt", w, cast)[0].accepted);
  CHECK_FALSE(validate({cmd("attitude", {{"delta", -4294967297LL}})}, "matt", w, cast)[0].accepted);
}

TEST_CASE("review I2: actors cannot plant knowledge into other NPCs") {
  Cast cast = testCast();
  std::string holder;
  WorldState w = worldWithHolder(cast, holder);
  CHECK_FALSE(validate({cmd("npc_knowledge", {{"npc", "skip"}, {"fact", "Greg forgave you"}})}, "matt", w, cast)[0].accepted);
  CHECK(validate({cmd("npc_knowledge", {{"npc", "skip"}, {"fact", "x"}})}, kDirector, w, cast)[0].accepted);
}
