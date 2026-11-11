// Plan 5: reasoning models, player-facing error hints, run summary.
#include <doctest/doctest.h>

#include <grog/ai/hints.h>
#include <grog/ai/llm.h>
#include <grog/ai/session.h>

#include <nlohmann/json.hpp>

#include <sstream>

#include "fake_http.h"
#include "test_helpers.h"

using namespace grog::ai;
using nlohmann::json;

namespace {
ProviderConfig openai(std::string model) {
  ProviderConfig c;
  c.kind = ProviderKind::OpenAiCompatible;
  c.baseUrl = "https://api.openai.com/v1";
  c.model = std::move(model);
  c.apiKey = "sk-proj-SECRET";
  c.temperature = 0.8;
  return c;
}
LlmRequest req300() {
  LlmRequest r;
  r.system = "You are Greg.";
  r.messages = {{ChatMessage::User, "Siema"}};
  r.replySchema = json{{"type", "object"}};
  r.maxTokens = 300;
  return r;
}
struct Fx {
  MockProvider actor, director;
  std::ostringstream log;
  Telemetry tel;
  std::unique_ptr<GameSession> s;
  explicit Fx(Pricing p = Pricing{}) : tel(log, false, std::move(p)) {
    auto d = loadGameData(pathFromUtf8(std::string(GROG_REPO_ROOT) + "/mod/data"));
    REQUIRE(d.ok());
    AiConfig cfg;
    s = std::make_unique<GameSession>(cfg, d.value(), actor, director, tel, 8);
  }
};
int countLines(const std::string& s, const std::string& needle) {
  int n = 0;
  for(size_t p = 0; (p = s.find(needle, p)) != std::string::npos; ++p)
    ++n;
  return n;
}
}  // namespace

// ---- Task 1 ----

TEST_CASE("reasoning model names are recognised") {
  for(const char* m : {"gpt-5", "gpt-5-mini", "GPT-5.4-nano", "o3", "o4-mini", "o1-preview"})
    CHECK(isReasoningModel(m));
  for(const char* m : {"gpt-4.1-mini", "gpt-4o", "deepseek-chat", "claude-sonnet-5-5", "omni-moderation-latest", ""})
    CHECK_FALSE(isReasoningModel(m));
}

TEST_CASE("reasoning models get max_completion_tokens and no temperature") {
  {
    FakeHttp http;
    http.reply(200, testutil::testData("openai_ok.json"));
    REQUIRE(makeProvider(openai("gpt-5-mini"), http)->complete(req300()).ok());
    auto body = json::parse(http.requests[0].body);
    CHECK_FALSE(body.contains("max_tokens"));
    CHECK_FALSE(body.contains("temperature"));
    REQUIRE(body.contains("max_completion_tokens"));
    CHECK(body["max_completion_tokens"].get<int>() >= 2000);  // hidden reasoning tokens count against the limit
  }
  {
    FakeHttp http;
    http.reply(200, testutil::testData("openai_ok.json"));
    REQUIRE(makeProvider(openai("gpt-4.1-mini"), http)->complete(req300()).ok());
    auto body = json::parse(http.requests[0].body);
    CHECK(body["max_tokens"] == 300);
    CHECK(body["temperature"].get<double>() == doctest::Approx(0.8));
    CHECK_FALSE(body.contains("max_completion_tokens"));
  }
}

// ---- Task 2 ----

TEST_CASE("player hints exist for the errors a player can fix, in both languages") {
  for(ErrorKind k : {ErrorKind::Auth, ErrorKind::RateLimit, ErrorKind::Network, ErrorKind::Timeout, ErrorKind::Server,
                     ErrorKind::Config})
    for(Language l : {Language::Pl, Language::En}) {
      std::string h = playerHint(k, l);
      CHECK_FALSE(h.empty());
      CHECK(h.find("sk-") == std::string::npos);
    }
  CHECK(playerHint(ErrorKind::Auth, Language::Pl).find("GrogAI.ini") != std::string::npos);
  CHECK(playerHint(ErrorKind::Auth, Language::En).find("GrogAI.ini") != std::string::npos);
  CHECK(playerHint(ErrorKind::BadResponse, Language::Pl).empty());
  CHECK(playerHint(ErrorKind::Validation, Language::En).empty());
}

