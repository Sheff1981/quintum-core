#pragma once

#include "core/types.hpp"

#include <cstddef>
#include <string_view>
#include <memory>
#include <optional>
#include <span>

namespace quintum::crypto {

// Timing is scoped to the calling sync verification thread, never mining workers.
class RandomXVerificationScope {
public:
    using Observer = void (*)(void*, std::string_view, std::uint64_t);
    RandomXVerificationScope(Observer observer, void* context) noexcept;
    ~RandomXVerificationScope();
    RandomXVerificationScope(const RandomXVerificationScope&) = delete;
    RandomXVerificationScope& operator=(const RandomXVerificationScope&) = delete;
private:
    friend void report_randomx_verification(std::string_view, std::uint64_t) noexcept;
    Observer observer_;
    void* context_;
    RandomXVerificationScope* previous_;
};

void report_randomx_verification(std::string_view name, std::uint64_t value) noexcept;

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

// Shared RandomX mining context. Multiple worker VMs share one initialized
// cache in light mode, or one full dataset in fast/full-memory mode.
// Each worker index owns a distinct VM and must be used by at most one
// hashing thread at a time.
class RandomXMiningContext {
public:
    RandomXMiningContext(
        std::span<const Byte> key,
        std::size_t workers,
        bool full_memory
    );

    ~RandomXMiningContext();

    RandomXMiningContext(
        const RandomXMiningContext&
    ) = delete;
    RandomXMiningContext& operator=(
        const RandomXMiningContext&
    ) = delete;

    RandomXMiningContext(
        RandomXMiningContext&&
    ) noexcept;
    RandomXMiningContext& operator=(
        RandomXMiningContext&&
    ) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::size_t workers() const noexcept;
    [[nodiscard]] bool full_memory() const noexcept;

    [[nodiscard]] std::optional<Hash256> hash(
        std::size_t worker,
        std::span<const Byte> input
    ) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_{};
};

} // namespace quintum::crypto

