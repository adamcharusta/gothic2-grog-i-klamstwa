#include <doctest/doctest.h>

#include <grog/ai/bridge.h>
#include <grog/ai/session.h>

#include <future>
#include <sstream>

using namespace grog::ai;
using nlohmann::json;

namespace {
struct Fx {
  MockProvider actor, director;
  std::ostringstream log;
  Telemetry tel{log, false, Pricing{}};
  std::unique_ptr<GameSession> s;
  explicit Fx(uint64_t seed = 8) {
    auto d = loadGameData(pathFromUtf8(std::string(GROG_REPO_ROOT) + "/mod/data"));
    REQUIRE(d.ok());
    AiConfig cfg;
    s = std::make_unique<GameSession>(cfg, d.value(), actor, director, tel, seed);
  }
  std::string say(const std::string& npc, const json& reply) {
    actor.scripted = {reply.dump()};
    return s->talk(npc, "...", 10).speech;
  }
  const std::string& holder(const char* item) { return s->world().itemHolder.at(item); }
};
json speech(json actions) { return {{"speech", "Hm."}, {"actions", std::move(actions)}}; }
}  // namespace

TEST_CASE("task1: player items are synced from the engine") {
  Fx f;
  f.s->syncPlayerItems({"greg_tankard"});
  CHECK(f.holder("greg_tankard") == "player");
  f.s->syncPlayerItems({});
  CHECK(f.holder("greg_tankard") == "lost");
}

TEST_CASE("task1: the AI sees neutral item names, never internal ids") {
  Fx f;
  f.s->syncPlayerItems({"tankard_lookalike"});
  f.say("greg", speech(json::array()));
  const std::string& sys = f.actor.requests.back().system;
  CHECK(sys.find("plain_pewter_tankard") != std::string::npos);
  CHECK(sys.find("tankard_lookalike") == std::string::npos);
  CHECK(sys.find("greg_tankard") == std::string::npos);
  CHECK(sys.find("The player carries") != std::string::npos);
}

TEST_CASE("task1: take_item only for items the player really carries") {
  Fx f;
  f.say("greg", speech({{{"type", "take_item"}, {"item", "dented_pewter_tankard"}}}));
  CHECK(f.holder("greg_tankard") != "greg");  // player doesn't have it
  f.s->syncPlayerItems({"greg_tankard"});
  f.say("greg", speech({{{"type", "take_item"}, {"item", "dented_pewter_tankard"}}}));
  CHECK(f.holder("greg_tankard") == "greg");
}

TEST_CASE("task1: bridge maps take_item to the engine") {
  Fx f;
  f.s->syncPlayerItems({"greg_tankard"});
  f.actor.scripted = {speech({{{"type", "take_item"}, {"item", "dented_pewter_tankard"}}}).dump()};
  auto r = f.s->talk("greg", "Masz.", 1);
  auto acts = toEngineActions(r.verdicts, "greg", f.s->data());
  REQUIRE(acts.size() == 1);
  CHECK(acts[0].kind == EngineAction::TakeItemFromPlayer);
  CHECK(acts[0].itemInstance == "ITMI_SILVERCUP");
  CHECK(acts[0].npcInstance == "PIR_1320_ADDON_GREG");
}

TEST_CASE("task2: returning the real tankard ends the game and hands over the key") {
  Fx f;
  f.s->syncPlayerItems({"greg_tankard"});
  f.say("greg", speech({{{"type", "take_item"}, {"item", "dented_pewter_tankard"}},
                        {{"type", "end_game"}, {"outcome", "returned"}}}));
  REQUIRE(f.s->ending().has_value());
  CHECK(f.s->ending()->outcome == "returned");
  CHECK(f.holder("key_reward_chest") == "player");
  json sum = f.s->summary();
  CHECK(sum["outcome"] == "returned");
  CHECK(sum.contains("seed"));
  CHECK(sum.contains("turns"));
  CHECK(sum.contains("culprit"));
}

TEST_CASE("task2 / review 1: the look-alike never grants the real ending") {
  Fx f;
  f.s->syncPlayerItems({"tankard_lookalike"});
  f.say("greg", speech({{{"type", "take_item"}, {"item", "plain_pewter_tankard"}},
                        {{"type", "end_game"}, {"outcome", "returned"}}}));
  CHECK_FALSE(f.s->ending().has_value());
  CHECK(f.holder("key_reward_chest") == "greg");
  f.say("greg", speech({{{"type", "end_game"}, {"outcome", "fooled"}}}));
  REQUIRE(f.s->ending().has_value());
  CHECK(f.s->ending()->outcome == "fooled");
  CHECK(f.holder("key_reward_chest") == "player");
}

