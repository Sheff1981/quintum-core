#include "net/nat_mapping.hpp"

#include <miniupnpc.h>
#include <natpmp.h>
#include <upnpcommands.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace quintum::net {
namespace {

constexpr std::uint32_t kNatPmpLifetimeSeconds = 7'200U;
constexpr std::size_t kNatPmpPollAttempts = 60U;
constexpr auto kNatPmpPollDelay =
    std::chrono::milliseconds(50);

bool wait_natpmp_response(
    natpmp_t& context,
    natpmpresp_t& response)
{
    for (std::size_t i = 0U;
         i < kNatPmpPollAttempts;
         ++i) {
        const int result =
            readnatpmpresponseorretry(
                &context,
                &response
            );

        if (result == 0) {
            return response.resultcode == 0U;
        }

        if (result != NATPMP_TRYAGAIN) {
            return false;
        }

        std::this_thread::sleep_for(
            kNatPmpPollDelay
        );
    }

    return false;
}

std::optional<std::uint16_t> map_natpmp(
    std::uint16_t local_port,
    std::uint32_t lifetime)
{
    natpmp_t context{};

    if (initnatpmp(
            &context,
            0,
            0) < 0) {
        return std::nullopt;
    }

    const int sent =
        sendnewportmappingrequest(
            &context,
            NATPMP_PROTOCOL_TCP,
            local_port,
            local_port,
            lifetime
        );

    if (sent < 0) {
        (void)closenatpmp(&context);
        return std::nullopt;
    }

    natpmpresp_t response{};
    const bool received =
        wait_natpmp_response(
            context,
            response
        );

    (void)closenatpmp(&context);

    if (!received ||
        response.type !=
            NATPMP_RESPTYPE_TCPPORTMAPPING ||
        response.pnu.newportmapping.
            privateport != local_port ||
        response.pnu.newportmapping.
            mappedpublicport == 0U) {
        return std::nullopt;
    }

    return response.pnu.newportmapping.
        mappedpublicport;
}

void remove_natpmp(
    std::uint16_t local_port,
    std::uint16_t external_port) noexcept
{
    natpmp_t context{};

    if (initnatpmp(
            &context,
            0,
            0) < 0) {
        return;
    }

    if (sendnewportmappingrequest(
            &context,
            NATPMP_PROTOCOL_TCP,
            local_port,
            external_port,
            0U) >= 0) {
        natpmpresp_t response{};
        (void)wait_natpmp_response(
            context,
            response
        );
    }

    (void)closenatpmp(&context);
}

} // namespace

struct NatPortMapper::Impl {
    NatMappingMethod method{
        NatMappingMethod::none
    };
    std::uint16_t local_port{0U};
    std::uint16_t external_port{0U};

    UPNPUrls upnp_urls{};
    IGDdatas upnp_data{};
    bool upnp_urls_valid{false};

    std::string external_address{};
};

NatPortMapper::NatPortMapper()
    : impl_(std::make_unique<Impl>())
{
}

NatPortMapper::~NatPortMapper()
{
    unmap();
}

