#pragma once

#include <cstdint>

namespace quintum {

class Chainstate;

namespace consensus {

inline constexpr std::uint32_t kVersionBitsTopMask =
    0xe0000000U;
inline constexpr std::uint32_t kVersionBitsTopBits =
    0x20000000U;
inline constexpr std::uint8_t kVersionBitsMaxBit = 28U;

enum class DeploymentState {
    defined,
    started,
    locked_in,
    active,
    failed,
};

struct DeploymentParams {
    std::uint8_t bit{0U};
    std::uint32_t start_height{0U};
    std::uint32_t timeout_height{0U};
    std::uint32_t period{0U};
    std::uint32_t threshold{0U};
    std::uint32_t min_activation_height{0U};
    bool lockin_on_timeout{false};
};

[[nodiscard]] bool valid_deployment(
    const DeploymentParams& params
) noexcept;

[[nodiscard]] bool versionbits_signals(
    std::uint32_t block_version,
    std::uint8_t bit
) noexcept;

// Returns the deployment state that applies to the next block extending
// the current active chain. State changes happen only on period boundaries.
[[nodiscard]] DeploymentState deployment_state(
    const Chainstate& chain,
    const DeploymentParams& params
);

} // namespace consensus
} // namespace quintum
