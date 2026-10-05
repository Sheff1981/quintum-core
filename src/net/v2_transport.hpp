#pragma once

#include "consensus/chainparams.hpp"
#include "core/types.hpp"
#include "crypto/secp256k1.hpp"
#include "net/protocol.hpp"

#include <monocypher.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace quintum::net {

inline constexpr std::size_t kV2PacketLengthSize = 4U;
inline constexpr std::size_t kV2PacketTagSize = 16U;
inline constexpr std::size_t kV2MaxPlaintext =
    kMessageHeaderSize +
    static_cast<std::size_t>(kMaxMessagePayload);
inline constexpr std::size_t kV2MaxPacketSize =
    kV2PacketLengthSize +
    kV2MaxPlaintext +
    kV2PacketTagSize;

enum class V2TransportError {
    none,
    random_failed,
    invalid_key,
    payload_too_large,
    malformed_packet,
    authentication_failed,
};

struct V2EphemeralKey {
    crypto::PrivateKey secret{};
    crypto::EllSwiftPublicKey public_key{};

    V2EphemeralKey() noexcept = default;
    ~V2EphemeralKey();

    V2EphemeralKey(const V2EphemeralKey&) = delete;
    V2EphemeralKey& operator=(const V2EphemeralKey&) = delete;

    V2EphemeralKey(V2EphemeralKey&& other) noexcept;
    V2EphemeralKey& operator=(V2EphemeralKey&& other) noexcept;
};

class V2Transport {
public:
    V2Transport(
        const std::array<Byte, 32>& send_key,
        const std::array<Byte, 32>& receive_key,
        const Hash256& session_id,
        const std::array<Byte, 4>& network_magic
    ) noexcept;

    ~V2Transport();

    V2Transport(const V2Transport&) = delete;
    V2Transport& operator=(const V2Transport&) = delete;

    V2Transport(V2Transport&& other) noexcept;
    V2Transport& operator=(V2Transport&& other) noexcept;

    [[nodiscard]] const Hash256& session_id() const noexcept;

    [[nodiscard]] Bytes encrypt(
        std::span<const Byte> plaintext,
        V2TransportError& error
    );

    [[nodiscard]] std::optional<Bytes> decrypt(
        std::span<const Byte> packet,
        V2TransportError& error
    );

private:
    crypto_aead_ctx send_ctx_{};
    crypto_aead_ctx receive_ctx_{};
    Hash256 session_id_{};
    std::array<Byte, 4> network_magic_{};
};

[[nodiscard]] std::optional<V2EphemeralKey>
create_v2_ephemeral() noexcept;

[[nodiscard]] std::optional<V2Transport>
derive_v2_transport(
    const consensus::ChainParams& params,
    const V2EphemeralKey& local,
    const crypto::EllSwiftPublicKey& initiator_public,
    const crypto::EllSwiftPublicKey& responder_public,
    bool initiating
) noexcept;

} // namespace quintum::net