TEST_CASE("task2 / review 5: end_game only from Greg and only when justified") {
  Fx f;
  f.say("matt", speech({{{"type", "end_game"}, {"outcome", "gave_up"}}}));
  CHECK_FALSE(f.s->ending().has_value());
  f.say("greg", speech({{{"type", "end_game"}, {"outcome", "returned"}}}));
  CHECK_FALSE(f.s->ending().has_value());
  f.say("greg", speech({{{"type", "end_game"}, {"outcome", "sunk"}}}));
  CHECK_FALSE(f.s->ending().has_value());
  f.say("greg", speech({{{"type", "end_game"}, {"outcome", "gave_up"}}}));
  REQUIRE(f.s->ending().has_value());
  CHECK(f.holder("key_reward_chest") == "greg");
}

TEST_CASE("task3: fights become facts; beating the culprit keeps the game solvable") {
  Fx f;
  auto& w = f.s->world();
  std::string culprit = w.scenario.culprit;
  std::string target = culprit.empty() || culprit == "greg" ? "matt" : culprit;
  f.s->reportFight(target, true, 50);
  f.s->reportFight(target, true, 60);
  f.s->reportFight(target, true, 70);
  CHECK(w.npcs.at(target).knockedOutByPlayer);
  CHECK_FALSE(w.facts.empty());
  CHECK(w.facts.back().knownBy.empty());  // the whole camp saw it
  CHECK(isSolvable(w.scenario, w.burnedNpcs()));
}

TEST_CASE("task4 / review 3: director clock respects interval, triggers and in-flight calls") {
  Fx f;
  CHECK_FALSE(f.s->directorDue(10));    // too early
  CHECK(f.s->directorDue(240));         // interval reached
  auto fut = f.s->directAsync("tick", 240);
  CHECK_FALSE(f.s->directorDue(1000));  // in flight
  fut.get();
  f.s->trigger("fight");
  CHECK_FALSE(f.s->directorDue(250));   // min gap 30 s
  CHECK(f.s->directorDue(271));         // trigger fires before the interval
  f.s->directAsync("fight", 271).get();
  CHECK_FALSE(f.s->directorDue(300));
  CHECK(f.director.requests.size() == 2);
}

TEST_CASE("task5: save and load round-trip") {
  Fx a;
  a.s->syncPlayerItems({"greg_tankard"});
  a.say("skip", speech({{{"type", "quest_entry"}, {"text", "Skip coś wie."}}}));
  a.s->reportFight("matt", true, 33);
  json saved = a.s->saveState();
  CHECK(saved["version"] == kWorldSaveVersion);

  Fx b(999);  // different seed: load must replace it
  REQUIRE(b.s->loadState(saved).ok());
  CHECK(b.s->saveState().dump() == saved.dump());
  CHECK(b.s->world().scenario.seed == a.s->world().scenario.seed);
}

TEST_CASE("task5 / review 2: an answer pending during load is discarded") {
  // Provider that blocks until released, so the load really happens while the answer is in flight.
  struct Gate : ILlmProvider {
    std::promise<void> entered, release;
    Result<LlmReply> complete(const LlmRequest&) override {
      entered.set_value();
      release.get_future().wait();
      LlmReply r;
      r.text = R"({"speech":"Za pozno.","actions":[{"type":"attitude","delta":2}]})";
      return r;
    }
    std::string_view id() const override { return "gate"; }
    std::string_view model() const override { return "gate"; }
  } gate;
  MockProvider dir;
  std::ostringstream log;
  Telemetry tel(log, false, Pricing{});
  auto d = loadGameData(pathFromUtf8(std::string(GROG_REPO_ROOT) + "/mod/data"));
  GameSession s(AiConfig{}, d.value(), gate, dir, tel, 8);
  json before = s.saveState();

  auto fut = s.talkAsync("skip", "Hej", 1);
  gate.entered.get_future().wait();
  REQUIRE(s.loadState(before).ok());
  gate.release.set_value();
  auto r = fut.get();
  REQUIRE(r.error.has_value());
  CHECK(s.worldSnapshot().npcs.at("skip").memory.empty());
  CHECK(s.worldSnapshot().npcs.at("skip").attitude == 0);
}

TEST_CASE("task5: old or broken save data is rejected") {
  Fx f;
  json bad = f.s->saveState();
  bad["version"] = 1;
  CHECK_FALSE(f.s->loadState(bad).ok());
  CHECK_FALSE(f.s->loadState(json::object()).ok());
}
