#include <grog/ai/telemetry.h>

namespace grog::ai {
using nlohmann::json;

double Pricing::estimate(const std::string& model, int inTok, int outTok) const {
  auto it = perMillion.find(model);
  if(it == perMillion.end()) {
    // Providers report dated snapshots ("gpt-4.1-mini-2025-04-14"): use the longest listed name followed by '-'.
    size_t best = 0;
    for(auto i = perMillion.begin(); i != perMillion.end(); ++i) {
      const std::string& k = i->first;
      if(k.size() > best && model.size() > k.size() && model.compare(0, k.size(), k) == 0 && model[k.size()] == '-') {
        best = k.size();
        it = i;
      }
    }
    if(best == 0)
      return -1.0;
  }
  return (inTok * it->second.first + outTok * it->second.second) / 1'000'000.0;
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

void Telemetry::request(std::string_view role, std::string_view npc, std::string_view provider,
                        const Result<LlmReply>& reply, std::string_view requestText) {
  json l = {{"kind", "request"}, {"role", role}, {"npc", npc}, {"provider", provider}, {"ok", reply.ok()}};
  if(reply.ok()) {
    const LlmReply& r = reply.value();
    double cost = pricing_.estimate(r.model, r.inputTokens, r.outputTokens);
    l["model"] = r.model;
    l["input_tokens"] = r.inputTokens;
    l["output_tokens"] = r.outputTokens;
    l["latency_ms"] = r.latencyMs;
    l["cost_usd"] = cost < 0 ? 0.0 : cost;
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
