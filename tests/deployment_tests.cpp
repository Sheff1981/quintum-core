#include "chain/chainstate.hpp"
#include "consensus/deployment.hpp"
#include "consensus/genesis.hpp"
#include "consensus/pow.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "mining/block_template.hpp"

#include <cassert>
#include <cstdint>

namespace {

quintum::Bytes payout_script()
{
    quintum::crypto::PrivateKey key{};
    key.back() = 7U;

    const auto pub =
        quintum::crypto::derive_public_key(
            key
        );
    assert(pub.has_value());

    return quintum::consensus::
        make_p2pk_locking_script(*pub);
}

void append_block(
    quintum::Chainstate& chain,
    std::uint64_t timestamp,
    bool signal,
    std::uint8_t bit)
{
    auto candidate =
        quintum::mining::
            create_block_template(
                chain,
                payout_script(),
                timestamp
            );

    assert(candidate.ok());

    candidate.value.block.header.version =
        signal
            ? quintum::consensus::
                  kVersionBitsTopBits |
                  (std::uint32_t{1U}
                   << bit)
            : 1U;

    const auto mined =
        quintum::consensus::mine_header(
            candidate.value.block.header,
            100'000U
        );

    assert(mined.found());

    const auto connected =
        chain.connect_block(
            candidate.value.block,
            timestamp
        );

    assert(connected.ok());
}

void test_versionbits_state_machine()
{
    using namespace quintum;
    using namespace quintum::consensus;

    const auto& params =
        regtest_params();

    Chainstate chain{params};

    const auto genesis =
        create_genesis_block(params);

    assert(chain.connect_block(
               genesis,
               params.genesis.timestamp)
               .ok());

    const DeploymentParams deployment{
        .bit = 5U,
        .start_height = 4U,
        .timeout_height = 20U,
        .period = 4U,
        .threshold = 3U,
        .min_activation_height = 12U,
        .lockin_on_timeout = false,
    };

    assert(valid_deployment(deployment));
    assert(deployment_state(
               chain,
               deployment) ==
           DeploymentState::defined);

    std::uint64_t time =
        params.genesis.timestamp;

    // Finish the first period: deployment enters STARTED for height 4.
    for (std::uint32_t h = 1U;
         h <= 3U;
         ++h) {
        append_block(
            chain,
            ++time,
            false,
            deployment.bit
        );
    }

    assert(deployment_state(
               chain,
               deployment) ==
           DeploymentState::started);

    // Signal 3/4 in the next period. At height 8 the deployment locks in.
    for (std::uint32_t h = 4U;
         h <= 7U;
         ++h) {
        append_block(
            chain,
            ++time,
            h <= 6U,
            deployment.bit
        );
    }

    assert(deployment_state(
               chain,
               deployment) ==
           DeploymentState::locked_in);

    // LOCKED_IN persists until the configured minimum activation boundary.
    for (std::uint32_t h = 8U;
         h <= 11U;
         ++h) {
        append_block(
            chain,
            ++time,
            false,
            deployment.bit
        );
    }

    assert(deployment_state(
               chain,
               deployment) ==
           DeploymentState::active);
}

void test_versionbits_marker_required()
{
    using namespace quintum::consensus;

    assert(versionbits_signals(
        kVersionBitsTopBits |
            (std::uint32_t{1U} << 7U),
        7U
    ));

    assert(!versionbits_signals(
        std::uint32_t{1U} << 7U,
        7U
    ));

    assert(!versionbits_signals(
        kVersionBitsTopBits,
        7U
    ));
}

} // namespace

int main()
{
    test_versionbits_state_machine();
    test_versionbits_marker_required();
    return 0;
}
