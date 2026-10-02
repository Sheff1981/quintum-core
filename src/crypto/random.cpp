#include "crypto/random.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <limits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#if defined(__linux__)
#include <sys/random.h>
#include <unistd.h>
#endif
#endif

namespace quintum::crypto {

bool secure_random_bytes(
    std::span<Byte> output) noexcept
{
    if (output.empty()) {
        return true;
    }

#ifdef _WIN32
    std::size_t offset{0U};

    while (offset < output.size()) {
        const std::size_t remaining =
            output.size() - offset;

        const auto chunk =
            static_cast<ULONG>(
                std::min<std::size_t>(
                    remaining,
                    static_cast<std::size_t>(
                        std::numeric_limits<ULONG>::max())
                )
            );

        const NTSTATUS status =
            BCryptGenRandom(
                nullptr,
                reinterpret_cast<PUCHAR>(
                    output.data() + offset),
                chunk,
                BCRYPT_USE_SYSTEM_PREFERRED_RNG
            );

        if (status < 0) {
            return false;
        }

        offset +=
            static_cast<std::size_t>(chunk);
    }

    return true;
#elif defined(__linux__)
    std::size_t offset{0U};

    while (offset < output.size()) {
        const ssize_t result =
            ::getrandom(
                output.data() + offset,
                output.size() - offset,
                0
            );

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        if (result == 0) {
            return false;
        }

        offset +=
            static_cast<std::size_t>(result);
    }

    return true;
#else
    std::FILE* file =
        std::fopen("/dev/urandom", "rb");

    if (file == nullptr) {
        return false;
    }

    const std::size_t read =
        std::fread(
            output.data(),
            1U,
            output.size(),
            file
        );

    const bool closed =
        std::fclose(file) == 0;

    return read == output.size() &&
           closed;
#endif
}

std::optional<PrivateKey>
generate_private_key() noexcept
{
    PrivateKey key{};

    for (std::size_t attempt = 0U;
         attempt < 1'024U;
         ++attempt) {
        if (!secure_random_bytes(key)) {
            secure_erase(key);
            return std::nullopt;
        }

        if (is_valid_private_key(key)) {
            return key;
        }
    }

    secure_erase(key);
    return std::nullopt;
}

void secure_erase(
    std::span<Byte> bytes) noexcept
{
    volatile Byte* pointer =
        bytes.data();

    for (std::size_t i = 0U;
         i < bytes.size();
         ++i) {
        pointer[i] = 0U;
    }
}

} // namespace quintum::crypto
