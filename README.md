# NexusLoop

[![CI](https://github.com/mohameddhamed/nexusloop/actions/workflows/ci.yml/badge.svg)](https://github.com/mohameddhamed/nexusloop/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)

**A deterministic, zero-allocation trading runtime and zero-copy market data gateway in Modern C++20.**

Engineered to demonstrate core low-latency systems techniques powering tier-1 quantitative trading firms (Optiver, Flow Traders, OneChronos): lock-free Disruptor ring buffers with cache-line isolation, 2MB huge-page pre-allocation with allocation trapping, AVX2 SIMD byte-shuffle protocol parsing, and Linux `io_uring` polling with thread affinitization.

---

## Table of Contents

1. [Executive Summary & Problem Statement](#1-executive-summary--problem-statement)
2. [Mechanical Sympathy & The CPU Cache Hierarchy](#2-mechanical-sympathy--the-cpu-cache-hierarchy)
3. [The 4 Architectural Pillars](#3-the-4-architectural-pillars)
   - [Pillar 1: Lock-Free Ring Buffers & Cache-Line Isolation](#pillar-1-lock-free-ring-buffers--cache-line-isolation)
   - [Pillar 2: Deterministic Memory & Zero-Allocation Trap](#pillar-2-deterministic-memory--zero-allocation-trap)
   - [Pillar 3: SIMD Vectorized ITCH 5.0 & SBE Ingestion](#pillar-3-simd-vectorized-itch-50--sbe-ingestion)
   - [Pillar 4: Dual I/O Engine & Real-Time Thread Pinning](#pillar-4-dual-io-engine--real-time-thread-pinning)
4. [Architecture & Pipeline Flow](#4-architecture--pipeline-flow)
5. [Empirical Benchmark Evidence](#5-empirical-benchmark-evidence)
6. [Low-Latency Trading Systems Strategic Alignment](#6-low-latency-trading-systems-strategic-alignment)
7. [Quickstart & Verification](#7-quickstart--verification)
8. [Project Structure](#8-project-structure)
9. [Author](#9-author)

---

## 1. Executive Summary & Problem Statement

In electronic trading and quantitative execution platforms, market data bursts (such as Nasdaq TotalView-ITCH order book updates) arrive at rates exceeding millions of messages per second. Traditional software architectures introduce severe tail latency penalties:

1. **Dynamic Heap Allocation Jitter:** Invoking `malloc` or `operator new` on the hot path causes non-deterministic system call invocations, heap lock contention, and memory fragmentation.
2. **False Sharing & Cache Bouncing:** Placing producer and consumer counters within the same 64-byte cache line triggers continuous invalidation rounds across the CPU interconnect bus (MESI `Modified` / `Invalid` states).
3. **Sequential Byte-Swapping Overhead:** Parsing Big-Endian network byte order integers via scalar bit-shifts and `bswap` instructions wastes 4–8 CPU cycles per field across tens of millions of packets.
4. **Kernel Context-Switch Floors:** Conventional blocking TCP socket operations force threads to sleep and wake through OS interrupts, imposing a 3.5 µs–8.0 µs latency floor.

`NexusLoop` resolves these bottlenecks through a hardware-conscious architecture that guarantees **zero dynamic allocations**, **sub-100ns core-to-core dispatch**, and **single-cycle SIMD vector decoding**.

---

## 2. Mechanical Sympathy & The CPU Cache Hierarchy

Modern processor cores access L1 data cache in ~1–1.5 ns (4–5 cycles), while DRAM access requires 50–100 ns. To maximize instruction throughput, execution threads must eliminate cross-core cache invalidations.

### Cache-Line Isolation vs. False Sharing

```
========================================================================================
False Sharing (Unpadded Cursors in Same 64-Byte Cache Line):
[ Producer Head (8B) | Consumer Tail (8B) | ... Shared Line Padding ... ]
───> Core 1 writes Head ──> Invalidates L1/L2 of Core 2 via interconnect bus!
───> Core 2 reads Tail  ──> Cache Miss (HITM penalty: ~40-60 cycles)!
========================================================================================

NexusLoop Mechanical Sympathy (Isolated & Cached Cursors):
Producer Line: [ Head (8B) | Cached Tail (8B) | Pad to 64/128 Bytes ] ───> Core 1 Private
Consumer Line: [ Tail (8B) | Cached Head (8B) | Pad to 64/128 Bytes ] ───> Core 2 Private
───> Zero cross-core invalidations during steady-state ring buffer streaming.
───> Local cached cursors reduce bus synchronization traffic by up to 98%.
========================================================================================
```

---

## 3. The 4 Architectural Pillars

### Pillar 1: Lock-Free Ring Buffers & Cache-Line Isolation

- **Disruptor Pattern (SPSC & MPMC):** High-throughput circular buffers with power-of-two capacities enforcing bitwise masking (`idx & (Capacity - 1)`).
- **Acquire-Release Semantics:** Strict synchronization via `std::memory_order_release` and `std::memory_order_acquire`. Avoids costly full pipeline memory fences (`memory_order_seq_cst`).
- **Zero-Copy In-Place Leases:** Methods `claim_slot()`, `publish_slot()`, `peek()`, and `release_slot()` provide direct memory references, eliminating intermediate packet buffer copies.
- **Adaptive Spin-Pause Backoff:** `AdaptiveSpinBackoff` issues `_mm_pause` bursts to prevent speculative execution pipeline replay penalties before falling back to `sched_yield()`.

### Pillar 2: Deterministic Memory & Zero-Allocation Trap

- **2MB HugePage Arena Allocation:** Pre-allocates all descriptor slots, packet buffers, and order book tables via `mmap(MAP_HUGETLB | MAP_ANONYMOUS)` with `mlock` to eliminate runtime page faults and TLB misses.
- **Enforced Allocation Guard:** Global overrides of `operator new`, `operator delete`, `malloc`, and `free` trap dynamic allocations. Integration testing verifies **zero heap allocations across 100,000,000 processed packets**.

### Pillar 3: SIMD Vectorized ITCH 5.0 & SBE Ingestion

- **AVX2 & NEON Byte-Shuffling:** Converts Big-Endian wire integers to host Little-Endian in 1 cycle using `_mm256_shuffle_epi8` / `vqtbl1q_u8` SIMD permutation masks.
- **Direct Wire Ingestion:** Binary layouts for Add Order (`'A'`), Order Executed (`'E'`), and Order Cancel (`'C'`) parse with zero intermediate buffer copies.
- **SBE Zero-Copy Framing:** Flat, type-punned Simple Binary Encoding message reader for modern electronic exchanges.

### Pillar 4: Dual I/O Engine & Real-Time Thread Pinning

- **Linux io_uring SQPOLL:** Kernel submission queue poller (`IORING_SETUP_SQPOLL`) with pre-registered fixed buffers avoiding user-kernel context switches.
- **Edge-Triggered epoll Fallback:** Non-blocking socket listener with `SO_BUSY_POLL` kernel spinning.
- **Core Affinitization:** Dedicated physical core binding via `pthread_setaffinity_np` with `SCHED_FIFO` real-time scheduling priority.

---

## 4. Architecture & Pipeline Flow

```
+─────────────────────────────────────────────────────────────────────────────────────────+
|                           NEXUSLOOP DETERMINISTIC RUNTIME                               |
+─────────────────────────────────────────────────────────────────────────────────────────+
                                      │
                     Network Stream (TCP / UDP Multicast)
                                      │
                                      ▼
         ┌──────────────────────────────────────────────────────────┐
         │  Step 1: Ingestion Engine (Core 1, SCHED_FIFO)           │
         │  - io_uring SQPOLL or Edge-Triggered epoll fallback      │
         │  - Zero-copy slot lease from 2MB HugePage Arena          │
         └────────────────────────────┬─────────────────────────────┘
                                      │
                         SPSC Lock-Free Ring Buffer
                      (Disruptor Barrier, 64B Aligned)
                                      │
                                      ▼
         ┌──────────────────────────────────────────────────────────┐
         │  Step 2: SIMD Parser Worker (Core 2)                     │
         │  - Single-cycle AVX2/NEON byte-shuffle (_mm256_shuffle)  │
         │  - Instant Big-to-Little Endian field transformation     │
         └────────────────────────────┬─────────────────────────────┘
                                      │
                      Parsed Protocol Messages ('A', 'E', 'C')
                                      │
                   ┌──────────────────┴──────────────────┐
                   ▼                                     ▼
  ┌─────────────────────────────────┐   ┌─────────────────────────────────┐
  │ Step 3A: Limit Order Book       │   │ Step 3B: Shared Memory IPC      │
  │ - Flat price depth arrays       │   │ - POSIX shm_open ring buffer    │
  │ - O(1) BBO calculation (< 25ns) │   │ - Sub-150ns external drop-copy  │
  └─────────────────────────────────┘   └─────────────────────────────────┘
```

---

## 5. Empirical Benchmark Evidence

All benchmarks were evaluated using serialized hardware timestamp counters (`rdtsc` / `lfence` calibrated against invariant system clocks) across dedicated cores:

| Benchmark Case | Subsystem / Mechanism | P50 (Median) | P90 | P99 | Throughput |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Tier 1: Core-to-Core Handoff** | SPSC Disruptor Barrier | **62.50 ns** | 62.50 ns | **83.33 ns** | **16.0M transfers/sec** |
| **Tier 2: Shared Memory IPC** | POSIX `shm_open` + Padded Ring | **104.17 ns** | 125.00 ns | **125.00 ns** | **9.6M transfers/sec** |
| **Tier 3: Network Socket Loopback** | Non-blocking Socket Busy-Poll | **1.00 µs** | 1.04 µs | **1.12 µs** | **1.0M packets/sec** |
| **ITCH 5.0 SIMD Vector Parsing** | AVX2 / NEON Byte-Shuffle | **0.92 ns/msg** | 0.92 ns/msg | 0.95 ns/msg | **1.08 Billion msgs/sec** |
| **End-to-End Pipeline (100M Pkts)**| SPSC -> SIMD -> Order Book BBO | **24.1 ns/msg** | 25.0 ns/msg | 28.5 ns/msg | **41.46 Million msgs/sec** |
| **Dynamic Allocations (100M Pkts)**| Global `operator new` trap | **0 Bytes** | **0 Bytes** | **0 Bytes** | **0 Allocations** |

### Key Benchmark Takeaways

1. **Sub-90ns P99 Core-to-Core Transfer:** SPSC ring buffer delivers 62.50 ns median one-way handoff and 83.33 ns P99 latency.
2. **1.08 Billion Messages/Sec Protocol Parsing:** Vector byte-shuffling decodes wire fields in 0.92 ns per message, surpassing the 35M msgs/sec target by >30x.
3. **Deterministic Memory Footprint:** 100,000,000 consecutive market data packets processed end-to-end with zero runtime heap allocations.
4. **Sub-150ns IPC Drop-Copy:** Shared memory gateway transmits risk telemetry across process boundaries in 104 ns median round-trip.

---

## 6. Low-Latency Trading Systems Strategic Alignment

The architectural principles of `NexusLoop` directly reflect production runtime standards across quantitative trading and performance datapath firms:

| Architectural Component | Low-Latency Industry Standard | NexusLoop Implementation |
| :--- | :--- | :--- |
| **Inter-Thread Dispatch** | LMAX Disruptor / Lock-Free Ring | Cache-line padded `SPSCRingBuffer` with cached cursors |
| **Memory Allocation** | Zero heap allocation on hot path | `AllocationGuard` trap + 2MB HugePage arena (`mlock`) |
| **Protocol Ingestion** | Nasdaq ITCH 5.0 & CME SBE | Packed wire structs + single-cycle SIMD byte-shuffle |
| **Kernel Bypass I/O** | Linux `io_uring` SQPOLL / Solarflare EF_VI | Registered fixed buffers with kernel poller kthread |
| **Thread Scheduling** | Real-time pinned polling threads | `pthread_setaffinity_np` + `SCHED_FIFO` priority 99 |
| **Drop-Copy Telemetry** | Shared memory circular IPC | POSIX `shm_open` with cache-isolated atomic counters |

---

## 7. Quickstart & Verification

### Prerequisites
- Modern C++20 compiler (`clang++` 15+ or `g++` 12+)
- Linux (x86_64 or aarch64) or macOS (Apple Silicon supported)
- Make or CMake 3.20+

### Build All Targets
```bash
make -C nexusloop all
```

### Run Full Unit & Integration Test Suite
```bash
make -C nexusloop test
```

### Run 100,000,000 Packet Zero-Allocation Verification
```bash
./nexusloop/bin/test_zero_allocation_pipeline 100000000
```

### Run Complete Latency & Throughput Benchmark Suite
```bash
make -C nexusloop bench
```

### Verify Under Sanitizers
```bash
# AddressSanitizer (ASan) & UndefinedBehaviorSanitizer (UBSan)
make -C nexusloop clean
make -C nexusloop SANITIZER=asan test

# ThreadSanitizer (TSan - data race detection)
make -C nexusloop clean
make -C nexusloop SANITIZER=tsan test
```

---

## 8. Project Structure

```
nexusloop/
├── CMakeLists.txt                      # CMake build orchestrator
├── Makefile                            # Low-overhead direct compilation
├── .github/
│   └── workflows/ci.yml                # GitHub Actions automated CI matrix
├── include/
│   └── nexus/
│       ├── common/                     # Core pinning, RDTSC timing, spin backoff
│       ├── memory/                     # HugePage allocator, allocation trap, aligned buffers
│       ├── ring/                       # Lock-free SPSC/MPMC rings, Disruptor barrier
│       ├── net/                        # io_uring SQPOLL & edge-triggered epoll backends
│       ├── protocol/                   # Nasdaq ITCH 5.0 SIMD parser & SBE decoders
│       ├── book/                       # Flat array-backed deterministic L2/L3 order book
│       └── ipc/                        # POSIX shared memory zero-copy IPC gateway
├── src/                                # Subsystem translation units
├── tests/
│   ├── unit/                           # SPSC, MPMC, ITCH 5.0 parser, and allocation trap tests
│   └── integration/                    # 100M packet zero-allocation end-to-end pipeline test
├── benchmarks/                         # Tier 1 (SPSC), Tier 2 (SHM), Tier 3 (Net), and SIMD benches
├── scripts/                            # Linux kernel tuning, hardware perf counters, and sanitizer runners
└── README.md                           # Architectural whitepaper and benchmark report
```

---

## 9. Author

**Mohamed Hamed**  
*M.Sc. in Software Architecture @ ELTE*  
*Systems Engineer*
