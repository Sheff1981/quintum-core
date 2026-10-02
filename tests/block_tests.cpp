#include "primitives/block.hpp"

#include <cassert>
#include <string>

namespace {

std::string bytes_to_hex(const quintum::Bytes& bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2U);
    for (const auto byte : bytes) {
        out.push_back(digits[(byte >> 4U) & 0x0fU]);
        out.push_back(digits[byte & 0x0fU]);
    }
    return out;
}

std::string hash_to_hex(const quintum::Hash256& hash)
{
    return bytes_to_hex(quintum::Bytes{hash.begin(), hash.end()});
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

void test_single_transaction_merkle()
{
    const auto coinbase = make_coinbase();
    const auto merkle = quintum::compute_merkle_root(
        std::span<const quintum::Transaction>{&coinbase, 1U}
    );

    assert(!merkle.mutated);
    assert(
        hash_to_hex(merkle.root) ==
        "1d321c47172414a200bc2089b0a3177e49757dca14b5e424118d56312f7c067b"
    );
}

void test_header_vector()
{
    quintum::Block block;
    block.transactions.push_back(make_coinbase());
    quintum::update_merkle_root(block);

    block.header.timestamp = 1700000000ULL;
    block.header.bits = 0x1f00ffffU;
    block.header.nonce = 42U;

    const auto serialized = quintum::serialize_block_header(block.header);
    assert(serialized.size() == 88U);
    assert(
        hash_to_hex(quintum::block_hash(block.header)) ==
        "8cd82dd06b85d2aa03f153a1c250782bb73d8ea573dc879fa0bca89b945f9329"
    );

    assert(
        quintum::validate_block_structure(block) ==
        quintum::BlockStructureError::none
    );
}

void test_merkle_mutation_detection()
{
    const auto coinbase = make_coinbase();
    const std::array<quintum::Transaction, 2> duplicate{coinbase, coinbase};
    const auto merkle = quintum::compute_merkle_root(duplicate);
    assert(merkle.mutated);
}

} // namespace

int main()
{
    test_single_transaction_merkle();
    test_header_vector();
    test_merkle_mutation_detection();
    return 0;
}
