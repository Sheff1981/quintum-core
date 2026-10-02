#include "consensus/pow.hpp"
#include "primitives/block.hpp"

#include <cassert>
#include <string>

namespace {

std::string hash_to_hex(const quintum::Hash256& hash)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(hash.size() * 2U);

    for (const auto byte : hash) {
        out.push_back(digits[(byte >> 4U) & 0x0fU]);
        out.push_back(digits[byte & 0x0fU]);
    }
    return out;
}

quintum::Transaction make_coinbase()
{
    quintum::Transaction tx;

    quintum::TxInput input;
    input.unlocking_script = {0x01U};
    tx.inputs.push_back(input);

    tx.outputs.push_back(quintum::TxOutput{
        .value = 5000U,
        .locking_script = {0x51U},
    });

    return tx;
}

quintum::BlockHeader make_header(std::uint32_t bits)
{
    quintum::Block block;
    block.transactions.push_back(make_coinbase());
    quintum::update_merkle_root(block);

    block.header.timestamp = 1700000000ULL;
    block.header.bits = bits;
    block.header.nonce = 0U;
    return block.header;
}

void test_compact_target_vectors()
{
    const auto bitcoin_style = quintum::consensus::decode_compact_target(0x1d00ffffU);
    assert(bitcoin_style.valid());
    assert(
        hash_to_hex(bitcoin_style.target) ==
        "00000000ffff0000000000000000000000000000000000000000000000000000"
    );
    assert(
        quintum::consensus::encode_compact_target(bitcoin_style.target) ==
        0x1d00ffffU
    );

    const auto easy = quintum::consensus::decode_compact_target(0x2100ffffU);
    assert(easy.valid());
    assert(
        hash_to_hex(easy.target) ==
        "ffff000000000000000000000000000000000000000000000000000000000000"
    );
    assert(
        quintum::consensus::encode_compact_target(easy.target) ==
        0x2100ffffU
    );
}

void test_invalid_targets()
{
    const auto zero = quintum::consensus::decode_compact_target(0U);
    assert(!zero.valid());

    const auto negative = quintum::consensus::decode_compact_target(0x1d80ffffU);
    assert(negative.negative);
    assert(!negative.valid());

    const auto overflow = quintum::consensus::decode_compact_target(0x2300ffffU);
    assert(overflow.overflow);
    assert(!overflow.valid());

    auto non_canonical_header = make_header(0x02000100U);
    assert(
        quintum::consensus::check_proof_of_work(non_canonical_header) ==
        quintum::consensus::PowCheckError::invalid_target
    );
}

void test_chain_work()
{
    const auto compact =
        quintum::consensus::decode_compact_target(0x1d00ffffU);

    const auto work =
        quintum::consensus::work_for_target(compact.target);

    assert(
        hash_to_hex(work) ==
        "0000000000000000000000000000000000000000000000000000000100010001"
    );

    quintum::Hash256 accumulated{};
    assert(quintum::consensus::add_chain_work(accumulated, work));
    assert(accumulated == work);

    assert(quintum::consensus::add_chain_work(accumulated, work));
    assert(
        hash_to_hex(accumulated) ==
        "0000000000000000000000000000000000000000000000000000000200020002"
    );

    quintum::Hash256 maximum{};
    maximum.fill(0xffU);
    const auto minimum_work =
        quintum::consensus::work_for_target(maximum);

    assert(
        hash_to_hex(minimum_work) ==
        "0000000000000000000000000000000000000000000000000000000000000001"
    );
}

void test_real_nonce_mining()
{
    auto header = make_header(0x2100ffffU);

    const auto result = quintum::consensus::mine_header(header, 10U);

    assert(result.found());
    assert(result.attempts == 1U);
    assert(result.nonce == 0U);
    assert(header.nonce == result.nonce);
    assert(
        hash_to_hex(result.hash) ==
        "98fff58d5138e732f7e72445fe26a853e1ac099ca0a8f674d17ce3ed127296e6"
    );
    assert(
        quintum::consensus::check_proof_of_work(header) ==
        quintum::consensus::PowCheckError::none
    );
}

void test_hash_above_target_is_rejected()
{
    auto header = make_header(0x1d00ffffU);

    assert(
        quintum::consensus::check_proof_of_work(header) ==
        quintum::consensus::PowCheckError::hash_above_target
    );
}

} // namespace

int main()
{
    test_compact_target_vectors();
    test_invalid_targets();
    test_chain_work();
    test_real_nonce_mining();
    test_hash_above_target_is_rejected();
    return 0;
}
