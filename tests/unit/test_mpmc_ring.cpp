#include "nexus/ring/mpmc_ring.hpp"
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>
#include <atomic>

int main() {
    std::printf("--- RUNNING MPMC RING BUFFER TESTS ---\n");
    nexus::ring::MPMCRingBuffer<uint64_t, 2048> ring;

    constexpr int PRODUCERS = 4;
    constexpr int CONSUMERS = 4;
    constexpr uint64_t ITEMS_PER_PRODUCER = 500'000;
    constexpr uint64_t TOTAL_ITEMS = PRODUCERS * ITEMS_PER_PRODUCER;

    std::atomic<uint64_t> total_consumed{0};
    std::atomic<uint64_t> sum_consumed{0};

    std::vector<std::thread> prod_threads;
    for (int p = 0; p < PRODUCERS; ++p) {
        prod_threads.emplace_back([&]() {
            for (uint64_t i = 1; i <= ITEMS_PER_PRODUCER; ++i) {
                while (!ring.try_push(i)) {
                    // backoff
                }
            }
        });
    }

    std::vector<std::thread> cons_threads;
    for (int c = 0; c < CONSUMERS; ++c) {
        cons_threads.emplace_back([&]() {
            while (total_consumed.load(std::memory_order_relaxed) < TOTAL_ITEMS) {
                uint64_t val = 0;
                if (ring.try_pop(val)) {
                    sum_consumed.fetch_add(val, std::memory_order_relaxed);
                    total_consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& t : prod_threads) t.join();
    for (auto& t : cons_threads) t.join();

    assert(total_consumed.load() == TOTAL_ITEMS);
    uint64_t expected_sum_per_producer = (ITEMS_PER_PRODUCER * (ITEMS_PER_PRODUCER + 1)) / 2;
    uint64_t expected_total_sum = expected_sum_per_producer * PRODUCERS;
    assert(sum_consumed.load() == expected_total_sum);

    std::printf("[PASS] test_mpmc_concurrency (%llu items processed across %d producers and %d consumers)\n",
                (unsigned long long)TOTAL_ITEMS, PRODUCERS, CONSUMERS);
    return 0;
}
