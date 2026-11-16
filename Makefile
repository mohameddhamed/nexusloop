CXX ?= clang++
CXXFLAGS ?= -std=c++20 -O3 -march=native -Wall -Wextra -Wpedantic -pthread -Iinclude

ifeq ($(SANITIZER),asan)
    CXXFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer -g
    LDFLAGS += -fsanitize=address,undefined
else ifeq ($(SANITIZER),tsan)
    CXXFLAGS += -fsanitize=thread -fno-omit-frame-pointer -g
    LDFLAGS += -fsanitize=thread
endif

BIN_DIR = bin
OBJ_DIR = obj

SRCS_LIB = \
    src/common/rdtsc.cpp \
    src/common/cpu.cpp \
    src/memory/huge_page_allocator.cpp \
    src/memory/allocation_guard.cpp \
    src/protocol/itch50_parser.cpp \
    src/net/io_uring_engine.cpp \
    src/net/epoll_engine.cpp \
    src/ipc/shm_gateway.cpp

OBJS_LIB = $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(SRCS_LIB))

TESTS = \
    $(BIN_DIR)/test_spsc_ring \
    $(BIN_DIR)/test_mpmc_ring \
    $(BIN_DIR)/test_itch50_parser \
    $(BIN_DIR)/test_allocation_guard \
    $(BIN_DIR)/test_zero_allocation_pipeline

BENCHMARKS = \
    $(BIN_DIR)/bench_spsc_latency \
    $(BIN_DIR)/bench_shm_ipc \
    $(BIN_DIR)/bench_itch_simd \
    $(BIN_DIR)/bench_network_gateway

.PHONY: all test bench clean dirs

all: dirs $(TESTS) $(BENCHMARKS)

dirs:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)/common $(OBJ_DIR)/memory $(OBJ_DIR)/protocol $(OBJ_DIR)/net $(OBJ_DIR)/ipc

$(OBJ_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Unit & Integration Tests
$(BIN_DIR)/test_spsc_ring: tests/unit/test_spsc_ring.cpp $(OBJS_LIB)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/test_mpmc_ring: tests/unit/test_mpmc_ring.cpp $(OBJS_LIB)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/test_itch50_parser: tests/unit/test_itch50_parser.cpp $(OBJS_LIB)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/test_allocation_guard: tests/unit/test_allocation_guard.cpp $(OBJS_LIB)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/test_zero_allocation_pipeline: tests/integration/test_zero_allocation_pipeline.cpp $(OBJS_LIB)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

# Benchmarks
$(BIN_DIR)/bench_spsc_latency: benchmarks/bench_spsc_latency.cpp $(OBJS_LIB)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/bench_shm_ipc: benchmarks/bench_shm_ipc.cpp $(OBJS_LIB)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/bench_itch_simd: benchmarks/bench_itch_simd.cpp $(OBJS_LIB)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/bench_network_gateway: benchmarks/bench_network_gateway.cpp $(OBJS_LIB)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

test: all
	@echo "\n================ RUNNING NEXUSLOOP TEST SUITE ================"
	$(BIN_DIR)/test_spsc_ring
	$(BIN_DIR)/test_mpmc_ring
	$(BIN_DIR)/test_itch50_parser
	$(BIN_DIR)/test_allocation_guard
	$(BIN_DIR)/test_zero_allocation_pipeline 1000000
	@echo "================ ALL TESTS PASSED SUCCESSFULLY ================\n"

bench: all
	@echo "\n================ EXECUTING NEXUSLOOP BENCHMARK SUITE ================"
	$(BIN_DIR)/bench_itch_simd
	$(BIN_DIR)/bench_spsc_latency
	$(BIN_DIR)/bench_shm_ipc
	$(BIN_DIR)/bench_network_gateway
	@echo "================ BENCHMARK SUITE COMPLETED ================\n"

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)
