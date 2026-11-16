cmake_minimum_required(VERSION 3.20)
project(NexusLoop VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Architectural optimization flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(
        -O3
        -march=native
        -DNDEBUG
        -fno-rtti
    )
endif()

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)

# Nexus Core Library
add_library(nexus_core
    src/common/rdtsc.cpp
    src/common/cpu.cpp
    src/memory/huge_page_allocator.cpp
    src/memory/allocation_guard.cpp
    src/protocol/itch50_parser.cpp
    src/net/io_uring_engine.cpp
    src/net/epoll_engine.cpp
    src/ipc/shm_gateway.cpp
)

target_include_directories(nexus_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(nexus_core PUBLIC pthread)

# Unit Tests
enable_testing()

add_executable(test_spsc_ring tests/unit/test_spsc_ring.cpp)
target_link_libraries(test_spsc_ring PRIVATE nexus_core)
add_test(NAME TestSPSC COMMAND test_spsc_ring)

add_executable(test_mpmc_ring tests/unit/test_mpmc_ring.cpp)
target_link_libraries(test_mpmc_ring PRIVATE nexus_core)
add_test(NAME TestMPMC COMMAND test_mpmc_ring)

add_executable(test_itch50_parser tests/unit/test_itch50_parser.cpp)
target_link_libraries(test_itch50_parser PRIVATE nexus_core)
add_test(NAME TestITCH50 COMMAND test_itch50_parser)

add_executable(test_allocation_guard tests/unit/test_allocation_guard.cpp)
target_link_libraries(test_allocation_guard PRIVATE nexus_core)
add_test(NAME TestAllocationGuard COMMAND test_allocation_guard)

add_executable(test_zero_allocation_pipeline tests/integration/test_zero_allocation_pipeline.cpp)
target_link_libraries(test_zero_allocation_pipeline PRIVATE nexus_core)
add_test(NAME TestZeroAllocationPipeline COMMAND test_zero_allocation_pipeline)

# Benchmarks
add_executable(bench_spsc_latency benchmarks/bench_spsc_latency.cpp)
target_link_libraries(bench_spsc_latency PRIVATE nexus_core)

add_executable(bench_shm_ipc benchmarks/bench_shm_ipc.cpp)
target_link_libraries(bench_shm_ipc PRIVATE nexus_core)

add_executable(bench_itch_simd benchmarks/bench_itch_simd.cpp)
target_link_libraries(bench_itch_simd PRIVATE nexus_core)

add_executable(bench_network_gateway benchmarks/bench_network_gateway.cpp)
target_link_libraries(bench_network_gateway PRIVATE nexus_core)
