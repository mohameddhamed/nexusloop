#include "nexus/net/io_uring_engine.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <cstring>
#include <cstdio>

namespace nexus::net {

IOUringEngine::IOUringEngine(nexus::memory::HugePageArena& arena, uint32_t queue_depth)
    : arena_(arena), queue_depth_(queue_depth) {
    init_ring();
}

IOUringEngine::~IOUringEngine() {
    cleanup_ring();
}

void IOUringEngine::init_ring() {
#if defined(__linux__) && defined(SYS_io_uring_setup)
    std::memset(&params_, 0, sizeof(params_));
    // Request kernel polling thread (SQPOLL) to eliminate enter_syscall overhead
    params_.flags = IORING_SETUP_SQPOLL | IORING_SETUP_CQSIZE;
    params_.sq_thread_idle = 2000; // 2 seconds idle spin before sleeping
    params_.cq_entries = queue_depth_ * 2;

    ring_fd_ = static_cast<int>(::syscall(SYS_io_uring_setup, queue_depth_, &params_));
    if (ring_fd_ >= 0) {
        sqpoll_enabled_ = true;
    } else {
        // Fallback without SQPOLL if host kernel denies CAP_SYS_ADMIN
        std::memset(&params_, 0, sizeof(params_));
        ring_fd_ = static_cast<int>(::syscall(SYS_io_uring_setup, queue_depth_, &params_));
        sqpoll_enabled_ = false;
    }
#else
    ring_fd_ = -1;
    sqpoll_enabled_ = false;
#endif
}

void IOUringEngine::cleanup_ring() {
    if (ring_fd_ >= 0) {
        ::close(ring_fd_);
        ring_fd_ = -1;
    }
}

bool IOUringEngine::register_socket(int fd) noexcept {
    registered_sockets_.push_back(fd);
    return true;
}

uint32_t IOUringEngine::poll(std::span<PacketSlot> slots) noexcept {
    if (slots.empty()) return 0;

    uint32_t count = 0;
    // Iterate over non-blocking registered sockets
    for (int fd : registered_sockets_) {
        if (count >= slots.size()) break;

        ssize_t n = ::recv(fd, slots[count].buffer, slots[count].capacity, MSG_DONTWAIT);
        if (n > 0) {
            slots[count].length = static_cast<uint32_t>(n);
            slots[count].socket_fd = fd;
            ++count;
        }
    }
    return count;
}

bool IOUringEngine::send(int socket_fd, const uint8_t* data, std::size_t len) noexcept {
    ssize_t sent = ::send(socket_fd, data, len, MSG_NOSIGNAL | MSG_DONTWAIT);
    return (sent == static_cast<ssize_t>(len));
}

} // namespace nexus::net
