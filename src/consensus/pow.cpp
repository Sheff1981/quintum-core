#include "consensus/pow.hpp"

#include "crypto/randomx.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace quintum::consensus {
namespace {

bool is_zero_hash(const Hash256& value) noexcept
{
    return std::all_of(
        value.begin(),
        value.end(),
        [](Byte byte) { return byte == 0U; }
    );
}

bool compact_is_canonical(
    std::uint32_t bits,
    const CompactTarget& compact)
{
    return compact.valid() &&
           encode_compact_target(compact.target) == bits;
}

bool greater_or_equal(
    const Hash256& lhs,
    const Hash256& rhs) noexcept
{
    return !std::lexicographical_compare(
        lhs.begin(),
        lhs.end(),
        rhs.begin(),
        rhs.end()
    );
}

void shift_left_one(Hash256& value) noexcept
{
    Byte carry{0U};

    for (std::size_t i = value.size(); i-- > 0U;) {
        const Byte next_carry =
            static_cast<Byte>((value[i] >> 7U) & 0x01U);

        value[i] = static_cast<Byte>(
            static_cast<unsigned>(value[i] << 1U) |
            static_cast<unsigned>(carry)
        );

        carry = next_carry;
    }
}

void subtract_in_place(
    Hash256& lhs,
    const Hash256& rhs) noexcept
{
    unsigned borrow{0U};

    for (std::size_t i = lhs.size(); i-- > 0U;) {
        const unsigned left = lhs[i];
        const unsigned right = static_cast<unsigned>(rhs[i]) + borrow;

        if (left >= right) {
            lhs[i] = static_cast<Byte>(left - right);
            borrow = 0U;
        } else {
            lhs[i] = static_cast<Byte>((left + 256U) - right);
            borrow = 1U;
        }
    }
}

bool increment(Hash256& value) noexcept
{
    for (std::size_t i = value.size(); i-- > 0U;) {
        ++value[i];
        if (value[i] != 0U) {
            return true;
        }
    }
    return false;
}

bool is_max_hash(const Hash256& value) noexcept
{
    return std::all_of(
        value.begin(),
        value.end(),
        [](Byte byte) { return byte == 0xffU; }
    );
}

crypto::RandomXLightHasher* cached_randomx_hasher(
    const Hash256& seed_key)
{
    struct Cache {
        bool initialized{false};
        Hash256 seed{};
        std::unique_ptr<
            crypto::RandomXLightHasher
        > hasher{};
    };

    thread_local Cache cache;

    if (!cache.initialized ||
        cache.seed != seed_key ||
        cache.hasher == nullptr ||
        !cache.hasher->valid()) {
        auto replacement =
            std::make_unique<
                crypto::RandomXLightHasher
            >(
                std::span<const Byte>{
                    seed_key
                }
            );

        if (!replacement->valid()) {
            return nullptr;
        }

        cache.seed = seed_key;
        cache.hasher =
            std::move(replacement);
        cache.initialized = true;
    }

    return cache.hasher.get();
}

} // namespace

bool CompactTarget::is_zero() const noexcept
{
    return is_zero_hash(target);
}

bool CompactTarget::valid() const noexcept
{
    return !negative && !overflow && !is_zero();
}

CompactTarget decode_compact_target(std::uint32_t bits)
{
    CompactTarget result;

    const std::uint32_t exponent = bits >> 24U;
    const std::uint32_t mantissa = bits & 0x007fffffU;

    result.negative = (mantissa != 0U) && ((bits & 0x00800000U) != 0U);
    result.overflow =
        (mantissa != 0U) &&
        (
            exponent > 34U ||
            (mantissa > 0xffU && exponent > 33U) ||
            (mantissa > 0xffffU && exponent > 32U)
        );

    if (mantissa == 0U) {
        return result;
    }

    if (exponent <= 3U) {
        const auto shifted =
            mantissa >> (8U * (3U - exponent));

        result.target[29] = static_cast<Byte>(shifted >> 16U);
        result.target[30] = static_cast<Byte>(shifted >> 8U);
        result.target[31] = static_cast<Byte>(shifted);
        return result;
    }

    const std::array<Byte, 3> bytes{
        static_cast<Byte>(mantissa >> 16U),
        static_cast<Byte>(mantissa >> 8U),
        static_cast<Byte>(mantissa),
    };

    const auto start = static_cast<std::int64_t>(32) -
                       static_cast<std::int64_t>(exponent);

    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto position = start + static_cast<std::int64_t>(i);
        if (position >= 0 && position < 32) {
            result.target[static_cast<std::size_t>(position)] = bytes[i];
        }
    }

    return result;
}

