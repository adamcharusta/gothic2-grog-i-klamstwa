#pragma once
#include <grog/ai/http.h>

#include <deque>
#include <vector>

// Records requests and returns canned results in order.
class FakeHttp : public grog::ai::IHttpClient {
 public:
  std::vector<grog::ai::HttpRequest> requests;
  std::deque<grog::ai::Result<grog::ai::HttpResponse>> replies;

  void reply(int status, std::string body) { replies.push_back(grog::ai::HttpResponse{status, std::move(body)}); }
  void fail(grog::ai::ErrorKind kind, std::string msg) { replies.push_back(grog::ai::Error{kind, std::move(msg)}); }

  grog::ai::Result<grog::ai::HttpResponse> post(const grog::ai::HttpRequest& req) override {
    requests.push_back(req);
    if(replies.empty())
      return grog::ai::Error{grog::ai::ErrorKind::Network, "no canned reply"};
    auto r = replies.front();
    replies.pop_front();
    return r;
  }

  std::string header(size_t i, const std::string& name) const {
    for(auto& [k, v] : requests.at(i).headers)
      if(k == name)
        return v;
    return {};
  }
  bool hasHeader(size_t i, const std::string& name) const {
    for(auto& [k, v] : requests.at(i).headers)
      if(k == name)
        return true;
    return false;
  }
};
