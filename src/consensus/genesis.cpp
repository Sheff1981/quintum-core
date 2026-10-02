#include "consensus/genesis.hpp"

#include "consensus/block_limits.hpp"
#include "consensus/monetary.hpp"
#include "consensus/pow.hpp"
#include "consensus/tx_auth.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace quintum::consensus {
namespace {

Bytes ascii_bytes(std::string_view text)
{
    Bytes out;
    out.reserve(text.size());

    for (const char ch : text) {
        out.push_back(static_cast<Byte>(
            static_cast<unsigned char>(ch)
        ));
    }

    return out;
}

Bytes genesis_locking_script()
{
    constexpr std::string_view marker{
        "QUINTUM-GENESIS"
    };

    const auto payload = ascii_bytes(marker);
    return make_provably_unspendable_script(payload);
}

bool is_zero_hash(const Hash256& value) noexcept
{
    return std::all_of(
        value.begin(),
        value.end(),
        [](Byte byte) {
            return byte == 0U;
        }
    );
}

} // namespace

Transaction create_genesis_coinbase(
    const ChainParams& params)
{
    Transaction tx;

    TxInput input;
    input.unlocking_script =
        ascii_bytes(params.genesis.message);
    tx.inputs.push_back(std::move(input));

    TxOutput output;
    output.value = block_subsidy(0U);
    output.locking_script =
        genesis_locking_script();
    tx.outputs.push_back(std::move(output));

    return tx;
}

Block create_genesis_block(
    const ChainParams& params)
{
    Block block;

    block.header.version = 1U;
    block.header.previous_block = {};
    block.header.timestamp =
        params.genesis.timestamp;
    block.header.bits =
        params.genesis.bits;
    block.header.nonce =
        params.genesis.nonce;

    block.transactions.push_back(
        create_genesis_coinbase(params)
    );

    update_merkle_root(block);
    return block;
}

bool verify_genesis(
    const ChainParams& params)
{
    if (!params.genesis.enforce ||
        params.genesis.message.empty()) {
        return false;
    }

    if (params.genesis.bits !=
        params.pow.pow_limit_bits) {
        return false;
    }

    const auto block =
        create_genesis_block(params);

    if (!is_zero_hash(
            block.header.previous_block)) {
        return false;
    }

    if (block.header.merkle_root !=
        params.genesis.merkle_root) {
        return false;
    }

    if (block_hash(block.header) !=
        params.genesis.hash) {
        return false;
    }

    if (validate_block_structure(block) !=
        BlockStructureError::none) {
        return false;
    }

    if (validate_block_resources(
            block,
            params.limits) !=
        BlockResourceError::none) {
        return false;
    }

    if (check_proof_of_work(
            block.header,
            params.pow) !=
        PowCheckError::none) {
        return false;
    }

    if (!coinbase_reward_is_valid(
            block.transactions.front(),
            0U,
            0U)) {
        return false;
    }

    if (block.transactions.front()
            .outputs.empty() ||
        !is_provably_unspendable(
            block.transactions.front()
                .outputs.front()
                .locking_script)) {
        return false;
    }

    return true;
}

} // namespace quintum::consensus
