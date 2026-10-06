#include "net/dandelion.hpp"

#include "crypto/random.hpp"

#include <array>
#include <limits>

namespace quintum::net {

bool dandelion_should_fluff(
    std::uint64_t random_value,
    std::uint32_t fluff_percent) noexcept
{
    if (fluff_percent == 0U) {
        return false;
    }

    if (fluff_percent >= 100U) {
        return true;
    }

    return random_value % 100U < fluff_percent;
}

std::uint64_t dandelion_embargo_delay(
    std::uint64_t random_value,
    std::uint64_t minimum_seconds,
    std::uint64_t jitter_seconds) noexcept
{
    if (jitter_seconds == 0U) {
        return minimum_seconds;
    }

    if (jitter_seconds ==
        std::numeric_limits<std::uint64_t>::max()) {
        return minimum_seconds;
    }

    const std::uint64_t extra =
        random_value % (jitter_seconds + 1U);

    if (minimum_seconds >
        std::numeric_limits<std::uint64_t>::max() -
            extra) {
        return std::numeric_limits<std::uint64_t>::max();
    }

    return minimum_seconds + extra;
}

std::optional<std::uint64_t>
secure_dandelion_random() noexcept
{
    std::array<Byte, 8> random{};

    if (!crypto::secure_random_bytes(random)) {
        return std::nullopt;
    }

    std::uint64_t value{0U};

    for (std::size_t i = 0U;
         i < random.size();
         ++i) {
        value |=
            static_cast<std::uint64_t>(
                random[i]
            ) << (8U * i);
    }

    return value;
}

} // namespace quintum::net
