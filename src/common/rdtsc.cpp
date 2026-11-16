#include "nexus/common/rdtsc.hpp"
#include <ctime>
#include <thread>
#include <chrono>

namespace nexus::common {

void TSCClock::calibrate() noexcept {
    if (calibrated_) {
        return;
    }

#if defined(__aarch64__) || defined(__arm64__)
    // On ARM64 (Apple Silicon / Linux aarch64), cntfrq_el0 holds the exact frequency in Hz
    uint64_t freq_hz;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq_hz));
    if (freq_hz > 0) {
        tsc_ghz_ = static_cast<double>(freq_hz) / 1e9;
        ns_per_cycle_ = 1e9 / static_cast<double>(freq_hz);
        calibrated_ = true;
        return;
    }
#endif

    // Fallback or x86_64 calibration against system monotonic clock
    struct timespec ts_start, ts_end;
#if defined(CLOCK_MONOTONIC_RAW)
    clockid_t clk_id = CLOCK_MONOTONIC_RAW;
#else
    clockid_t clk_id = CLOCK_MONOTONIC;
#endif

    clock_gettime(clk_id, &ts_start);
    uint64_t tsc_start = rdtsc_start();

    // Busy spin for ~50 milliseconds to get accurate calibration
    const uint64_t spin_ns = 50'000'000;
    while (true) {
        clock_gettime(clk_id, &ts_end);
        uint64_t elapsed_ns = static_cast<uint64_t>(ts_end.tv_sec - ts_start.tv_sec) * 1'000'000'000ULL +
                              static_cast<uint64_t>(ts_end.tv_nsec - ts_start.tv_nsec);
        if (elapsed_ns >= spin_ns) {
            uint64_t tsc_end = rdtsc_end();
            uint64_t elapsed_cycles = tsc_end - tsc_start;
            tsc_ghz_ = static_cast<double>(elapsed_cycles) / static_cast<double>(elapsed_ns);
            ns_per_cycle_ = static_cast<double>(elapsed_ns) / static_cast<double>(elapsed_cycles);
            calibrated_ = true;
            break;
        }
    }
}

double TSCClock::get_frequency_ghz() noexcept {
    if (!calibrated_) calibrate();
    return tsc_ghz_;
}

double TSCClock::cycles_to_ns(uint64_t cycles) noexcept {
    if (!calibrated_) calibrate();
    return static_cast<double>(cycles) * ns_per_cycle_;
}

uint64_t TSCClock::ns_to_cycles(double ns) noexcept {
    if (!calibrated_) calibrate();
    return static_cast<uint64_t>(ns * tsc_ghz_);
}

} // namespace nexus::common
