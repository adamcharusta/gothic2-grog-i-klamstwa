// Non-Windows builds (Linux CI): no real HTTP yet. Tests use FakeHttp.
#include <grog/ai/http.h>

namespace grog::ai {
namespace {
class StubHttp : public IHttpClient {
 public:
  Result<HttpResponse> post(const HttpRequest&) override {
    return Error{ErrorKind::Unsupported, "HTTP is not implemented on this platform yet"};
  }
};
}  // namespace

std::unique_ptr<IHttpClient> makeHttpClient() { return std::make_unique<StubHttp>(); }

}  // namespace grog::ai
