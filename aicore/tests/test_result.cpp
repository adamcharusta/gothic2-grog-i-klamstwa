#include <doctest/doctest.h>

#include <grog/ai/result.h>
#include <grog/ai/version.h>

#include "test_helpers.h"

using namespace grog::ai;

TEST_CASE("Result holds a value") {
  Result<int> r(42);
  CHECK(r.ok());
  CHECK(r.value() == 42);
}

TEST_CASE("Result holds an error") {
  Result<int> r(Error{ErrorKind::Auth, "bad key"});
  CHECK_FALSE(r.ok());
  CHECK(r.error().kind == ErrorKind::Auth);
  CHECK(r.error().message == "bad key");
}

TEST_CASE("errorKindName names every kind") {
  CHECK(std::string(errorKindName(ErrorKind::RateLimit)) == "RateLimit");
  CHECK(std::string(errorKindName(ErrorKind::Timeout)) == "Timeout");
}

TEST_CASE("version matches the VERSION file") {
  std::string v = testutil::repoFile("VERSION");
  while(!v.empty() && (v.back() == '\n' || v.back() == '\r' || v.back() == ' '))
    v.pop_back();
  CHECK(std::string(version()) == v);
}
