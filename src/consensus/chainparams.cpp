#include "consensus/chainparams.hpp"

namespace quintum::consensus {
namespace {

constexpr ChainParams kMainnet{
    .network = Network::mainnet,
    .name = "mainnet",
    .message_start = {0x51U, 0xb7U, 0x4cU, 0xa3U},
    .p2p_port = 28444U,
    .rpc_port = 28445U,
    .pow = PowParams{
        .target_spacing_seconds = 600U,
        .retarget_interval = 2016U,
        .pow_limit_bits = 0x1e0ffff0U,
        .allow_min_difficulty_blocks = false,
        .no_retargeting = false,
    },
    .time = TimeParams{},
    .limits = ResourceLimits{},
};

constexpr ChainParams kTestnet{
    .network = Network::testnet,
    .name = "testnet",
    .message_start = {0xb7U, 0xd7U, 0x16U, 0x5aU},
    .p2p_port = 38444U,
    .rpc_port = 38445U,
    .pow = PowParams{
        .target_spacing_seconds = 600U,
        .retarget_interval = 2016U,
        .pow_limit_bits = 0x1e0ffff0U,
        .allow_min_difficulty_blocks = true,
        .no_retargeting = false,
    },
    .time = TimeParams{},
    .limits = ResourceLimits{},
};

constexpr ChainParams kRegtest{
    .network = Network::regtest,
    .name = "regtest",
    .message_start = {0x33U, 0x20U, 0xe2U, 0xeeU},
    .p2p_port = 48444U,
    .rpc_port = 48445U,
    .pow = PowParams{
        .target_spacing_seconds = 1U,
        .retarget_interval = 144U,
        .pow_limit_bits = 0x2100ffffU,
        .allow_min_difficulty_blocks = false,
        .no_retargeting = true,
    },
    .time = TimeParams{},
    .limits = ResourceLimits{},
};

} // namespace

const ChainParams& mainnet_params() noexcept
{
    return kMainnet;
}

const ChainParams& testnet_params() noexcept
{
    return kTestnet;
}

const ChainParams& regtest_params() noexcept
{
    return kRegtest;
}

const ChainParams& chain_params(Network network) noexcept
{
    switch (network) {
    case Network::mainnet:
        return kMainnet;
    case Network::testnet:
        return kTestnet;
    case Network::regtest:
    default:
        return kRegtest;
    }
}

} // namespace quintum::consensus
