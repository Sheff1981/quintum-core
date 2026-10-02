#include "consensus/time.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace quintum::consensus {

std::optional<std::uint64_t> median_timestamp(
    std::span<const std::uint64_t> timestamps)
{
    if (timestamps.empty()) {
        return std::nullopt;
    }

    std::vector<std::uint64_t> values{
        timestamps.begin(),
        timestamps.end()
    };

    std::sort(values.begin(), values.end());
    return values[values.size() / 2U];
}

bool timestamp_not_too_far_future(
    std::uint64_t block_timestamp,
    std::uint64_t adjusted_time,
    std::uint64_t max_future_seconds) noexcept
{
    const std::uint64_t maximum =
        adjusted_time >
                std::numeric_limits<std::uint64_t>::max() -
                    max_future_seconds
            ? std::numeric_limits<std::uint64_t>::max()
            : adjusted_time + max_future_seconds;

    return block_timestamp <= maximum;
}

} // namespace quintum::consensus
