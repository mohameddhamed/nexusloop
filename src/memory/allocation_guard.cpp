#include "nexus/memory/allocation_guard.hpp"
#include <cstdlib>
#include <cstdio>
#include <new>

namespace nexus::memory {

void AllocationGuard::record_violation() noexcept {
    violations_count_.fetch_add(1, std::memory_order_relaxed);
    if (abort_on_violation_) {
        std::fprintf(stderr, "\n[FATAL] NexusLoop AllocationGuard: Forbidden heap allocation detected in hot path!\n");
        std::fflush(stderr);
        std::abort();
    }
}

} // namespace nexus::memory

// Global new / delete overrides
#if !defined(__SANITIZE_THREAD__) && !(defined(__has_feature) && __has_feature(thread_sanitizer))
void* operator new(std::size_t size) {
    if (nexus::memory::AllocationGuard::is_trapping()) [[unlikely]] {
        nexus::memory::AllocationGuard::record_violation();
    }
    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void* operator new[](std::size_t size) {
    if (nexus::memory::AllocationGuard::is_trapping()) [[unlikely]] {
        nexus::memory::AllocationGuard::record_violation();
    }
    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (nexus::memory::AllocationGuard::is_trapping()) [[unlikely]] {
        nexus::memory::AllocationGuard::record_violation();
    }
    return std::malloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    if (nexus::memory::AllocationGuard::is_trapping()) [[unlikely]] {
        nexus::memory::AllocationGuard::record_violation();
    }
    return std::malloc(size);
}

void operator delete(void* ptr) noexcept {
    if (nexus::memory::AllocationGuard::is_trapping()) [[unlikely]] {
        nexus::memory::AllocationGuard::record_violation();
    }
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    if (nexus::memory::AllocationGuard::is_trapping()) [[unlikely]] {
        nexus::memory::AllocationGuard::record_violation();
    }
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    if (nexus::memory::AllocationGuard::is_trapping()) [[unlikely]] {
        nexus::memory::AllocationGuard::record_violation();
    }
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    if (nexus::memory::AllocationGuard::is_trapping()) [[unlikely]] {
        nexus::memory::AllocationGuard::record_violation();
    }
    std::free(ptr);
}
#endif
