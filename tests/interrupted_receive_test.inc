#ifndef _WIN32
#include <atomic>
#include <chrono>
#include <csignal>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t interrupted_receive_signals = 0;
void interrupt_receive(int) { interrupted_receive_signals = 1; }

void test_interrupted_peer_receive(bool during_wait = false)
{
    interrupted_receive_signals = 0;
    using namespace quintum;
    using namespace quintum::net;
    int sockets[2]{};
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    timeval timeout{2, 0};
    assert(setsockopt(sockets[0], SOL_SOCKET, SO_RCVTIMEO,
                     &timeout, sizeof(timeout)) == 0);
    struct sigaction action{};
    struct sigaction previous{};
    action.sa_handler = interrupt_receive;
    sigemptyset(&action.sa_mask);
    assert(sigaction(SIGUSR1, &action, &previous) == 0);
    const auto& params = consensus::regtest_params();
    PeerSession receiver(params, static_cast<std::uintptr_t>(sockets[0]),
                         false, version(0x1234U, 0U));
    PeerSession sender(params, static_cast<std::uintptr_t>(sockets[1]),
                       true, version(0x5678U, 0U));
    PeerError error{PeerError::none};
    WireMessage message;
    std::atomic<bool> ready{false};
    std::thread worker([&] {
        ready.store(true);
        if (during_wait) assert(receiver.wait_readable(2'000U));
        error = receiver.receive_command(message);
    });
    while (!ready.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(pthread_kill(worker.native_handle(), SIGUSR1) == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(sender.send_command("ping", serialize_nonce(42U)) == PeerError::none);
    worker.join();
    assert(sigaction(SIGUSR1, &previous, nullptr) == 0);
    assert(interrupted_receive_signals == 1);
    assert(error == PeerError::none);
    assert(message.command == "ping");
    assert(parse_nonce(message.payload) == 42U);
}
} // namespace
#endif
