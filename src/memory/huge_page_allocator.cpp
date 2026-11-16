#include "nexus/memory/huge_page_allocator.hpp"
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <utility>
#include <algorithm>

namespace nexus::memory {

HugePageArena::HugePageArena(std::size_t capacity, std::size_t page_size)
    : page_size_(page_size) {
    // Round capacity up to the nearest multiple of page_size_
    capacity_ = (capacity + page_size_ - 1) & ~(page_size_ - 1);

#if defined(__linux__) && defined(MAP_HUGETLB)
    buffer_ = ::mmap(
        nullptr,
        capacity_,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
        -1,
        0
    );

    if (buffer_ != MAP_FAILED) {
        is_huge_page_ = true;
    } else {
        // Fallback to standard anonymous mmap if huge pages are exhausted or unconfigured
        buffer_ = ::mmap(
            nullptr,
            capacity_,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0
        );
        is_huge_page_ = false;
    }
#elif defined(__APPLE__)
    // Darwin allows VM_FLAGS_SUPERPAGE_SIZE_2MB on certain Mach architectures, fallback to standard mmap
    buffer_ = ::mmap(
        nullptr,
        capacity_,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON,
        -1,
        0
    );
    is_huge_page_ = false;
#else
    buffer_ = ::mmap(
        nullptr,
        capacity_,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );
    is_huge_page_ = false;
#endif

    if (buffer_ != MAP_FAILED) {
        // Lock pages in RAM to prevent swapping/page-faults during runtime
        ::mlock(buffer_, capacity_);

        // Pre-fault memory to ensure all pages are resident in physical RAM
        std::memset(buffer_, 0, capacity_);
    } else {
        buffer_ = nullptr;
        capacity_ = 0;
    }
}

HugePageArena::~HugePageArena() {
    if (buffer_ && buffer_ != MAP_FAILED) {
        ::munlock(buffer_, capacity_);
        ::munmap(buffer_, capacity_);
        buffer_ = nullptr;
    }
}

HugePageArena::HugePageArena(HugePageArena&& other) noexcept
    : buffer_(other.buffer_),
      capacity_(other.capacity_),
      offset_(other.offset_),
      page_size_(other.page_size_),
      is_huge_page_(other.is_huge_page_) {
    other.buffer_ = nullptr;
    other.capacity_ = 0;
    other.offset_ = 0;
}

HugePageArena& HugePageArena::operator=(HugePageArena&& other) noexcept {
    if (this != &other) {
        if (buffer_ && buffer_ != MAP_FAILED) {
            ::munlock(buffer_, capacity_);
            ::munmap(buffer_, capacity_);
        }
        buffer_ = other.buffer_;
        capacity_ = other.capacity_;
        offset_ = other.offset_;
        page_size_ = other.page_size_;
        is_huge_page_ = other.is_huge_page_;

        other.buffer_ = nullptr;
        other.capacity_ = 0;
        other.offset_ = 0;
    }
    return *this;
}

void* HugePageArena::allocate_raw(std::size_t bytes, std::size_t alignment) noexcept {
    if (!buffer_ || bytes == 0) return nullptr;

    uintptr_t current_addr = reinterpret_cast<uintptr_t>(buffer_) + offset_;
    uintptr_t aligned_addr = (current_addr + alignment - 1) & ~(alignment - 1);
    std::size_t new_offset = (aligned_addr - reinterpret_cast<uintptr_t>(buffer_)) + bytes;

    if (new_offset > capacity_) {
        return nullptr; // Out of memory in pre-allocated arena
    }

    offset_ = new_offset;
    return reinterpret_cast<void*>(aligned_addr);
}

} // namespace nexus::memory
