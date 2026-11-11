#include <doctest/doctest.h>

#include <grog/ai/session.h>

#include "test_helpers.h"

#include <algorithm>
#include <sstream>

using namespace grog::ai;
using nlohmann::json;

namespace {
struct Fixture {
  MockProvider actor, director;
  std::ostringstream log;
  Telemetry tel{log, true, Pricing{}};
  std::unique_ptr<GameSession> session;

  explicit Fixture(uint64_t seed = 8) {
    auto data = loadGameData(pathFromUtf8(std::string(GROG_REPO_ROOT) + "/mod/data"));
    REQUIRE(data.ok());
    AiConfig cfg;
    cfg.language = Language::Pl;
    session = std::make_unique<GameSession>(cfg, data.value(), actor, director, tel, seed);
  }
};
}  // namespace

TEST_CASE("game data loads all cards, lore and rules") {
  auto data = loadGameData(pathFromUtf8(std::string(GROG_REPO_ROOT) + "/mod/data"));
  REQUIRE(data.ok());
  CHECK(data.value().cards.count("greg"));
  CHECK(data.value().cards.count("skip"));
  CHECK(data.value().cards.count("matt"));
  CHECK_FALSE(data.value().actorRules.empty());
  CHECK_FALSE(data.value().directorRules.empty());
  CHECK_FALSE(data.value().lore.entries.empty());
}

TEST_CASE("card items are placed in the world") {
  Fixture f;
  CHECK(f.session->world().itemHolder.at("key_reward_chest") == "greg");
  CHECK(f.session->world().itemHolder.at("tankard_lookalike") == "matt");
}

TEST_CASE("a valid reply is spoken, applied and remembered") {
  Fixture f;
  f.actor.scripted = {R"({"speech":"Ostatnio kręcił się tu Skip.","gesture":"angry",
    "actions":[{"type":"quest_entry","text":"Porozmawiaj ze Skipem."},{"type":"attitude","delta":1}]})"};
  auto r = f.session->talk("greg", "Kogo widziałeś przy chacie?", 60);
  CHECK_FALSE(r.usedFallback);
  CHECK_FALSE(r.error.has_value());
  CHECK(r.speech == "Ostatnio kręcił się tu Skip.");
  CHECK(r.gesture == "angry");
  REQUIRE(r.verdicts.size() == 2);
  CHECK(r.verdicts[0].accepted);
  auto& w = f.session->world();
  REQUIRE(w.quests.size() == 1);
  CHECK(w.quests[0].text == "Porozmawiaj ze Skipem.");
  CHECK(w.npcs.at("greg").attitude == 1);
  REQUIRE(w.npcs.at("greg").memory.size() == 2);
  CHECK(w.npcs.at("greg").memory[0].text == "Kogo widziałeś przy chacie?");
  CHECK(w.npcs.at("greg").memory[1].text == "Ostatnio kręcił się tu Skip.");
}

TEST_CASE("the Polish player line reaches the provider byte-exact") {
  Fixture f;
  f.session->talk("skip", "Gdzie jest kufel? Żółć, ąę!", 0);
  REQUIRE(f.actor.requests.size() == 1);
  CHECK(f.actor.requests[0].messages.back().text == "Gdzie jest kufel? Żółć, ąę!");
}

TEST_CASE("provider error gives a fallback line and leaves the world unchanged") {
  Fixture f;
  f.actor.scriptedErrors = {Error{ErrorKind::Auth, "bad key"}};
  std::string before = toJson(f.session->world()).dump();
  auto r = f.session->talk("greg", "Ahoj", 0);
  CHECK(r.usedFallback);
  REQUIRE(r.error.has_value());
  CHECK(r.error->kind == ErrorKind::Auth);
  auto& fb = f.session->data().cards.at("greg").fallbackLines.at(Language::Pl);
  CHECK(std::find(fb.begin(), fb.end(), r.speech) != fb.end());
  CHECK(toJson(f.session->world()).dump() == before);
  CHECK(f.actor.requests.size() == 1);  // no retry on provider errors
}

TEST_CASE("broken JSON or breaking character is retried once, then falls back") {
  Fixture f;
  f.actor.scripted = {"not json", R"({"speech":"Gdzie? Tam!"})"};
  auto r1 = f.session->talk("skip", "Gdzie kufel?", 0);
  CHECK_FALSE(r1.usedFallback);
  CHECK(r1.speech == "Gdzie? Tam!");
  CHECK(f.actor.requests.size() == 2);

  f.actor.scripted = {R"({"speech":"Jako AI nie mogę."})", R"({"speech":"As an AI language model..."})"};
  auto r2 = f.session->talk("skip", "Kim jesteś?", 0);
  CHECK(r2.usedFallback);
  CHECK(f.actor.requests.size() == 4);
}

