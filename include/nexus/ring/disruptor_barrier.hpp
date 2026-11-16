#pragma once

#include "nexus/common/compiler.hpp"
#include "nexus/common/spin_backoff.hpp"
#include <atomic>
#include <cstdint>
#include <algorithm>

namespace nexus::ring {

/**
 * @brief Cache-padded atomic sequence number (LMAX Disruptor style).
 */
class alignas(NEXUS_CACHE_LINE_SIZE) Sequence {
public:
    explicit Sequence(int64_t initial_value = -1) noexcept
        : value_(initial_value) {}

    [[nodiscard]] NEXUS_FORCE_INLINE int64_t get() const noexcept {
        return value_.load(std::memory_order_acquire);
    }

    [[nodiscard]] NEXUS_FORCE_INLINE int64_t get_relaxed() const noexcept {
        return value_.load(std::memory_order_relaxed);
    }

    NEXUS_FORCE_INLINE void set(int64_t value) noexcept {
        value_.store(value, std::memory_order_release);
    }

    NEXUS_FORCE_INLINE void set_relaxed(int64_t value) noexcept {
        value_.store(value, std::memory_order_relaxed);
    }

    [[nodiscard]] NEXUS_FORCE_INLINE bool compare_and_set(int64_t expected, int64_t desired) noexcept {
        return value_.compare_exchange_strong(expected, desired, std::memory_order_acq_rel);
    }

    [[nodiscard]] NEXUS_FORCE_INLINE int64_t increment_and_get() noexcept {
        return value_.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

private:
    std::atomic<int64_t> value_;
    uint8_t pad_[NEXUS_CACHE_LINE_SIZE - sizeof(std::atomic<int64_t>)];
};

/**
 * @brief SequenceBarrier coordinates downstream consumers waiting on upstream producer/consumer sequences.
 */
class SequenceBarrier {
public:
    explicit SequenceBarrier(const Sequence& cursor) noexcept
        : cursor_(cursor) {}

    /**
     * @brief Busy-spins with adaptive backoff until sequence becomes available.
     * Returns the highest available sequence published by upstream cursor.
     */
    [[nodiscard]] NEXUS_FORCE_INLINE int64_t wait_for(int64_t expected_sequence) const noexcept {
        nexus::common::AdaptiveSpinBackoff backoff;
        int64_t available;
        while ((available = cursor_.get()) < expected_sequence) {
            backoff.pause();
        }
        return available;
    }

    /**
     * @brief Non-blocking check for available sequence.
     */
    [[nodiscard]] NEXUS_FORCE_INLINE int64_t check_available() const noexcept {
        return cursor_.get();
    }

private:
    const Sequence& cursor_;
};

} // namespace nexus::ring
