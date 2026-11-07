#pragma once
#include <grog/ai/result.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace grog::ai {

struct HttpRequest {
  std::string url;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
  int timeoutS = 20;
};

struct HttpResponse {
  int status = 0;
  std::string body;
};

// Blocking POST. Call from a worker thread (GameSession::talkAsync does this).
// Errors: Network (DNS, refused, TLS), Timeout. HTTP error statuses are returned as responses.
class IHttpClient {
 public:
  virtual ~IHttpClient() = default;
  virtual Result<HttpResponse> post(const HttpRequest& request) = 0;
};

// WinHTTP on Windows; elsewhere a stub that returns ErrorKind::Unsupported.
std::unique_ptr<IHttpClient> makeHttpClient();

}  // namespace grog::ai