TEST_CASE("HTTP 400 and 404 mean the settings don't fit the provider") {
  for(int status : {400, 404}) {
    FakeHttp http;
    http.reply(status, "{}");
    auto r = makeProvider(openai("gpt-4.1-mini"), http)->complete(req300());
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().kind == ErrorKind::Config);
  }
}

TEST_CASE("a hint is shown once per error kind") {
  Fx f;
  f.actor.scriptedErrors = {Error{ErrorKind::Auth, "bad key"}, Error{ErrorKind::Auth, "bad key"}};
  auto a = f.s->talk("greg", "Siema", 10);
  auto b = f.s->talk("greg", "Halo?", 20);
  CHECK(a.usedFallback);
  CHECK(b.usedFallback);
  CHECK(a.hint == playerHint(ErrorKind::Auth, Language::Pl));
  CHECK(b.hint.empty());
}

TEST_CASE("transient errors are retried once, key and quota errors are not") {
  {
    Fx f;
    f.actor.scriptedErrors = {Error{ErrorKind::Timeout, "slow"}};
    f.actor.scripted = {R"({"speech":"Czego?"})"};
    auto r = f.s->talk("greg", "Siema", 10);
    CHECK_FALSE(r.usedFallback);
    CHECK(r.speech == "Czego?");
    CHECK(r.hint.empty());
    CHECK(f.actor.requests.size() == 2);
  }
  for(ErrorKind k : {ErrorKind::Auth, ErrorKind::RateLimit, ErrorKind::Config}) {
    Fx f;
    f.actor.scriptedErrors = {Error{k, "no"}};
    f.actor.scripted = {R"({"speech":"Czego?"})"};
    auto r = f.s->talk("greg", "Siema", 10);
    CHECK(r.usedFallback);
    CHECK(f.actor.requests.size() == 1);
  }
}

// ---- Task 3 ----

TEST_CASE("summary has cost, tokens, latency and the rejected-command rate") {
  Pricing p;
  p.perMillion["mock"] = {1.0, 2.0};
  Fx f(p);
  f.actor.scripted = {R"({"speech":"A.","actions":[{"type":"attitude","delta":1},{"type":"attitude","delta":9}]})",
                      R"({"speech":"B."})"};
  f.s->talk("greg", "x", 10);
  f.s->talk("greg", "y", 20);
  json s = f.s->summary();
  for(const char* k : {"cost_usd", "input_tokens", "output_tokens", "latency_ms_avg", "latency_ms_max", "requests",
                       "rejected_rate", "provider", "model", "director_model", "outcome"})
    CHECK(s.contains(k));
  CHECK(s["outcome"] == "playing");
  CHECK(s["requests"] == 2);
  CHECK(s["rejected_rate"].get<double>() == doctest::Approx(0.5));
  // Mock replies report their own token counts; the cost must follow the price list.
  double expected = (s["input_tokens"].get<double>() * 1.0 + s["output_tokens"].get<double>() * 2.0) / 1e6;
  CHECK(s["cost_usd"].get<double>() == doctest::Approx(expected));
  CHECK(s["latency_ms_max"].get<double>() >= s["latency_ms_avg"].get<double>());
}

TEST_CASE("summary stats survive save and load") {
  Fx f;
  f.actor.scripted = {R"({"speech":"A."})"};
  f.s->talk("greg", "x", 10);
  json saved = f.s->saveState();
  Fx g;
  REQUIRE(g.s->loadState(saved).ok());
  CHECK(g.s->summary()["requests"] == 1);
  CHECK(g.s->summary()["input_tokens"] == f.s->summary()["input_tokens"]);
  // A beta.5 save without the new stats still loads.
  saved["stats"].erase("requests");
  saved["stats"].erase("cost_usd");
  CHECK(g.s->loadState(saved).ok());
}

TEST_CASE("telemetry request returns the estimated cost") {
  std::ostringstream log;
  Pricing p;
  p.perMillion["m"] = {1.0, 1.0};
  Telemetry t(log, false, p);
  LlmReply r;
  r.model = "m";
  r.inputTokens = 500000;
  r.outputTokens = 500000;
  CHECK(t.request("actor", "greg", "mock", Result<LlmReply>(r), "x") == doctest::Approx(1.0));
  r.model = "unknown";
  CHECK(t.request("actor", "greg", "mock", Result<LlmReply>(r), "x") == 0.0);
  CHECK(countLines(log.str(), "\"kind\":\"request\"") == 2);
}
