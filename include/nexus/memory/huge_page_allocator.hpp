#pragma once

#include "nexus/common/compiler.hpp"
#include <cstddef>
#include <cstdint>
#include <new>

namespace nexus::memory {

class alignas(NEXUS_CACHE_LINE_SIZE) HugePageArena {
public:
    static constexpr std::size_t DEFAULT_HUGE_PAGE_SIZE = 2 * 1024 * 1024; // 2MB

    explicit HugePageArena(std::size_t capacity, std::size_t page_size = DEFAULT_HUGE_PAGE_SIZE);
    ~HugePageArena();

    HugePageArena(const HugePageArena&) = delete;
    HugePageArena& operator=(const HugePageArena&) = delete;
    HugePageArena(HugePageArena&& other) noexcept;
    HugePageArena& operator=(HugePageArena&& other) noexcept;

    /**
     * @brief Bump allocates a chunk of raw memory aligned to the specified alignment.
     */
    [[nodiscard]] void* allocate_raw(std::size_t bytes, std::size_t alignment = 64) noexcept;

    /**
     * @brief Allocates an array of type T within the arena.
     */
    template <typename T>
    [[nodiscard]] T* allocate(std::size_t count = 1) noexcept {
        void* ptr = allocate_raw(sizeof(T) * count, alignof(T));
        return static_cast<T*>(ptr);
    }

    void reset() noexcept {
        offset_ = 0;
    }

    [[nodiscard]] void* data() const noexcept { return buffer_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t allocated_bytes() const noexcept { return offset_; }
    [[nodiscard]] bool is_huge_page_backed() const noexcept { return is_huge_page_; }

private:
    void* buffer_{nullptr};
    std::size_t capacity_{0};
    std::size_t offset_{0};
    std::size_t page_size_{DEFAULT_HUGE_PAGE_SIZE};
    bool is_huge_page_{false};
};

} // namespace nexus::memory
