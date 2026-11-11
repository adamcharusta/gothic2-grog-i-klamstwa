#pragma once
#include <grog/ai/config.h>
#include <grog/ai/http.h>
#include <grog/ai/result.h>

#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace grog::ai {

struct TtsRequest {
  std::string text;
  std::string voice;
  std::string instructions;  // delivery style; omitted from the request when empty
};

struct TtsReply {
  std::string wav;  // PCM RIFF/WAVE bytes
  double latencyMs = 0;
  int audioMs = 0;
  std::string model;
};

class ITtsProvider {
 public:
  virtual ~ITtsProvider() = default;
  virtual Result<TtsReply> synthesize(const TtsRequest& r) = 0;
  virtual std::string_view model() const = 0;
};

// Returns a 100 ms silent 16-bit mono 24 kHz WAV.
class MockTts : public ITtsProvider {
 public:
  std::deque<Error> scriptedErrors;  // consumed first
  std::vector<TtsRequest> requests;

  Result<TtsReply> synthesize(const TtsRequest& r) override;
  std::string_view model() const override { return "mock"; }
};

// `http` must outlive the provider. Returns nullptr for TtsKind::Off.
std::unique_ptr<ITtsProvider> makeTtsProvider(const TtsConfig& c, IHttpClient& http);

// Duration of a PCM RIFF/WAVE file; nullopt if it is not one (unknown chunks are skipped).
std::optional<int> wavDurationMs(std::string_view wav);

// Longer texts are cut at a sentence end (or hard, on a UTF-8 boundary) before synthesis.
constexpr size_t kTtsMaxChars = 600;

}  // namespace grog::ai
