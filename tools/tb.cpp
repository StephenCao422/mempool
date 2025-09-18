#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "../include/concurrent_alloc.h"

using Clock = std::chrono::steady_clock;
using ns    = std::chrono::nanoseconds;

struct StartBarrier {
  explicit StartBarrier(int n) : n_(n) {}
  void arrive_and_wait() {
    std::unique_lock<std::mutex> lk(m_);
    if (++arrived_ == n_) { go_ = true; cv_.notify_all(); }
    else cv_.wait(lk, [&]{ return go_; });
  }
 private:
  int n_;
  int arrived_{0};
  bool go_{false};
  std::mutex m_;
  std::condition_variable cv_;
};

using AllocFn = void* (*)(size_t);
using FreeFn  = void  (*)(void*);

static void* LibcAlloc(size_t n) { return std::malloc(n); }
static void  LibcFree(void* p)   { std::free(p); }

struct RunConfig {
  int threads = 4;
  int iters_per_thread = 200'000;
  size_t min_size = 16;
  size_t max_size = 256;
  double keep_ratio = 0.75;
  uint64_t seed = 0x1234'5678'eceb'def0ULL;
};

struct RunResult {
  uint64_t total_ops{};
  ns elapsed{};
};

RunResult run_bench(const char* name, AllocFn A, FreeFn F, const RunConfig& cfg) {
  std::vector<std::thread> threads;
  std::vector<ns> t_elapsed(cfg.threads);
  StartBarrier barrier(cfg.threads);

  {
    std::vector<void*> warm;
    warm.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
      void* p = A(64);
      warm.push_back(p);
    }
    for (void* p : warm) F(p);
  }

  auto t0 = Clock::now();

  for (int tid = 0; tid < cfg.threads; ++tid) {
    threads.emplace_back([&, tid] {
      std::mt19937_64 rng(cfg.seed ^ (0x9e3779b97f4a7c15ULL * (uint64_t)tid));
      std::uniform_int_distribution<size_t> dsz(cfg.min_size, cfg.max_size);
      std::bernoulli_distribution keep(cfg.keep_ratio);

      std::vector<void*> kept;
      kept.reserve(static_cast<size_t>(cfg.iters_per_thread * cfg.keep_ratio) + 8);

      barrier.arrive_and_wait();
      auto ts = Clock::now();

      for (int i = 0; i < cfg.iters_per_thread; ++i) {
        size_t n = dsz(rng);
        void* p = A(n);
        if (!p) {
          std::cerr << "alloc returned null for size=" << n << "\n";
          std::abort();
        }
        if (keep(rng)) {
          kept.push_back(p);
        } else {
          F(p);
        }

        if (kept.size() > 1u << 16) {
          for (size_t k = 0; k < (1u << 14); ++k) {
            F(kept.back());
            kept.pop_back();
          }
        }
      }

      for (void* p : kept) F(p);

      t_elapsed[tid] = std::chrono::duration_cast<ns>(Clock::now() - ts);
    });
  }

  for (auto& th : threads) th.join();

  auto t1 = Clock::now();
  RunResult r;
  r.total_ops = static_cast<uint64_t>(cfg.iters_per_thread) * static_cast<uint64_t>(cfg.threads);
  r.elapsed = std::chrono::duration_cast<ns>(t1 - t0);

  double ms = r.elapsed.count() / 1e6;
  double ops_per_sec = (double)r.total_ops / (r.elapsed.count() / 1e9);
  std::cout << std::fixed << std::setprecision(2);
  std::cout << "[" << name << "] "
            << cfg.threads << " thr × " << cfg.iters_per_thread << " iters  "
            << "sizes " << cfg.min_size << "-" << cfg.max_size << " bytes  "
            << "keep=" << (int)std::round(cfg.keep_ratio * 100) << "%  "
            << "time=" << ms << " ms  "
            << "throughput=" << ops_per_sec/1e6 << " Mops/s\n";

  return r;
}

int main(int argc, char** argv) {
  RunConfig cfg;

  if (argc >= 2) cfg.threads = std::max(1, std::atoi(argv[1]));
  if (argc >= 3) cfg.iters_per_thread = std::max(1, std::atoi(argv[2]));
  if (argc >= 4) cfg.min_size = (size_t)std::max(1, std::atoi(argv[3]));
  if (argc >= 5) cfg.max_size = (size_t)std::max((int)cfg.min_size, std::atoi(argv[4]));
  if (argc >= 6) cfg.keep_ratio = std::clamp(std::atof(argv[5]) / 100.0, 0.0, 1.0);

  auto libc = run_bench("malloc/free", &LibcAlloc, &LibcFree, cfg);
  auto mine = run_bench("ConcurrentAlloc/Free", &ConcurrentAlloc, &ConcurrentFree, cfg);

  double speedup = (double)libc.elapsed.count() / (double)mine.elapsed.count();
  std::cout << std::fixed << std::setprecision(3);
  std::cout << "Speedup (custom vs malloc): " << speedup << "x\n";
  return 0;
}
