#pragma once
#include <condition_variable>
#include <atomic>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <thread>

namespace quintum::net {
// NDK 26's libc++ does not provide every C++20 jthread/stop_token facility.
// An owned std::thread and a monotonic atomic cancellation token are portable.
struct ValidationStopToken {
    const std::atomic<bool>& requested;
    [[nodiscard]] bool stop_requested() const noexcept { return requested.load(); }
};
// A persistent thread with one admitted task, no backlog and no detached work.
// The caller retains task data until the returned future has completed.
class ValidationWorker {
public:
    ValidationWorker() : thread_([this] { run(); }) {}
    ~ValidationWorker() { stop(); }
    ValidationWorker(const ValidationWorker&) = delete;
    ValidationWorker& operator=(const ValidationWorker&) = delete;
    [[nodiscard]] std::optional<std::future<bool>> submit(
        std::function<bool(ValidationStopToken)> work)
    {
        std::lock_guard lock(mutex_);
        if (busy_ || stopping_) return std::nullopt;
        task_.emplace(std::move(work));
        auto result = task_->get_future();
        busy_ = true;
        ready_.notify_one();
        return result;
    }
    void stop() noexcept
    {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        stop_requested_.store(true);
        ready_.notify_one();
        if (thread_.joinable()) thread_.join();
    }
private:
    void run()
    {
        for (;;) {
            std::unique_lock lock(mutex_);
            ready_.wait(lock, [&] { return task_.has_value() || stopping_; });
            if (!task_) return;
            auto task = std::move(*task_);
            task_.reset();
            lock.unlock();
            // packaged_task transports exceptions to the owner through get().
            task(ValidationStopToken{stop_requested_});
            lock.lock();
            busy_ = false;
            if (stopping_) return;
        }
    }
    std::mutex mutex_;
    std::condition_variable ready_;
    std::optional<std::packaged_task<bool(ValidationStopToken)>> task_;
    bool busy_{false}, stopping_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread thread_;
};
} // namespace quintum::net
