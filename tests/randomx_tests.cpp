#include "crypto/randomx.hpp"
#include "consensus/pow.hpp"
#include <chrono>
#include <iostream>
#include "core/serialize.hpp"

#include <array>
#include <cassert>
#include <string_view>

namespace {

quintum::Bytes bytes(std::string_view text)
{
    quintum::Bytes out;
    out.reserve(text.size());

    for (const unsigned char ch : text) {
        out.push_back(
            static_cast<quintum::Byte>(ch)
        );
    }

    return out;
}

quintum::Hash256 hash_from_hex(
    std::string_view hex)
{
    assert(hex.size() == 64U);

    const auto nibble = [](char ch) -> quintum::Byte {
        if (ch >= '0' && ch <= '9') {
            return static_cast<quintum::Byte>(ch - '0');
        }
        if (ch >= 'a' && ch <= 'f') {
            return static_cast<quintum::Byte>(
                10 + ch - 'a'
            );
        }
        assert(false);
        return 0U;
    };

    quintum::Hash256 out{};

    for (std::size_t i = 0U;
         i < out.size();
         ++i) {
        out[i] = static_cast<quintum::Byte>(
            (nibble(hex[i * 2U]) << 4U) |
            nibble(hex[i * 2U + 1U])
        );
    }

    return out;
}

void test_upstream_v2_vector_a()
{
    const auto key =
        bytes("test key 000");
    const auto input =
        bytes("This is a test");

    quintum::crypto::RandomXLightHasher hasher{
        key
    };

    assert(hasher.valid());

    const auto hash =
        hasher.hash(input);

    assert(hash.has_value());
    assert(*hash == hash_from_hex(
        "22ec6b861b3eb23686b2efbad69513c967ecfce80983df66c9c5b4fbfb4cdb6f"
    ));
}

void test_upstream_v2_vector_b()
{
    const auto key =
        bytes("test key 001");
    const auto input =
        bytes(
            "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua"
        );

    quintum::crypto::RandomXLightHasher hasher{
        key
    };

    assert(hasher.valid());

    const auto hash =
        hasher.hash(input);

    assert(hash.has_value());
    assert(*hash == hash_from_hex(
        "97024134686ce27d362ea8d86d8ef16483ac272abdabd46ef13359400777fe5e"
    ));
}

void test_shared_light_mining_context()
{
    const auto key =
        bytes("test key 000");
    const auto input =
        bytes("This is a test");

    quintum::crypto::RandomXMiningContext
        context{
            key,
            2U,
            false
        };

    assert(context.valid());
    assert(context.workers() == 2U);
    assert(!context.full_memory());

    const auto first =
        context.hash(0U, input);
    const auto second =
        context.hash(1U, input);

    assert(first.has_value());
    assert(second.has_value());
    assert(*first == *second);
    assert(*first == hash_from_hex(
        "22ec6b861b3eb23686b2efbad69513c967ecfce80983df66c9c5b4fbfb4cdb6f"
    ));
}

void test_empty_key_is_rejected()
{
    const std::array<quintum::Byte, 0> empty{};

    quintum::crypto::RandomXLightHasher hasher{
        empty
    };

    assert(!hasher.valid());
}

void test_exact_seed_and_input_hash_reuse()
{
    using namespace quintum;
    using namespace quintum::consensus;
    BlockHeader header;
    header.bits = 0x2100ffffU;
    header.nonce = 0x706572662d70726fU;
    Hash256 seed{}; seed[0] = 0x73U;
    PowParams params;
    params.pow_limit_bits = header.bits;
    std::uint64_t calls = 0U;
    crypto::RandomXVerificationScope scope(
        [](void* context, std::string_view name, std::uint64_t) {
            if (name == "randomx_verify_ms") ++*static_cast<std::uint64_t*>(context);
        }, &calls);
    const auto started = std::chrono::steady_clock::now();
    const auto raw = randomx_pow_hash(header, seed);
    assert(raw);
    const auto cache_warm = std::chrono::steady_clock::now();
    const auto raw_again = randomx_pow_hash(header, seed);
    assert(raw_again == raw);
    const auto baseline_done = std::chrono::steady_clock::now();
    const auto first = check_randomx_proof_of_work(header, params, seed);
    const auto verified_done = std::chrono::steady_clock::now();
    const auto calls_before_reuse = calls;
    assert(check_randomx_proof_of_work(header, params, seed) == first);
    const auto reused_done = std::chrono::steady_clock::now();
    assert(calls == calls_before_reuse);
    // A different input or seed must perform a real hash.
    ++header.nonce;
    (void)check_randomx_proof_of_work(header, params, seed);
    assert(calls == calls_before_reuse + 1U);
    seed[0] ^= 1U;
    (void)check_randomx_proof_of_work(header, params, seed);
    assert(calls == calls_before_reuse + 2U);
    const auto us = [](auto duration) {
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    };
    std::cout << "RandomX measurement: raw_two_hash_us=" << us(baseline_done - started)
        << " uncached_single_hash_us=" << us(baseline_done - cache_warm)
        << " first_verification_us=" << us(verified_done - baseline_done)
        << " exact_reuse_us=" << us(reused_done - verified_done)
        << " real_calls=" << calls << '\n';
}

} // namespace

int main()
{
    test_exact_seed_and_input_hash_reuse();
    test_upstream_v2_vector_a();
    test_upstream_v2_vector_b();
    test_shared_light_mining_context();
    test_empty_key_is_rejected();
    return 0;
}
