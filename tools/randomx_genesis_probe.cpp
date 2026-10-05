#include "consensus/chainparams.hpp"
#include "consensus/genesis.hpp"
#include "consensus/pow.hpp"
#include "consensus/randomx_seed.hpp"
#include "primitives/block.hpp"

#include <cstdint>
#include <iostream>
#include <string>

namespace {

std::string hex(const quintum::Hash256& value)
{
    static constexpr char digits[] =
        "0123456789abcdef";

    std::string out;
    out.reserve(value.size() * 2U);

    for (const auto byte : value) {
        out.push_back(
            digits[(byte >> 4U) & 0x0fU]
        );
        out.push_back(
            digits[byte & 0x0fU]
        );
    }

    return out;
}

} // namespace

int main()
{
    using namespace quintum;
    using namespace quintum::consensus;

    ChainParams params;
    params.network = Network::testnet;
    params.name = "randomx-testnet";
    params.message_start =
        {0x3bU, 0xbfU, 0xb9U, 0xf0U};
    params.p2p_port = 39444U;
    params.rpc_port = 39445U;

    params.pow.target_spacing_seconds = 120U;
    params.pow.retarget_interval = 1U;
    params.pow.pow_limit_bits = 0x1f7fffffU;
    params.pow.allow_min_difficulty_blocks = false;
    params.pow.no_retargeting = false;
    params.pow.difficulty_algorithm =
        DifficultyAlgorithm::asert;
    params.pow.asert_half_life_seconds = 34'560U;
    params.pow.asert_anchor_height = 0U;
    params.pow.pow_algorithm =
        PowAlgorithm::randomx_v2;

    params.monetary.schedule =
        MonetarySchedule::randomx_v1;
    params.monetary.max_money =
        kRandomXMoneyRange;
    params.monetary.coinbase_maturity =
        kRandomXCoinbaseMaturity;

    params.genesis.enforce = false;
    params.genesis.message =
        "QUINTUM 05/Oct/2026 RandomX public testnet v1 | CPU PoW";
    params.genesis.timestamp =
        1'791'158'400ULL;
    params.genesis.bits =
        params.pow.pow_limit_bits;
    params.genesis.nonce = 0U;

    Block block =
        create_genesis_block(params);

    const Hash256 zero{};
    const Hash256 seed =
        randomx_seed_key(0U, zero);

    constexpr std::uint64_t max_attempts{
        2'000'000U
    };

    const auto mined =
        mine_randomx_header(
            block.header,
            seed,
            max_attempts
        );

    if (!mined.found()) {
        std::cerr
            << "RANDOMX_GENESIS_NOT_FOUND attempts="
            << mined.attempts
            << " status="
            << static_cast<int>(mined.status)
            << '\n';
        return 1;
    }

    std::cout
        << "RANDOMX_GENESIS_VECTOR "
        << "timestamp="
        << block.header.timestamp
        << " bits=0x"
        << std::hex
        << block.header.bits
        << std::dec
        << " nonce="
        << block.header.nonce
        << " merkle="
        << hex(block.header.merkle_root)
        << " block_id="
        << hex(block_hash(block.header))
        << " pow_hash="
        << hex(mined.hash)
        << " seed="
        << hex(seed)
        << " attempts="
        << mined.attempts
        << '\n';

    return 0;
}