std::uint32_t encode_compact_target(const Hash256& target)
{
    std::size_t first = 0U;
    while (first < target.size() && target[first] == 0U) {
        ++first;
    }

    if (first == target.size()) {
        return 0U;
    }

    std::uint32_t exponent =
        static_cast<std::uint32_t>(target.size() - first);

    std::uint32_t mantissa{0U};

    if (exponent <= 3U) {
        for (std::size_t i = first; i < target.size(); ++i) {
            mantissa = (mantissa << 8U) | target[i];
        }
        mantissa <<= 8U * (3U - exponent);
    } else {
        mantissa = static_cast<std::uint32_t>(target[first]) << 16U;

        if (first + 1U < target.size()) {
            mantissa |= static_cast<std::uint32_t>(target[first + 1U]) << 8U;
        }
        if (first + 2U < target.size()) {
            mantissa |= static_cast<std::uint32_t>(target[first + 2U]);
        }
    }

    if ((mantissa & 0x00800000U) != 0U) {
        mantissa >>= 8U;
        ++exponent;
    }

    return (exponent << 24U) | (mantissa & 0x007fffffU);
}

bool hash_meets_target(
    const Hash256& hash,
    const Hash256& target) noexcept
{
    return !std::lexicographical_compare(
        target.begin(),
        target.end(),
        hash.begin(),
        hash.end()
    );
}

Hash256 work_for_target(const Hash256& target)
{
    Hash256 work{};

    if (is_zero_hash(target)) {
        return work;
    }

    if (is_max_hash(target)) {
        work.back() = 1U;
        return work;
    }

    Hash256 numerator{};
    std::transform(
        target.begin(),
        target.end(),
        numerator.begin(),
        [](Byte byte) { return static_cast<Byte>(~byte); }
    );

    Hash256 denominator = target;
    if (!increment(denominator)) {
        work.back() = 1U;
        return work;
    }

    Hash256 remainder{};
    Hash256 quotient{};

    for (std::size_t bit = 0U; bit < 256U; ++bit) {
        shift_left_one(remainder);

        const std::size_t byte_index = bit / 8U;
        const unsigned bit_index = 7U - static_cast<unsigned>(bit % 8U);
        const Byte incoming = static_cast<Byte>(
            (numerator[byte_index] >> bit_index) & 0x01U
        );
        remainder.back() = static_cast<Byte>(remainder.back() | incoming);

        if (greater_or_equal(remainder, denominator)) {
            subtract_in_place(remainder, denominator);

            const Byte mask = static_cast<Byte>(1U << bit_index);
            quotient[byte_index] =
                static_cast<Byte>(quotient[byte_index] | mask);
        }
    }

    (void)increment(quotient);
    return quotient;
}

bool add_chain_work(
    Hash256& accumulated,
    const Hash256& work) noexcept
{
    unsigned carry{0U};

    for (std::size_t i = accumulated.size(); i-- > 0U;) {
        const unsigned sum =
            static_cast<unsigned>(accumulated[i]) +
            static_cast<unsigned>(work[i]) +
            carry;

        accumulated[i] = static_cast<Byte>(sum & 0xffU);
        carry = sum >> 8U;
    }

    return carry == 0U;
}

PowCheckError check_proof_of_work(const BlockHeader& header)
{
    const auto compact = decode_compact_target(header.bits);
    if (!compact_is_canonical(header.bits, compact)) {
        return PowCheckError::invalid_target;
    }

    const auto hash = block_hash(header);
    if (!hash_meets_target(hash, compact.target)) {
        return PowCheckError::hash_above_target;
    }

    return PowCheckError::none;
}

PowCheckError check_proof_of_work(
    const BlockHeader& header,
    const PowParams& params)
{
    const auto compact = decode_compact_target(header.bits);
    if (!compact_is_canonical(header.bits, compact)) {
        return PowCheckError::invalid_target;
    }

    const auto limit = decode_compact_target(params.pow_limit_bits);
    if (!compact_is_canonical(params.pow_limit_bits, limit)) {
        return PowCheckError::invalid_target;
    }

    if (std::lexicographical_compare(
            limit.target.begin(),
            limit.target.end(),
            compact.target.begin(),
            compact.target.end())) {
        return PowCheckError::target_above_pow_limit;
    }

    const auto hash = block_hash(header);
    if (!hash_meets_target(hash, compact.target)) {
        return PowCheckError::hash_above_target;
    }

    return PowCheckError::none;
}

