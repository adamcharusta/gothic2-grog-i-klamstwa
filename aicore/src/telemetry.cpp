#include <grog/ai/telemetry.h>

namespace grog::ai {
using nlohmann::json;

namespace {
// Exact name, else the longest listed name followed by '-': providers report dated snapshots
// ("gpt-4.1-mini-2025-04-14").
template <class Map>
typename Map::const_iterator findModel(const Map& m, const std::string& model) {
  auto it = m.find(model);
  if(it != m.end())
    return it;
  size_t best = 0;
  for(auto i = m.begin(); i != m.end(); ++i) {
    const std::string& k = i->first;
    if(k.size() > best && model.size() > k.size() && model.compare(0, k.size(), k) == 0 && model[k.size()] == '-') {
      best = k.size();
      it = i;
    }
  }
  return best == 0 ? m.end() : it;
}
}  // namespace

double Pricing::estimate(const std::string& model, int inTok, int outTok) const {
  auto it = findModel(perMillion, model);
  if(it == perMillion.end())
    return -1.0;
  return (inTok * it->second.first + outTok * it->second.second) / 1'000'000.0;
}

double Pricing::estimateTts(const std::string& model, int audioMs) const {
  auto it = findModel(ttsPerMinute, model);
  if(it == ttsPerMinute.end())
    return -1.0;
  return it->second * audioMs / 60'000.0;
}

Result<Pricing> loadPricing(const json& j) {
  if(j.value("schema", 0) != 1)
    return Error{ErrorKind::Io, "pricing.json: unsupported schema"};
  Pricing p;
  const json models = j.value("models", json::object());  // keep alive: items() must not iterate a temporary
  for(auto& [model, v] : models.items()) {
    if(!v.contains("input") || !v.contains("output") || !v["input"].is_number() || !v["output"].is_number())
      return Error{ErrorKind::Io, "pricing.json: model " + model + " needs numeric input/output"};
    p.perMillion[model] = {v["input"].get<double>(), v["output"].get<double>()};
  }
  const json tts = j.value("tts", json::object());
  for(auto& [model, v] : tts.items()) {
    if(!v.is_object() || !v.contains("usd_per_minute") || !v["usd_per_minute"].is_number())
      return Error{ErrorKind::Io, "pricing.json: tts model " + model + " needs a numeric usd_per_minute"};
    p.ttsPerMinute[model] = v["usd_per_minute"].get<double>();
  }
  return p;
}

Telemetry::Telemetry(std::ostream& out, bool logContent, Pricing pricing)
    : out_(out), logContent_(logContent), pricing_(std::move(pricing)), t0_(std::chrono::steady_clock::now()) {}

void Telemetry::write(json line) {
  std::lock_guard<std::mutex> lock(mu_);
  line["t"] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0_).count();
  // replace: never throw on odd bytes from a model
  out_ << line.dump(-1, ' ', false, json::error_handler_t::replace) << "\n";
  out_.flush();
}

void Telemetry::session(const Scenario& s, const AiConfig& c) {
  json sc = toJson(s);
  write({{"kind", "session"},
         {"seed", s.seed},
         {"scenario", sc},
         {"provider", providerKindName(c.actor.kind)},
         {"actor_model", c.actor.model},
         {"director_model", c.director.model},
         {"language", languageName(c.language)}});
}

double Telemetry::request(std::string_view role, std::string_view npc, std::string_view provider,
                          const Result<LlmReply>& reply, std::string_view requestText) {
  double spent = 0.0;
  json l = {{"kind", "request"}, {"role", role}, {"npc", npc}, {"provider", provider}, {"ok", reply.ok()}};
  if(reply.ok()) {
    const LlmReply& r = reply.value();
    double cost = pricing_.estimate(r.model, r.inputTokens, r.outputTokens);
    l["model"] = r.model;
    l["input_tokens"] = r.inputTokens;
    l["output_tokens"] = r.outputTokens;
    l["latency_ms"] = r.latencyMs;
    spent = cost < 0 ? 0.0 : cost;
    l["cost_usd"] = spent;
    if(cost < 0)
      l["price_unknown"] = true;
    if(logContent_) {
      l["request"] = requestText;
      l["response"] = r.text;
    }
  } else {
    l["error"] = errorKindName(reply.error().kind);
    l["error_message"] = reply.error().message;
    if(logContent_)
      l["request"] = requestText;
  }
  write(std::move(l));
  return spent;
}

double Telemetry::tts(std::string_view npc, std::string_view model, const Result<TtsReply>& reply, size_t chars,
                      bool cached) {
  double spent = 0.0;
  json l = {{"kind", "tts"}, {"npc", npc}, {"ok", reply.ok()}, {"chars", chars}, {"cached", cached}};
  if(reply.ok()) {
    const TtsReply& r = reply.value();
    std::string m = r.model.empty() ? std::string(model) : r.model;
    l["model"] = m;
    l["audio_ms"] = r.audioMs;
    l["latency_ms"] = r.latencyMs;
    if(!cached) {
      double cost = pricing_.estimateTts(m, r.audioMs);
      spent = cost < 0 ? 0.0 : cost;
      if(cost < 0)
        l["price_unknown"] = true;
    }
    l["cost_usd"] = spent;
  } else {
    l["model"] = model;
    l["audio_ms"] = 0;
    l["latency_ms"] = 0;
    l["cost_usd"] = 0.0;
    l["error"] = errorKindName(reply.error().kind);
  }
  write(std::move(l));
  return spent;
}

void Telemetry::verdicts(std::string_view speaker, const std::vector<Verdict>& vs) {
  json arr = json::array();
  for(auto& v : vs)
    arr.push_back({{"type", v.command.type}, {"args", v.command.args}, {"accepted", v.accepted}, {"reason", v.reason}});
  write({{"kind", "verdicts"}, {"speaker", speaker}, {"verdicts", arr}});
}

void Telemetry::event(std::string_view kind, const json& data) {
  json l = data.is_object() ? data : json{{"data", data}};
  l["kind"] = kind;
  write(std::move(l));
}

}  // namespace grog::ai
