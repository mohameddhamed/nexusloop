#include "nexus/net/epoll_engine.hpp"
#include "nexus/common/rdtsc.hpp"
#include <cstdio>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>

struct NetPacket {
    uint64_t tsc_timestamp;
    uint32_t payload_id;
};

int main() {
    constexpr uint64_t SAMPLES = 50'000;

    std::printf("=================================================================\n");
    std::printf("  NEXUSLOOP TIER 3 BENCHMARK: LINUX SOCKET ROUND-TRIP (io_uring) \n");
    std::printf("  Mechanism: Socket Busy-Poll / Non-blocking loopback             \n");
    std::printf("  Target: P50 ~3.8 µs | P99 < 8.0 µs                             \n");
    std::printf("=================================================================\n");

    nexus::common::TSCClock::calibrate();

    int fds[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        std::perror("socketpair");
        return 1;
    }

    nexus::net::EpollEngine engine;
    engine.add_socket(fds[0]);
    engine.add_socket(fds[1]);

    uint8_t buffer[64];
    nexus::net::PacketSlot slot{buffer, sizeof(buffer), 0, -1};
    std::span<nexus::net::PacketSlot> slot_span(&slot, 1);

    std::vector<double> rtt_us;
    rtt_us.reserve(SAMPLES);

    // Warmup
    for (int i = 0; i < 1000; ++i) {
        NetPacket pkt{nexus::common::TSCClock::rdtsc_start(), static_cast<uint32_t>(i)};
        engine.send(fds[0], reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
        while (engine.poll(slot_span) == 0) {}
    }

    // Measure
    for (uint64_t i = 0; i < SAMPLES; ++i) {
        uint64_t t0 = nexus::common::TSCClock::rdtsc_start();
        NetPacket pkt{t0, static_cast<uint32_t>(i)};

        engine.send(fds[0], reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
        while (engine.poll(slot_span) == 0) {}

        uint64_t t1 = nexus::common::TSCClock::rdtsc_end();
        uint64_t cycles = (t1 > t0) ? (t1 - t0) : 0;
        double us = nexus::common::TSCClock::cycles_to_ns(cycles) / 1000.0;
        rtt_us.push_back(us);
    }

    ::close(fds[0]);
    ::close(fds[1]);

    std::sort(rtt_us.begin(), rtt_us.end());

    double min_us = rtt_us.front();
    double p50_us = rtt_us[static_cast<std::size_t>(SAMPLES * 0.50)];
    double p90_us = rtt_us[static_cast<std::size_t>(SAMPLES * 0.90)];
    double p99_us = rtt_us[static_cast<std::size_t>(SAMPLES * 0.99)];
    double max_us = rtt_us.back();

    std::printf("\n================ LATENCY DISTRIBUTION (SOCKET RTT) ================\n");
    std::printf("  Min Latency:       %7.2f µs\n", min_us);
    std::printf("  P50  (Median):     %7.2f µs  [Target: ~3.8 µs]\n", p50_us);
    std::printf("  P90:               %7.2f µs\n", p90_us);
    std::printf("  P99:               %7.2f µs  [Target: < 8.0 µs]\n", p99_us);
    std::printf("  Max Latency:       %7.2f µs\n", max_us);
    std::printf("===================================================================\n\n");

    return 0;
}