std::optional<Hash256> randomx_pow_hash(
    const BlockHeader& header,
    const Hash256& seed_key)
{
    auto* hasher =
        cached_randomx_hasher(
            seed_key
        );

    if (hasher == nullptr) {
        return std::nullopt;
    }

    const auto input =
        serialize_block_header(header);

    return hasher->hash(input);
}

PowCheckError check_randomx_proof_of_work(
    const BlockHeader& header,
    const PowParams& params,
    const Hash256& seed_key)
{
    const auto compact =
        decode_compact_target(header.bits);

    if (!compact_is_canonical(
            header.bits,
            compact)) {
        return PowCheckError::invalid_target;
    }

    const auto limit =
        decode_compact_target(
            params.pow_limit_bits
        );

    if (!compact_is_canonical(
            params.pow_limit_bits,
            limit)) {
        return PowCheckError::invalid_target;
    }

    if (std::lexicographical_compare(
            limit.target.begin(),
            limit.target.end(),
            compact.target.begin(),
            compact.target.end())) {
        return PowCheckError::target_above_pow_limit;
    }

    const auto hash =
        randomx_pow_hash(
            header,
            seed_key
        );

    if (!hash) {
        return PowCheckError::hashing_failed;
    }

    if (!hash_meets_target(
            *hash,
            compact.target)) {
        return PowCheckError::hash_above_target;
    }

    return PowCheckError::none;
}

MiningResult mine_header(
    BlockHeader& header,
    std::uint64_t max_attempts)
{
    const auto compact = decode_compact_target(header.bits);

    MiningResult result;
    result.nonce = header.nonce;

    if (!compact_is_canonical(header.bits, compact)) {
        result.status = MineStatus::invalid_target;
        return result;
    }

    for (std::uint64_t attempt = 0U; attempt < max_attempts; ++attempt) {
        result.hash = block_hash(header);
        result.nonce = header.nonce;
        result.attempts = attempt + 1U;

        if (hash_meets_target(result.hash, compact.target)) {
            result.status = MineStatus::found;
            return result;
        }

        if (header.nonce == std::numeric_limits<std::uint64_t>::max()) {
            result.status = MineStatus::exhausted;
            return result;
        }

        ++header.nonce;
    }

    result.status = MineStatus::exhausted;
    return result;
}

MiningResult mine_randomx_header(
    BlockHeader& header,
    const Hash256& seed_key,
    std::uint64_t max_attempts)
{
    const auto compact =
        decode_compact_target(header.bits);

    MiningResult result;
    result.nonce = header.nonce;

    if (!compact_is_canonical(
            header.bits,
            compact)) {
        result.status =
            MineStatus::invalid_target;
        return result;
    }

    auto* hasher =
        cached_randomx_hasher(
            seed_key
        );

    if (hasher == nullptr) {
        result.status =
            MineStatus::hashing_failed;
        return result;
    }

    for (std::uint64_t attempt = 0U;
         attempt < max_attempts;
         ++attempt) {
        const auto input =
            serialize_block_header(header);
        const auto hash =
            hasher->hash(input);

        if (!hash) {
            result.status =
                MineStatus::hashing_failed;
            return result;
        }

        result.hash = *hash;
        result.nonce = header.nonce;
        result.attempts = attempt + 1U;

        if (hash_meets_target(
                result.hash,
                compact.target)) {
            result.status = MineStatus::found;
            return result;
        }

        if (header.nonce ==
            std::numeric_limits<std::uint64_t>::max()) {
            result.status = MineStatus::exhausted;
            return result;
        }

        ++header.nonce;
    }

    result.status = MineStatus::exhausted;
    return result;
}