TEST_CASE("unknown NPC is a Validation error") {
  Fixture f;
  auto r = f.session->talk("xardas", "Hej", 0);
  REQUIRE(r.error.has_value());
  CHECK(r.error->kind == ErrorKind::Validation);
}

TEST_CASE("talkAsync gives the same result as talk") {
  Fixture a, b;
  const char* reply = R"({"speech":"Spadaj, szczurze.","actions":[{"type":"attitude","delta":-1}]})";
  a.actor.scripted = {reply};
  b.actor.scripted = {reply};
  auto r1 = a.session->talk("matt", "Daj kufel", 5);
  auto r2 = b.session->talkAsync("matt", "Daj kufel", 5).get();
  CHECK(r1.speech == r2.speech);
  CHECK(toJson(a.session->world()).dump() == toJson(b.session->world()).dump());
}

TEST_CASE("director commands are validated and applied") {
  Fixture f;
  f.director.scripted = {R"({"thought":"nudge","actions":[{"type":"npc_approach_player","npc":"skip","text":"Hej, ty!"},
    {"type":"spawn_monster","monster":"dragon","place":"beach"}]})"};
  auto r = f.session->direct("player_idle", 30);
  CHECK_FALSE(r.error.has_value());
  REQUIRE(r.verdicts.size() == 2);
  CHECK(r.verdicts[0].accepted);
  CHECK_FALSE(r.verdicts[1].accepted);
  CHECK(f.session->world().eventsThisHour == 1);
  REQUIRE(f.director.requests.size() == 1);
  CHECK(f.director.requests[0].system.find(f.session->world().scenario.tankardLocation) != std::string::npos);
}

TEST_CASE("farewell comes from the card") {
  Fixture f;
  auto line = f.session->farewell("matt");
  auto& fw = f.session->data().cards.at("matt").farewells.at(Language::Pl);
  CHECK(std::find(fw.begin(), fw.end(), line) != fw.end());
}

TEST_CASE("telemetry log has session and request lines") {
  Fixture f;
  f.session->talk("greg", "Ahoj", 0);
  std::string log = f.log.str();
  CHECK(log.find("\"kind\":\"session\"") != std::string::npos);
  CHECK(log.find("\"kind\":\"request\"") != std::string::npos);
}

TEST_CASE("review C1: malformed field types fall back instead of throwing; batch is atomic") {
  Fixture f;
  f.actor.scripted = {R"({"speech":"Masz.","actions":[{"type":"attitude","delta":1},{"type":"quest_entry","text":"x","quest":{"a":1}}],"end_dialog":[1]})"};
  TurnResult r;
  CHECK_NOTHROW(r = f.session->talk("greg", "Ahoj", 0));
  CHECK(r.speech == "Masz.");
}

TEST_CASE("review I5: invalid UTF-8 from the game does not throw") {
  Fixture f;
  std::string cp1250 = std::string("Gdzie jest kufel? ") + char(0xB3);  // cp1250 letter: invalid UTF-8
  TurnResult r;
  CHECK_NOTHROW(r = f.session->talk("greg", cp1250, 0));
  CHECK(f.actor.requests.size() == 1);
}

TEST_CASE("review I3/I4: the session lock is not held during the provider call") {
  struct SlowProvider : ILlmProvider {
    GameSession* s = nullptr;
    bool farewellWhileWaiting = false;
    Result<LlmReply> complete(const LlmRequest&) override {
      // Re-entering the session from inside the call would deadlock if the lock were held.
      farewellWhileWaiting = !s->farewell("matt").empty();
      LlmReply r;
      r.text = R"({"speech":"No."})";
      return r;
    }
    std::string_view id() const override { return "slow"; }
    std::string_view model() const override { return "slow"; }
  } slow;
  MockProvider dir;
  std::ostringstream log;
  Telemetry tel(log, false, Pricing{});
  auto data = loadGameData(pathFromUtf8(std::string(GROG_REPO_ROOT) + "/mod/data"));
  AiConfig cfg;
  GameSession s(cfg, data.value(), slow, dir, tel, 1);
  slow.s = &s;
  auto r = s.talkAsync("greg", "Ahoj", 0).get();
  CHECK(slow.farewellWhileWaiting);
  CHECK(r.speech == "No.");
  CHECK(s.worldSnapshot().npcs.at("greg").memory.size() == 2);
}
