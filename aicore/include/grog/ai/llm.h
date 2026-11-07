#pragma once
#include <grog/ai/config.h>
#include <grog/ai/http.h>
#include <grog/ai/result.h>

#include <nlohmann/json.hpp>

#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace grog::ai {

struct ChatMessage {
  enum Role { User, Assistant } role = User;
  std::string text;
};

struct LlmRequest {
  std::string system;
  std::vector<ChatMessage> messages;
  nlohmann::json replySchema;  // JSON schema of the expected reply object
  int maxTokens = 600;
};

struct LlmReply {
  std::string text;
  int inputTokens = 0;
  int outputTokens = 0;
  double latencyMs = 0;
  std::string model;
};

class ILlmProvider {
 public:
  virtual ~ILlmProvider() = default;
  virtual Result<LlmReply> complete(const LlmRequest& request) = 0;
  virtual std::string_view id() const = 0;  // "anthropic", "openai_compatible", "mock"
  virtual std::string_view model() const = 0;
};

// Scripted provider for tests and offline play.
class MockProvider : public ILlmProvider {
 public:
  std::deque<std::string> scripted;
  std::deque<Error> scriptedErrors;  // consumed before scripted replies
  std::vector<LlmRequest> requests;

  Result<LlmReply> complete(const LlmRequest& request) override;
  std::string_view id() const override { return "mock"; }
  std::string_view model() const override { return "mock"; }
};

// `http` must outlive the provider. Mock configs return a MockProvider.
std::unique_ptr<ILlmProvider> makeProvider(const ProviderConfig& config, IHttpClient& http);

// Instruction text appended to the system prompt describing the reply schema.
std::string schemaInstruction(const nlohmann::json& schema);

}  // namespace grog::ai
