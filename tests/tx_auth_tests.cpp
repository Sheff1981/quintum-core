#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"

#include <cassert>

namespace {

quintum::crypto::PrivateKey private_key(quintum::Byte value)
{
    quintum::crypto::PrivateKey key{};
    key.back() = value;
    return key;
}

quintum::TxOutput locked_output(
    quintum::Amount value,
    const quintum::crypto::PrivateKey& key)
{
    const auto public_key =
        quintum::crypto::derive_public_key(key);
    assert(public_key);

    return quintum::TxOutput{
        .value = value,
        .locking_script =
            quintum::consensus::make_p2pk_locking_script(
                *public_key
            ),
    };
}

quintum::Transaction unsigned_spend(
    const quintum::OutPoint& previous,
    const quintum::TxOutput& destination)
{
    quintum::Transaction tx;
    tx.inputs.push_back(quintum::TxInput{
        .previous_output = previous,
    });
    tx.outputs.push_back(destination);
    return tx;
}

void test_p2pk_sign_and_verify()
{
    const auto owner = private_key(1U);
    const auto receiver = private_key(2U);

    const auto previous_output =
        locked_output(1000U, owner);

    quintum::OutPoint previous;
    previous.txid[0] = 0x42U;
    previous.index = 3U;

    auto tx = unsigned_spend(
        previous,
        locked_output(900U, receiver)
    );

    assert(
        quintum::consensus::sign_p2pk_input(
            tx,
            0U,
            previous_output,
            owner
        ) == quintum::consensus::InputAuthError::none
    );

    assert(
        quintum::consensus::verify_input_authorization(
            tx,
            0U,
            previous_output
        ) == quintum::consensus::InputAuthError::none
    );

    auto tampered = tx;
    tampered.outputs[0].value = 899U;

    assert(
        quintum::consensus::verify_input_authorization(
            tampered,
            0U,
            previous_output
        ) ==
        quintum::consensus::InputAuthError::invalid_signature
    );
}

void test_wrong_key_and_malformed_scripts()
{
    const auto owner = private_key(1U);
    const auto attacker = private_key(2U);

    const auto previous_output =
        locked_output(1000U, owner);

    quintum::OutPoint previous;
    previous.txid[0] = 0x24U;
    previous.index = 0U;

    auto tx = unsigned_spend(
        previous,
        locked_output(900U, attacker)
    );

    assert(
        quintum::consensus::sign_p2pk_input(
            tx,
            0U,
            previous_output,
            attacker
        ) ==
        quintum::consensus::InputAuthError::wrong_private_key
    );

    assert(
        quintum::consensus::verify_input_authorization(
            tx,
            0U,
            previous_output
        ) ==
        quintum::consensus::InputAuthError::malformed_unlocking_script
    );

    auto malformed_previous = previous_output;
    malformed_previous.locking_script = {0x51U};

    assert(
        quintum::consensus::verify_input_authorization(
            tx,
            0U,
            malformed_previous
        ) ==
        quintum::consensus::InputAuthError::malformed_locking_script
    );
}

} // namespace

int main()
{
    test_p2pk_sign_and_verify();
    test_wrong_key_and_malformed_scripts();
    return 0;
}
