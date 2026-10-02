#pragma once

#include "core/types.hpp"

#include <span>

namespace quintum::crypto {

[[nodiscard]] Hash256 sha256(std::span<const Byte> data);
[[nodiscard]] Hash256 double_sha256(std::span<const Byte> data);

} // namespace quintum::crypto
