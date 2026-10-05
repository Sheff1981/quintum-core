#include "rpc/server.hpp"

#include "crypto/random.hpp"
#include "wallet/file_security.hpp"

#include <array>
#include <fstream>
#include <limits>
#include <span>
#include <system_error>
#include <utility>

namespace quintum::rpc {
namespace {

std::string hex_encode(
    std::span<const Byte> bytes)
{
    constexpr std::array<char, 16> digits{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };

    std::string out;
    out.reserve(bytes.size() * 2U);

    for (const Byte byte : bytes) {
        out.push_back(digits[byte >> 4U]);
        out.push_back(digits[byte & 0x0fU]);
    }

    return out;
}

std::string base64_encode(
    std::string_view input)
{
    constexpr std::string_view alphabet{
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/"
    };

    std::string out;
    out.reserve(
        ((input.size() + 2U) / 3U) * 4U
    );

    std::uint32_t accumulator{0U};
    unsigned bits{0U};

    for (const char raw : input) {
        const auto ch =
            static_cast<unsigned char>(raw);

        accumulator =
            (accumulator << 8U) |
            static_cast<std::uint32_t>(ch);
        bits += 8U;

        while (bits >= 6U) {
            bits -= 6U;
            out.push_back(
                alphabet[
                    (accumulator >> bits) &
                    0x3fU
                ]
            );
        }
    }

    if (bits != 0U) {
        accumulator <<=
            6U - bits;
        out.push_back(
            alphabet[
                accumulator & 0x3fU
            ]
        );
    }

    while ((out.size() & 3U) != 0U) {
        out.push_back('=');
    }

    return out;
}

bool constant_time_equal(
    std::string_view lhs,
    std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    unsigned difference{0U};

    for (std::size_t i = 0U;
         i < lhs.size();
         ++i) {
        difference |=
            static_cast<unsigned>(
                static_cast<unsigned char>(
                    lhs[i])) ^
            static_cast<unsigned>(
                static_cast<unsigned char>(
                    rhs[i]));
    }

    return difference == 0U;
}

void remove_file(
    const std::filesystem::path& path) noexcept
{
    std::error_code ignored;
    (void)std::filesystem::remove(
        path,
        ignored
    );
}

} // namespace

RpcServer::RpcServer(
    const consensus::ChainParams& params,
    net::NetworkRuntime& runtime,
    std::filesystem::path directory)
    : params_(params),
      runtime_(runtime),
      directory_(std::move(directory)),
      cookie_path_(directory_ / ".cookie"),
      dispatcher_(params_, runtime_)
{
}

RpcServer::~RpcServer()
{
    stop();
}

RpcServerStartResult
RpcServer::start(
    std::uint16_t requested_port,
    std::size_t max_request_bytes)
{
    RpcServerStartResult out;
    out.cookie_path = cookie_path_;

    if (running_.load() ||
        worker_.joinable()) {
        out.error =
            RpcServerError::
                already_running;
        return out;
    }

    if (max_request_bytes == 0U) {
        out.error =
            RpcServerError::invalid_port;
        return out;
    }

    std::array<Byte, 32> random{};

    if (!crypto::secure_random_bytes(
            random)) {
        out.error =
            RpcServerError::random_failed;
        return out;
    }

    const std::string cookie =
        "__cookie__:" +
        hex_encode(random);

    crypto::secure_erase(
        std::span<Byte>{random}
    );

    const std::filesystem::path
        temporary_path =
            directory_ / ".cookie.tmp";

    std::error_code filesystem_error;
    std::filesystem::create_directories(
        directory_,
        filesystem_error
    );

    if (filesystem_error) {
        out.error =
            RpcServerError::
                cookie_write_failed;
        return out;
    }

    remove_file(temporary_path);

    {
        std::ofstream output{
            temporary_path,
            std::ios::binary |
                std::ios::trunc
        };

        if (!output.is_open()) {
            out.error =
                RpcServerError::
                    cookie_write_failed;
            return out;
        }

        output.write(
            cookie.data(),
            static_cast<std::streamsize>(
                cookie.size())
        );
        output.put('\n');
        output.flush();

        if (!output.good()) {
            output.close();
            remove_file(
                temporary_path
            );
            out.error =
                RpcServerError::
                    cookie_write_failed;
            return out;
        }
    }

    if (!wallet::restrict_file_permissions(
            temporary_path)) {
        remove_file(temporary_path);
        out.error =
            RpcServerError::
                cookie_permission_failed;
        return out;
    }

    remove_file(cookie_path_);

    std::filesystem::rename(
        temporary_path,
        cookie_path_,
        filesystem_error
    );

    if (filesystem_error) {
        remove_file(temporary_path);
        out.error =
            RpcServerError::
                cookie_write_failed;
        return out;
    }

    authorization_header_ =
        "Basic " +
        base64_encode(cookie);

    server_ =
        std::make_unique<
            httplib::Server>();

    server_->set_payload_max_length(
        max_request_bytes
    );

    server_->Post(
        "/",
        [this](
            const httplib::Request& request,
            httplib::Response& response) {
            response.set_header(
                "Cache-Control",
                "no-store"
            );

            if (!authorized(request)) {
                response.status = 401;
                response.set_header(
                    "WWW-Authenticate",
                    "Basic realm=\"QUINTUM JSON-RPC\""
                );
                response.set_content(
                    "Unauthorized\n",
                    "text/plain"
                );
                return;
            }

            const std::string body =
                dispatcher_.handle(
                    request.body
                );

            if (body.empty()) {
                response.status = 204;
                return;
            }

            response.status = 200;
            response.set_content(
                body,
                "application/json"
            );
        }
    );

    server_->set_error_handler(
        [](
            const httplib::Request&,
            httplib::Response& response) {
            if (response.status == 413) {
                response.set_content(
                    "Request too large\n",
                    "text/plain"
                );
            }
        }
    );

    const int bound =
        server_->bind_to_port(
            "127.0.0.1",
            static_cast<int>(
                requested_port)
        );

    if (bound <= 0 ||
        bound >
            static_cast<int>(
                std::numeric_limits<
                    std::uint16_t>::max())) {
        server_.reset();
        authorization_header_.clear();
        remove_file(cookie_path_);
        out.error =
            RpcServerError::bind_failed;
        return out;
    }

    port_ =
        static_cast<std::uint16_t>(
            bound
        );

    running_.store(true);

    try {
        worker_ =
            std::thread{
                [this]() {
                    const bool served =
                        server_->
                            listen_after_bind();

                    if (!served) {
                        running_.store(false);
                    }
                }
            };
    } catch (...) {
        running_.store(false);
        server_->stop();
        server_.reset();
        port_ = 0U;
        authorization_header_.clear();
        remove_file(cookie_path_);
        out.error =
            RpcServerError::
                thread_start_failed;
        return out;
    }

    out.port = port_;
    return out;
}

void RpcServer::stop() noexcept
{
    if (server_) {
        server_->stop();
    }

    if (worker_.joinable()) {
        worker_.join();
    }

    server_.reset();
    running_.store(false);
    port_ = 0U;

    if (!authorization_header_.empty()) {
        crypto::secure_erase(
            std::span<Byte>{
                reinterpret_cast<Byte*>(
                    authorization_header_.
                        data()),
                authorization_header_.
                    size()
            }
        );
        authorization_header_.clear();
        authorization_header_.
            shrink_to_fit();
    }

    remove_file(cookie_path_);
}

bool RpcServer::running() const noexcept
{
    return running_.load();
}

std::uint16_t
RpcServer::port() const noexcept
{
    return port_;
}

const std::filesystem::path&
RpcServer::cookie_path() const noexcept
{
    return cookie_path_;
}

bool RpcServer::authorized(
    const httplib::Request& request) const
    noexcept
{
    if (!request.has_header(
            "Authorization")) {
        return false;
    }

    return constant_time_equal(
        request.get_header_value(
            "Authorization"),
        authorization_header_
    );
}

} // namespace quintum::rpc
