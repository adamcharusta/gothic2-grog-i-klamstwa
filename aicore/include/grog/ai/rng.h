#pragma once
#include <cstdint>
#include <utility>
#include <vector>

namespace grog::ai {

// xoshiro256** seeded with splitmix64. Fully specified, so the same seed gives the same
// scenario on every compiler (std:: distributions are implementation-defined).
class Rng {
 public:
  explicit Rng(uint64_t seed);
  uint64_t next();
  uint32_t uniform(uint32_t n);  // [0, n), unbiased (rejection sampling); n must be > 0

  template <class T>
  const T& pick(const std::vector<T>& v) {
    return v[uniform(static_cast<uint32_t>(v.size()))];
  }

  // Fisher-Yates with uniform().
  template <class T>
  void shuffle(std::vector<T>& v) {
    for(size_t i = v.size(); i > 1; --i)
      std::swap(v[i - 1], v[uniform(static_cast<uint32_t>(i))]);
  }

 private:
  uint64_t s_[4];
};

}  // namespace grog::ai
