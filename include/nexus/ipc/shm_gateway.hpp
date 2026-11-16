#pragma once

#include "nexus/common/compiler.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <span>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <new>
#include <cstdio>

namespace nexus::ipc {

struct alignas(NEXUS_CACHE_LINE_SIZE) SHMHeader {
    static constexpr uint64_t SHM_MAGIC = 0x4E4558555353484DLL; // "NEXUSSHM"

    uint64_t magic;
    uint32_t version;
    uint32_t slot_capacity;
    uint32_t slot_size;
    uint32_t pad0;

    // --- PRODUCER CACHE LINE ---
    alignas(NEXUS_CACHE_LINE_SIZE) std::atomic<uint64_t> producer_head{0};
    uint8_t pad_prod[NEXUS_CACHE_LINE_SIZE - sizeof(std::atomic<uint64_t>)];

    // --- CONSUMER CACHE LINE ---
    alignas(NEXUS_CACHE_LINE_SIZE) std::atomic<uint64_t> consumer_tail{0};
    uint8_t pad_cons[NEXUS_CACHE_LINE_SIZE - sizeof(std::atomic<uint64_t>)];
};

template <typename T, std::size_t Capacity = 4096>
    requires (Capacity > 0 && ((Capacity & (Capacity - 1)) == 0))
class SHMGateway {
public:
    static constexpr std::size_t MASK = Capacity - 1;

    enum class Mode {
        Producer,
        Consumer
    };

    SHMGateway(std::string_view shm_name, Mode mode)
        : name_(shm_name), mode_(mode) {
        init();
    }

    ~SHMGateway() {
        cleanup();
    }

    SHMGateway(const SHMGateway&) = delete;
    SHMGateway& operator=(const SHMGateway&) = delete;

    [[nodiscard]] bool is_valid() const noexcept { return base_ptr_ != nullptr; }

    /**
     * @brief Producer writes data into SHM ring buffer without locks.
     */
    [[nodiscard]] bool try_write(const T& item) noexcept {
        if (!header_) return false;
        uint64_t head = header_->producer_head.load(std::memory_order_relaxed);
        uint64_t tail = header_->consumer_tail.load(std::memory_order_acquire);

        if (head - tail >= Capacity) {
            return false; // SHM Ring full
        }

        slots_[head & MASK] = item;
        header_->producer_head.store(head + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Consumer reads data from SHM ring buffer without locks.
     */
    [[nodiscard]] bool try_read(T& item) noexcept {
        if (!header_) return false;
        uint64_t tail = header_->consumer_tail.load(std::memory_order_relaxed);
        uint64_t head = header_->producer_head.load(std::memory_order_acquire);

        if (tail == head) {
            return false; // SHM Ring empty
        }

        item = slots_[tail & MASK];
        header_->consumer_tail.store(tail + 1, std::memory_order_release);
        return true;
    }

private:
    std::string name_;
    Mode mode_;
    int fd_{-1};
    void* base_ptr_{nullptr};
    std::size_t total_size_{0};
    SHMHeader* header_{nullptr};
    T* slots_{nullptr};
    bool is_file_backed_{false};

    std::string get_fallback_path() const {
        std::string clean_name = name_;
        if (!clean_name.empty() && clean_name.front() == '/') {
            clean_name.erase(0, 1);
        }
        return "/tmp/" + clean_name;
    }

    void init() {
        total_size_ = sizeof(SHMHeader) + sizeof(T) * Capacity;
        // Align total size to 4KB page boundary
        total_size_ = (total_size_ + 4095) & ~4095;

        if (mode_ == Mode::Producer) {
            ::shm_unlink(name_.c_str());
            fd_ = ::shm_open(name_.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
            if (fd_ < 0) {
                // If shm_open is restricted by environment sandbox, fallback to memory-mapped /tmp file
                std::string fallback = get_fallback_path();
                ::unlink(fallback.c_str());
                fd_ = ::open(fallback.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
                if (fd_ >= 0) {
                    is_file_backed_ = true;
                } else {
                    return;
                }
            }

            if (::ftruncate(fd_, total_size_) != 0) {
                ::close(fd_);
                fd_ = -1;
                return;
            }
        } else {
            fd_ = ::shm_open(name_.c_str(), O_RDWR, 0666);
            if (fd_ < 0) {
                std::string fallback = get_fallback_path();
                fd_ = ::open(fallback.c_str(), O_RDWR, 0666);
                if (fd_ >= 0) {
                    is_file_backed_ = true;
                } else {
                    return;
                }
            }
        }

        base_ptr_ = ::mmap(nullptr, total_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (base_ptr_ == MAP_FAILED) {
            base_ptr_ = nullptr;
            ::close(fd_);
            fd_ = -1;
            return;
        }

        ::mlock(base_ptr_, total_size_);

        header_ = reinterpret_cast<SHMHeader*>(base_ptr_);
        uint8_t* slot_addr = reinterpret_cast<uint8_t*>(base_ptr_) + sizeof(SHMHeader);
        slots_ = reinterpret_cast<T*>(slot_addr);

        if (mode_ == Mode::Producer) {
            header_->magic = SHMHeader::SHM_MAGIC;
            header_->version = 1;
            header_->slot_capacity = Capacity;
            header_->slot_size = sizeof(T);
            header_->producer_head.store(0, std::memory_order_relaxed);
            header_->consumer_tail.store(0, std::memory_order_relaxed);
        }
    }

    void cleanup() {
        if (base_ptr_ && base_ptr_ != MAP_FAILED) {
            ::munlock(base_ptr_, total_size_);
            ::munmap(base_ptr_, total_size_);
            base_ptr_ = nullptr;
        }
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        if (mode_ == Mode::Producer) {
            if (is_file_backed_) {
                std::string fallback = get_fallback_path();
                ::unlink(fallback.c_str());
            } else {
                ::shm_unlink(name_.c_str());
            }
        }
    }
};

} // namespace nexus::ipc
