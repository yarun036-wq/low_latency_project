#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace lob {

class LatencyHistogram {
public:
  static constexpr std::size_t bucket_count = 64;

  void record(std::uint64_t value_ns) noexcept {
    ++samples_;
    max_ns_ = std::max(max_ns_, value_ns);

    std::size_t bucket = 0;
    std::uint64_t probe = value_ns;
    while (probe > 0 && bucket + 1 < bucket_count) {
      probe >>= 1U;
      ++bucket;
    }
    ++buckets_[bucket];
  }

  [[nodiscard]] std::uint64_t percentile(double p) const noexcept {
    if (samples_ == 0) {
      return 0;
    }

    const auto target = static_cast<std::uint64_t>((p / 100.0) * static_cast<double>(samples_ - 1));
    std::uint64_t seen = 0;
    for (std::size_t index = 0; index < bucket_count; ++index) {
      seen += buckets_[index];
      if (seen > target) {
        return index == 0 ? 0 : (1ULL << (index - 1U));
      }
    }
    return max_ns_;
  }

  [[nodiscard]] std::uint64_t count() const noexcept {
    return samples_;
  }

private:
  std::array<std::uint64_t, bucket_count> buckets_{};
  std::uint64_t samples_{};
  std::uint64_t max_ns_{};
};

}  // namespace lob
