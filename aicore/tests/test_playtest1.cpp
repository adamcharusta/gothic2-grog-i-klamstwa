// Plan 5a: fixes from the first playtest (beta.5, gpt-4.1-mini). Each case reproduces what the log showed.
#include <doctest/doctest.h>

#include <grog/ai/bridge.h>
#include <grog/ai/commands.h>
#include <grog/ai/prompt.h>
#include <grog/ai/session.h>

#include <sstream>

#include "test_helpers.h"

using namespace grog::ai;
using nlohmann::json;

namespace {
GameData data() {
  auto d = loadGameData(pathFromUtf8(std::string(GROG_REPO_ROOT) + "/mod/data"));
  REQUIRE(d.ok());
  return d.value();
}
struct Fx {
  MockProvider actor, director;
  std::ostringstream log;
  Telemetry tel{log, false, Pricing{}};
  GameData d = data();
  std::unique_ptr<GameSession> s;
  Fx() {
    AiConfig cfg;
    s = std::make_unique<GameSession>(cfg, d, actor, director, tel, 8);
  }
  TurnResult say(const std::string& npc, const json& reply) {
    actor.scripted = {reply.dump()};
    return s->talk(npc, "...", 10);
  }
};
}  // namespace

TEST_CASE("playtest: quest_entry sent with 'fact' instead of 'text' is accepted") {
  // Log: {"type":"quest_entry","fact":"Greg pamięta, że pił z Mattem..."} -> rejected "empty text" (3 of 3).
  auto r = parseActorReply(R"({"speech":"Szukaj!","actions":[{"type":"quest_entry","fact":"Greg pił z Mattem."}]})");
  REQUIRE(r.ok());
  REQUIRE(r.value().actions.size() == 1);
  CHECK(r.value().actions[0].args["text"] == "Greg pił z Mattem.");

  Fx f;
  auto t = f.say("greg", {{"speech", "Szukaj!"}, {"actions", {{{"type", "quest_entry"}, {"fact", "Greg pił z Mattem."}}}}});
  REQUIRE(t.verdicts.size() == 1);
  CHECK(t.verdicts[0].accepted);
  REQUIRE_FALSE(f.s->world().quests.empty());
  CHECK(f.s->world().quests.back().text == "Greg pił z Mattem.");
}

TEST_CASE("playtest: attitude nested in 'items' or given as a string is accepted") {
  // Log: {"type":"attitude","items":{"npc":"Matt","delta":1}} -> rejected "delta must be an integer".
  for(const char* raw : {R"({"speech":"x","actions":[{"type":"attitude","items":{"npc":"Matt","delta":1}}]})",
                         R"({"speech":"x","actions":[{"type":"attitude","delta":"1"}]})",
                         R"({"speech":"x","actions":[{"type":"attitude","delta":1.0}]})"}) {
    CAPTURE(raw);
    auto r = parseActorReply(raw);
    REQUIRE(r.ok());
    REQUIRE(r.value().actions.size() == 1);
    CHECK(r.value().actions[0].args["delta"] == 1);
    CHECK(r.value().actions[0].args["delta"].is_number_integer());
  }
  // Still rejected: not a number at all.
  auto bad = parseActorReply(R"({"speech":"x","actions":[{"type":"attitude","delta":"dużo"}]})");
  REQUIRE(bad.ok());
  Fx f;
  CHECK_FALSE(validate(bad.value().actions, "greg", f.s->world(), f.d.cast)[0].accepted);
}

TEST_CASE("playtest: director writes player-visible text in the game language") {
  // Log: language=pl, but npc_approach_player text came in English.
  Fx f;
  auto pl = buildDirectorRequest(f.s->world(), f.d.cast, f.d.lore, "tick", f.d.directorRules, Language::Pl);
  CHECK(pl.system.find("in Polish") != std::string::npos);
  auto en = buildDirectorRequest(f.s->world(), f.d.cast, f.d.lore, "tick", f.d.directorRules, Language::En);
  CHECK(en.system.find("in English") != std::string::npos);
}

TEST_CASE("playtest: the same NPC is not sent to the player again and again") {
  // Log: Matt was sent three times in four minutes with the same line.
  Fx f;
  const Cast& cast = f.d.cast;
  WorldState w = f.s->world();
  auto approach = [](const char* npc, const char* text) {
    return Command{"npc_approach_player", {{"npc", npc}, {"text", text}}};
  };
  // An approach needs the line the NPC opens with.
  CHECK_FALSE(validate({Command{"npc_approach_player", {{"npc", "matt"}}}}, kDirector, w, cast, 100)[0].accepted);
  auto v = validate({approach("matt", "Ej, ty! Chodź tu.")}, kDirector, w, cast, 100);
  REQUIRE(v[0].accepted);
  apply(v, kDirector, w, 100);
  CHECK_FALSE(validate({approach("matt", "Znowu ja.")}, kDirector, w, cast, 100 + 300)[0].accepted);
  CHECK(validate({approach("skip", "Hej.")}, kDirector, w, cast, 100 + 300)[0].accepted);
  CHECK(validate({approach("matt", "Wróciłem.")}, kDirector, w, cast, 100 + kApproachCooldownS)[0].accepted);
  // The opening line becomes the NPC's own words in its memory, so the conversation continues from it.
  REQUIRE_FALSE(w.npcs.at("matt").memory.empty());
  CHECK(w.npcs.at("matt").memory.back().role == ChatMessage::Assistant);
  CHECK(w.npcs.at("matt").memory.back().text == "Ej, ty! Chodź tu.");
}

