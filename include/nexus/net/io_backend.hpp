#pragma once

#include "nexus/common/compiler.hpp"
#include <cstdint>
#include <cstddef>
#include <span>

namespace nexus::net {

struct PacketSlot {
    uint8_t* buffer;
    uint32_t capacity;
    uint32_t length;
    int32_t  socket_fd;
};

class IOBackend {
public:
    virtual ~IOBackend() = default;

    /**
     * @brief Polls network interface for packets.
     * @param slots Output span of packet slots to populate.
     * @return Number of packets received.
     */
    virtual uint32_t poll(std::span<PacketSlot> slots) noexcept = 0;

    /**
     * @brief Transmits a packet buffer.
     */
    virtual bool send(int socket_fd, const uint8_t* data, std::size_t len) noexcept = 0;
};

} // namespace nexus::net
