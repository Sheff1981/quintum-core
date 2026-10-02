#pragma once

#include "core/types.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace quintum::consensus {

enum class Network {
    mainnet,
    testnet,
    regtest,
};

struct PowParams {
    std::uint64_t target_spacing_seconds{0U};
    std::uint32_t retarget_interval{0U};
    std::uint32_t pow_limit_bits{0U};
    bool allow_min_difficulty_blocks{false};
    bool no_retargeting{false};
};

struct TimeParams {
    std::uint32_t median_time_span{11U};
    std::uint64_t max_future_seconds{7'200U};
};

struct ResourceLimits {
    std::uint64_t max_block_serialized_bytes{1'000'000U};
    std::uint32_t max_block_transactions{10'000U};
    std::uint32_t max_script_bytes{10'000U};
    std::uint32_t max_coinbase_script_bytes{100U};
};

struct GenesisParams {
    bool enforce{false};
    std::string_view message{};
    std::uint64_t timestamp{0U};
    std::uint32_t bits{0U};
    std::uint64_t nonce{0U};
    Hash256 merkle_root{};
    Hash256 hash{};
};

struct ChainParams {
    Network network{Network::regtest};
    std::string_view name{};
    std::array<Byte, 4> message_start{};
    std::uint16_t p2p_port{0U};
    std::uint16_t rpc_port{0U};
    PowParams pow{};
    TimeParams time{};
    ResourceLimits limits{};
    GenesisParams genesis{};
};

[[nodiscard]] const ChainParams& mainnet_params() noexcept;
[[nodiscard]] const ChainParams& testnet_params() noexcept;
[[nodiscard]] const ChainParams& regtest_params() noexcept;
[[nodiscard]] const ChainParams& chain_params(Network network) noexcept;

} // namespace quintum::consensus
