#include "nexus/ipc/shm_gateway.hpp"
#include "nexus/common/rdtsc.hpp"
#include "nexus/common/cpu.hpp"
#include <cstdio>
#include <vector>
#include <thread>
#include <algorithm>
#include <cassert>

struct SHMPacket {
    uint64_t tsc_timestamp;
    uint64_t id;
};

int main() {
    constexpr uint64_t NUM_SAMPLES = 200'000;
    constexpr uint64_t WARMUP = 10'000;

    std::printf("=================================================================\n");
    std::printf("  NEXUSLOOP TIER 2 BENCHMARK: SHARED MEMORY (SHM) IPC GATEWAY     \n");
    std::printf("  Mechanism: POSIX shm_open + Cache-Line Padded Ring Cursors      \n");
    std::printf("  Target: P50 ~280 ns | P99 < 650 ns\n");
    std::printf("=================================================================\n");
    std::fflush(stdout);

    nexus::common::TSCClock::calibrate();

    const char* shm_ping_name = "/nexus_shm_ping";
    const char* shm_pong_name = "/nexus_shm_pong";

    // Producer creates SHM segments first
    nexus::ipc::SHMGateway<SHMPacket, 2048> shm_ping(shm_ping_name, nexus::ipc::SHMGateway<SHMPacket, 2048>::Mode::Producer);
    nexus::ipc::SHMGateway<SHMPacket, 2048> shm_pong(shm_pong_name, nexus::ipc::SHMGateway<SHMPacket, 2048>::Mode::Producer);

    assert(shm_ping.is_valid());
    assert(shm_pong.is_valid());

    std::atomic<bool> producer_created{false};
    std::atomic<bool> worker_ready{false};
    std::atomic<bool> done_flag{false};

    // Subprocess / consumer thread emulating external risk/drop-copy daemon
    std::thread worker([&]() {
        nexus::common::affinitize_current_thread(3, 99);

        // Wait until producer has finished initializing the shared memory mapping
        while (!producer_created.load(std::memory_order_acquire)) {}

        nexus::ipc::SHMGateway<SHMPacket, 2048> client_ping(shm_ping_name, nexus::ipc::SHMGateway<SHMPacket, 2048>::Mode::Consumer);
        nexus::ipc::SHMGateway<SHMPacket, 2048> client_pong(shm_pong_name, nexus::ipc::SHMGateway<SHMPacket, 2048>::Mode::Consumer);

        assert(client_ping.is_valid());
        assert(client_pong.is_valid());

        // Signal back that client is connected
        worker_ready.store(true, std::memory_order_release);

        SHMPacket pkt;
        while (!done_flag.load(std::memory_order_relaxed)) {
            if (client_ping.try_read(pkt)) {
                while (!client_pong.try_write(pkt)) {
                    // backoff
                }
            }
        }
    });

    nexus::common::affinitize_current_thread(0, 99);
    producer_created.store(true, std::memory_order_release);

    // Wait until worker is fully initialized and attached
    while (!worker_ready.load(std::memory_order_acquire)) {}

    std::printf("Warming up shared memory ring buffers (%llu cycles)...\n", (unsigned long long)WARMUP);
    std::fflush(stdout);

    // Warmup
    for (uint64_t i = 0; i < WARMUP; ++i) {
        SHMPacket pkt{nexus::common::TSCClock::rdtsc_start(), i};
        while (!shm_ping.try_write(pkt)) {}
        SHMPacket reply;
        while (!shm_pong.try_read(reply)) {}
    }

    std::printf("Collecting %llu round-trip SHM IPC samples...\n", (unsigned long long)NUM_SAMPLES);
    std::fflush(stdout);

    std::vector<double> samples_ns;
    samples_ns.reserve(NUM_SAMPLES);

    for (uint64_t i = 0; i < NUM_SAMPLES; ++i) {
        uint64_t t0 = nexus::common::TSCClock::rdtsc_start();
        SHMPacket pkt{t0, i};

        while (!shm_ping.try_write(pkt)) {}
        SHMPacket reply;
        while (!shm_pong.try_read(reply)) {}

        uint64_t t1 = nexus::common::TSCClock::rdtsc_end();
        uint64_t rtt = (t1 > t0) ? (t1 - t0) : 0;
        double one_way_ns = nexus::common::TSCClock::cycles_to_ns(rtt) / 2.0;
        samples_ns.push_back(one_way_ns);
    }

    done_flag.store(true, std::memory_order_release);
    worker.join();

    std::sort(samples_ns.begin(), samples_ns.end());

    double min_ns = samples_ns.front();
    double p50_ns = samples_ns[static_cast<std::size_t>(NUM_SAMPLES * 0.50)];
    double p90_ns = samples_ns[static_cast<std::size_t>(NUM_SAMPLES * 0.90)];
    double p99_ns = samples_ns[static_cast<std::size_t>(NUM_SAMPLES * 0.99)];
    double p999_ns = samples_ns[static_cast<std::size_t>(NUM_SAMPLES * 0.999)];
    double max_ns = samples_ns.back();

    std::printf("\n================ LATENCY DISTRIBUTION (SHM IPC ONE-WAY) ================\n");
    std::printf("  Min Latency:       %7.2f ns\n", min_ns);
    std::printf("  P50  (Median):     %7.2f ns  [Target: ~280 ns]\n", p50_ns);
    std::printf("  P90:               %7.2f ns\n", p90_ns);
    std::printf("  P99:               %7.2f ns  [Target: < 650 ns]\n", p99_ns);
    std::printf("  P99.9:             %7.2f ns\n", p999_ns);
    std::printf("  Max Latency:       %7.2f ns\n", max_ns);
    std::printf("=========================================================================\n\n");
    std::fflush(stdout);

    return 0;
}
