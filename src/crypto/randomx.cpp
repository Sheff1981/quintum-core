#include "crypto/randomx.hpp"

#include <randomx.h>

#include <cstddef>
#include <utility>

namespace quintum::crypto {

struct RandomXLightHasher::Impl {
    randomx_cache* cache{nullptr};
    randomx_vm* vm{nullptr};

    ~Impl()
    {
        if (vm != nullptr) {
            randomx_destroy_vm(vm);
        }
        if (cache != nullptr) {
            randomx_release_cache(cache);
        }
    }
};

RandomXLightHasher::RandomXLightHasher(
    std::span<const Byte> key)
    : impl_(std::make_unique<Impl>())
{
    if (key.empty()) {
        impl_.reset();
        return;
    }

    // Use the upstream-recommended CPU flags (AES/JIT/Argon2 where
    // supported) while keeping light mode and the explicit v2 ruleset.
    // RandomX guarantees identical hash output across these execution modes;
    // fixed cross-platform vectors enforce that consensus property in CI.
    const auto flags =
        static_cast<randomx_flags>(
            randomx_get_flags() |
            RANDOMX_FLAG_V2
        );

    impl_->cache =
        randomx_alloc_cache(flags);

    if (impl_->cache == nullptr) {
        impl_.reset();
        return;
    }

    randomx_init_cache(
        impl_->cache,
        key.data(),
        key.size()
    );

    impl_->vm =
        randomx_create_vm(
            flags,
            impl_->cache,
            nullptr
        );

    if (impl_->vm == nullptr) {
        impl_.reset();
    }
}

RandomXLightHasher::~RandomXLightHasher() = default;

RandomXLightHasher::RandomXLightHasher(
    RandomXLightHasher&&) noexcept = default;

RandomXLightHasher&
RandomXLightHasher::operator=(
    RandomXLightHasher&&) noexcept = default;

bool RandomXLightHasher::valid() const noexcept
{
    return impl_ != nullptr &&
           impl_->cache != nullptr &&
           impl_->vm != nullptr;
}

std::optional<Hash256> RandomXLightHasher::hash(
    std::span<const Byte> input) const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    Hash256 out{};

    randomx_calculate_hash(
        impl_->vm,
        input.data(),
        input.size(),
        out.data()
    );

    return out;
}

} // namespace quintum::crypto
