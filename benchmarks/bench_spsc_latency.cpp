#include "nexus/ring/spsc_ring.hpp"
#include "nexus/common/rdtsc.hpp"
#include "nexus/common/cpu.hpp"
#include <cstdio>
#include <vector>
#include <thread>
#include <algorithm>
#include <numeric>

struct PingPongMessage {
    uint64_t tsc_timestamp;
    uint64_t sequence;
};

int main() {
    constexpr uint64_t NUM_SAMPLES = 2'000'000;
    constexpr uint64_t WARMUP = 100'000;

    std::printf("=================================================================\n");
    std::printf("  NEXUSLOOP TIER 1 BENCHMARK: CORE-TO-CORE INTRA-PROCESS LATENCY  \n");
    std::printf("  Measurement: Hardware Invariant RDTSC / LFENCE Serialized Cycles \n");
    std::printf("  Target: P50 ~38 ns | P99 < 90 ns\n");
    std::printf("=================================================================\n");

    nexus::common::TSCClock::calibrate();
    std::printf("Calibrated Hardware TSC Frequency: %.3f GHz (%.4f ns/cycle)\n\n",
                nexus::common::TSCClock::get_frequency_ghz(),
                nexus::common::TSCClock::cycles_to_ns(1));

    // Two SPSC rings for bidirectional core-to-core ping-pong
    static nexus::ring::SPSCRingBuffer<PingPongMessage, 1024> ring_ping;
    static nexus::ring::SPSCRingBuffer<PingPongMessage, 1024> ring_pong;

    std::vector<double> rtt_samples_ns;
    rtt_samples_ns.reserve(NUM_SAMPLES);

    std::atomic<bool> start_flag{false};
    std::atomic<bool> done_flag{false};

    // Consumer / Reflector thread pinned to Core 2
    std::thread reflector([&]() {
        nexus::common::affinitize_current_thread(2, 99);
        while (!start_flag.load(std::memory_order_acquire)) {}

        PingPongMessage msg;
        while (!done_flag.load(std::memory_order_relaxed)) {
            if (ring_ping.try_pop(msg)) {
                while (!ring_pong.try_push(msg)) {
                    // spin
                }
            }
        }
    });

    // Producer / Initiator thread pinned to Core 1
    nexus::common::affinitize_current_thread(1, 99);

    std::printf("Warming up cache lines and instruction TLBs (%llu cycles)...\n", (unsigned long long)WARMUP);
    start_flag.store(true, std::memory_order_release);

    // Warmup phase
    for (uint64_t i = 0; i < WARMUP; ++i) {
        PingPongMessage msg{nexus::common::TSCClock::rdtsc_start(), i};
        while (!ring_ping.try_push(msg)) {}
        PingPongMessage reply;
        while (!ring_pong.try_pop(reply)) {}
    }

    std::printf("Collecting %llu round-trip latency samples...\n", (unsigned long long)NUM_SAMPLES);

    for (uint64_t i = 0; i < NUM_SAMPLES; ++i) {
        uint64_t t0 = nexus::common::TSCClock::rdtsc_start();
        PingPongMessage msg{t0, i};
        
        while (!ring_ping.try_push(msg)) {}

        PingPongMessage reply;
        while (!ring_pong.try_pop(reply)) {}

        uint64_t t1 = nexus::common::TSCClock::rdtsc_end();
        uint64_t rtt_cycles = (t1 > t0) ? (t1 - t0) : 0;
        
        // One-way handoff latency = RTT / 2
        double one_way_ns = nexus::common::TSCClock::cycles_to_ns(rtt_cycles) / 2.0;
        rtt_samples_ns.push_back(one_way_ns);
    }

    done_flag.store(true, std::memory_order_release);
    reflector.join();

    // Sort to compute percentile distribution
    std::sort(rtt_samples_ns.begin(), rtt_samples_ns.end());

    double min_ns = rtt_samples_ns.front();
    double p50_ns = rtt_samples_ns[static_cast<std::size_t>(NUM_SAMPLES * 0.50)];
    double p90_ns = rtt_samples_ns[static_cast<std::size_t>(NUM_SAMPLES * 0.90)];
    double p99_ns = rtt_samples_ns[static_cast<std::size_t>(NUM_SAMPLES * 0.99)];
    double p999_ns = rtt_samples_ns[static_cast<std::size_t>(NUM_SAMPLES * 0.999)];
    double max_ns = rtt_samples_ns.back();

    std::printf("\n================ LATENCY DISTRIBUTION (ONE-WAY HANDOFF) ================\n");
    std::printf("  Min Latency:       %7.2f ns\n", min_ns);
    std::printf("  P50  (Median):     %7.2f ns  [Target: ~38 ns]\n", p50_ns);
    std::printf("  P90:               %7.2f ns\n", p90_ns);
    std::printf("  P99:               %7.2f ns  [Target: < 90 ns]\n", p99_ns);
    std::printf("  P99.9:             %7.2f ns\n", p999_ns);
    std::printf("  Max Latency:       %7.2f ns\n", max_ns);
    std::printf("========================================================================\n\n");

    return 0;
}
