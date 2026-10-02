#include "primitives/transaction.hpp"

#include <cassert>
#include <cstdint>
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

quintum::Transaction make_reference_transaction()
{
    quintum::Transaction tx;

    quintum::TxInput input;
    for (std::size_t i = 0; i < input.previous_output.txid.size(); ++i) {
        input.previous_output.txid[i] = static_cast<quintum::Byte>(i);
    }
    input.previous_output.index = 1U;
    input.unlocking_script = {0x51U};
    input.sequence = 0xfffffffeU;
    tx.inputs.push_back(input);

    quintum::TxOutput output;
    output.value = 5000U;
    output.locking_script = {0x51U};
    tx.outputs.push_back(output);

    return tx;
}

void test_reference_serialization_and_txid()
{
    const auto tx = make_reference_transaction();
    const auto bytes = quintum::serialize_transaction(tx);

    assert(
        bytes_to_hex(bytes) ==
        "0100000001000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
        "010000000151feffffff018813000000000000015100000000"
    );

    assert(
        hash_to_hex(quintum::transaction_id(tx)) ==
        "3fc0ae68a31e60f972bdbad7815d242dd5c39aec6163475a0ccc7263a65616c9"
    );
}

void test_structure_validation()
{
    quintum::Transaction empty;
    assert(
        quintum::validate_transaction_structure(empty) ==
        quintum::TxStructureError::no_inputs
    );

    auto tx = make_reference_transaction();
    assert(
        quintum::validate_transaction_structure(tx) ==
        quintum::TxStructureError::none
    );

    tx.inputs.push_back(tx.inputs.front());
    assert(
        quintum::validate_transaction_structure(tx) ==
        quintum::TxStructureError::duplicate_input
    );
}

void test_coinbase_shape()
{
    quintum::Transaction tx;
    tx.inputs.push_back(quintum::TxInput{});
    tx.outputs.push_back(quintum::TxOutput{});

    assert(tx.inputs.front().previous_output.is_null());
    assert(tx.is_coinbase());
}

} // namespace

int main()
{
    test_reference_serialization_and_txid();
    test_structure_validation();
    test_coinbase_shape();
    return 0;
}
