#include "consensus/chainparams.hpp"

namespace quintum::consensus {
namespace {

consteval Byte hex_nibble(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return static_cast<Byte>(ch - '0');
    }
    if (ch >= 'a' && ch <= 'f') {
        return static_cast<Byte>(10 + (ch - 'a'));
    }
    if (ch >= 'A' && ch <= 'F') {
        return static_cast<Byte>(10 + (ch - 'A'));
    }
    return 0U;
}

consteval Hash256 hash256(std::string_view hex)
{
    Hash256 out{};

    for (std::size_t i = 0U; i < out.size(); ++i) {
        out[i] = static_cast<Byte>(
            (hex_nibble(hex[i * 2U]) << 4U) |
            hex_nibble(hex[i * 2U + 1U])
        );
    }

    return out;
}

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
    .genesis = GenesisParams{
        .enforce = true,
        .message = "QUINTUM 02/Oct/2026 1bac54a3f46c Independent PoW digital cash | mainnet",
        .timestamp = 1'790'960'400ULL,
        .bits = 0x1e0ffff0U,
        .nonce = 591'080ULL,
        .merkle_root = hash256("b1d9e1aedbe90d5b88148d4af6b0a64d6fb20c286d72192713147869e69580c5"),
        .hash = hash256("0000008b82073109dc079e6c5b7eac0c2fba8a5633822f87fc33719633ebab8c"),
    },
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
    .genesis = GenesisParams{
        .enforce = true,
        .message = "QUINTUM 02/Oct/2026 1bac54a3f46c Independent PoW digital cash | testnet",
        .timestamp = 1'790'960'400ULL,
        .bits = 0x1e0ffff0U,
        .nonce = 969'294ULL,
        .merkle_root = hash256("d5be95629c8ce60e22e817bc54accc3ed75983d5267b89724cd808f422a8c179"),
        .hash = hash256("0000039bf09b49dfa4c9bcb924c0c38257fdeef9952021baabceb86fa099e872"),
    },
};

constexpr ChainParams kRandomXTestnet{
    .network = Network::randomx_testnet,
    .name = "randomx-testnet",
    .p2p_protocol_version = 2U,
    .message_start = {0x3bU, 0xbfU, 0xb9U, 0xf0U},
    .p2p_port = 39444U,
    .rpc_port = 39445U,
    .pow = PowParams{
        .target_spacing_seconds = 120U,
        .retarget_interval = 1U,
        .pow_limit_bits = 0x1f7fffffU,
        .allow_min_difficulty_blocks = false,
        .no_retargeting = false,
        .difficulty_algorithm = DifficultyAlgorithm::asert,
        .asert_half_life_seconds = 34'560U,
        .asert_anchor_height = 0U,
        .pow_algorithm = PowAlgorithm::randomx_v2,
    },
    .monetary = MonetaryParams{
        .schedule = MonetarySchedule::randomx_v1,
        .max_money = kRandomXMoneyRange,
        .coinbase_maturity = kRandomXCoinbaseMaturity,
        .founder_payout_enabled = true,
        // TESTNET ONLY. Mainnet must use a separately generated custody key.
        .founder_public_key = crypto::PublicKey{
            0x03U, 0x01U, 0x26U, 0x2eU, 0x10U, 0xa8U, 0x41U, 0xefU,
            0x12U, 0xa9U, 0xe5U, 0x76U, 0x86U, 0x19U, 0xafU, 0x8dU,
            0xc1U, 0xdeU, 0xe9U, 0x1aU, 0x22U, 0xeeU, 0x01U, 0xf4U,
            0x89U, 0x9cU, 0x7dU, 0xcdU, 0xb9U, 0x69U, 0xe6U, 0x8aU,
            0x33U,
        },
    },
    .time = TimeParams{},
    .limits = ResourceLimits{},
    .genesis = GenesisParams{
        .enforce = true,
        .message = "QUINTUM 05/Oct/2026 RandomX public testnet v1 | CPU PoW",
        .timestamp = 1'791'158'400ULL,
        .bits = 0x1f7fffffU,
        .nonce = 80ULL,
        .merkle_root = hash256("1e11ac64fba90f543acd87018ec0d9da7ce11d56892026037728688b8c15cd0a"),
        .hash = hash256("89477dab8594e000e155b2a1e020ce8a73a3b03fa32568ac8773900794dca360"),
    },
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
    .genesis = GenesisParams{
        .enforce = true,
        .message = "QUINTUM 02/Oct/2026 1bac54a3f46c Independent PoW digital cash | regtest",
        .timestamp = 1'790'960'400ULL,
        .bits = 0x2100ffffU,
        .nonce = 0ULL,
        .merkle_root = hash256("01b9f141fff566d6d50e700ff0c59f07ff0a89123921340bd946f30386c09d89"),
        .hash = hash256("211c0cdb97dfb8bc2f0190e40132d1eb3be5ab9a03c9717aa3b2e90a98b3fcd6"),
    },
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

const ChainParams& randomx_testnet_params() noexcept
{
    return kRandomXTestnet;
}

const ChainParams& chain_params(Network network) noexcept
{
    switch (network) {
    case Network::mainnet:
        return kMainnet;
    case Network::testnet:
        return kTestnet;
    case Network::randomx_testnet:
        return kRandomXTestnet;
    case Network::regtest:
    default:
        return kRegtest;
    }
}

} // namespace quintum::consensus
