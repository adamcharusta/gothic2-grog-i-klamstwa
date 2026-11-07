#include <doctest/doctest.h>

#include <grog/ai/telemetry.h>

#include "test_helpers.h"

#include <sstream>

using namespace grog::ai;
using nlohmann::json;

namespace {
std::vector<json> lines(const std::string& s) {
  std::vector<json> out;
  std::istringstream in(s);
  std::string l;
  while(std::getline(in, l))
    if(!l.empty())
      out.push_back(json::parse(l));  // throws (test fails) if a line is not valid JSON
  return out;
}
AiConfig cfg() {
  AiConfig c;
  c.actor.kind = ProviderKind::Anthropic;
  c.actor.model = "claude-sonnet-5-5";
  c.actor.apiKey = "sk-ant-NEVER-LOG-ME-123456";
  c.director = c.actor;
  return c;
}
Scenario scen() {
  auto cast = loadCast(json::parse(testutil::repoFile("mod/data/cast.json")),
                       json::parse(testutil::repoFile("mod/data/places.json")));
  return rollScenario(77, cast.value());
}
}  // namespace

TEST_CASE("telemetry writes one JSON object per line and never the key") {
  std::ostringstream out;
  Pricing pricing;
  pricing.perMillion["claude-sonnet-5-5"] = {3.0, 15.0};
  Telemetry t(out, true, pricing);
  t.session(scen(), cfg());
  LlmReply rep;
  rep.text = "{\"speech\":\"Spadaj.\"}";
  rep.inputTokens = 1000;
  rep.outputTokens = 100;
  rep.latencyMs = 850;
  rep.model = "claude-sonnet-5-5";
  t.request("actor", "matt", "anthropic", Result<LlmReply>(rep), "Gdzie kufel?");
  t.request("actor", "matt", "anthropic", Result<LlmReply>(Error{ErrorKind::RateLimit, "429"}), "x");
  t.verdicts("matt", {Verdict{Command{"attitude", {{"delta", 1}}}, true, ""}});
  t.event("fight", {{"winner", "player"}});

  std::string text = out.str();
  CHECK(text.find("sk-ant-NEVER-LOG-ME") == std::string::npos);
  auto ls = lines(text);
  REQUIRE(ls.size() == 5);
  for(auto& l : ls) {
    CHECK(l.contains("t"));
    CHECK(l.contains("kind"));
  }
  CHECK(ls[0]["kind"] == "session");
  CHECK(ls[0]["seed"] == 77);
  CHECK(ls[0]["scenario"]["type"].is_string());
  CHECK(ls[0]["actor_model"] == "claude-sonnet-5-5");
  CHECK(ls[1]["kind"] == "request");
  CHECK(ls[1]["ok"] == true);
  CHECK(ls[1]["input_tokens"] == 1000);
  CHECK(ls[1]["cost_usd"].get<double>() == doctest::Approx(0.0045));
  CHECK(ls[1]["request"] == "Gdzie kufel?");
  CHECK(ls[1]["response"] == "{\"speech\":\"Spadaj.\"}");
  CHECK(ls[2]["ok"] == false);
  CHECK(ls[2]["error"] == "RateLimit");
  CHECK(ls[3]["kind"] == "verdicts");
  CHECK(ls[4]["kind"] == "fight");
}

TEST_CASE("content logging can be switched off; unknown prices are flagged") {
  std::ostringstream out;
  Telemetry t(out, false, Pricing{});
  LlmReply rep;
  rep.text = "secret story";
  rep.model = "some-model";
  t.request("director", "", "openai_compatible", Result<LlmReply>(rep), "the truth");
  auto ls = lines(out.str());
  REQUIRE(ls.size() == 1);
  CHECK_FALSE(ls[0].contains("request"));
  CHECK_FALSE(ls[0].contains("response"));
  CHECK(ls[0]["price_unknown"] == true);
  CHECK(ls[0]["cost_usd"] == 0.0);
}

TEST_CASE("pricing loads from mod data") {
  auto p = loadPricing(json::parse(testutil::repoFile("mod/data/pricing.json")));
  REQUIRE(p.ok());
}
