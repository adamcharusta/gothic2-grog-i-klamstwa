#include <doctest/doctest.h>

#include <grog/ai/tts.h>

#include <nlohmann/json.hpp>

#include <optional>

#include "fake_http.h"

using namespace grog::ai;
using nlohmann::json;

namespace {

std::string le32(uint32_t v) {
  std::string s(4, '\0');
  for(int i = 0; i < 4; ++i)
    s[size_t(i)] = char((v >> (8 * i)) & 0xff);
  return s;
}
std::string le16(uint16_t v) { return std::string{char(v & 0xff), char(v >> 8)}; }

uint32_t rd32(const std::string& s, size_t o) {
  return uint32_t(uint8_t(s[o])) | uint32_t(uint8_t(s[o + 1])) << 8 | uint32_t(uint8_t(s[o + 2])) << 16 |
         uint32_t(uint8_t(s[o + 3])) << 24;
}

// Mono 24 kHz PCM. `riffSize`/`dataSize` override the stated sizes (streamed files write 0xFFFFFFFF).
std::string makeWav(int dataBytes, bool withListChunk = false, uint16_t bits = 16,
                    std::optional<uint32_t> riffSize = std::nullopt, std::optional<uint32_t> dataSize = std::nullopt) {
  const uint16_t align = uint16_t(bits / 8);
  std::string body = "WAVE";
  if(withListChunk)
    body += "LIST" + le32(4) + "INFO";
  body += "fmt " + le32(16) + le16(1) + le16(1) + le32(24000) + le32(24000u * align) + le16(align) + le16(bits);
  body += "data" + le32(dataSize.value_or(uint32_t(dataBytes))) + std::string(size_t(dataBytes), '\0');
  return "RIFF" + le32(riffSize.value_or(uint32_t(body.size()))) + body;
}

TtsConfig cfg() {
  TtsConfig c;
  c.kind = TtsKind::OpenAiCompatible;
  c.baseUrl = "https://api.openai.com/v1";
  c.apiKey = "sk-t";
  return c;
}

}  // namespace

TEST_CASE("wavDurationMs") {
  CHECK(wavDurationMs(makeWav(48000)) == 1000);
  CHECK(wavDurationMs(makeWav(4800)) == 100);
  CHECK(wavDurationMs(makeWav(48000, true)) == 1000);
  CHECK_FALSE(wavDurationMs(makeWav(48000).substr(0, 30)).has_value());
  CHECK_FALSE(wavDurationMs("ID3\x04junkjunkjunkjunkjunkjunkjunkjunkjunkjunkjunk").has_value());
  CHECK_FALSE(wavDurationMs("").has_value());
}

TEST_CASE("makeTtsProvider returns nullptr for Off") {
  FakeHttp http;
  CHECK(makeTtsProvider(TtsConfig{}, http) == nullptr);
}

TEST_CASE("openai tts request") {
  FakeHttp http;
  http.reply(200, makeWav(4800));
  auto p = makeTtsProvider(cfg(), http);
  REQUIRE(p);
  auto r = p->synthesize({"Arr!", "onyx", "gruff"});
  REQUIRE(r.ok());
  CHECK(http.requests[0].url == "https://api.openai.com/v1/audio/speech");
  CHECK(http.header(0, "Authorization") == "Bearer sk-t");
  CHECK(json::parse(http.requests[0].body) ==
        json::parse(R"({"model":"gpt-4o-mini-tts","voice":"onyx","input":"Arr!","instructions":"gruff","response_format":"wav"})"));
  CHECK(r.value().audioMs == 100);
  CHECK(r.value().model == "gpt-4o-mini-tts");
  CHECK(r.value().wav.rfind("RIFF", 0) == 0);
}

TEST_CASE("long text is cut at a sentence end within 600 chars") {
  {
    std::string text(550, 'a');
    text += ".";
    text += std::string(349, 'b');
    REQUIRE(text.size() == 900);
    FakeHttp http;
    http.reply(200, makeWav(4800));
    REQUIRE(makeTtsProvider(cfg(), http)->synthesize({text, "onyx", ""}).ok());
    auto in = json::parse(http.requests[0].body)["input"].get<std::string>();
    CHECK(in.size() == 551);
    CHECK(in.back() == '.');
  }
  {
    FakeHttp http;
    http.reply(200, makeWav(4800));
    REQUIRE(makeTtsProvider(cfg(), http)->synthesize({std::string(900, 'x'), "onyx", ""}).ok());
    CHECK(json::parse(http.requests[0].body)["input"].get<std::string>().size() == kTtsMaxChars);
  }
  {
    // hard cut never splits a UTF-8 sequence: 599 ASCII + 2-byte char straddling byte 600
    std::string text(599, 'x');
    text += "\xC5\x82";  // l-stroke
    text += std::string(100, 'y');
    FakeHttp http;
    http.reply(200, makeWav(4800));
    REQUIRE(makeTtsProvider(cfg(), http)->synthesize({text, "onyx", ""}).ok());
    auto in = json::parse(http.requests[0].body)["input"].get<std::string>();
    CHECK(in.size() == 599);
  }
}

