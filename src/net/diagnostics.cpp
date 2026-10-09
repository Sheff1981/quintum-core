#include "diagnostics.hpp"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace quintum::net {
namespace {

constexpr size_t log_limit = 256 * 1024;
constexpr std::string_view metric_names[] = {
    "headers_received", "headers_verified", "headers_rejected", "headers_known",
    "header_batch_size", "headers_verify_ms", "randomx_calls",
    "randomx_verify_ms", "randomx_cache_ms", "blocks_requested",
    "blocks_received", "blocks_accepted", "blocks_rejected", "block_verify_ms",
    "header_batches_received", "header_batches_accepted", "header_batches_rejected",
    "header_wait_ms", "block_wait_ms", "handshake", "core_running",
    "active_peers", "known_peers", "headers_unvalidated", "randomx_active"
};

std::string bounded(std::string_view input, size_t maximum = 256)
{
    std::string result(input.substr(0, maximum));
    for (auto& character : result) {
        if (static_cast<unsigned char>(character) < 32) character = ' ';
    }
    return result;
}

std::string quote(std::string_view input)
{
    std::string result = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char character : input) {
        if (character == '"' || character == '\\') {
            result += '\\';
            result += static_cast<char>(character);
        } else if (character < 32 || character >= 127) {
            result += "\\u00";
            result += hex[character >> 4];
            result += hex[character & 15];
        } else {
            result += static_cast<char>(character);
        }
    }
    return result + '"';
}

std::string utc()
{
    const auto time = std::time(nullptr);
    std::tm calendar{};
#ifdef _WIN32
    gmtime_s(&calendar, &time);
#else
    gmtime_r(&time, &calendar);
#endif
    std::ostringstream result;
    result << std::put_time(&calendar, "%Y-%m-%dT%H:%M:%SZ");
    return result.str();
}

uint64_t millis(std::chrono::steady_clock::time_point time)
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - time).count());
}

bool metric(std::string_view name)
{
    return std::find(std::begin(metric_names), std::end(metric_names), name)
        != std::end(metric_names);
}

uint64_t add(uint64_t value, uint64_t delta)
{
    const auto maximum = std::numeric_limits<uint64_t>::max();
    return delta > maximum - value ? maximum : value + delta;
}

// Bound preexisting journals as well as journals produced by this process.
void cap_file(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path) || std::filesystem::file_size(path) <= log_limit)
        return;
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("diagnostic log read failed");
    input.seekg(-static_cast<std::streamoff>(log_limit), std::ios::end);
    std::string tail(log_limit, '\0');
    input.read(tail.data(), static_cast<std::streamsize>(tail.size()));
    if (!input) throw std::runtime_error("diagnostic log read failed");
    const auto newline = tail.find('\n');
    tail = newline == std::string::npos ? std::string{} : tail.substr(newline + 1);
    input.close();
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << tail;
    output.flush();
    if (!output) throw std::runtime_error("diagnostic log write failed");
}

std::string read(const std::filesystem::path& path)
{
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        if (error) throw std::runtime_error("diagnostic log stat failed");
        return {};
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("diagnostic log read failed");
    std::string result(log_limit, '\0');
    input.read(result.data(), static_cast<std::streamsize>(result.size()));
    result.resize(static_cast<size_t>(input.gcount()));
    if (input.bad()) throw std::runtime_error("diagnostic log read failed");
    return result;
}

void write_metrics(std::ostream& output,
    const std::unordered_map<std::string, uint64_t>& values)
{
    for (auto name : metric_names) {
        const auto found = values.find(std::string(name));
        output << "," << quote(name) << ":"
               << (found == values.end() ? 0 : found->second);
    }
}

} // namespace

Diagnostics::Diagnostics(std::filesystem::path directory)
    : directory_(std::move(directory)),
      current_(directory_ / "diagnostics.jsonl"),
      previous_(directory_ / "diagnostics.previous.jsonl")
{
    try {
        std::filesystem::create_directories(directory_);
        cap_file(previous_);
        cap_file(current_);
        const auto records = read(previous_) + read(current_);
        size_t position = 0;
        constexpr std::string_view marker = "\"last_sync_utc\":\"";
        while ((position = records.find(marker, position)) != std::string::npos) {
            position += marker.size();
            const auto end = records.find('"', position);
            const auto date = records.substr(position, end - position);
            if (date.size() == 20 && date[4] == '-' && date[10] == 'T' && date[19] == 'Z')
                last_sync_ = date;
        }
    } catch (...) {
        note_error("diagnostic initialization/read failed");
    }
}

