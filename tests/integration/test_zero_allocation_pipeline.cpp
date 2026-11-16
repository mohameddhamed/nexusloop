#include "nexus/memory/allocation_guard.hpp"
#include "nexus/memory/huge_page_allocator.hpp"
#include "nexus/ring/spsc_ring.hpp"
#include "nexus/protocol/itch50/itch50_parser.hpp"
#include "nexus/book/order_book.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <chrono>

struct PacketSlot {
    uint8_t raw[64];
};

void build_test_packet(uint8_t* buf, uint64_t ref, uint32_t price, uint32_t qty) {
    buf[0] = 'A';
    uint16_t loc = __builtin_bswap16(1);
    std::memcpy(buf + 1, &loc, 2);
    uint16_t trk = __builtin_bswap16(1);
    std::memcpy(buf + 3, &trk, 2);
    uint64_t ts = __builtin_bswap64(500000000ULL);
    std::memcpy(buf + 5, reinterpret_cast<const char*>(&ts) + 2, 6);
    uint64_t ref_be = __builtin_bswap64(ref);
    std::memcpy(buf + 11, &ref_be, 8);
    buf[19] = 'B';
    uint32_t sh_be = __builtin_bswap32(qty);
    std::memcpy(buf + 20, &sh_be, 4);
    std::memcpy(buf + 24, "NVDA    ", 8);
    uint32_t px_be = __builtin_bswap32(price);
    std::memcpy(buf + 32, &px_be, 4);
}

int main(int argc, char** argv) {
    uint64_t iterations = 10'000'000;
    if (argc > 1) {
        iterations = std::stoull(argv[1]);
    }

    std::printf("=================================================================\n");
    std::printf("  NEXUSLOOP ZERO-ALLOCATION END-TO-END PIPELINE INTEGRATION TEST  \n");
    std::printf("  Target: %llu Packets Processed through SPSC -> SIMD -> Book\n", (unsigned long long)iterations);
    std::printf("=================================================================\n");

    // Pre-allocation phase (Allocations permitted only before the trap is engaged)
    nexus::memory::HugePageArena arena(8 * 1024 * 1024); // 8MB pre-allocated
    auto* ring = arena.allocate<nexus::ring::SPSCRingBuffer<PacketSlot, 16384>>();
    new (ring) nexus::ring::SPSCRingBuffer<PacketSlot, 16384>();

    nexus::protocol::itch50::ITCH50Parser parser;
    nexus::book::OrderBook<32, 16384> book;

    // Reset stats before engaging trap
    nexus::memory::AllocationGuard::reset_stats();

    auto start_time = std::chrono::steady_clock::now();

    {
        // ENGAGE ZERO-ALLOCATION ENFORCEMENT TRAP
        // Any single call to malloc/operator new here will trigger an immediate abort!
        nexus::memory::ScopedAllocationTrap trap(true);

        nexus::protocol::itch50::ParsedAddOrder add_msg{};
        PacketSlot temp_packet{};
        build_test_packet(temp_packet.raw, 1001, 1205000, 100);

        for (uint64_t i = 0; i < iterations; ++i) {
            // 1. Zero-copy claim & publish on SPSC ring
            PacketSlot* slot = ring->claim_slot();
            while (!slot) {
                // Drain if ring gets full in single-thread test loop
                PacketSlot drained;
                [[maybe_unused]] bool popped = ring->try_pop(drained);
                slot = ring->claim_slot();
            }

            *slot = temp_packet;
            // Mutate order ref in place safely using memcpy to respect packed wire alignment
            uint64_t ref_val = __builtin_bswap64(i + 1);
            std::memcpy(slot->raw + 11, &ref_val, sizeof(ref_val));
            ring->publish_slot();

            // 2. Consume from SPSC ring
            const PacketSlot* incoming = ring->peek();
            if (incoming) {
                // 3. SIMD AVX2/NEON ITCH 5.0 Deserialization
                bool ok = parser.parse_add_order(incoming->raw, add_msg);
                if (ok) [[likely]] {
                    // 4. Ingest into Order Book and compute BBO
                    book.on_itch_add(add_msg);
                }
                ring->release_slot();
            }
        }

        // Validate that BBO is populated
        const auto& bbo = book.get_bbo();
        assert(bbo.bid_price == 1205000);
    } // TRAP DISENGAGED

    auto end_time = std::chrono::steady_clock::now();
    double elapsed_sec = std::chrono::duration<double>(end_time - start_time).count();
    double msgs_per_sec = static_cast<double>(iterations) / elapsed_sec;

    uint64_t violations = nexus::memory::AllocationGuard::get_violations_count();

    std::printf("\n--- VERIFICATION RESULTS ---\n");
    std::printf("  Total Packets Processed: %llu\n", (unsigned long long)iterations);
    std::printf("  Elapsed Time:            %.4f seconds\n", elapsed_sec);
    std::printf("  Throughput:              %.2f million packets/sec\n", msgs_per_sec / 1e6);
    std::printf("  Dynamic Allocations:     %llu [ASSERTION: 0]\n", (unsigned long long)violations);
    std::printf("-----------------------------------------------------------------\n");

    assert(violations == 0);
    std::printf("[SUCCESS] ZERO HEAP ALLOCATIONS ACROSS PIPELINE EXECUTION!\n\n");
    return 0;
}
