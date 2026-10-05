#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"

#include <array>
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

void test_two_of_three_multisig_primitives()
{
    using namespace quintum;
    using namespace quintum::consensus;

    const auto key_a = private_key(3U);
    const auto key_b = private_key(4U);
    const auto key_c = private_key(5U);

    const auto pub_a =
        crypto::derive_public_key(key_a);
    const auto pub_b =
        crypto::derive_public_key(key_b);
    const auto pub_c =
        crypto::derive_public_key(key_c);

    assert(pub_a && pub_b && pub_c);

    const std::array<crypto::PublicKey, 3>
        public_keys{
            *pub_a,
            *pub_b,
            *pub_c,
        };

    const auto locking =
        make_multisig_locking_script(
            2U,
            public_keys
        );

    assert(locking.has_value());

    TxOutput previous_output{
        .value = 5'000U,
        .locking_script = *locking,
    };

    OutPoint previous;
    previous.txid[0] = 0x91U;
    previous.index = 1U;

    auto tx = unsigned_spend(
        previous,
        locked_output(
            4'500U,
            private_key(6U)
        )
    );

    const auto sig_a =
        sign_multisig_signature(
            tx,
            0U,
            previous_output,
            0U,
            key_a
        );
    const auto sig_c =
        sign_multisig_signature(
            tx,
            0U,
            previous_output,
            2U,
            key_c
        );

    assert(sig_a.has_value());
    assert(sig_c.has_value());

    const std::array<MultisigSignature, 2>
        signatures{
            *sig_a,
            *sig_c,
        };

    const auto unlocking =
        make_multisig_unlocking_script(
            signatures
        );

    assert(unlocking.has_value());

    tx.inputs[0].unlocking_script =
        *unlocking;

    assert(
        verify_multisig_authorization(
            tx,
            0U,
            previous_output
        ) == InputAuthError::none
    );

    // A single signature cannot satisfy a 2-of-3 policy.
    const std::array<MultisigSignature, 1>
        one_signature{*sig_a};

    tx.inputs[0].unlocking_script =
        *make_multisig_unlocking_script(
            one_signature
        );

    assert(
        verify_multisig_authorization(
            tx,
            0U,
            previous_output
        ) ==
        InputAuthError::
            insufficient_signatures
    );

    // The canonical unlock encoding rejects duplicate/out-of-order signers.
    const std::array<MultisigSignature, 2>
        duplicate{
            *sig_a,
            *sig_a,
        };

    assert(
        !make_multisig_unlocking_script(
            duplicate
        ).has_value()
    );

    // A key that is not assigned to the selected slot cannot sign it.
    assert(
        !sign_multisig_signature(
            tx,
            0U,
            previous_output,
            1U,
            key_a
        ).has_value()
    );
}

} // namespace

int main()
{
    test_p2pk_sign_and_verify();
    test_wrong_key_and_malformed_scripts();
    test_two_of_three_multisig_primitives();
    return 0;
}
