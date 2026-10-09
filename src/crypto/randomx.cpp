#include "crypto/randomx.hpp"

#include <randomx.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <thread>
#include <utility>
#include <vector>

namespace quintum::crypto {
namespace {
thread_local RandomXVerificationScope* verification_scope = nullptr;
}

void report_randomx_verification(
    std::string_view name,
    std::uint64_t microseconds) noexcept
{
    if (verification_scope != nullptr && verification_scope->observer_ != nullptr) {
        try {
            verification_scope->observer_(
                verification_scope->context_, name, microseconds);
        } catch (...) {
            // Timing observers cannot affect consensus hashes.
        }
    }
}

namespace {
void report_verification(
    std::string_view name,
    std::chrono::steady_clock::time_point started) noexcept
{
    report_randomx_verification(name,
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count());
}
}

RandomXVerificationScope::RandomXVerificationScope(
    Observer observer,
    void* context) noexcept
    : observer_(observer),
      context_(context),
      previous_(verification_scope)
{
    verification_scope = this;
}

RandomXVerificationScope::~RandomXVerificationScope()
{
    report_randomx_verification("randomx_operation_complete", 0U);
    verification_scope = previous_;
}

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
    // Android may forbid executable JIT mappings (W^X/SELinux).
    // Interpreter mode computes the same consensus hashes without JIT.
#if defined(__ANDROID__)
    const auto flags = static_cast<randomx_flags>(RANDOMX_FLAG_V2);
#else
    const auto flags = static_cast<randomx_flags>(
        randomx_get_flags() | RANDOMX_FLAG_V2
    );
#endif

    impl_->cache =
        randomx_alloc_cache(flags);

    if (impl_->cache == nullptr) {
        impl_.reset();
        return;
    }

    const auto cache_started = std::chrono::steady_clock::now();
    report_randomx_verification("randomx_cache_started", 0U);
    randomx_init_cache(
        impl_->cache,
        key.data(),
        key.size()
    );

    report_verification("randomx_cache_ms", cache_started);
    report_randomx_verification("randomx_operation_complete", 0U);
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

    const auto hash_started = std::chrono::steady_clock::now();
    report_randomx_verification("randomx_hash_started", 0U);
    randomx_calculate_hash(
        impl_->vm,
        input.data(),
        input.size(),
        out.data()
    );

    report_verification("randomx_verify_ms", hash_started);
    report_randomx_verification("randomx_operation_complete", 0U);
    return out;
}

struct RandomXMiningContext::Impl {
    randomx_cache* cache{nullptr};
    randomx_dataset* dataset{nullptr};
    std::vector<randomx_vm*> vms{};
    std::size_t worker_count{0U};
    bool full_memory_mode{false};

    ~Impl()
    {
        for (auto* vm : vms) {
            if (vm != nullptr) {
                randomx_destroy_vm(vm);
            }
        }
        if (dataset != nullptr) {
            randomx_release_dataset(dataset);
        }
        if (cache != nullptr) {
            randomx_release_cache(cache);
        }
    }
};

RandomXMiningContext::RandomXMiningContext(
    std::span<const Byte> key,
    std::size_t workers,
    bool full_memory)
    : impl_(std::make_unique<Impl>())
{
    if (key.empty() || workers == 0U) {
        impl_.reset();
        return;
    }

    try {
        const std::size_t bounded_workers =
            std::min<std::size_t>(
                workers,
                64U
            );

        // Android uses portable interpreter mode; no executable JIT pages.
#if defined(__ANDROID__)
        auto flags = static_cast<randomx_flags>(RANDOMX_FLAG_V2);
#else
        auto flags = static_cast<randomx_flags>(
            randomx_get_flags() | RANDOMX_FLAG_V2
        );
#endif

        if (full_memory) {
            flags =
                static_cast<randomx_flags>(
                    flags |
                    RANDOMX_FLAG_FULL_MEM
                );
        }

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

        if (full_memory) {
            impl_->dataset =
                randomx_alloc_dataset(flags);

            if (impl_->dataset == nullptr) {
                impl_.reset();
                return;
            }

            const unsigned long item_count =
                randomx_dataset_item_count();

            const std::size_t init_workers =
                std::min<std::size_t>(
                    bounded_workers,
                    static_cast<std::size_t>(
                        item_count)
                );

            std::vector<std::thread> initializers;
            initializers.reserve(init_workers);

            const unsigned long base =
                item_count /
                static_cast<unsigned long>(
                    init_workers);
            const unsigned long remainder =
                item_count %
                static_cast<unsigned long>(
                    init_workers);

            unsigned long start{0UL};

            for (std::size_t i = 0U;
                 i < init_workers;
                 ++i) {
                const unsigned long count =
                    base +
                    (i <
                             static_cast<std::size_t>(
                                 remainder)
                         ? 1UL
                         : 0UL);

                const unsigned long range_start =
                    start;
                start += count;

                initializers.emplace_back(
                    [this,
                     range_start,
                     count] {
                        randomx_init_dataset(
                            impl_->dataset,
                            impl_->cache,
                            range_start,
                            count
                        );
                    }
                );
            }

            for (auto& thread : initializers) {
                thread.join();
            }
        }

        impl_->vms.reserve(bounded_workers);

        for (std::size_t i = 0U;
             i < bounded_workers;
             ++i) {
            randomx_vm* vm =
                randomx_create_vm(
                    flags,
                    full_memory
                        ? nullptr
                        : impl_->cache,
                    full_memory
                        ? impl_->dataset
                        : nullptr
                );

            if (vm == nullptr) {
                impl_.reset();
                return;
            }

            impl_->vms.push_back(vm);
        }

        impl_->worker_count =
            bounded_workers;
        impl_->full_memory_mode =
            full_memory;
    } catch (...) {
        impl_.reset();
    }
}

RandomXMiningContext::~RandomXMiningContext() = default;

RandomXMiningContext::RandomXMiningContext(
    RandomXMiningContext&&) noexcept = default;

RandomXMiningContext&
RandomXMiningContext::operator=(
    RandomXMiningContext&&) noexcept = default;

bool RandomXMiningContext::valid() const noexcept
{
    return impl_ != nullptr &&
           impl_->cache != nullptr &&
           !impl_->vms.empty() &&
           impl_->worker_count ==
               impl_->vms.size() &&
           (!impl_->full_memory_mode ||
            impl_->dataset != nullptr);
}

std::size_t
RandomXMiningContext::workers() const noexcept
{
    return valid()
        ? impl_->worker_count
        : 0U;
}

bool RandomXMiningContext::full_memory() const noexcept
{
    return valid() &&
           impl_->full_memory_mode;
}

std::optional<Hash256>
RandomXMiningContext::hash(
    std::size_t worker,
    std::span<const Byte> input) const noexcept
{
    if (!valid() ||
        worker >= impl_->vms.size()) {
        return std::nullopt;
    }

    Hash256 out{};

    randomx_calculate_hash(
        impl_->vms[worker],
        input.data(),
        input.size(),
        out.data()
    );

    return out;
}

} // namespace quintum::crypto

