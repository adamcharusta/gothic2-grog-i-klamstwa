#include <grog/ai/rng.h>

namespace grog::ai {
namespace {
uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
uint64_t splitmix64(uint64_t& x) {
  uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}
}  // namespace

Rng::Rng(uint64_t seed) {
  for(auto& s : s_)
    s = splitmix64(seed);
}

uint64_t Rng::next() {
  const uint64_t result = rotl(s_[1] * 5, 7) * 9;
  const uint64_t t = s_[1] << 17;
  s_[2] ^= s_[0];
  s_[3] ^= s_[1];
  s_[1] ^= s_[2];
  s_[0] ^= s_[3];
  s_[2] ^= t;
  s_[3] = rotl(s_[3], 45);
  return result;
}

uint32_t Rng::uniform(uint32_t n) {
  if(n <= 1)
    return 0;
  // Rejection sampling on the high 32 bits: unbiased and fully specified.
  const uint32_t limit = UINT32_MAX - (UINT32_MAX % n);
  for(;;) {
    uint32_t v = static_cast<uint32_t>(next() >> 32);
    if(v < limit)
      return v % n;
  }
}

}  // namespace grog::ai
