#pragma once

#include "nexus/common/compiler.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <concepts>
#include <utility>
#include <new>

namespace nexus::ring {

template <typename T, std::size_t Capacity>
    requires (Capacity > 0 && ((Capacity & (Capacity - 1)) == 0)) // Must be power of two
class alignas(NEXUS_CACHE_LINE_SIZE) SPSCRingBuffer {
public:
    static constexpr std::size_t MASK = Capacity - 1;
    static constexpr std::size_t CAPACITY = Capacity;

    SPSCRingBuffer() = default;
    ~SPSCRingBuffer() = default;

    SPSCRingBuffer(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer(SPSCRingBuffer&&) = delete;
    SPSCRingBuffer& operator=(SPSCRingBuffer&&) = delete;

    /**
     * @brief Producer attempts to push an item by copy/move.
     */
    [[nodiscard]] NEXUS_FORCE_INLINE bool try_push(const T& item) noexcept {
        const uint64_t current_head = producer_head_.load(std::memory_order_relaxed);
        
        // Fast path: check against local cached tail to avoid cross-core coherency ping
        if (current_head - cached_consumer_tail_ >= Capacity) {
            cached_consumer_tail_ = consumer_tail_.load(std::memory_order_acquire);
            if (current_head - cached_consumer_tail_ >= Capacity) {
                return false; // Ring buffer is full
            }
        }

        slots_[current_head & MASK] = item;
        producer_head_.store(current_head + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] NEXUS_FORCE_INLINE bool try_push(T&& item) noexcept {
        const uint64_t current_head = producer_head_.load(std::memory_order_relaxed);
        
        if (current_head - cached_consumer_tail_ >= Capacity) {
            cached_consumer_tail_ = consumer_tail_.load(std::memory_order_acquire);
            if (current_head - cached_consumer_tail_ >= Capacity) {
                return false;
            }
        }

        slots_[current_head & MASK] = std::move(item);
        producer_head_.store(current_head + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Zero-copy in-place slot claim for producer.
     * Returns pointer to writable slot, or nullptr if full.
     */
    [[nodiscard]] NEXUS_FORCE_INLINE T* claim_slot() noexcept {
        const uint64_t current_head = producer_head_.load(std::memory_order_relaxed);
        if (current_head - cached_consumer_tail_ >= Capacity) {
            cached_consumer_tail_ = consumer_tail_.load(std::memory_order_acquire);
            if (current_head - cached_consumer_tail_ >= Capacity) {
                return nullptr;
            }
        }
        return &slots_[current_head & MASK];
    }

    /**
     * @brief Publishes the slot previously claimed via claim_slot().
     */
    NEXUS_FORCE_INLINE void publish_slot() noexcept {
        const uint64_t current_head = producer_head_.load(std::memory_order_relaxed);
        producer_head_.store(current_head + 1, std::memory_order_release);
    }

    /**
     * @brief Consumer attempts to pop an item.
     */
    [[nodiscard]] NEXUS_FORCE_INLINE bool try_pop(T& item) noexcept {
        const uint64_t current_tail = consumer_tail_.load(std::memory_order_relaxed);

        // Fast path: check against local cached head
        if (current_tail == cached_producer_head_) {
            cached_producer_head_ = producer_head_.load(std::memory_order_acquire);
            if (current_tail == cached_producer_head_) {
                return false; // Ring buffer is empty
            }
        }

        item = std::move(slots_[current_tail & MASK]);
        consumer_tail_.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Zero-copy peek for consumer.
     * Returns pointer to current readable slot, or nullptr if empty.
     */
    [[nodiscard]] NEXUS_FORCE_INLINE const T* peek() noexcept {
        const uint64_t current_tail = consumer_tail_.load(std::memory_order_relaxed);
        if (current_tail == cached_producer_head_) {
            cached_producer_head_ = producer_head_.load(std::memory_order_acquire);
            if (current_tail == cached_producer_head_) {
                return nullptr;
            }
        }
        return &slots_[current_tail & MASK];
    }

    /**
     * @brief Releases the slot previously peeked by consumer.
     */
    NEXUS_FORCE_INLINE void release_slot() noexcept {
        const uint64_t current_tail = consumer_tail_.load(std::memory_order_relaxed);
        consumer_tail_.store(current_tail + 1, std::memory_order_release);
    }

    [[nodiscard]] NEXUS_FORCE_INLINE bool empty() const noexcept {
        return consumer_tail_.load(std::memory_order_relaxed) == producer_head_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] NEXUS_FORCE_INLINE std::size_t size() const noexcept {
        uint64_t head = producer_head_.load(std::memory_order_relaxed);
        uint64_t tail = consumer_tail_.load(std::memory_order_relaxed);
        return (head >= tail) ? (head - tail) : 0;
    }

private:
    // --- PRODUCER CACHE LINE ---
    alignas(NEXUS_CACHE_LINE_SIZE) std::atomic<uint64_t> producer_head_{0};
    uint64_t cached_consumer_tail_{0};
    uint8_t pad_prod_[NEXUS_CACHE_LINE_SIZE - sizeof(std::atomic<uint64_t>) - sizeof(uint64_t)];

    // --- CONSUMER CACHE LINE ---
    alignas(NEXUS_CACHE_LINE_SIZE) std::atomic<uint64_t> consumer_tail_{0};
    uint64_t cached_producer_head_{0};
    uint8_t pad_cons_[NEXUS_CACHE_LINE_SIZE - sizeof(std::atomic<uint64_t>) - sizeof(uint64_t)];

    // --- SLOTS BUFFER ---
    alignas(NEXUS_CACHE_LINE_SIZE) T slots_[Capacity];
};

} // namespace nexus::ring
