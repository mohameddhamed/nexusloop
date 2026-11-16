#include "nexus/ring/spsc_ring.hpp"
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>

void test_basic_push_pop() {
    nexus::ring::SPSCRingBuffer<int, 1024> ring;
    assert(ring.empty());
    assert(ring.size() == 0);

    for (int i = 0; i < 1000; ++i) {
        bool ok = ring.try_push(i);
        assert(ok);
    }
    assert(ring.size() == 1000);
    assert(!ring.empty());

    for (int i = 0; i < 1000; ++i) {
        int val = -1;
        bool ok = ring.try_pop(val);
        assert(ok);
        assert(val == i);
    }
    assert(ring.empty());
    std::printf("[PASS] test_basic_push_pop\n");
}

void test_zero_copy_slots() {
    struct MarketPacket {
        uint64_t seq;
        uint32_t price;
    };

    nexus::ring::SPSCRingBuffer<MarketPacket, 512> ring;

    // Producer claims slot in-place
    MarketPacket* slot = ring.claim_slot();
    assert(slot != nullptr);
    slot->seq = 42;
    slot->price = 10550;
    ring.publish_slot();

    // Consumer peeks in-place
    const MarketPacket* read_slot = ring.peek();
    assert(read_slot != nullptr);
    assert(read_slot->seq == 42);
    assert(read_slot->price == 10550);
    ring.release_slot();

    assert(ring.empty());
    std::printf("[PASS] test_zero_copy_slots\n");
}

void test_concurrent_spsc() {
    nexus::ring::SPSCRingBuffer<uint64_t, 4096> ring;
    constexpr uint64_t TOTAL_ITEMS = 5'000'000;

    std::thread producer([&]() {
        for (uint64_t i = 1; i <= TOTAL_ITEMS; ++i) {
            while (!ring.try_push(i)) {
                // spin
            }
        }
    });

    std::thread consumer([&]() {
        uint64_t expected = 1;
        while (expected <= TOTAL_ITEMS) {
            uint64_t val = 0;
            if (ring.try_pop(val)) {
                assert(val == expected);
                ++expected;
            }
        }
    });

    producer.join();
    consumer.join();
    assert(ring.empty());
    std::printf("[PASS] test_concurrent_spsc (%llu items verified)\n", (unsigned long long)TOTAL_ITEMS);
}

int main() {
    std::printf("--- RUNNING SPSC RING BUFFER TESTS ---\n");
    test_basic_push_pop();
    test_zero_copy_slots();
    test_concurrent_spsc();
    std::printf("All SPSC tests passed successfully!\n");
    return 0;
}
