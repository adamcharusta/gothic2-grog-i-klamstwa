// Windows HTTP(S) client on WinHTTP. Blocking; run it on a worker thread.
#include <grog/ai/http.h>

#include <windows.h>
#include <winhttp.h>

#include <string>
#include <vector>

namespace grog::ai {
namespace {

std::wstring widen(std::string_view s) {
  if(s.empty())
    return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
  std::wstring w(size_t(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
  return w;
}

struct Handle {
  HINTERNET h = nullptr;
  explicit Handle(HINTERNET x) : h(x) {}
  ~Handle() {
    if(h)
      WinHttpCloseHandle(h);
  }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  explicit operator bool() const { return h != nullptr; }
};

Error lastError(const char* what) {
  DWORD e = GetLastError();
  if(e == ERROR_WINHTTP_TIMEOUT)
    return {ErrorKind::Timeout, "the AI provider did not answer in time"};
  return {ErrorKind::Network, std::string("network error (") + what + ", code " + std::to_string(e) + ")"};
}

class WinHttpClient : public IHttpClient {
 public:
  Result<HttpResponse> post(const HttpRequest& req) override {
    std::wstring url = widen(req.url);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = DWORD(std::size(host));
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = DWORD(std::size(path));
    if(!WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
      return Error{ErrorKind::Config, "invalid URL: " + req.url};

    Handle session(WinHttpOpen(L"GrogIKlamstwa/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0));
    if(!session)
      return lastError("open");
    int ms = req.timeoutS * 1000;
    WinHttpSetTimeouts(session.h, ms, ms, ms, ms);

    Handle conn(WinHttpConnect(session.h, host, uc.nPort, 0));
    if(!conn)
      return lastError("connect");
    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    Handle request(WinHttpOpenRequest(conn.h, L"POST", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                      flags));
    if(!request)
      return lastError("request");

    std::wstring headers;
    for(auto& [k, v] : req.headers)
      headers += widen(k) + L": " + widen(v) + L"\r\n";
    if(!WinHttpSendRequest(request.h, headers.c_str(), DWORD(-1L), const_cast<char*>(req.body.data()),
                           DWORD(req.body.size()), DWORD(req.body.size()), 0))
      return lastError("send");
    if(!WinHttpReceiveResponse(request.h, nullptr))
      return lastError("receive");

    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &size, WINHTTP_NO_HEADER_INDEX);

    HttpResponse out;
    out.status = int(status);
    for(;;) {
      DWORD avail = 0;
      if(!WinHttpQueryDataAvailable(request.h, &avail))
        return lastError("read");
      if(avail == 0)
        break;
      std::vector<char> buf(avail);
      DWORD read = 0;
      if(!WinHttpReadData(request.h, buf.data(), avail, &read))
        return lastError("read");
      out.body.append(buf.data(), read);
    }
    return out;
  }
};

}  // namespace

std::unique_ptr<IHttpClient> makeHttpClient() { return std::make_unique<WinHttpClient>(); }

}  // namespace grog::ai
