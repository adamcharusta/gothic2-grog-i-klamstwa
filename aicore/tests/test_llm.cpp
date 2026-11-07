#include <doctest/doctest.h>

#include <grog/ai/llm.h>

#include <nlohmann/json.hpp>

#include "fake_http.h"
#include "test_helpers.h"

using namespace grog::ai;
using nlohmann::json;

namespace {
const char* kKey = "sk-ant-TOPSECRET-0000000000";

ProviderConfig anthropicCfg() {
  ProviderConfig c;
  c.kind = ProviderKind::Anthropic;
  c.baseUrl = "https://api.anthropic.com";
  c.model = "claude-sonnet-5-5";
  c.apiKey = kKey;
  c.temperature = 0.7;
  c.timeoutS = 15;
  return c;
}
ProviderConfig openaiCfg(std::string base, std::string key) {
  ProviderConfig c;
  c.kind = ProviderKind::OpenAiCompatible;
  c.baseUrl = std::move(base);
  c.model = "deepseek-chat";
  c.apiKey = std::move(key);
  return c;
}
LlmRequest sampleRequest() {
  LlmRequest r;
  r.system = "You are Greg.";
  r.messages = {{ChatMessage::User, "Gdzie jest kufel?"}, {ChatMessage::Assistant, "Nie wiem."}, {ChatMessage::User, "Na pewno?"}};
  r.replySchema = json{{"type", "object"}};
  r.maxTokens = 300;
  return r;
}
}  // namespace

TEST_CASE("anthropic request shape") {
  FakeHttp http;
  http.reply(200, testutil::testData("anthropic_ok.json"));
  auto p = makeProvider(anthropicCfg(), http);
  auto r = p->complete(sampleRequest());
  REQUIRE(r.ok());
  REQUIRE(http.requests.size() == 1);
  CHECK(http.requests[0].url == "https://api.anthropic.com/v1/messages");
  CHECK(http.requests[0].timeoutS == 15);
  CHECK(http.header(0, "x-api-key") == kKey);
  CHECK(http.header(0, "anthropic-version") == "2023-06-01");
  CHECK(http.header(0, "content-type") == "application/json");
  auto body = json::parse(http.requests[0].body);
  CHECK(body["model"] == "claude-sonnet-5-5");
  CHECK(body["max_tokens"] == 300);
  CHECK(body["temperature"].get<double>() == doctest::Approx(0.7));
  CHECK(body["system"].get<std::string>().find("You are Greg.") == 0);
  CHECK(body["system"].get<std::string>().find("JSON") != std::string::npos);  // schema instruction appended
  REQUIRE(body["messages"].size() == 3);
  CHECK(body["messages"][0]["role"] == "user");
  CHECK(body["messages"][0]["content"] == "Gdzie jest kufel?");
  CHECK(body["messages"][1]["role"] == "assistant");
}

TEST_CASE("anthropic reply parsing joins text blocks and reads usage") {
  FakeHttp http;
  http.reply(200, testutil::testData("anthropic_ok.json"));
  auto r = makeProvider(anthropicCfg(), http)->complete(sampleRequest());
  REQUIRE(r.ok());
  CHECK(r.value().text == "{\"speech\": \"Kufel? Spadaj, szczurze lądowy.\"}");
  CHECK(r.value().inputTokens == 812);
  CHECK(r.value().outputTokens == 37);
  CHECK(r.value().model == "claude-sonnet-5-5");
}

TEST_CASE("openai-compatible request shape and parsing") {
  FakeHttp http;
  http.reply(200, testutil::testData("openai_ok.json"));
  auto r = makeProvider(openaiCfg("https://api.deepseek.com", "dk-123"), http)->complete(sampleRequest());
  REQUIRE(r.ok());
  CHECK(http.requests[0].url == "https://api.deepseek.com/chat/completions");
  CHECK(http.header(0, "Authorization") == "Bearer dk-123");
  auto body = json::parse(http.requests[0].body);
  CHECK(body["model"] == "deepseek-chat");
  CHECK(body["max_tokens"] == 300);
  CHECK(body["response_format"]["type"] == "json_object");
  REQUIRE(body["messages"].size() == 4);
  CHECK(body["messages"][0]["role"] == "system");
  CHECK(body["messages"][1]["content"] == "Gdzie jest kufel?");
  CHECK(r.value().text == "{\"speech\": \"Ahoj! Czego chcesz?\"}");
  CHECK(r.value().inputTokens == 640);
  CHECK(r.value().outputTokens == 21);
}

TEST_CASE("openai-compatible without a key sends no Authorization header (Ollama)") {
  FakeHttp http;
  http.reply(200, testutil::testData("openai_ok.json"));
  auto r = makeProvider(openaiCfg("http://localhost:11434/v1", ""), http)->complete(sampleRequest());
  REQUIRE(r.ok());
  CHECK(http.requests[0].url == "http://localhost:11434/v1/chat/completions");
  CHECK_FALSE(http.hasHeader(0, "Authorization"));
}

TEST_CASE("provider errors map to distinct kinds without leaking the key") {
  struct Case {
    int status;
    ErrorKind kind;
  };
  for(auto c : {Case{401, ErrorKind::Auth}, Case{403, ErrorKind::Auth}, Case{429, ErrorKind::RateLimit},
                Case{500, ErrorKind::Server}, Case{529, ErrorKind::Server}, Case{400, ErrorKind::BadResponse}}) {
    FakeHttp http;
    http.reply(c.status, std::string("{\"error\":{\"message\":\"echo ") + kKey + "\"}}");
    auto r = makeProvider(anthropicCfg(), http)->complete(sampleRequest());
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().kind == c.kind);
    CHECK(r.error().message.find(kKey) == std::string::npos);
  }
  {
    FakeHttp http;
    http.fail(ErrorKind::Timeout, "timed out");
    auto r = makeProvider(anthropicCfg(), http)->complete(sampleRequest());
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().kind == ErrorKind::Timeout);
  }
  {
    FakeHttp http;
    http.fail(ErrorKind::Network, "no route");
    auto r = makeProvider(openaiCfg("https://x.y", "k"), http)->complete(sampleRequest());
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().kind == ErrorKind::Network);
  }
  {
    FakeHttp http;
    http.reply(200, "<html>oops</html>");
    auto r = makeProvider(anthropicCfg(), http)->complete(sampleRequest());
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().kind == ErrorKind::BadResponse);
  }
  {
    FakeHttp http;
    http.reply(200, R"({"choices":[]})");
    auto r = makeProvider(openaiCfg("https://x.y", "k"), http)->complete(sampleRequest());
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().kind == ErrorKind::BadResponse);
  }
}

TEST_CASE("mock provider returns scripted replies in order, then a default") {
  MockProvider mock;
  mock.scripted = {"one", "two"};
  CHECK(mock.complete(sampleRequest()).value().text == "one");
  CHECK(mock.complete(sampleRequest()).value().text == "two");
  auto def = mock.complete(sampleRequest());
  REQUIRE(def.ok());
  CHECK(json::parse(def.value().text).contains("speech"));
  CHECK(mock.requests.size() == 3);
  CHECK(std::string(mock.id()) == "mock");
}

TEST_CASE("mock provider can return scripted errors") {
  MockProvider mock;
  mock.scriptedErrors = {Error{ErrorKind::Auth, "bad key"}};
  auto r = mock.complete(sampleRequest());
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().kind == ErrorKind::Auth);
}