void Diagnostics::note_error(std::string_view error) const noexcept
{
    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        log_error_ = bounded(error);
    } catch (...) {}
}

void Diagnostics::stage(std::string_view next_stage, std::string_view component)
{
    try {
        const auto next = bounded(next_stage, 64);
        std::lock_guard<std::recursive_mutex> transition_lock(stage_mutex_);
        uint64_t elapsed = 0;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (next == stage_) return;
            elapsed = millis(stage_started_);
        }
        // Capture the preceding stage before replacing it. IO never holds the state lock.
        event(component, "info", "stage_complete", 0, elapsed);
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            stage_ = next;
            stage_started_ = Clock::now();
        }
        event(component, "info", "stage");
    } catch (...) {}
}

void Diagnostics::peer(std::string endpoint, uint32_t protocol, uint32_t height)
{
    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (!endpoint.empty()) endpoint_ = bounded(endpoint);
        protocol_ = protocol;
        remote_height_ = height;
    } catch (...) {}
}

void Diagnostics::attempt(std::string endpoint)
{
    try {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            endpoint_ = bounded(endpoint);
            attempts_ = add(attempts_, 1);
            retries_ = attempts_ - 1;
            connection_started_ = Clock::now();
            connection_seen_ = true;
        }
        event("p2p", "info", "attempt");
    } catch (...) {}
}

void Diagnostics::failure(std::string_view component, int code, std::string_view description)
{
    try {
        uint64_t elapsed = 0;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            elapsed = millis(stage_started_);
            error_code_ = code;
            error_description_ = bounded(description);
        }
        event(component, "error", "failure", code, elapsed);
    } catch (...) {}
}

void Diagnostics::network_progress()
{
    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        progress_ = Clock::now();
        progress_seen_ = true;
    } catch (...) {}
}

void Diagnostics::local_height(uint32_t height)
{
    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        local_height_ = height;
        have_local_height_ = true;
    } catch (...) {}
}

void Diagnostics::counter(std::string_view name, uint64_t delta)
{
    try {
        if (!metric(name)) return;
        std::lock_guard<std::mutex> lock(state_mutex_);
        auto& value = values_[std::string(name)];
        value = add(value, delta);
    } catch (...) {}
}

void Diagnostics::gauge(std::string_view name, uint64_t value)
{
    try {
        if (!metric(name)) return;
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (name == "randomx_active" && value != 0U && values_[std::string(name)] != value) randomx_started_ = Clock::now();
        values_[std::string(name)] = value;
    } catch (...) {}
}

void Diagnostics::duration(std::string_view name, uint64_t microseconds)
{
    try {
        if (!metric(name)) return;
        std::lock_guard<std::mutex> lock(state_mutex_);
        const auto key = std::string(name);
        auto& total = values_[key + "_microseconds"];
        total = add(total, microseconds);
        values_[key] = total / 1000;
    } catch (...) {}
}

void Diagnostics::event(std::string_view component, std::string_view severity,
    std::string_view name, int code, uint64_t duration_ms)
{
    try {
        append(component, severity, name, code, duration_ms);
    } catch (...) {
        note_error("diagnostic journal write failed");
    }
}

void Diagnostics::synchronized()
{
    try {
        std::lock_guard<std::recursive_mutex> transition_lock(stage_mutex_);
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (stage_ == "fully_synchronized") return;
            last_sync_ = utc();
        }
        stage("fully_synchronized");
        event("sync", "info", "synchronized");
    } catch (...) {}
}

