#include <doctest/doctest.h>

#include <grog/ai/config.h>

#include "test_helpers.h"

using namespace grog::ai;

namespace {
EnvLookup noEnv() {
  return [](std::string_view) -> std::optional<std::string> { return std::nullopt; };
}
EnvLookup envWithKey(std::string key) {
  return [key](std::string_view name) -> std::optional<std::string> {
    if(name == "GROGAI_API_KEY")
      return key;
    return std::nullopt;
  };
}
const char* kSecret = "sk-ant-SECRET-1234567890";
}  // namespace

TEST_CASE("example config parses with defaults") {
  std::string ini = testutil::repoFile("GrogAI.example.ini");
  REQUIRE_FALSE(ini.empty());
  auto r = parseConfig(ini, envWithKey(kSecret));
  REQUIRE(r.ok());
  const auto& c = r.value();
  CHECK(c.actor.kind == ProviderKind::Anthropic);
  CHECK(c.actor.model == "claude-sonnet-5-5");
  CHECK(c.actor.baseUrl == "https://api.anthropic.com");
  CHECK(c.actor.timeoutS == 20);
  CHECK(c.actor.temperature == doctest::Approx(0.8));
  CHECK(c.actor.apiKey == kSecret);  // from GROGAI_API_KEY
  CHECK(c.language == Language::Pl);
  CHECK_FALSE(c.seed.has_value());
  CHECK_FALSE(c.debug);
  CHECK(c.logDir == "logs");
  CHECK(c.director.model == c.actor.model);  // director_model empty -> copy of actor
}

TEST_CASE("inline comments, spacing and CRLF are tolerated") {
  auto r = parseConfig("[ai]\r\nprovider=anthropic ; comment\r\n  model =  claude-x  \r\napi_key = abc ; k\r\n", noEnv());
  REQUIRE(r.ok());
  CHECK(r.value().actor.model == "claude-x");
  CHECK(r.value().actor.apiKey == "abc");
}

TEST_CASE("file key wins over environment") {
  auto r = parseConfig("[ai]\nprovider = anthropic\nmodel = m\napi_key = from-file\n", envWithKey("from-env"));
  REQUIRE(r.ok());
  CHECK(r.value().actor.apiKey == "from-file");
}

TEST_CASE("openai_compatible requires base_url and may have no key") {
  auto bad = parseConfig("[ai]\nprovider = openai_compatible\nmodel = llama3\n", noEnv());
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().kind == ErrorKind::Config);
  CHECK(bad.error().message.find("base_url") != std::string::npos);

  auto ok = parseConfig("[ai]\nprovider = openai_compatible\nbase_url = http://localhost:11434/v1/\nmodel = llama3\n", noEnv());
  REQUIRE(ok.ok());
  CHECK(ok.value().actor.baseUrl == "http://localhost:11434/v1");  // trailing slash removed
  CHECK(ok.value().actor.apiKey.empty());
}

TEST_CASE("anthropic without any key is a Config error") {
  auto r = parseConfig("[ai]\nprovider = anthropic\nmodel = m\n", noEnv());
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().kind == ErrorKind::Config);
  CHECK(r.error().message.find("api_key") != std::string::npos);
}

TEST_CASE("director_model overrides only the model") {
  auto r = parseConfig("[ai]\nprovider = anthropic\nmodel = big\ndirector_model = small\napi_key = k\n", noEnv());
  REQUIRE(r.ok());
  CHECK(r.value().director.model == "small");
  CHECK(r.value().director.apiKey == "k");
  CHECK(r.value().actor.model == "big");
}

TEST_CASE("mock provider needs no key or model") {
  auto r = parseConfig("[ai]\nprovider = mock\n", noEnv());
  REQUIRE(r.ok());
  CHECK(r.value().actor.kind == ProviderKind::Mock);
}

TEST_CASE("invalid values are rejected") {
  CHECK_FALSE(parseConfig("[ai]\nprovider = skynet\n", noEnv()).ok());
  CHECK_FALSE(parseConfig("[ai]\nprovider = mock\n[game]\nlanguage = de\n", noEnv()).ok());
  CHECK_FALSE(parseConfig("[ai]\nprovider = mock\ntimeout_s = abc\n", noEnv()).ok());
  CHECK_FALSE(parseConfig("[ai]\nprovider = mock\n[game]\nseed = -1\n", noEnv()).ok());
}

TEST_CASE("seed, language en and debug are parsed") {
  auto r = parseConfig("[ai]\nprovider = mock\n[game]\nlanguage = en\nseed = 42\ndebug = true\n", noEnv());
  REQUIRE(r.ok());
  CHECK(r.value().language == Language::En);
  CHECK(r.value().seed == 42u);
  CHECK(r.value().debug);
}

TEST_CASE("the API key never leaks into summaries or errors") {
  auto r = parseConfig(std::string("[ai]\nprovider = anthropic\nmodel = m\napi_key = ") + kSecret + "\n", noEnv());
  REQUIRE(r.ok());
  auto s = redactedSummary(r.value());
  CHECK(s.find(kSecret) == std::string::npos);
  CHECK(s.find("***") != std::string::npos);

  auto bad = parseConfig(std::string("[ai]\nprovider = anthropic\nmodel = m\napi_key = ") + kSecret +
                             "\ntimeout_s = nope\n",
                         noEnv());
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().message.find(kSecret) == std::string::npos);
}

TEST_CASE("tts is off without a [tts] section") {
  auto r = parseConfig("[ai]\nprovider = mock\n", noEnv());
  REQUIRE(r.ok());
  CHECK(r.value().tts.kind == TtsKind::Off);
  CHECK(redactedSummary(r.value()).find("tts=off") != std::string::npos);
}

