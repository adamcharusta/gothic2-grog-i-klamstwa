#pragma once
#include <grog/ai/commands.h>
#include <grog/ai/config.h>
#include <grog/ai/llm.h>
#include <grog/ai/scenario.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <map>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace grog::ai {

struct Pricing {
  std::map<std::string, std::pair<double, double>> perMillion;  // model -> (input, output) USD per 1M tokens
  // Returns -1 when the model is unknown.
  double estimate(const std::string& model, int inTok, int outTok) const;
};

Result<Pricing> loadPricing(const nlohmann::json& j);

// JSONL benchmark log. Thread-safe. Never writes API keys.
class Telemetry {
 public:
  Telemetry(std::ostream& out, bool logContent, Pricing pricing);

  void session(const Scenario& scenario, const AiConfig& config);
  // Returns the estimated cost in USD (0 when the request failed or the model has no price).
  double request(std::string_view role, std::string_view npc, std::string_view provider, const Result<LlmReply>& reply,
                 std::string_view requestText);
  void verdicts(std::string_view speaker, const std::vector<Verdict>& verdicts);
  void event(std::string_view kind, const nlohmann::json& data);

 private:
  void write(nlohmann::json line);

  std::ostream& out_;
  bool logContent_;
  Pricing pricing_;
  std::chrono::steady_clock::time_point t0_;
  std::mutex mu_;
};

}  // namespace grog::ai