void Diagnostics::append(std::string_view component, std::string_view severity,
    std::string_view name, int code, uint64_t duration_ms)
{
    std::string record;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        std::ostringstream output;
        output << "{\"utc\":" << quote(utc())
               << ",\"component\":" << quote(bounded(component, 64))
               << ",\"severity\":" << quote(bounded(severity, 16))
               << ",\"event\":" << quote(bounded(name, 64))
               << ",\"stage\":" << quote(stage_)
               << ",\"peer_endpoint\":" << quote(endpoint_)
               << ",\"protocol_version\":" << protocol_
               << ",\"remote_height\":" << remote_height_
               << ",\"error_code\":" << code
               << ",\"error_description\":" << quote(error_description_)
               << ",\"duration_ms\":" << duration_ms
               << ",\"retry_count\":" << retries_
               << ",\"connection_duration_ms\":"
               << (connection_seen_ ? millis(connection_started_) : 0)
               << ",\"last_sync_utc\":" << quote(last_sync_);
        write_metrics(output, values_);
        output << ",\"randomx_operation_elapsed_ms\":" << (values_.contains("randomx_active") && values_.at("randomx_active") != 0U ? millis(randomx_started_) : 0U);
        output << "}\n";
        record = output.str();
    }
    std::lock_guard<std::mutex> lock(log_mutex_);
    const auto size = std::filesystem::exists(current_)
        ? std::filesystem::file_size(current_) : 0;
    if (size + record.size() > log_limit) {
        std::error_code error;
        std::filesystem::remove(previous_, error);
        if (error) throw std::runtime_error("diagnostic remove failed");
        std::filesystem::rename(current_, previous_);
    }
    std::ofstream output(current_, std::ios::app | std::ios::binary);
    if (!output) throw std::runtime_error("diagnostic open failed");
    output << record;
    output.flush();
    if (!output) throw std::runtime_error("diagnostic write failed");
}

std::string Diagnostics::snapshot_json() const
{
    try {
        std::lock_guard<std::mutex> lock(state_mutex_);
        std::ostringstream output;
        output << "{\"stage\":" << quote(stage_)
               << ",\"stage_elapsed_ms\":" << millis(stage_started_)
               << ",\"peer_endpoint\":" << quote(endpoint_)
               << ",\"protocol_version\":" << protocol_
               << ",\"remote_height\":" << remote_height_
               << ",\"local_height\":"
               << (have_local_height_ ? std::to_string(local_height_) : "null")
               << ",\"attempt_count\":" << attempts_
               << ",\"retry_count\":" << retries_
               << ",\"last_error_code\":" << error_code_
               << ",\"last_error_description\":" << quote(error_description_);
        write_metrics(output, values_);
        output << ",\"randomx_operation_elapsed_ms\":" << (values_.contains("randomx_active") && values_.at("randomx_active") != 0U ? millis(randomx_started_) : 0U);
        const auto accepted = values_.find("blocks_accepted");
        const double throughput = accepted == values_.end() ? 0.0
            : static_cast<double>(accepted->second) * 1000.0
                / static_cast<double>(std::max<uint64_t>(1, millis(started_)));
        output << ",\"last_network_progress_ms\":" << (progress_seen_ ? millis(progress_) : 0)
               << ",\"network_progress_seen\":" << (progress_seen_ ? "true" : "false")
               << ",\"connection_duration_ms\":"
               << (connection_seen_ ? millis(connection_started_) : 0)
               << ",\"last_sync_utc\":" << quote(last_sync_)
               << ",\"throughput_blocks_per_second\":" << throughput
               << ",\"log_error\":" << quote(log_error_) << "}";
        return output.str();
    } catch (...) {
        return "{\"log_error\":\"snapshot unavailable\"}";
    }
}

std::string Diagnostics::export_log() const
{
    try {
        std::lock_guard<std::mutex> lock(log_mutex_);
        return read(previous_) + read(current_);
    } catch (...) {
        note_error("diagnostic export read failed");
        return {};
    }
}

bool Diagnostics::clear_log()
{
    try {
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::error_code current_error, previous_error;
        std::filesystem::remove(current_, current_error);
        std::filesystem::remove(previous_, previous_error);
        if (current_error || previous_error) {
            note_error("diagnostic clear failed");
            return false;
        }
        return true;
    } catch (...) {
        note_error("diagnostic clear failed");
        return false;
    }
}

} // namespace quintum::net