NatMappingResult NatPortMapper::map_tcp(
    std::uint16_t local_port)
{
    unmap();

    if (local_port == 0U) {
        return {};
    }

    if (const auto mapped =
            map_natpmp(
                local_port,
                kNatPmpLifetimeSeconds)) {
        if (*mapped == local_port) {
            impl_->method =
                NatMappingMethod::nat_pmp;
            impl_->local_port = local_port;
            impl_->external_port = *mapped;

            return NatMappingResult{
                .error = NatMappingError::none,
                .method = impl_->method,
                .external_port = *mapped,
            };
        }

        // QUINTUM advertises its listening port in the P2P version
        // message. Accepting a different public port would cause peers
        // to learn an unreachable endpoint from the observed public IP.
        remove_natpmp(
            local_port,
            *mapped
        );
    }

    int discovery_error{0};
    UPNPDev* devices =
        upnpDiscover(
            1'500,
            nullptr,
            nullptr,
            UPNP_LOCAL_PORT_ANY,
            0,
            2U,
            &discovery_error
        );

    if (devices == nullptr) {
        return NatMappingResult{
            .error =
                discovery_error ==
                    UPNPDISCOVER_SUCCESS
                    ? NatMappingError::unavailable
                    : NatMappingError::failed,
        };
    }

    std::array<char, 64> lan_address{};
    std::array<char, 64> wan_address{};

    const int valid_igd =
        UPNP_GetValidIGD(
            devices,
            &impl_->upnp_urls,
            &impl_->upnp_data,
            lan_address.data(),
            static_cast<int>(
                lan_address.size()),
            wan_address.data(),
            static_cast<int>(
                wan_address.size())
        );

    freeUPNPDevlist(devices);

    if (valid_igd != UPNP_CONNECTED_IGD &&
        valid_igd != UPNP_PRIVATEIP_IGD) {
        FreeUPNPUrls(
            &impl_->upnp_urls
        );
        std::memset(
            &impl_->upnp_urls,
            0,
            sizeof(impl_->upnp_urls)
        );
        return NatMappingResult{
            .error = NatMappingError::unavailable,
        };
    }

    impl_->upnp_urls_valid = true;

    const std::string port =
        std::to_string(local_port);

    const int result =
        UPNP_AddPortMapping(
            impl_->upnp_urls.controlURL,
            impl_->upnp_data.first.servicetype,
            port.c_str(),
            port.c_str(),
            lan_address.data(),
            "QUINTUM QMU P2P",
            "TCP",
            nullptr,
            "0"
        );

    if (result != UPNPCOMMAND_SUCCESS) {
        FreeUPNPUrls(
            &impl_->upnp_urls
        );
        std::memset(
            &impl_->upnp_urls,
            0,
            sizeof(impl_->upnp_urls)
        );
        impl_->upnp_urls_valid = false;

        return NatMappingResult{
            .error = NatMappingError::failed,
        };
    }

    impl_->method = NatMappingMethod::upnp;
    impl_->local_port = local_port;
    impl_->external_port = local_port;

    if (wan_address[0] != '\0') {
        impl_->external_address =
            wan_address.data();
    }

    return NatMappingResult{
        .error = NatMappingError::none,
        .method = impl_->method,
        .external_port =
            impl_->external_port,
        .external_address =
            impl_->external_address,
    };
}

bool NatPortMapper::renew()
{
    if (!active()) {
        return false;
    }

    if (impl_->method ==
        NatMappingMethod::upnp) {
        return true;
    }

    if (impl_->method !=
        NatMappingMethod::nat_pmp) {
        return false;
    }

    const auto mapped =
        map_natpmp(
            impl_->local_port,
            kNatPmpLifetimeSeconds
        );

    if (!mapped) {
        return false;
    }

    if (*mapped != impl_->local_port) {
        remove_natpmp(
            impl_->local_port,
            *mapped
        );
        return false;
    }

    impl_->external_port = *mapped;
    return true;
}

void NatPortMapper::unmap() noexcept
{
    if (!impl_) {
        return;
    }

    if (impl_->method ==
            NatMappingMethod::nat_pmp &&
        impl_->local_port != 0U &&
        impl_->external_port != 0U) {
        remove_natpmp(
            impl_->local_port,
            impl_->external_port
        );
    } else if (
        impl_->method ==
            NatMappingMethod::upnp &&
        impl_->upnp_urls_valid &&
        impl_->external_port != 0U) {
        std::array<char, 6> port{};
        const int written =
            std::snprintf(
                port.data(),
                port.size(),
                "%u",
                static_cast<unsigned int>(
                    impl_->external_port)
            );

        if (written > 0 &&
            static_cast<std::size_t>(written) <
                port.size()) {
            (void)UPNP_DeletePortMapping(
                impl_->upnp_urls.controlURL,
                impl_->upnp_data.first.servicetype,
                port.data(),
                "TCP",
                nullptr
            );
        }
    }

    if (impl_->upnp_urls_valid) {
        FreeUPNPUrls(
            &impl_->upnp_urls
        );
    }

    *impl_ = Impl{};
}

bool NatPortMapper::active() const noexcept
{
    return impl_ != nullptr &&
           impl_->method !=
               NatMappingMethod::none &&
           impl_->local_port != 0U &&
           impl_->external_port != 0U;
}

NatMappingMethod NatPortMapper::method() const noexcept
{
    return impl_ == nullptr
        ? NatMappingMethod::none
        : impl_->method;
}

std::uint16_t
NatPortMapper::external_port() const noexcept
{
    return impl_ == nullptr
        ? 0U
        : impl_->external_port;
}

} // namespace quintum::net
