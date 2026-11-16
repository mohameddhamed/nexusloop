#pragma once

#include "nexus/net/io_backend.hpp"
#include "nexus/common/spin_backoff.hpp"
#include <vector>

#if defined(__linux__)
#include <sys/epoll.h>
#elif defined(__APPLE__)
#include <sys/event.h>
#endif

namespace nexus::net {

class EpollEngine final : public IOBackend {
public:
    static constexpr int MAX_EVENTS = 64;

    EpollEngine();
    ~EpollEngine() override;

    bool add_socket(int fd) noexcept;
    bool enable_busy_poll(int fd, uint32_t busy_poll_us = 50) noexcept;

    uint32_t poll(std::span<PacketSlot> slots) noexcept override;
    bool send(int socket_fd, const uint8_t* data, std::size_t len) noexcept override;

private:
    int poll_fd_{-1};
    std::vector<int> registered_fds_;
    nexus::common::AdaptiveSpinBackoff spin_backoff_;

#if defined(__linux__)
    struct epoll_event events_[MAX_EVENTS];
#elif defined(__APPLE__)
    struct kevent events_[MAX_EVENTS];
#endif
};

} // namespace nexus::net
