#pragma once

#include "consensus/chainparams.hpp"
#include "net/runtime.hpp"

#include <string>
#include <string_view>

namespace quintum::rpc {

class RpcDispatcher {
public:
    RpcDispatcher(
        const consensus::ChainParams& params,
        net::NetworkRuntime& runtime
    ) noexcept;

    [[nodiscard]] std::string handle(
        std::string_view request_body
    );

private:
    consensus::ChainParams params_{};
    net::NetworkRuntime& runtime_;
};

} // namespace quintum::rpc
