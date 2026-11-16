#pragma once

#include "nexus/common/compiler.hpp"
#include "nexus/protocol/sbe/sbe_header.hpp"
#include <span>
#include <cstdint>
#include <cstddef>

namespace nexus::protocol::sbe {

class SBEDecoder {
public:
    explicit SBEDecoder(std::span<const uint8_t> buffer) noexcept
        : buffer_(buffer.data()), size_(buffer.size()), offset_(0) {}

    [[nodiscard]] NEXUS_FORCE_INLINE const MessageHeader* decode_header() noexcept {
        if (offset_ + sizeof(MessageHeader) > size_) [[unlikely]] {
            return nullptr;
        }
        const auto* hdr = reinterpret_cast<const MessageHeader*>(buffer_ + offset_);
        offset_ += sizeof(MessageHeader);
        return hdr;
    }

    template <typename T>
    [[nodiscard]] NEXUS_FORCE_INLINE const T* decode_root_block(uint16_t block_length) noexcept {
        if (offset_ + block_length > size_) [[unlikely]] {
            return nullptr;
        }
        const auto* block = reinterpret_cast<const T*>(buffer_ + offset_);
        offset_ += block_length;
        return block;
    }

    [[nodiscard]] NEXUS_FORCE_INLINE const GroupSizeEncoding* decode_group_header() noexcept {
        if (offset_ + sizeof(GroupSizeEncoding) > size_) [[unlikely]] {
            return nullptr;
        }
        const auto* grp = reinterpret_cast<const GroupSizeEncoding*>(buffer_ + offset_);
        offset_ += sizeof(GroupSizeEncoding);
        return grp;
    }

    [[nodiscard]] NEXUS_FORCE_INLINE std::span<const MarketDataEntry> decode_entries(uint16_t count, uint16_t entry_size) noexcept {
        std::size_t total_bytes = static_cast<std::size_t>(count) * entry_size;
        if (offset_ + total_bytes > size_) [[unlikely]] {
            return {};
        }
        const auto* entries = reinterpret_cast<const MarketDataEntry*>(buffer_ + offset_);
        offset_ += total_bytes;
        return std::span<const MarketDataEntry>(entries, count);
    }

    [[nodiscard]] std::size_t bytes_remaining() const noexcept {
        return (size_ >= offset_) ? (size_ - offset_) : 0;
    }

private:
    const uint8_t* buffer_{nullptr};
    std::size_t size_{0};
    std::size_t offset_{0};
};

} // namespace nexus::protocol::sbe
