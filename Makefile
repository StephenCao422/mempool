CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g -Iinc -pthread
LDLIBS := -pthread

SRC_DIR := src
BUILD_DIR := build/obj
BIN_DIR := bin

CORE_SRCS := $(SRC_DIR)/CentralCache.cpp \
			 $(SRC_DIR)/PageCache.cpp \
			 $(SRC_DIR)/ThreadCache.cpp
CORE_OBJS := $(CORE_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

PERF_SRC := $(SRC_DIR)/test.cpp
PERF_OBJ := $(BUILD_DIR)/test.o
UNIT_SRC := $(SRC_DIR)/unit_test.cpp
UNIT_OBJ := $(BUILD_DIR)/unit_test.o
BENCH_SRC := $(SRC_DIR)/allocator_bench.cpp
BENCH_OBJ := $(BUILD_DIR)/allocator_bench.o

PERF_BIN := $(BIN_DIR)/perf_test
UNIT_BIN := $(BIN_DIR)/unit_tests
BENCH_BIN := $(BIN_DIR)/allocator_bench

.PHONY: all clean run-perf run-unit dirs

all: dirs $(PERF_BIN) $(UNIT_BIN) $(BENCH_BIN)

dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(PERF_BIN): $(CORE_OBJS) $(PERF_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(UNIT_BIN): $(CORE_OBJS) $(UNIT_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(BENCH_BIN): $(CORE_OBJS) $(BENCH_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

run-perf: $(PERF_BIN)
	$(PERF_BIN)

run-unit: $(UNIT_BIN)
	$(UNIT_BIN)

run-bench: $(BENCH_BIN)
	$(BENCH_BIN)

clean:
	@echo Cleaning build artifacts
	rm -rf $(BUILD_DIR) $(BIN_DIR)
