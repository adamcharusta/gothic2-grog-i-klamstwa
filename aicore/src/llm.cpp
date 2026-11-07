#include <grog/ai/llm.h>

#include <chrono>

namespace grog::ai {
namespace {

using nlohmann::json;
using Clock = std::chrono::steady_clock;

// Model/game text may contain invalid UTF-8 (e.g. cp1250 from the engine): never throw while serialising.
std::string dumpSafe(const json& j) { return j.dump(-1, ' ', false, json::error_handler_t::replace); }

int intAt(const json& j, const char* a, const char* b) {
  if(j.is_object() && j.contains(a) && j[a].is_object() && j[a].contains(b) && j[a][b].is_number_integer())
    return j[a][b].get<int>();
  return 0;
}

double msSince(Clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// Maps an HTTP status to an error. The provider body is NOT copied into the message (it may echo the key).
Error statusError(int status, std::string_view provider) {
  std::string p(provider);
  if(status == 401 || status == 403)
    return {ErrorKind::Auth, p + ": the API key was rejected (HTTP " + std::to_string(status) + ")"};
  if(status == 429)
    return {ErrorKind::RateLimit, p + ": rate limit or quota exceeded (HTTP 429)"};
  if(status >= 500)
    return {ErrorKind::Server, p + ": provider server error (HTTP " + std::to_string(status) + ")"};
  return {ErrorKind::BadResponse, p + ": request rejected (HTTP " + std::to_string(status) + ")"};
}

json chatMessages(const LlmRequest& req) {
  json arr = json::array();
  for(auto& m : req.messages)
    arr.push_back({{"role", m.role == ChatMessage::User ? "user" : "assistant"}, {"content", m.text}});
  return arr;
}

class AnthropicProvider : public ILlmProvider {
 public:
  AnthropicProvider(ProviderConfig cfg, IHttpClient& http) : cfg_(std::move(cfg)), http_(http) {}

  Result<LlmReply> complete(const LlmRequest& req) override {
    json body = {{"model", cfg_.model},
                 {"max_tokens", req.maxTokens},
                 {"temperature", cfg_.temperature},
                 {"system", req.system + "\n\n" + schemaInstruction(req.replySchema)},
                 {"messages", chatMessages(req)}};
    HttpRequest hr{cfg_.baseUrl + "/v1/messages",
                   {{"x-api-key", cfg_.apiKey}, {"anthropic-version", "2023-06-01"}, {"content-type", "application/json"}},
                   dumpSafe(body),
                   cfg_.timeoutS};
    auto t0 = Clock::now();
    auto res = http_.post(hr);
    if(!res.ok())
      return res.error();
    if(res.value().status != 200)
      return statusError(res.value().status, "anthropic");
    json j = json::parse(res.value().body, nullptr, false);
    if(j.is_discarded() || !j.contains("content") || !j["content"].is_array())
      return Error{ErrorKind::BadResponse, "anthropic: unexpected reply format"};
    LlmReply out;
    for(auto& c : j["content"])
      if(c.is_object() && c.contains("type") && c["type"] == "text" && c.contains("text") && c["text"].is_string())
        out.text += c["text"].get<std::string>();
    if(out.text.empty())
      return Error{ErrorKind::BadResponse, "anthropic: empty reply"};
    out.inputTokens = intAt(j, "usage", "input_tokens");
    out.outputTokens = intAt(j, "usage", "output_tokens");
    out.model = j.contains("model") && j["model"].is_string() ? j["model"].get<std::string>() : cfg_.model;
    out.latencyMs = msSince(t0);
    return out;
  }
  std::string_view id() const override { return "anthropic"; }
  std::string_view model() const override { return cfg_.model; }

 private:
  ProviderConfig cfg_;
  IHttpClient& http_;
};

class OpenAiCompatibleProvider : public ILlmProvider {
 public:
  OpenAiCompatibleProvider(ProviderConfig cfg, IHttpClient& http) : cfg_(std::move(cfg)), http_(http) {}

  Result<LlmReply> complete(const LlmRequest& req) override {
    json msgs = json::array();
    msgs.push_back({{"role", "system"}, {"content", req.system + "\n\n" + schemaInstruction(req.replySchema)}});
    for(auto& m : chatMessages(req))
      msgs.push_back(m);
    json body = {{"model", cfg_.model},
                 {"temperature", cfg_.temperature},
                 {"max_tokens", req.maxTokens},
                 {"messages", msgs},
                 {"response_format", {{"type", "json_object"}}}};
    HttpRequest hr{cfg_.baseUrl + "/chat/completions", {{"content-type", "application/json"}}, dumpSafe(body),
                   cfg_.timeoutS};
    if(!cfg_.apiKey.empty())
      hr.headers.push_back({"Authorization", "Bearer " + cfg_.apiKey});
    auto t0 = Clock::now();
    auto res = http_.post(hr);
    if(!res.ok())
      return res.error();
    if(res.value().status != 200)
      return statusError(res.value().status, "openai_compatible");
    json j = json::parse(res.value().body, nullptr, false);
    if(j.is_discarded() || !j.contains("choices") || !j["choices"].is_array() || j["choices"].empty())
      return Error{ErrorKind::BadResponse, "openai_compatible: unexpected reply format"};
    const json& ch = j["choices"][0];
    json content = ch.is_object() && ch.contains("message") && ch["message"].is_object() && ch["message"].contains("content")
                       ? ch["message"]["content"]
                       : json();
    if(!content.is_string() || content.get<std::string>().empty())
      return Error{ErrorKind::BadResponse, "openai_compatible: empty reply"};
    LlmReply out;
    out.text = content.get<std::string>();
    out.inputTokens = intAt(j, "usage", "prompt_tokens");
    out.outputTokens = intAt(j, "usage", "completion_tokens");
    out.model = j.contains("model") && j["model"].is_string() ? j["model"].get<std::string>() : cfg_.model;
    out.latencyMs = msSince(t0);
    return out;
  }
  std::string_view id() const override { return "openai_compatible"; }
  std::string_view model() const override { return cfg_.model; }

 private:
  ProviderConfig cfg_;
  IHttpClient& http_;
};

}  // namespace

std::string schemaInstruction(const nlohmann::json& schema) {
  if(schema.is_null() || schema.empty())
    return "Reply with a single JSON object and nothing else.";
  return "Reply with a single JSON object and nothing else (no prose, no code fences). "
         "It must match this JSON schema:\n" +
         schema.dump();
}

Result<LlmReply> MockProvider::complete(const LlmRequest& request) {
  requests.push_back(request);
  if(!scriptedErrors.empty()) {
    Error e = scriptedErrors.front();
    scriptedErrors.pop_front();
    return e;
  }
  LlmReply r;
  r.model = "mock";
  if(!scripted.empty()) {
    r.text = scripted.front();
    scripted.pop_front();
  } else {
    r.text = R"({"speech":"Hmm... Grog mi szumi w glowie. Mow dalej.","gesture":"think","actions":[]})";
  }
  r.inputTokens = static_cast<int>(request.system.size() / 4);
  r.outputTokens = static_cast<int>(r.text.size() / 4);
  return r;
}

std::unique_ptr<ILlmProvider> makeProvider(const ProviderConfig& config, IHttpClient& http) {
  switch(config.kind) {
    case ProviderKind::Anthropic: return std::make_unique<AnthropicProvider>(config, http);
    case ProviderKind::OpenAiCompatible: return std::make_unique<OpenAiCompatibleProvider>(config, http);
    case ProviderKind::Mock: return std::make_unique<MockProvider>();
  }
  return std::make_unique<MockProvider>();
}

}  // namespace grog::ai