TEST_CASE("tts reuses the ai key for the same base_url") {
  auto r = parseConfig(
      "[ai]\nprovider = openai_compatible\nbase_url = https://api.openai.com/v1\nmodel = m\napi_key = sk-a\n"
      "[tts]\nprovider = openai_compatible\nbase_url = https://api.openai.com/v1/\n",
      noEnv());
  REQUIRE(r.ok());
  CHECK(r.value().tts.kind == TtsKind::OpenAiCompatible);
  CHECK(r.value().tts.apiKey == "sk-a");
  CHECK(r.value().tts.baseUrl == "https://api.openai.com/v1");
  CHECK(r.value().tts.model == "gpt-4o-mini-tts");
  CHECK(r.value().tts.timeoutS == 15);
  CHECK(r.value().tts.cacheDir == "tts_cache");
}

TEST_CASE("tts does not reuse the ai key for another base_url or provider") {
  // A different server: the [ai] key must not leak to it. A TTS server without a key is allowed (local servers).
  auto other = parseConfig(
      "[ai]\nprovider = openai_compatible\nbase_url = https://api.openai.com/v1\nmodel = m\napi_key = sk-a\n"
      "[tts]\nprovider = openai_compatible\nbase_url = http://localhost:8000/v1\n",
      noEnv());
  REQUIRE(other.ok());
  CHECK(other.value().tts.kind == TtsKind::OpenAiCompatible);
  CHECK(other.value().tts.apiKey.empty());
  CHECK(redactedSummary(other.value()).find("tts_key=(none)") != std::string::npos);

  // An Anthropic key is never sent to a TTS server, even with the same base_url.
  auto anth = parseConfig(
      "[ai]\nprovider = anthropic\nbase_url = https://api.openai.com/v1\nmodel = m\napi_key = sk-ant\n"
      "[tts]\nprovider = openai_compatible\nbase_url = https://api.openai.com/v1\n",
      noEnv());
  REQUIRE(anth.ok());
  CHECK(anth.value().tts.apiKey.empty());
  CHECK(anth.value().tts.apiKey != "sk-ant");
}

TEST_CASE("tts key from env and own key wins") {
  auto env = [](std::string_view name) -> std::optional<std::string> {
    if(name == "GROGAI_TTS_API_KEY")
      return std::string("sk-t");
    return std::nullopt;
  };
  auto r = parseConfig("[ai]\nprovider = mock\n[tts]\nprovider = openai_compatible\nbase_url = http://localhost:8880/v1\n",
                       env);
  REQUIRE(r.ok());
  CHECK(r.value().tts.apiKey == "sk-t");

  auto r2 = parseConfig(
      "[ai]\nprovider = mock\n[tts]\nprovider = openai_compatible\nbase_url = http://localhost:8880/v1\napi_key = sk-own\n",
      env);
  REQUIRE(r2.ok());
  CHECK(r2.value().tts.apiKey == "sk-own");
}

TEST_CASE("tts without base_url is a config error") {
  auto r = parseConfig("[ai]\nprovider = mock\napi_key = sk-secret\n[tts]\nprovider = openai_compatible\napi_key = sk-secret\n",
                       noEnv());
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().message.find("[tts] base_url") != std::string::npos);
  CHECK(r.error().message.find("sk-secret") == std::string::npos);
}

TEST_CASE("unknown tts provider is a config error") {
  auto r = parseConfig("[ai]\nprovider = mock\n[tts]\nprovider = elevenlabs\n", noEnv());
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().message.find("tts") != std::string::npos);
}

TEST_CASE("voices config") {
  const std::string base =
      "[ai]\nprovider = mock\n[tts]\nprovider = openai_compatible\nbase_url = http://x/v1\napi_key = sk-t\n";
  auto def = parseConfig(base, noEnv());
  REQUIRE(def.ok());
  CHECK(def.value().tts.voices == TtsVoices::OpenAi);
  auto openai = parseConfig(base + "voices = OpenAI\n", noEnv());
  REQUIRE(openai.ok());
  CHECK(openai.value().tts.voices == TtsVoices::OpenAi);
  auto orig = parseConfig(base + "voices = original\n", noEnv());
  REQUIRE(orig.ok());
  CHECK(orig.value().tts.voices == TtsVoices::Original);
  auto bad = parseConfig(base + "voices = cloned\n", noEnv());
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().message.find("[tts] voices") != std::string::npos);
  CHECK(bad.error().message.find("sk-t") == std::string::npos);
}

TEST_CASE("tts timeout_s is clamped to 5..60") {
  auto lo = parseConfig("[ai]\nprovider = mock\n[tts]\nprovider = openai_compatible\nbase_url = http://x/v1\ntimeout_s = 1\n",
                        noEnv());
  REQUIRE(lo.ok());
  CHECK(lo.value().tts.timeoutS == 5);
  auto hi = parseConfig("[ai]\nprovider = mock\n[tts]\nprovider = openai_compatible\nbase_url = http://x/v1\ntimeout_s = 999\n",
                        noEnv());
  REQUIRE(hi.ok());
  CHECK(hi.value().tts.timeoutS == 60);
}

TEST_CASE("redacted summary hides the tts key") {
  auto r = parseConfig(
      "[ai]\nprovider = mock\n[tts]\nprovider = openai_compatible\nbase_url = http://x/v1\napi_key = sk-t\n", noEnv());
  REQUIRE(r.ok());
  auto s = redactedSummary(r.value());
  CHECK(s.find("tts=openai_compatible") != std::string::npos);
  CHECK(s.find("tts_key=***") != std::string::npos);
  CHECK(s.find("sk-t") == std::string::npos);
}
