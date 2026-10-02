#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace quintum {

using Byte = std::uint8_t;
using Hash256 = std::array<Byte, 32>;

static_assert(sizeof(Byte) == 1);
static_assert(sizeof(Hash256) == 32);

} // namespace quintum
