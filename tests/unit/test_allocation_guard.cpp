#include "nexus/memory/allocation_guard.hpp"
#include <cassert>
#include <cstdio>
#include <vector>

void test_trap_disabled() {
    nexus::memory::AllocationGuard::reset_stats();
    assert(nexus::memory::AllocationGuard::get_violations_count() == 0);

    // Dynamic heap allocation while trap disabled
    int* p = new int(42);
    assert(*p == 42);
    delete p;

    assert(nexus::memory::AllocationGuard::get_violations_count() == 0);
    std::printf("[PASS] test_trap_disabled\n");
}

void test_trap_detection() {
    nexus::memory::AllocationGuard::reset_stats();
    nexus::memory::AllocationGuard::set_abort_on_violation(false); // Don't abort, just record

    {
        nexus::memory::ScopedAllocationTrap trap(false);
        assert(nexus::memory::AllocationGuard::is_trapping());

        // Forbidden heap allocation inside guarded scope
        char* volatile ptr = new char[128];
        assert(ptr != nullptr);
        delete[] ptr;

        assert(nexus::memory::AllocationGuard::get_violations_count() >= 1);
    }

    assert(!nexus::memory::AllocationGuard::is_trapping());
    std::printf("[PASS] test_trap_detection (violations caught: %llu)\n",
                (unsigned long long)nexus::memory::AllocationGuard::get_violations_count());
}

int main() {
    std::printf("--- RUNNING ALLOCATION GUARD UNIT TESTS ---\n");
    test_trap_disabled();
    test_trap_detection();
    std::printf("All AllocationGuard unit tests passed successfully!\n");
    return 0;
}
