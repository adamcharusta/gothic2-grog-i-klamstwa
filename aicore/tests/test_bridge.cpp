#include <doctest/doctest.h>

#include <grog/ai/bridge.h>

using namespace grog::ai;
using nlohmann::json;

namespace {
GameData data() {
  auto d = loadGameData(pathFromUtf8(std::string(GROG_REPO_ROOT) + "/mod/data"));
  REQUIRE(d.ok());
  return d.value();
}
Verdict ok(std::string type, json args) { return Verdict{Command{std::move(type), std::move(args)}, true, ""}; }
}  // namespace

TEST_CASE("Polish letters round-trip through the game code page") {
  std::string pl = "Żółć ąęłńóśźż ĄĘŁŃÓŚŹŻ";
  std::string game = toGameText(pl);
  CHECK(game.size() == utf8Length(pl));  // one byte per character
  CHECK(static_cast<unsigned char>(toGameText("ł")[0]) == 0xB3);
  CHECK(static_cast<unsigned char>(toGameText("ż")[0]) == 0xBF);
  CHECK(fromGameText(game) == pl);
}

TEST_CASE("typographic punctuation and unknown characters become plain ASCII") {
  CHECK(toGameText("„cytat” – x… ‘a’ — b") == "\"cytat\" - x... 'a' - b");
  CHECK(toGameText("emoji 😀 and 中") == "emoji ? and ?");
  CHECK(toGameText("plain ASCII") == "plain ASCII");
}

TEST_CASE("invalid UTF-8 input does not crash and is replaced") {
  std::string bad = std::string("ab") + char(0xC5) + "c";  // truncated sequence
  CHECK(toGameText(bad) == "ab?c");
}

TEST_CASE("NPC instance lookup is case-insensitive") {
  GameData d = data();
  CHECK(npcIdForInstance(d, "PIR_1320_ADDON_GREG") == std::optional<std::string>("greg"));
  CHECK(npcIdForInstance(d, "pir_1368_addon_malcom") == std::optional<std::string>("malcolm"));
  CHECK_FALSE(npcIdForInstance(d, "BDT_1000_BANDIT").has_value());
  CHECK(instanceForNpc(d, "skip") == "PIR_1355_ADDON_SKIP");
}

TEST_CASE("accepted verdicts become engine actions; rejected ones do nothing") {
  GameData d = data();
  std::vector<Verdict> vs = {
      ok("give_item", {{"item", "greg_tankard"}}),
      ok("quest_entry", {{"text", "Porozmawiaj ze Skipem."}}),
      ok("start_fight", json::object()),
      ok("attitude", {{"delta", 1}}),  // no engine effect
      Verdict{Command{"give_item", {{"item", "key_reward_chest"}}}, false, "no"},
  };
  auto acts = toEngineActions(vs, "matt", d);
  REQUIRE(acts.size() == 3);
  CHECK(acts[0].kind == EngineAction::GiveItemToPlayer);
  CHECK(acts[0].npcInstance == "PIR_1365_ADDON_MATT");
  CHECK(acts[0].itemInstance == "ITMI_SILVERCUP");
  CHECK(acts[1].kind == EngineAction::QuestEntry);
  CHECK(acts[1].text == toGameText("Porozmawiaj ze Skipem."));
  CHECK(acts[2].kind == EngineAction::StartFight);
  CHECK(acts[2].npcInstance == "PIR_1365_ADDON_MATT");
}

TEST_CASE("director actions map npcs and places to instances and waypoints") {
  GameData d = data();
  auto acts = toEngineActions({ok("npc_goto", {{"npc", "skip"}, {"place", "campfire"}}),
                               ok("npc_fight_npc", {{"npc", "angus"}, {"npc2", "hank"}}),
                               ok("spawn_monster", {{"monster", "swampshark"}, {"place", "beach"}}),
                               ok("npc_approach_player", {{"npc", "bill"}}),
                               ok("ambient_line", {{"npc", "samuel"}, {"text", "Grog za darmo!"}})},
                              kDirector, d);
  REQUIRE(acts.size() == 5);
  CHECK(acts[0].kind == EngineAction::NpcGoto);
  CHECK(acts[0].npcInstance == "PIR_1355_ADDON_SKIP");
  CHECK(acts[0].waypoint == "ADW_PIRATECAMP_CENTER");
  CHECK(acts[1].npc2Instance == "PIR_1371_ADDON_HANK");
  CHECK(acts[2].kind == EngineAction::SpawnMonster);
  CHECK(acts[2].monsterInstance == "SWAMPSHARK");
  CHECK(acts[2].waypoint == "ADW_PIRATECAMP_BEACH_10");
  CHECK(acts[3].kind == EngineAction::NpcApproachPlayer);
  CHECK(acts[4].text == toGameText("Grog za darmo!"));
}
