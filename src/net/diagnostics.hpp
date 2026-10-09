#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace quintum::net {

// Local metadata only. Operations are best effort and never affect validation.
class Diagnostics {
public:
    explicit Diagnostics(std::filesystem::path directory);
    void stage(std::string_view stage, std::string_view component = "p2p");
    void peer(std::string endpoint, uint32_t protocol, uint32_t height);
    void peer_state(std::string_view state, std::string_view reason = {},
        std::string_view timeout = {});
    void attempt(std::string endpoint);
    void failure(std::string_view component, int code, std::string_view description);
    void network_progress();
    void local_height(uint32_t height);
    void counter(std::string_view name, uint64_t delta);
    void gauge(std::string_view name, uint64_t value);
    void duration(std::string_view name, uint64_t microseconds);
    void event(std::string_view component, std::string_view severity,
        std::string_view name, int code = 0, uint64_t duration_ms = 0);
    void synchronized();
    std::string peer_state_description() const;
    std::string snapshot_json() const;
    std::string export_log() const;
    bool clear_log();

private:
    using Clock = std::chrono::steady_clock;
    void append(std::string_view component, std::string_view severity,
        std::string_view name, int code, uint64_t duration_ms);
    void note_error(std::string_view error) const noexcept;

    std::filesystem::path directory_, current_, previous_;
    mutable std::mutex state_mutex_, log_mutex_;
    std::recursive_mutex stage_mutex_;
    std::string peer_state_ = "disconnected", disconnect_reason_, timeout_reason_, last_message_utc_, last_activity_utc_;
    std::string stage_ = "idle", endpoint_, error_description_, last_sync_;
    mutable std::string log_error_;
    uint32_t protocol_ = 0, remote_height_ = 0, local_height_ = 0;
    bool have_local_height_ = false, progress_seen_ = false, connection_seen_ = false;
    uint64_t attempts_ = 0, retries_ = 0;
    int error_code_ = 0;
    std::unordered_map<std::string, uint64_t> values_;
    Clock::time_point started_ = Clock::now(), stage_started_ = started_;
    Clock::time_point progress_ = started_, connection_started_ = started_, randomx_started_ = started_;
};

} // namespace quintum::net
