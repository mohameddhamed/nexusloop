#include "nexus/protocol/itch50/itch50_parser.hpp"
#include <cstdio>
#include <vector>
#include <chrono>
#include <cstring>

void prepare_sample_packet(uint8_t* buf, uint64_t ref) {
    buf[0] = 'A';
    uint16_t loc = __builtin_bswap16(42);
    std::memcpy(buf + 1, &loc, 2);
    uint16_t trk = __builtin_bswap16(100);
    std::memcpy(buf + 3, &trk, 2);
    uint64_t ts = __builtin_bswap64(987654321000ULL);
    std::memcpy(buf + 5, reinterpret_cast<const char*>(&ts) + 2, 6);
    uint64_t ref_be = __builtin_bswap64(ref);
    std::memcpy(buf + 11, &ref_be, 8);
    buf[19] = 'B';
    uint32_t sh_be = __builtin_bswap32(1000);
    std::memcpy(buf + 20, &sh_be, 4);
    std::memcpy(buf + 24, "MSFT    ", 8);
    uint32_t px_be = __builtin_bswap32(4155000);
    std::memcpy(buf + 32, &px_be, 4);
}

int main() {
    constexpr uint64_t TOTAL_MESSAGES = 50'000'000;

    std::printf("=================================================================\n");
    std::printf("  NEXUSLOOP BENCHMARK: ITCH 5.0 SIMD vs SCALAR PARSING ENGINE     \n");
    std::printf("  Batch Size: %llu Messages                                       \n", (unsigned long long)TOTAL_MESSAGES);
    std::printf("  Target: > 35.0 Million Messages/sec per Core                    \n");
    std::printf("=================================================================\n");
    std::fflush(stdout);

    alignas(64) uint8_t wire_packet[64]{0};
    prepare_sample_packet(wire_packet, 12345678ULL);

    nexus::protocol::itch50::ITCH50Parser parser;
    nexus::protocol::itch50::ParsedAddOrder msg{};

    // --- BENCHMARK 1: SIMD VECTOR PARSER ---
    std::printf("Running SIMD Vectorized Parser...\n");
    std::fflush(stdout);
    auto start_simd = std::chrono::high_resolution_clock::now();

    uint64_t checksum_simd = 0;
    for (uint64_t i = 0; i < TOTAL_MESSAGES; ++i) {
        // Vary wire input slightly to prevent static loop invariant hoisting
        std::memcpy(wire_packet + 11, &i, sizeof(i));
        bool ok = parser.parse_add_order(wire_packet, msg);
        checksum_simd += msg.price + msg.order_reference_number + ok;
        asm volatile("" : "+r"(checksum_simd) : "r"(&msg) : "memory");
    }

    auto end_simd = std::chrono::high_resolution_clock::now();
    double simd_sec = std::chrono::duration<double>(end_simd - start_simd).count();
    if (simd_sec <= 0.0) simd_sec = 1e-9;
    double simd_mps = (static_cast<double>(TOTAL_MESSAGES) / simd_sec) / 1e6;
    double simd_ns_per_msg = (simd_sec / static_cast<double>(TOTAL_MESSAGES)) * 1e9;

    // --- BENCHMARK 2: SCALAR PARSER ---
    std::printf("Running Scalar Fallback Parser...\n");
    std::fflush(stdout);
    auto start_scalar = std::chrono::high_resolution_clock::now();

    uint64_t checksum_scalar = 0;
    for (uint64_t i = 0; i < TOTAL_MESSAGES; ++i) {
        std::memcpy(wire_packet + 11, &i, sizeof(i));
        bool ok = nexus::protocol::itch50::ITCH50Parser::parse_add_order_scalar(wire_packet, msg);
        checksum_scalar += msg.price + msg.order_reference_number + ok;
        asm volatile("" : "+r"(checksum_scalar) : "r"(&msg) : "memory");
    }

    auto end_scalar = std::chrono::high_resolution_clock::now();
    double scalar_sec = std::chrono::duration<double>(end_scalar - start_scalar).count();
    if (scalar_sec <= 0.0) scalar_sec = 1e-9;
    double scalar_mps = (static_cast<double>(TOTAL_MESSAGES) / scalar_sec) / 1e6;
    double scalar_ns_per_msg = (scalar_sec / static_cast<double>(TOTAL_MESSAGES)) * 1e9;

    std::printf("\n======================== PERFORMANCE COMPARISON ========================\n");
    std::printf("  SIMD Vector Parser:   %6.2f M msgs/sec | %5.2f ns/msg (checksum: %llu)\n",
                simd_mps, simd_ns_per_msg, (unsigned long long)checksum_simd);
    std::printf("  Scalar Parser:        %6.2f M msgs/sec | %5.2f ns/msg (checksum: %llu)\n",
                scalar_mps, scalar_ns_per_msg, (unsigned long long)checksum_scalar);
    std::printf("  Speedup Factor:       %6.2fx\n", simd_mps / scalar_mps);
    std::printf("========================================================================\n\n");
    std::fflush(stdout);

    return 0;
}
