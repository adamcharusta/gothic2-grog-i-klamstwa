#include <grog/ai/config.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <map>

namespace grog::ai {
namespace {

std::string trim(std::string_view s) {
  size_t b = 0, e = s.size();
  while(b < e && std::isspace(static_cast<unsigned char>(s[b])))
    ++b;
  while(e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
    --e;
  return std::string(s.substr(b, e - b));
}

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return s;
}

// section.key -> value
using IniMap = std::map<std::string, std::string>;

IniMap parseIni(std::string_view text) {
  IniMap out;
  std::string section;
  size_t pos = 0;
  while(pos <= text.size()) {
    size_t nl = text.find('\n', pos);
    std::string_view line = text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
    pos = (nl == std::string_view::npos) ? text.size() + 1 : nl + 1;

    std::string l = trim(line);
    if(l.empty() || l[0] == ';' || l[0] == '#')
      continue;
    if(l.front() == '[' && l.back() == ']') {
      section = lower(trim(std::string_view(l).substr(1, l.size() - 2)));
      continue;
    }
    size_t eq = l.find('=');
    if(eq == std::string::npos)
      continue;
    std::string key = lower(trim(std::string_view(l).substr(0, eq)));
    std::string val(std::string_view(l).substr(eq + 1));
    size_t c = val.find(';');
    if(c != std::string::npos)
      val = val.substr(0, c);
    out[section + "." + key] = trim(val);
  }
  return out;
}

std::string get(const IniMap& m, const std::string& k) {
  auto it = m.find(k);
  return it == m.end() ? std::string() : it->second;
}

Error configError(std::string msg) { return Error{ErrorKind::Config, "GrogAI.ini: " + std::move(msg)}; }

}  // namespace

EnvLookup systemEnv() {
  return [](std::string_view name) -> std::optional<std::string> {
    std::string n(name);
#ifdef _MSC_VER
    char* buf = nullptr;
    size_t len = 0;
    if(_dupenv_s(&buf, &len, n.c_str()) != 0 || buf == nullptr)
      return std::nullopt;
    std::string v(buf);
    free(buf);
    return v;
#else
    const char* v = std::getenv(n.c_str());
    if(v == nullptr)
      return std::nullopt;
    return std::string(v);
#endif
  };
}

Result<AiConfig> parseConfig(std::string_view iniText, const EnvLookup& env) {
  IniMap m = parseIni(iniText);
  AiConfig c;

  std::string provider = lower(get(m, "ai.provider"));
  if(provider == "anthropic")
    c.actor.kind = ProviderKind::Anthropic;
  else if(provider == "openai_compatible")
    c.actor.kind = ProviderKind::OpenAiCompatible;
  else if(provider == "mock")
    c.actor.kind = ProviderKind::Mock;
  else
    return configError("unknown provider '" + provider + "' (expected anthropic, openai_compatible or mock)");

  c.actor.baseUrl = get(m, "ai.base_url");
  while(!c.actor.baseUrl.empty() && c.actor.baseUrl.back() == '/')
    c.actor.baseUrl.pop_back();
  if(c.actor.baseUrl.empty()) {
    if(c.actor.kind == ProviderKind::Anthropic)
      c.actor.baseUrl = "https://api.anthropic.com";
    else if(c.actor.kind == ProviderKind::OpenAiCompatible)
      return configError("base_url is required for provider openai_compatible");
  }

  c.actor.model = get(m, "ai.model");
  if(c.actor.model.empty() && c.actor.kind != ProviderKind::Mock)
    return configError("model is required");

  c.actor.apiKey = get(m, "ai.api_key");
  if(c.actor.apiKey.empty()) {
    if(auto k = env("GROGAI_API_KEY"))
      c.actor.apiKey = *k;
  }
  if(c.actor.apiKey.empty() && c.actor.kind == ProviderKind::Anthropic)
    return configError("api_key is empty (set it in GrogAI.ini or the GROGAI_API_KEY environment variable)");

  if(auto t = get(m, "ai.timeout_s"); !t.empty()) {
    int v = 0;
    auto [p, ec] = std::from_chars(t.data(), t.data() + t.size(), v);
    if(ec != std::errc() || p != t.data() + t.size() || v <= 0 || v > 600)
      return configError("timeout_s must be a whole number of seconds (1-600)");
    c.actor.timeoutS = v;
  }
  if(auto t = get(m, "ai.temperature"); !t.empty()) {
    char* end = nullptr;
    double v = std::strtod(t.c_str(), &end);
    if(end != t.c_str() + t.size() || v < 0.0 || v > 2.0)
      return configError("temperature must be a number between 0 and 2");
    c.actor.temperature = v;
  }

  if(auto t = get(m, "ai.director_interval_s"); !t.empty()) {
    int v = 0;
    auto [p, ec] = std::from_chars(t.data(), t.data() + t.size(), v);
    if(ec != std::errc() || p != t.data() + t.size() || v < 30)
      return configError("director_interval_s must be a whole number >= 30");
    c.directorIntervalS = v;
  }

  c.director = c.actor;
  if(auto dm = get(m, "ai.director_model"); !dm.empty())
    c.director.model = dm;

  std::string lang = lower(get(m, "game.language"));
  if(lang.empty() || lang == "pl")
    c.language = Language::Pl;
  else if(lang == "en")
    c.language = Language::En;
  else
    return configError("language must be pl or en");

  if(auto s = get(m, "game.seed"); !s.empty()) {
    uint64_t v = 0;
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if(ec != std::errc() || p != s.data() + s.size())
      return configError("seed must be a non-negative whole number");
    c.seed = v;
  }
  std::string dbg = lower(get(m, "game.debug"));
  c.debug = (dbg == "true" || dbg == "1" || dbg == "yes");

  if(auto d = get(m, "telemetry.log_dir"); !d.empty())
    c.logDir = d;
  return c;
}

const char* providerKindName(ProviderKind kind) {
  switch(kind) {
    case ProviderKind::Anthropic: return "anthropic";
    case ProviderKind::OpenAiCompatible: return "openai_compatible";
    case ProviderKind::Mock: return "mock";
  }
  return "unknown";
}

const char* languageName(Language lang) { return lang == Language::Pl ? "pl" : "en"; }

std::string redactedSummary(const AiConfig& c) {
  std::string s = "provider=";
  s += providerKindName(c.actor.kind);
  s += " base_url=" + c.actor.baseUrl;
  s += " model=" + c.actor.model;
  s += " director_model=" + c.director.model;
  s += " api_key=";
  s += c.actor.apiKey.empty() ? "(none)" : "***";
  s += " language=";
  s += languageName(c.language);
  s += " timeout_s=" + std::to_string(c.actor.timeoutS);
  return s;
}

}  // namespace grog::ai
