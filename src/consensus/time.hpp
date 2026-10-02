#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace quintum::consensus {

[[nodiscard]] std::optional<std::uint64_t> median_timestamp(
    std::span<const std::uint64_t> timestamps
);

[[nodiscard]] bool timestamp_not_too_far_future(
    std::uint64_t block_timestamp,
    std::uint64_t adjusted_time,
    std::uint64_t max_future_seconds
) noexcept;

} // namespace quintum::consensus