MiningResult mine_randomx_header_parallel(
    BlockHeader& header,
    const Hash256& seed_key,
    std::uint64_t max_attempts,
    std::size_t worker_count,
    bool full_memory)
{
    if (worker_count <= 1U) {
        return mine_randomx_header(
            header,
            seed_key,
            max_attempts
        );
    }

    const auto compact =
        decode_compact_target(
            header.bits
        );

    MiningResult result;
    result.nonce = header.nonce;

    if (!compact_is_canonical(
            header.bits,
            compact)) {
        result.status =
            MineStatus::invalid_target;
        return result;
    }

    struct CachedContext {
        bool initialized{false};
        Hash256 seed{};
        std::size_t workers{0U};
        bool full_memory{false};
        std::unique_ptr<
            crypto::RandomXMiningContext
        > context{};
    };

    thread_local CachedContext cache;

    const std::size_t bounded_workers =
        std::min<std::size_t>(
            worker_count,
            64U
        );

    if (!cache.initialized ||
        cache.seed != seed_key ||
        cache.workers != bounded_workers ||
        cache.full_memory != full_memory ||
        cache.context == nullptr ||
        !cache.context->valid()) {
        auto replacement =
            std::make_unique<
                crypto::RandomXMiningContext
            >(
                std::span<const Byte>{
                    seed_key
                },
                bounded_workers,
                full_memory
            );

        if (!replacement->valid()) {
            result.status =
                MineStatus::hashing_failed;
            return result;
        }

        cache.seed = seed_key;
        cache.workers = bounded_workers;
        cache.full_memory = full_memory;
        cache.context =
            std::move(replacement);
        cache.initialized = true;
    }

    if (max_attempts == 0U) {
        result.status =
            MineStatus::exhausted;
        return result;
    }

    const std::uint64_t start_nonce =
        header.nonce;

    std::atomic<bool> stop{false};
    std::atomic<bool> hashing_failed{false};
    std::atomic<std::uint64_t>
        attempts_done{0U};
    std::mutex result_mutex;

    auto* const mining_context =
        cache.context.get();

    if (mining_context == nullptr) {
        result.status =
            MineStatus::hashing_failed;
        return result;
    }

    std::vector<std::thread> workers;
    workers.reserve(bounded_workers);

    for (std::size_t worker = 0U;
         worker < bounded_workers;
         ++worker) {
        workers.emplace_back(
            [&, worker] {
                for (std::uint64_t offset =
                         static_cast<std::uint64_t>(
                             worker);
                     offset < max_attempts &&
                     !stop.load(
                         std::memory_order_relaxed);
                     offset +=
                         static_cast<std::uint64_t>(
                             bounded_workers)) {
                    if (offset >
                        std::numeric_limits<
                            std::uint64_t>::max() -
                            start_nonce) {
                        break;
                    }

                    BlockHeader candidate =
                        header;
                    candidate.nonce =
                        start_nonce + offset;

                    const auto input =
                        serialize_block_header(
                            candidate
                        );

                    const auto hash =
                        mining_context->hash(
                            worker,
                            input
                        );

                    attempts_done.fetch_add(
                        1U,
                        std::memory_order_relaxed
                    );

                    if (!hash) {
                        hashing_failed.store(
                            true,
                            std::memory_order_relaxed
                        );
                        stop.store(
                            true,
                            std::memory_order_relaxed
                        );
                        break;
                    }

                    if (!hash_meets_target(
                            *hash,
                            compact.target)) {
                        continue;
                    }

                    bool expected{false};

                    if (stop.compare_exchange_strong(
                            expected,
                            true,
                            std::memory_order_relaxed)) {
                        std::scoped_lock lock(
                            result_mutex
                        );
                        result.status =
                            MineStatus::found;
                        result.nonce =
                            candidate.nonce;
                        result.hash = *hash;
                    }

                    break;
                }
            }
        );
    }

    for (auto& worker : workers) {
        worker.join();
    }

    result.attempts =
        attempts_done.load(
            std::memory_order_relaxed
        );

    if (result.status ==
        MineStatus::found) {
        header.nonce = result.nonce;
        return result;
    }

    if (hashing_failed.load(
            std::memory_order_relaxed)) {
        result.status =
            MineStatus::hashing_failed;
        return result;
    }

    result.status =
        MineStatus::exhausted;

    const std::uint64_t last_offset =
        max_attempts - 1U;

    if (last_offset <=
        std::numeric_limits<
            std::uint64_t>::max() -
            start_nonce) {
        result.nonce =
            start_nonce + last_offset;
        header.nonce = result.nonce;
    } else {
        result.nonce =
            std::numeric_limits<
                std::uint64_t>::max();
        header.nonce = result.nonce;
    }

    return result;
}

} // namespace quintum::consensus
