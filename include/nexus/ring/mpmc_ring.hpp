#pragma once

#include "nexus/common/compiler.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <new>

namespace nexus::ring {

template <typename T, std::size_t Capacity>
    requires (Capacity > 1 && ((Capacity & (Capacity - 1)) == 0))
class alignas(NEXUS_CACHE_LINE_SIZE) MPMCRingBuffer {
public:
    static constexpr std::size_t MASK = Capacity - 1;

    MPMCRingBuffer() {
        for (std::size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    ~MPMCRingBuffer() = default;

    MPMCRingBuffer(const MPMCRingBuffer&) = delete;
    MPMCRingBuffer& operator=(const MPMCRingBuffer&) = delete;

    [[nodiscard]] NEXUS_FORCE_INLINE bool try_push(const T& data) noexcept {
        Cell* cell;
        uint64_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &buffer_[pos & MASK];
            uint64_t seq = cell->sequence.load(std::memory_order_acquire);
            int64_t dif = static_cast<int64_t>(seq) - static_cast<int64_t>(pos);
            if (dif == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (dif < 0) {
                return false; // Queue full
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }

        cell->data = data;
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] NEXUS_FORCE_INLINE bool try_push(T&& data) noexcept {
        Cell* cell;
        uint64_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &buffer_[pos & MASK];
            uint64_t seq = cell->sequence.load(std::memory_order_acquire);
            int64_t dif = static_cast<int64_t>(seq) - static_cast<int64_t>(pos);
            if (dif == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (dif < 0) {
                return false;
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }

        cell->data = std::move(data);
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] NEXUS_FORCE_INLINE bool try_pop(T& data) noexcept {
        Cell* cell;
        uint64_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &buffer_[pos & MASK];
            uint64_t seq = cell->sequence.load(std::memory_order_acquire);
            int64_t dif = static_cast<int64_t>(seq) - static_cast<int64_t>(pos + 1);
            if (dif == 0) {
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (dif < 0) {
                return false; // Queue empty
            } else {
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }

        data = std::move(cell->data);
        cell->sequence.store(pos + Capacity, std::memory_order_release);
        return true;
    }

private:
    struct Cell {
        std::atomic<uint64_t> sequence;
        T data;
    };

    // --- ENQUEUE CACHE LINE ---
    alignas(NEXUS_CACHE_LINE_SIZE) std::atomic<uint64_t> enqueue_pos_{0};
    uint8_t pad_enq_[NEXUS_CACHE_LINE_SIZE - sizeof(std::atomic<uint64_t>)];

    // --- DEQUEUE CACHE LINE ---
    alignas(NEXUS_CACHE_LINE_SIZE) std::atomic<uint64_t> dequeue_pos_{0};
    uint8_t pad_deq_[NEXUS_CACHE_LINE_SIZE - sizeof(std::atomic<uint64_t>)];

    // --- CELLS BUFFER ---
    alignas(NEXUS_CACHE_LINE_SIZE) Cell buffer_[Capacity];
};

} // namespace nexus::ring
