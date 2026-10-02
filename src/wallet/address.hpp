#pragma once

#include "consensus/chainparams.hpp"
#include "crypto/secp256k1.hpp"

#include <string>
#include <string_view>

namespace quintum::wallet {

inline constexpr Byte kAddressTypeP2pk = 0x01U;

enum class AddressError {
    none,
    invalid_format,
    wrong_network,
    invalid_checksum,
    unsupported_type,
    invalid_public_key,
};

struct AddressDecodeResult {
    AddressError error{AddressError::none};
    crypto::PublicKey public_key{};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == AddressError::none;
    }
};

[[nodiscard]] std::string_view address_hrp(
    consensus::Network network
) noexcept;

[[nodiscard]] std::string encode_address(
    consensus::Network network,
    const crypto::PublicKey& public_key
);

[[nodiscard]] AddressDecodeResult decode_address(
    consensus::Network network,
    std::string_view address
);

} // namespace quintum::wallet
