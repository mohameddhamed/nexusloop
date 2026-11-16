#include "nexus/net/epoll_engine.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <cstring>

namespace nexus::net {

EpollEngine::EpollEngine() {
#if defined(__linux__)
    poll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
#elif defined(__APPLE__)
    poll_fd_ = ::kqueue();
#endif
}

EpollEngine::~EpollEngine() {
    if (poll_fd_ >= 0) {
        ::close(poll_fd_);
        poll_fd_ = -1;
    }
}

bool EpollEngine::enable_busy_poll(int fd, uint32_t busy_poll_us) noexcept {
#if defined(__linux__) && defined(SO_BUSY_POLL)
    int val = static_cast<int>(busy_poll_us);
    return ::setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &val, sizeof(val)) == 0;
#else
    (void)fd;
    (void)busy_poll_us;
    return false;
#endif
}

bool EpollEngine::add_socket(int fd) noexcept {
    // Set non-blocking mode
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

#if defined(__linux__)
    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET; // Edge-Triggered
    ev.data.fd = fd;
    if (::epoll_ctl(poll_fd_, EPOLL_CTL_ADD, fd, &ev) == 0) {
        registered_fds_.push_back(fd);
        return true;
    }
    return false;

#elif defined(__APPLE__)
    struct kevent change{};
    EV_SET(&change, fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, nullptr);
    if (::kevent(poll_fd_, &change, 1, nullptr, 0, nullptr) == 0) {
        registered_fds_.push_back(fd);
        return true;
    }
    return false;
#else
    registered_fds_.push_back(fd);
    return true;
#endif
}

uint32_t EpollEngine::poll(std::span<PacketSlot> slots) noexcept {
    if (slots.empty()) return 0;
    uint32_t received = 0;

#if defined(__linux__)
    int nfds = ::epoll_wait(poll_fd_, events_, MAX_EVENTS, 0); // Non-blocking epoll
    if (nfds > 0) {
        spin_backoff_.reset();
        for (int i = 0; i < nfds && received < slots.size(); ++i) {
            int fd = events_[i].data.fd;
            ssize_t n = ::recv(fd, slots[received].buffer, slots[received].capacity, MSG_DONTWAIT);
            if (n > 0) {
                slots[received].length = static_cast<uint32_t>(n);
                slots[received].socket_fd = fd;
                ++received;
            }
        }
    } else {
        spin_backoff_.pause();
    }
#elif defined(__APPLE__)
    struct timespec timeout{0, 0};
    int nev = ::kevent(poll_fd_, nullptr, 0, events_, MAX_EVENTS, &timeout);
    if (nev > 0) {
        spin_backoff_.reset();
        for (int i = 0; i < nev && received < slots.size(); ++i) {
            int fd = static_cast<int>(events_[i].ident);
            ssize_t n = ::recv(fd, slots[received].buffer, slots[received].capacity, 0);
            if (n > 0) {
                slots[received].length = static_cast<uint32_t>(n);
                slots[received].socket_fd = fd;
                ++received;
            }
        }
    } else {
        spin_backoff_.pause();
    }
#else
    for (int fd : registered_fds_) {
        if (received >= slots.size()) break;
        ssize_t n = ::recv(fd, slots[received].buffer, slots[received].capacity, 0);
        if (n > 0) {
            slots[received].length = static_cast<uint32_t>(n);
            slots[received].socket_fd = fd;
            ++received;
        }
    }
    if (received == 0) spin_backoff_.pause();
    else spin_backoff_.reset();
#endif

    return received;
}

bool EpollEngine::send(int socket_fd, const uint8_t* data, std::size_t len) noexcept {
    ssize_t sent = ::send(socket_fd, data, len, 0);
    return (sent == static_cast<ssize_t>(len));
}

} // namespace nexus::net
