#pragma once

#include "nexus/common/compiler.hpp"
#include <cstddef>
#include <cstdint>
#include <array>
#include <span>

namespace nexus::memory {

template <std::size_t Size, std::size_t Alignment = NEXUS_CACHE_LINE_SIZE>
struct alignas(Alignment) AlignedBuffer {
    uint8_t data[Size];

    [[nodiscard]] constexpr std::size_t size() const noexcept { return Size; }
    [[nodiscard]] uint8_t* ptr() noexcept { return data; }
    [[nodiscard]] const uint8_t* ptr() const noexcept { return data; }
    [[nodiscard]] std::span<uint8_t> as_span() noexcept { return std::span<uint8_t>(data, Size); }
    [[nodiscard]] std::span<const uint8_t> as_span() const noexcept { return std::span<const uint8_t>(data, Size); }
};

} // namespace nexus::memory