TEST_CASE("empty instructions are not sent") {
  FakeHttp http;
  http.reply(200, makeWav(4800));
  REQUIRE(makeTtsProvider(cfg(), http)->synthesize({"Hej", "onyx", ""}).ok());
  CHECK_FALSE(json::parse(http.requests[0].body).contains("instructions"));
}

TEST_CASE("non-wav reply is a BadResponse error") {
  FakeHttp http;
  http.reply(200, "ID3\x04\x00\x00 not a wav at all, just mp3 bytes");
  auto r = makeTtsProvider(cfg(), http)->synthesize({"Hej", "onyx", ""});
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().kind == ErrorKind::BadResponse);
}

TEST_CASE("a streamed wav with placeholder sizes is normalised") {
  // ffmpeg-piped servers write 0xFFFFFFFF RIFF and data sizes; the engine's decoder needs the real ones.
  FakeHttp http;
  http.reply(200, makeWav(4800, true, 16, 0xFFFFFFFFu, 0xFFFFFFFFu));
  auto r = makeTtsProvider(cfg(), http)->synthesize({"Hej", "onyx", ""});
  REQUIRE(r.ok());
  const std::string& w = r.value().wav;
  CHECK(rd32(w, 4) == w.size() - 8);
  size_t data = w.find("data");
  REQUIRE(data != std::string::npos);
  CHECK(w.compare(12, 4, "LIST") == 0);  // chunk order kept
  CHECK(rd32(w, data + 4) == 4800);
  CHECK(w.size() == data + 8 + 4800);
  CHECK(wavDurationMs(w) == 100);
  CHECK(r.value().audioMs == 100);
}

TEST_CASE("normalizeWav trims the body and pads an odd data chunk") {
  // A chunk after the data (here with a bogus length) is dropped.
  std::string w = makeWav(4800) + "junk" + le32(0xFFFFFFF0u);
  auto n = normalizeWav(w);
  REQUIRE(n);
  CHECK(rd32(*n, 4) == n->size() - 8);
  CHECK(n->size() == 44 + 4800);
  // 8-bit mono with an odd sample count gets a pad byte.
  auto odd = normalizeWav(makeWav(4801, false, 8));
  REQUIRE(odd);
  CHECK(rd32(*odd, 40) == 4801);
  CHECK(odd->size() == 44 + 4802);
  CHECK(rd32(*odd, 4) == odd->size() - 8);
  CHECK_FALSE(normalizeWav(makeWav(4800, false, 24)).has_value());
  CHECK_FALSE(normalizeWav("not a wav").has_value());
}

TEST_CASE("a 24-bit pcm reply is a BadResponse error") {
  FakeHttp http;
  http.reply(200, makeWav(7200, false, 24));
  REQUIRE(wavDurationMs(makeWav(7200, false, 24)) == 100);  // a valid PCM WAV, just not one the engine plays
  auto r = makeTtsProvider(cfg(), http)->synthesize({"Hej", "onyx", ""});
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().kind == ErrorKind::BadResponse);
}

TEST_CASE("http errors map like the llm client") {
  struct Case {
    int status;
    ErrorKind kind;
  };
  for(auto c : {Case{401, ErrorKind::Auth}, Case{429, ErrorKind::RateLimit}, Case{500, ErrorKind::Server},
                Case{404, ErrorKind::Config}}) {
    FakeHttp http;
    http.reply(c.status, "echo sk-t");
    auto r = makeTtsProvider(cfg(), http)->synthesize({"Hej", "onyx", ""});
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().kind == c.kind);
    CHECK(r.error().message.find("sk-t") == std::string::npos);
  }
  FakeHttp http;
  http.fail(ErrorKind::Network, "no route");
  auto r = makeTtsProvider(cfg(), http)->synthesize({"Hej", "onyx", ""});
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().kind == ErrorKind::Network);
}

TEST_CASE("mock tts returns a 100 ms wav and scripted errors") {
  MockTts m;
  m.scriptedErrors.push_back({ErrorKind::Timeout, "t"});
  auto e = m.synthesize({"a", "v", ""});
  REQUIRE_FALSE(e.ok());
  CHECK(e.error().kind == ErrorKind::Timeout);
  auto r = m.synthesize({"b", "v", ""});
  REQUIRE(r.ok());
  CHECK(r.value().audioMs == 100);
  CHECK(wavDurationMs(r.value().wav) == 100);
  CHECK(m.requests.size() == 2);
}
