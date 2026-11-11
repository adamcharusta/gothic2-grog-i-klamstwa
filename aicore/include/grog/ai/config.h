#pragma once
#include <grog/ai/result.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace grog::ai {

enum class ProviderKind { Anthropic, OpenAiCompatible, Mock };
enum class Language { Pl, En };
enum class TtsKind { Off, OpenAiCompatible };

struct ProviderConfig {
  ProviderKind kind = ProviderKind::Mock;
  std::string baseUrl;  // no trailing slash
  std::string model;
  std::string apiKey;   // never logged
  double temperature = 0.8;
  int timeoutS = 20;
};

struct TtsConfig {
  TtsKind kind = TtsKind::Off;
  std::string baseUrl;  // no trailing slash
  std::string model = "gpt-4o-mini-tts";
  std::string apiKey;   // never logged
  int timeoutS = 15;
  std::string cacheDir = "tts_cache";
};

struct AiConfig {
  ProviderConfig actor;
  ProviderConfig director;  // same provider; model may differ (director_model)
  TtsConfig tts;
  Language language = Language::Pl;
  std::optional<uint64_t> seed;
  bool debug = false;
  std::string logDir = "logs";
  int directorIntervalS = 240;  // game seconds between Game Master calls
  int directorMinGapS = 30;     // minimum gap, also for event triggers
};

using EnvLookup = std::function<std::optional<std::string>(std::string_view)>;

// Reads the process environment.
EnvLookup systemEnv();

// Parses GrogAI.ini text. Errors never contain the API key.
Result<AiConfig> parseConfig(std::string_view iniText, const EnvLookup& env);

// One line for logs/UI, key shown as "***" or "(none)".
std::string redactedSummary(const AiConfig& config);

const char* providerKindName(ProviderKind kind);
const char* languageName(Language lang);  // "pl" / "en"

}  // namespace grog::ai
