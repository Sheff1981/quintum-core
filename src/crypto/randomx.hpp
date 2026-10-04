#pragma once

#include "core/types.hpp"

#include <memory>
#include <optional>
#include <span>

namespace quintum::crypto {

// Portable RandomX v2 light-mode context.
//
// The cache is initialized once per seed key and reused for all hashes.
// This is intentionally the verification/correctness path (~256 MiB cache),
// not the future full-memory mining path (~2 GiB dataset).
class RandomXLightHasher {
public:
    explicit RandomXLightHasher(
        std::span<const Byte> key
    );

    ~RandomXLightHasher();

    RandomXLightHasher(
        const RandomXLightHasher&
    ) = delete;
    RandomXLightHasher& operator=(
        const RandomXLightHasher&
    ) = delete;

    RandomXLightHasher(
        RandomXLightHasher&&
    ) noexcept;
    RandomXLightHasher& operator=(
        RandomXLightHasher&&
    ) noexcept;

    [[nodiscard]] bool valid() const noexcept;

    [[nodiscard]] std::optional<Hash256> hash(
        std::span<const Byte> input
    ) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_{};
};

} // namespace quintum::crypto