TEST_CASE("playtest: ambient_line without an npc is rejected with a clear reason") {
  Fx f;
  auto v = validate({Command{"ambient_line", {{"text", "Ognisko trzaska."}}}}, kDirector, f.s->world(), f.d.cast, 0);
  CHECK_FALSE(v[0].accepted);
  CHECK(v[0].reason.find("needs the npc") != std::string::npos);
}

TEST_CASE("playtest: the director event limit resets every game hour") {
  Fx f;
  f.director.scripted = {
      R"({"actions":[{"type":"npc_goto","npc":"skip","place":"campfire"}]})",
      R"({"actions":[{"type":"npc_goto","npc":"skip","place":"campfire"}]})",
  };
  // Pretend the hour is full (through a save, the way a real game gets there).
  json st = f.s->saveState();
  st["world"]["events_this_hour"] = kMaxEventsPerHour;
  st["world"]["events_hour_start"] = 0;
  REQUIRE(f.s->loadState(st).ok());
  auto r1 = f.s->direct("tick", 100);
  REQUIRE(r1.verdicts.size() == 1);
  CHECK_FALSE(r1.verdicts[0].accepted);  // still the same hour
  auto r2 = f.s->direct("tick", 3600 + 100);
  REQUIRE(r2.verdicts.size() == 1);
  CHECK(r2.verdicts[0].accepted);  // new hour
}

TEST_CASE("playtest: new world fields survive a save") {
  Fx f;
  WorldState w = f.s->world();
  w.lastApproachS["matt"] = 123;
  w.eventsHourStartS = 3600;
  auto back = worldFromJson(toJson(w));
  REQUIRE(back.ok());
  CHECK(back.value().lastApproachS.at("matt") == 123);
  CHECK(back.value().eventsHourStartS == 3600);
  // v2 saves from beta.5 have no such fields and still load.
  json old = toJson(f.s->world());
  old.erase("last_approach");
  old.erase("events_hour_start");
  CHECK(worldFromJson(old).ok());
}

TEST_CASE("playtest: the director's approach line reaches the engine") {
  GameData d = data();
  std::vector<Verdict> v{Verdict{Command{"npc_approach_player", {{"npc", "matt"}, {"text", "Ej, ty!"}}}, true, ""}};
  auto a = toEngineActions(v, kDirector, d);
  REQUIRE(a.size() == 1);
  CHECK(a[0].kind == EngineAction::NpcApproachPlayer);
  CHECK(a[0].text == "Ej, ty!");
}

TEST_CASE("playtest: story items are not vanilla loot and have in-game names") {
  // Silver and gold cups lie all over Jarkendar: any of them counted as Greg's tankard.
  GameData d = data();
  for(const char* id : {"greg_tankard", "tankard_lookalike"}) {
    CAPTURE(id);
    const std::string& inst = d.items.at(id);
    CHECK(inst != "ITMI_SILVERCUP");
    CHECK(inst != "ITMI_GOLDCUP");
    CHECK(inst != "ITMI_SILVERCHALICE");
    CHECK(inst != "ITMI_GOLDCHALICE");
    REQUIRE(d.itemGameNames.count(id));
    CHECK_FALSE(d.itemGameNames.at(id).at(Language::Pl).empty());
    CHECK_FALSE(d.itemGameNames.at(id).at(Language::En).empty());
    REQUIRE(d.itemGameTexts.count(id));
    CHECK_FALSE(d.itemGameTexts.at(id).at(Language::Pl).empty());
  }
  CHECK(d.items.at("greg_tankard") != d.items.at("tankard_lookalike"));
}

TEST_CASE("playtest: the hero starts with a weapon") {
  GameData d = data();
  bool weapon = false, gold = false;
  for(auto& [inst, count] : d.start.items) {
    CHECK(count > 0);
    weapon |= inst.rfind("ITMW_", 0) == 0;
    gold |= inst == "ITMI_GOLD";
  }
  CHECK(weapon);
  CHECK(gold);
}

TEST_CASE("playtest: cost is estimated for dated model names") {
  // Log: model "gpt-4.1-mini-2025-04-14", cost_usd 0, price_unknown.
  GameData d = data();
  CHECK(d.pricing.estimate("gpt-4.1-mini", 1'000'000, 0) > 0);
  CHECK(d.pricing.estimate("gpt-4.1-mini-2025-04-14", 1'000'000, 0) == d.pricing.estimate("gpt-4.1-mini", 1'000'000, 0));
  CHECK(d.pricing.estimate("gpt-4.1-2025-04-14", 1'000'000, 0) == d.pricing.estimate("gpt-4.1", 1'000'000, 0));
  CHECK(d.pricing.estimate("no-such-model", 1, 1) < 0);
}

TEST_CASE("playtest: actor rules tell pirates to take up brawls and not to invent culprits") {
  GameData d = data();
  CHECK(d.actorRules.find("fist fight") != std::string::npos);
  CHECK(d.actorRules.find("Never name someone as having the tankard") != std::string::npos);
  // Each command lists its fields.
  for(const char* field : {"\"text\"", "\"delta\"", "\"item\"", "\"outcome\""})
    CHECK(d.actorRules.find(field) != std::string::npos);
  CHECK(d.directorRules.find("`npc`") != std::string::npos);
}

TEST_CASE("playtest 2: a new game starts with the 'talk to Greg' entry in the player's language") {
  Fx f;  // default language
  REQUIRE(f.s->world().quests.size() == 1);
  CHECK(f.s->world().quests[0].quest == "tankard");
  CHECK(f.s->world().quests[0].text == f.d.start.firstQuest.at(languageName(AiConfig{}.language)));

  AiConfig en;
  en.language = Language::En;
  GameSession s(en, f.d, f.actor, f.director, f.tel, 8);
  REQUIRE(s.world().quests.size() == 1);
  CHECK(s.world().quests[0].text == f.d.start.firstQuest.at("en"));
}
