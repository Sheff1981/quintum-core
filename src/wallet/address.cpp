#include "wallet/address.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace quintum::wallet {
namespace {

constexpr std::string_view kCharset{
    "qpzry9x8gf2tvdw0s3jn54khce6mua7l"
};
constexpr std::uint32_t kBech32mConstant{
    0x2bc830a3U
};

std::uint32_t polymod_step(
    std::uint32_t checksum,
    Byte value) noexcept
{
    static constexpr std::array<
        std::uint32_t, 5> generator{
        0x3b6a57b2U,
        0x26508e6dU,
        0x1ea119faU,
        0x3d4233ddU,
        0x2a1462b3U,
    };

    const std::uint32_t top =
        checksum >> 25U;

    checksum =
        ((checksum & 0x01ffffffU) << 5U) ^
        static_cast<std::uint32_t>(value);

    for (std::size_t i = 0U;
         i < generator.size();
         ++i) {
        if (((top >> i) & 1U) != 0U) {
            checksum ^= generator[i];
        }
    }

    return checksum;
}

std::uint32_t polymod(
    std::string_view hrp,
    std::span<const Byte> data) noexcept
{
    std::uint32_t checksum{1U};

    for (const char ch : hrp) {
        checksum = polymod_step(
            checksum,
            static_cast<Byte>(
                static_cast<unsigned char>(ch) >>
                5U)
        );
    }

    checksum =
        polymod_step(checksum, 0U);

    for (const char ch : hrp) {
        checksum = polymod_step(
            checksum,
            static_cast<Byte>(
                static_cast<unsigned char>(ch) &
                0x1fU)
        );
    }

    for (const Byte value : data) {
        checksum =
            polymod_step(checksum, value);
    }

    return checksum;
}

std::vector<Byte> convert_8_to_5(
    std::span<const Byte> input)
{
    std::vector<Byte> output;
    output.reserve(
        (input.size() * 8U + 4U) / 5U
    );

    std::uint32_t accumulator{0U};
    unsigned bits{0U};

    for (const Byte value : input) {
        accumulator =
            (accumulator << 8U) |
            static_cast<std::uint32_t>(value);
        bits += 8U;

        while (bits >= 5U) {
            bits -= 5U;
            output.push_back(
                static_cast<Byte>(
                    (accumulator >> bits) &
                    0x1fU)
            );
        }
    }

    if (bits > 0U) {
        output.push_back(
            static_cast<Byte>(
                (accumulator << (5U - bits)) &
                0x1fU)
        );
    }

    return output;
}

std::optional<Bytes> convert_5_to_8(
    std::span<const Byte> input)
{
    Bytes output;
    output.reserve(
        input.size() * 5U / 8U
    );

    std::uint32_t accumulator{0U};
    unsigned bits{0U};

    for (const Byte value : input) {
        if (value > 31U) {
            return std::nullopt;
        }

        accumulator =
            (accumulator << 5U) |
            static_cast<std::uint32_t>(value);
        bits += 5U;

        while (bits >= 8U) {
            bits -= 8U;
            output.push_back(
                static_cast<Byte>(
                    (accumulator >> bits) &
                    0xffU)
            );
        }
    }

    if (bits >= 5U) {
        return std::nullopt;
    }

    if (bits > 0U &&
        ((accumulator <<
              (8U - bits)) &
         0xffU) != 0U) {
        return std::nullopt;
    }

    return output;
}

std::array<Byte, 6> create_checksum(
    std::string_view hrp,
    std::span<const Byte> data)
{
    std::vector<Byte> values;
    values.reserve(data.size() + 6U);
    values.insert(
        values.end(),
        data.begin(),
        data.end()
    );
    values.resize(values.size() + 6U, 0U);

    const std::uint32_t value =
        polymod(hrp, values) ^
        kBech32mConstant;

    std::array<Byte, 6> checksum{};

    for (std::size_t i = 0U;
         i < checksum.size();
         ++i) {
        const unsigned shift =
            static_cast<unsigned>(
                5U * (5U - i));
        checksum[i] =
            static_cast<Byte>(
                (value >> shift) &
                0x1fU
            );
    }

    return checksum;
}

bool mixed_case(
    std::string_view text) noexcept
{
    bool lower{false};
    bool upper{false};

    for (const char ch : text) {
        const unsigned char value =
            static_cast<unsigned char>(ch);

        lower =
            lower ||
            std::islower(value) != 0;
        upper =
            upper ||
            std::isupper(value) != 0;
    }

    return lower && upper;
}

std::string lowercase(
    std::string_view text)
{
    std::string output{text};

    std::transform(
        output.begin(),
        output.end(),
        output.begin(),
        [](char ch) {
            return static_cast<char>(
                std::tolower(
                    static_cast<unsigned char>(
                        ch))
            );
        }
    );

    return output;
}

} // namespace

