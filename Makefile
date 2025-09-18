# ===== Linux-only Makefile =====

CXX       := g++
BUILD     ?= release

INC_DIR   := include
SRC_DIR   := src
OBJ_DIR   := build/obj
BIN_DIR   := build/bin

CXXSTD    := -std=gnu++17
CXXWARN   := -Wall -Wextra -Wpedantic -Wshadow
THREAD    := -pthread

ifeq ($(BUILD),release)
  CXXOPT  := -O2 -g
  SAN     :=
else ifeq ($(BUILD),asan)
  CXXOPT  := -O1 -g -fsanitize=address -fno-omit-frame-pointer
  SAN     := -fsanitize=address
else
  CXXOPT  := -O0 -g
  SAN     :=
endif

CXXFLAGS  := $(CXXSTD) $(CXXOPT) $(THREAD) $(CXXWARN) -I$(INC_DIR) -MMD -MP
LDFLAGS   := $(THREAD) $(SAN)

CORE_SRCS := $(SRC_DIR)/central_cache.cpp $(SRC_DIR)/page_cache.cpp $(SRC_DIR)/thread_cache.cpp
CORE_OBJS := $(CORE_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

TB_SRC := tools/tb.cpp
TB_OBJ := $(OBJ_DIR)/tb.o
TB_BIN := $(BIN_DIR)/tb

ALLOC_BENCH_SRC := $(wildcard tools/allocator_tb.cpp)
ALLOC_BENCH_OBJ := $(ALLOC_BENCH_SRC:tools/%.cpp=$(OBJ_DIR)/%.o)
ALLOC_BENCH_BIN := $(if $(ALLOC_BENCH_SRC),$(BIN_DIR)/allocator_tb,)

BINS := $(TB_BIN) $(ALLOC_BENCH_BIN)

.PHONY: all clean run-bench asan veryclean

all: $(BINS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: tools/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TB_BIN): $(CORE_OBJS) $(TB_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/allocator_tb: $(CORE_OBJS) $(ALLOC_BENCH_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@


NTHREADS ?= 8
ITERS    ?= 300000
MIN      ?= 8
MAX      ?= 256
KEEP     ?= 75

run-bench: $(TB_BIN)
	$(TB_BIN) $(NTHREADS) $(ITERS) $(MIN) $(MAX) $(KEEP)

asan:
	$(MAKE) clean
	$(MAKE) BUILD=asan

clean:
	@echo "cleaning build files"
	rm -rf $(OBJ_DIR) $(BIN_DIR)

veryclean: clean

-include $(CORE_OBJS:.o=.d) $(TB_OBJ:.o=.d) $(ALLOC_BENCH_OBJ:.o=.d)
