#include "net/validation_worker.hpp"
#include <cassert>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
using namespace std::chrono_literals;
int main() {
    quintum::net::ValidationWorker worker;
    std::promise<void> entered, release;
    auto gate = release.get_future().share();
    auto result = worker.submit([&](quintum::net::ValidationStopToken) {
        entered.set_value(); gate.wait(); return true;
    });
    assert(result);
    entered.get_future().wait();
    assert(!worker.submit([](quintum::net::ValidationStopToken) { return true; }));
    assert(result->wait_for(0ms) == std::future_status::timeout);
    release.set_value(); assert(result->get());
    std::atomic<bool> stopping_started{false};
    std::optional<std::future<bool>> cancelled;
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    do {
        cancelled = worker.submit([&](quintum::net::ValidationStopToken stop) {
            stopping_started.store(true);
            while (!stop.stop_requested()) std::this_thread::yield();
            return false;
        });
    } while (!cancelled && std::chrono::steady_clock::now() < deadline);
    assert(cancelled);
    while (!stopping_started.load()) std::this_thread::yield();
    worker.stop(); assert(!cancelled->get());
    assert(!worker.submit([](quintum::net::ValidationStopToken) { return true; }));
}
