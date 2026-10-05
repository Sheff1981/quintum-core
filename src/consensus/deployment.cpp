#include "consensus/deployment.hpp"

#include "chain/chainstate.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace quintum::consensus {

bool valid_deployment(
    const DeploymentParams& params) noexcept
{
    if (params.bit >
            kVersionBitsMaxBit ||
        params.period == 0U ||
        params.threshold == 0U ||
        params.threshold > params.period ||
        params.timeout_height <=
            params.start_height) {
        return false;
    }

    if (params.start_height %
            params.period != 0U ||
        params.timeout_height %
            params.period != 0U ||
        params.min_activation_height %
            params.period != 0U) {
        return false;
    }

    return true;
}

bool versionbits_signals(
    std::uint32_t block_version,
    std::uint8_t bit) noexcept
{
    if (bit > kVersionBitsMaxBit ||
        (block_version &
             kVersionBitsTopMask) !=
            kVersionBitsTopBits) {
        return false;
    }

    const std::uint32_t mask =
        std::uint32_t{1U} << bit;

    return (block_version & mask) != 0U;
}

DeploymentState deployment_state(
    const Chainstate& chain,
    const DeploymentParams& params)
{
    if (!valid_deployment(params)) {
        return DeploymentState::failed;
    }

    const std::uint64_t next_height =
        chain.height()
            ? static_cast<std::uint64_t>(
                  *chain.height()) +
                  1U
            : 0U;

    DeploymentState state =
        DeploymentState::defined;

    const std::uint64_t period =
        params.period;

    for (std::uint64_t period_start = 0U;
         period_start + period <=
             next_height;
         period_start += period) {
        const std::uint64_t
            next_period_start =
                period_start + period;

        switch (state) {
        case DeploymentState::defined:
            if (next_period_start >=
                params.start_height) {
                state =
                    DeploymentState::
                        started;
            }
            break;

        case DeploymentState::started: {
            if (next_period_start >=
                params.timeout_height) {
                state =
                    params.lockin_on_timeout
                        ? DeploymentState::
                              locked_in
                        : DeploymentState::
                              failed;
                break;
            }

            std::uint32_t signals{0U};

            for (std::uint64_t height =
                     period_start;
                 height <
                     next_period_start;
                 ++height) {
                if (height >
                    std::numeric_limits<
                        std::uint32_t>::max()) {
                    break;
                }

                const auto header =
                    chain.active_header(
                        static_cast<
                            std::uint32_t>(
                            height)
                    );

                if (header &&
                    versionbits_signals(
                        header->version,
                        params.bit)) {
                    ++signals;
                }
            }

            if (signals >=
                params.threshold) {
                state =
                    DeploymentState::
                        locked_in;
            }
            break;
        }

        case DeploymentState::locked_in:
            if (next_period_start >=
                std::max(
                    params.
                        min_activation_height,
                    params.start_height)) {
                state =
                    DeploymentState::
                        active;
            }
            break;

        case DeploymentState::active:
        case DeploymentState::failed:
            return state;
        }
    }

    return state;
}

} // namespace quintum::consensus
