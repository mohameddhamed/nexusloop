#pragma once

#include "nexus/common/compiler.hpp"
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#elif defined(__aarch64__) || defined(__arm64__)
#include <arm_neon.h>
#endif

namespace nexus::common {

class TSCClock {
public:
    // Calibrate TSC against monotonic system clock
    static void calibrate() noexcept;

    [[nodiscard]] static double get_frequency_ghz() noexcept;
    [[nodiscard]] static double cycles_to_ns(uint64_t cycles) noexcept;
    [[nodiscard]] static uint64_t ns_to_cycles(double ns) noexcept;

    // High-resolution serialized timestamp start
    [[nodiscard]] NEXUS_FORCE_INLINE static uint64_t rdtsc_start() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_lfence();
        uint64_t tsc = __rdtsc();
        _mm_lfence();
        return tsc;
#elif defined(__aarch64__) || defined(__arm64__)
        uint64_t val;
        asm volatile(
            "isb\n\t"
            "mrs %0, cntvct_el0\n\t"
            : "=r"(val) :: "memory"
        );
        return val;
#else
        return 0;
#endif
    }

    // High-resolution serialized timestamp end
    [[nodiscard]] NEXUS_FORCE_INLINE static uint64_t rdtsc_end() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        unsigned int aux;
        uint64_t tsc = __rdtscp(&aux);
        _mm_lfence();
        return tsc;
#elif defined(__aarch64__) || defined(__arm64__)
        uint64_t val;
        asm volatile(
            "isb\n\t"
            "mrs %0, cntvct_el0\n\t"
            : "=r"(val) :: "memory"
        );
        return val;
#else
        return 0;
#endif
    }

private:
    static inline double tsc_ghz_{3.0};
    static inline double ns_per_cycle_{0.333333};
    static inline bool calibrated_{false};
};

} // namespace nexus::common
