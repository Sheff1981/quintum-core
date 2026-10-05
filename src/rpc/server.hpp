#pragma once

#include "consensus/chainparams.hpp"
#include "net/runtime.hpp"
#include "rpc/rpc.hpp"

#include <httplib.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

namespace quintum::rpc {

inline constexpr std::size_t
    kDefaultRpcMaxRequestBytes =
        1U * 1024U * 1024U;

enum class RpcServerError {
    none,
    already_running,
    invalid_port,
    random_failed,
    cookie_write_failed,
    cookie_permission_failed,
    bind_failed,
    thread_start_failed,
};

struct RpcServerStartResult {
    RpcServerError error{
        RpcServerError::none
    };
    std::uint16_t port{0U};
    std::filesystem::path cookie_path{};

    [[nodiscard]] bool ok() const noexcept
    {
        return error ==
            RpcServerError::none;
    }
};

class RpcServer {
public:
    RpcServer(
        const consensus::ChainParams& params,
        net::NetworkRuntime& runtime,
        std::filesystem::path directory
    );

    ~RpcServer();

    RpcServer(const RpcServer&) = delete;
    RpcServer& operator=(
        const RpcServer&) = delete;

    [[nodiscard]] RpcServerStartResult
    start(
        std::uint16_t port,
        std::size_t max_request_bytes =
            kDefaultRpcMaxRequestBytes
    );

    void stop() noexcept;

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] std::uint16_t port() const noexcept;
    [[nodiscard]] const std::filesystem::path&
    cookie_path() const noexcept;

private:
    [[nodiscard]] bool authorized(
        const httplib::Request& request
    ) const noexcept;

    consensus::ChainParams params_{};
    net::NetworkRuntime& runtime_;
    std::filesystem::path directory_{};
    std::filesystem::path cookie_path_{};
    RpcDispatcher dispatcher_;
    std::unique_ptr<httplib::Server> server_{};
    std::thread worker_{};
    std::string authorization_header_{};
    std::atomic<bool> running_{false};
    std::uint16_t port_{0U};
};

} // namespace quintum::rpc
