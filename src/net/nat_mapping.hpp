#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace quintum::net {

enum class NatMappingMethod {
    none,
    nat_pmp,
    upnp,
};

enum class NatMappingError {
    none,
    unavailable,
    failed,
};

struct NatMappingResult {
    NatMappingError error{NatMappingError::unavailable};
    NatMappingMethod method{NatMappingMethod::none};
    std::uint16_t external_port{0U};
    std::string external_address{};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == NatMappingError::none &&
               method != NatMappingMethod::none &&
               external_port != 0U;
    }
};

class NatPortMapper {
public:
    NatPortMapper();
    ~NatPortMapper();

    NatPortMapper(const NatPortMapper&) = delete;
    NatPortMapper& operator=(const NatPortMapper&) = delete;

    [[nodiscard]] NatMappingResult map_tcp(
        std::uint16_t local_port
    );

    [[nodiscard]] bool renew();
    void unmap() noexcept;

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] NatMappingMethod method() const noexcept;
    [[nodiscard]] std::uint16_t external_port() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quintum::net