std::string_view address_hrp(
    consensus::Network network) noexcept
{
    switch (network) {
    case consensus::Network::mainnet:
        return "qtm";
    case consensus::Network::testnet:
        return "tqtm";
    case consensus::Network::regtest:
    default:
        return "rqtm";
    }
}

std::string encode_address(
    consensus::Network network,
    const crypto::PublicKey& public_key)
{
    if (!crypto::is_valid_public_key(
            public_key)) {
        return {};
    }

    Bytes payload;
    payload.reserve(
        1U + public_key.size()
    );
    payload.push_back(kAddressTypeP2pk);
    payload.insert(
        payload.end(),
        public_key.begin(),
        public_key.end()
    );

    const auto data =
        convert_8_to_5(payload);
    const auto hrp =
        address_hrp(network);
    const auto checksum =
        create_checksum(hrp, data);

    std::string output;
    output.reserve(
        hrp.size() + 1U +
        data.size() + checksum.size()
    );
    output.append(hrp);
    output.push_back('1');

    for (const Byte value : data) {
        output.push_back(
            kCharset[
                static_cast<std::size_t>(
                    value)]
        );
    }

    for (const Byte value : checksum) {
        output.push_back(
            kCharset[
                static_cast<std::size_t>(
                    value)]
        );
    }

    return output;
}

AddressDecodeResult decode_address(
    consensus::Network network,
    std::string_view address)
{
    AddressDecodeResult out;

    if (address.size() < 12U ||
        address.size() > 90U ||
        mixed_case(address)) {
        out.error =
            AddressError::invalid_format;
        return out;
    }

    const std::string normalized =
        lowercase(address);

    const auto separator =
        normalized.rfind('1');

    if (separator == std::string::npos ||
        separator == 0U ||
        separator + 7U >
            normalized.size()) {
        out.error =
            AddressError::invalid_format;
        return out;
    }

    const std::string_view hrp{
        normalized.data(),
        separator
    };

    if (hrp != address_hrp(network)) {
        out.error =
            AddressError::wrong_network;
        return out;
    }

    std::vector<Byte> data;
    data.reserve(
        normalized.size() -
        separator - 1U
    );

    for (std::size_t i =
             separator + 1U;
         i < normalized.size();
         ++i) {
        const auto pos =
            kCharset.find(
                normalized[i]
            );

        if (pos == std::string_view::npos ||
            pos > 31U) {
            out.error =
                AddressError::invalid_format;
            return out;
        }

        data.push_back(
            static_cast<Byte>(pos)
        );
    }

    if (data.size() < 6U ||
        polymod(hrp, data) !=
            kBech32mConstant) {
        out.error =
            AddressError::invalid_checksum;
        return out;
    }

    data.resize(
        data.size() - 6U
    );

    const auto payload =
        convert_5_to_8(data);

    if (!payload ||
        payload->size() !=
            1U +
            crypto::PublicKey{}.size()) {
        out.error =
            AddressError::invalid_format;
        return out;
    }

    if ((*payload)[0] !=
        kAddressTypeP2pk) {
        out.error =
            AddressError::unsupported_type;
        return out;
    }

    std::copy(
        payload->begin() + 1,
        payload->end(),
        out.public_key.begin()
    );

    if (!crypto::is_valid_public_key(
            out.public_key)) {
        out.error =
            AddressError::invalid_public_key;
    }

    return out;
}

} // namespace quintum::wallet
