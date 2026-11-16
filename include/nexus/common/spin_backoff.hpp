#pragma once

#include "nexus/common/compiler.hpp"
#include <cstdint>
#include <sched.h>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(__arm64__)
#include <arm_neon.h>
#endif

namespace nexus::common {

/**
 * @brief Adaptive spin-pause backoff mechanism for busy-wait polling loops.
 * 
 * Prevents instruction pipeline starvation and high-power thrashing:
 * 1. Fast spin: hardware pause instruction (_mm_pause or ARM yield)
 * 2. Medium spin: unrolled pause burst
 * 3. Slow spin: sched_yield() without triggering kernel sleep timers
 */
class AdaptiveSpinBackoff {
public:
    static constexpr uint32_t FAST_SPIN_LIMIT = 64;
    static constexpr uint32_t PAUSE_SPIN_LIMIT = 1024;

    NEXUS_FORCE_INLINE void pause() noexcept {
        if (spin_count_ < FAST_SPIN_LIMIT) {
            emit_pause();
        } else if (spin_count_ < PAUSE_SPIN_LIMIT) {
            emit_pause();
            emit_pause();
            emit_pause();
            emit_pause();
        } else {
            sched_yield();
        }
        ++spin_count_;
    }

    NEXUS_FORCE_INLINE void reset() noexcept {
        spin_count_ = 0;
    }

    [[nodiscard]] NEXUS_FORCE_INLINE uint32_t count() const noexcept {
        return spin_count_;
    }

private:
    NEXUS_FORCE_INLINE static void emit_pause() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_pause();
#elif defined(__aarch64__) || defined(__arm64__)
        asm volatile("yield" ::: "memory");
#else
        // Fallback no-op memory fence
        asm volatile("" ::: "memory");
#endif
    }

    uint32_t spin_count_{0};
};

} // namespace nexus::common
