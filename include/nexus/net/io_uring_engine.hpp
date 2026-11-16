#pragma once

#include "nexus/net/io_backend.hpp"
#include "nexus/memory/huge_page_allocator.hpp"
#include <vector>
#include <memory>

#if defined(__linux__)
#include <linux/io_uring.h>
#include <sys/syscall.h>
#endif

namespace nexus::net {

class IOUringEngine final : public IOBackend {
public:
    static constexpr uint32_t DEFAULT_ENTRIES = 1024;

    explicit IOUringEngine(nexus::memory::HugePageArena& arena, uint32_t queue_depth = DEFAULT_ENTRIES);
    ~IOUringEngine() override;

    IOUringEngine(const IOUringEngine&) = delete;
    IOUringEngine& operator=(const IOUringEngine&) = delete;

    bool register_socket(int fd) noexcept;
    uint32_t poll(std::span<PacketSlot> slots) noexcept override;
    bool send(int socket_fd, const uint8_t* data, std::size_t len) noexcept override;

    [[nodiscard]] bool is_sqpoll_enabled() const noexcept { return sqpoll_enabled_; }

private:
    [[maybe_unused]] nexus::memory::HugePageArena& arena_;
    [[maybe_unused]] uint32_t queue_depth_;
    int ring_fd_{-1};
    bool sqpoll_enabled_{false};
    std::vector<int> registered_sockets_;

#if defined(__linux__)
    struct io_uring_params params_{};
    void* sq_ptr_{nullptr};
    void* cq_ptr_{nullptr};
    std::size_t sq_mmap_size_{0};
    std::size_t cq_mmap_size_{0};
#endif

    void init_ring();
    void cleanup_ring();
};

} // namespace nexus::net
