#include "net/nat_mapping.hpp"

namespace quintum::net {

struct NatPortMapper::Impl {};

NatPortMapper::NatPortMapper()
    : impl_(std::make_unique<Impl>())
{
}

NatPortMapper::~NatPortMapper() = default;

NatMappingResult NatPortMapper::map_tcp(std::uint16_t)
{
    return NatMappingResult{
        .error = NatMappingError::unavailable,
    };
}

bool NatPortMapper::renew()
{
    return false;
}

void NatPortMapper::unmap() noexcept
{
}

bool NatPortMapper::active() const noexcept
{
    return false;
}

NatMappingMethod NatPortMapper::method() const noexcept
{
    return NatMappingMethod::none;
}

std::uint16_t NatPortMapper::external_port() const noexcept
{
    return 0U;
}

} // namespace quintum::net
