#pragma once

#include <cstdint>
#include <optional>

namespace quintum::net {

inline constexpr std::uint32_t kDefaultDandelionFluffPercent = 10U;
inline constexpr std::uint64_t kDefaultDandelionEmbargoMinSeconds = 10U;
inline constexpr std::uint64_t kDefaultDandelionEmbargoJitterSeconds = 20U;
inline constexpr std::uint64_t kDefaultDandelionEpochSeconds = 600U;

[[nodiscard]] bool dandelion_should_fluff(
    std::uint64_t random_value,
    std::uint32_t fluff_percent
) noexcept;

[[nodiscard]] std::uint64_t dandelion_embargo_delay(
    std::uint64_t random_value,
    std::uint64_t minimum_seconds,
    std::uint64_t jitter_seconds
) noexcept;

[[nodiscard]] std::optional<std::uint64_t>
secure_dandelion_random() noexcept;

} // namespace quintum::net
