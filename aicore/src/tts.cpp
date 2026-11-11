#include <grog/ai/tts.h>

#include <grog/ai/llm.h>

#include <chrono>
#include <cstdint>

namespace grog::ai {
namespace {

using nlohmann::json;
using Clock = std::chrono::steady_clock;

uint32_t rd32(std::string_view s, size_t o) {
  return uint32_t(uint8_t(s[o])) | uint32_t(uint8_t(s[o + 1])) << 8 | uint32_t(uint8_t(s[o + 2])) << 16 |
         uint32_t(uint8_t(s[o + 3])) << 24;
}
uint16_t rd16(std::string_view s, size_t o) { return uint16_t(uint8_t(s[o]) | uint8_t(s[o + 1]) << 8); }

void put32(std::string& s, uint32_t v) {
  for(int i = 0; i < 4; ++i)
    s.push_back(char((v >> (8 * i)) & 0xff));
}
void put16(std::string& s, uint16_t v) {
  s.push_back(char(v & 0xff));
  s.push_back(char(v >> 8));
}

std::string silentWav(int ms) {
  const uint32_t dataBytes = uint32_t(24000 * 2 * ms / 1000);
  std::string w = "RIFF";
  put32(w, 36 + dataBytes);
  w += "WAVEfmt ";
  put32(w, 16);
  put16(w, 1);      // PCM
  put16(w, 1);      // mono
  put32(w, 24000);  // sample rate
  put32(w, 48000);  // byte rate
  put16(w, 2);      // block align
  put16(w, 16);     // bits
  w += "data";
  put32(w, dataBytes);
  w.append(dataBytes, '\0');
  return w;
}

bool isSentenceEnd(std::string_view t, size_t i) {
  char c = t[i];
  if(c == '.' || c == '!' || c == '?')
    return true;
  // U+2026 horizontal ellipsis: E2 80 A6; i points at the last byte
  return i >= 2 && uint8_t(c) == 0xA6 && uint8_t(t[i - 1]) == 0x80 && uint8_t(t[i - 2]) == 0xE2;
}

std::string truncateText(const std::string& text) {
  if(text.size() <= kTtsMaxChars)
    return text;
  std::string_view head(text.data(), kTtsMaxChars);
  for(size_t i = head.size(); i > 0; --i)
    if(isSentenceEnd(head, i - 1))
      return std::string(head.substr(0, i));
  size_t cut = kTtsMaxChars;
  // Do not split a multi-byte sequence: back up while the first dropped byte is a continuation byte.
  while(cut > 0 && (uint8_t(text[cut]) & 0xC0) == 0x80)
    --cut;
  return text.substr(0, cut);
}

class OpenAiTts : public ITtsProvider {
 public:
  OpenAiTts(TtsConfig cfg, IHttpClient& http) : cfg_(std::move(cfg)), http_(http) {}

  Result<TtsReply> synthesize(const TtsRequest& r) override {
    json body = {{"model", cfg_.model},
                 {"voice", r.voice},
                 {"input", truncateText(r.text)},
                 {"response_format", "wav"}};
    if(!r.instructions.empty())
      body["instructions"] = r.instructions;
    HttpRequest hr{cfg_.baseUrl + "/audio/speech",
                   {{"content-type", "application/json"}},
                   body.dump(-1, ' ', false, json::error_handler_t::replace),
                   cfg_.timeoutS};
    if(!cfg_.apiKey.empty())
      hr.headers.push_back({"Authorization", "Bearer " + cfg_.apiKey});
    auto t0 = Clock::now();
    auto res = http_.post(hr);
    if(!res.ok())
      return res.error();
    if(res.value().status != 200)
      return statusError(res.value().status, "openai_tts");
    auto ms = wavDurationMs(res.value().body);
    if(!ms)
      return Error{ErrorKind::BadResponse, "openai_tts: the reply is not a PCM WAV file"};
    TtsReply out;
    out.wav = std::move(res.value().body);
    out.audioMs = *ms;
    out.model = cfg_.model;
    out.latencyMs = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    return out;
  }
  std::string_view model() const override { return cfg_.model; }

 private:
  TtsConfig cfg_;
  IHttpClient& http_;
};

}  // namespace

std::optional<int> wavDurationMs(std::string_view wav) {
  if(wav.size() < 12 || wav.substr(0, 4) != "RIFF" || wav.substr(8, 4) != "WAVE")
    return std::nullopt;
  uint32_t byteRate = 0;
  bool pcm = false;
  size_t pos = 12;
  while(pos + 8 <= wav.size()) {
    std::string_view id = wav.substr(pos, 4);
    uint32_t len = rd32(wav, pos + 4);
    size_t body = pos + 8;
    if(id == "fmt ") {
      if(len < 16 || body + 16 > wav.size())
        return std::nullopt;
      pcm = rd16(wav, body) == 1;
      byteRate = rd32(wav, body + 8);
    } else if(id == "data") {
      if(!pcm || byteRate == 0)
        return std::nullopt;
      // Streamed files may carry a bogus length; trust what is actually there.
      uint64_t avail = wav.size() - body;
      uint64_t n = len < avail ? len : avail;
      return int(n * 1000 / byteRate);
    }
    pos = body + len + (len & 1);
  }
  return std::nullopt;
}

Result<TtsReply> MockTts::synthesize(const TtsRequest& r) {
  requests.push_back(r);
  if(!scriptedErrors.empty()) {
    Error e = scriptedErrors.front();
    scriptedErrors.pop_front();
    return e;
  }
  TtsReply out;
  out.wav = silentWav(100);
  out.audioMs = 100;
  out.model = "mock";
  return out;
}

std::unique_ptr<ITtsProvider> makeTtsProvider(const TtsConfig& c, IHttpClient& http) {
  switch(c.kind) {
    case TtsKind::OpenAiCompatible: return std::make_unique<OpenAiTts>(c, http);
    case TtsKind::Off: break;
  }
  return nullptr;
}

}  // namespace grog::ai
