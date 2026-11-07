#include <doctest/doctest.h>

#include <grog/ai/rng.h>

using namespace grog::ai;

TEST_CASE("xoshiro256** seeded by splitmix64 matches reference values (portable seeds)") {
  // Reference: Python implementation of the published algorithms, seed 42.
  Rng r(42);
  CHECK(r.next() == 0x15780b2e0c2ec716ULL);
  CHECK(r.next() == 0x6104d9866d113a7eULL);
  CHECK(r.next() == 0xae17533239e499a1ULL);
}

TEST_CASE("uniform stays in range and hits every value") {
  Rng r(7);
  int seen[7] = {};
  for(int i = 0; i < 10000; ++i) {
    uint32_t v = r.uniform(7);
    REQUIRE(v < 7);
    ++seen[v];
  }
  for(int c : seen)
    CHECK(c > 1000);
}

TEST_CASE("same seed, same sequence") {
  Rng a(123), b(123);
  for(int i = 0; i < 100; ++i)
    CHECK(a.uniform(1000) == b.uniform(1000));
}
