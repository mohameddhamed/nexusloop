#pragma once

#include "nexus/common/compiler.hpp"
#include <cstddef>
#include <cstdint>
#include <atomic>

namespace nexus::memory {

class AllocationGuard {
public:
    static void enable_trap() noexcept {
        trap_active_ = true;
    }

    static void disable_trap() noexcept {
        trap_active_ = false;
    }

    [[nodiscard]] static bool is_trapping() noexcept {
        return trap_active_;
    }

    static void record_violation() noexcept;

    static void reset_stats() noexcept {
        violations_count_.store(0, std::memory_order_relaxed);
    }

    [[nodiscard]] static uint64_t get_violations_count() noexcept {
        return violations_count_.load(std::memory_order_relaxed);
    }

    static void set_abort_on_violation(bool should_abort) noexcept {
        abort_on_violation_ = should_abort;
    }

    [[nodiscard]] static bool get_abort_on_violation() noexcept {
        return abort_on_violation_;
    }

private:
    static inline thread_local bool trap_active_{false};
    static inline std::atomic<uint64_t> violations_count_{0};
    static inline bool abort_on_violation_{false};
};

/**
 * @brief RAII Guard for zero-allocation scope enforcement.
 */
class ScopedAllocationTrap {
public:
    explicit ScopedAllocationTrap(bool abort_on_violation = false) {
        prev_abort_ = AllocationGuard::get_abort_on_violation();
        AllocationGuard::set_abort_on_violation(abort_on_violation);
        AllocationGuard::enable_trap();
    }

    ~ScopedAllocationTrap() {
        AllocationGuard::disable_trap();
        AllocationGuard::set_abort_on_violation(prev_abort_);
    }

    ScopedAllocationTrap(const ScopedAllocationTrap&) = delete;
    ScopedAllocationTrap& operator=(const ScopedAllocationTrap&) = delete;

private:
    bool prev_abort_{false};
};

} // namespace nexus::memory
