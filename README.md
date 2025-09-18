# mempool

A tcmalloc‑inspired memory pool for **Linux** with:

- **8 KB internal pages** backed by `mmap/munmap` (alignment shim ensures 8 KB boundaries)
- **Per-thread small-object caches** to serve most allocations without locks
- **Central caches per size class** for batched recycle and load balancing
- **2-level Radix-tree page map** for O(1) page→span lookups with lock-free read

---

## Build

```bash
make

# defaults: 8 thr, 300k iters, sizes 8–256, keep 75%
make run-bench

# customize:
make run-bench NTHREADS=16 ITERS=400000 MIN=8 MAX=256 KEEP=75

```

**Core sources** :

- `src/central_cache.cpp`
- `src/page_cache.cpp`
- `src/thread_cache.cpp`

**Tools**

- `tools/tb.cpp` → `build/bin/tb`
- `tools/allocator_tb.cpp` → `build/bin/allocator_tb`

---

## Benchmark: tb

### Usage

```bash
./build/bin/tb <threads> <iters_per_thread> <min_size> <max_size> <keep_percent>
```

#### Examples

```bash
# Typical small‑object test (8–256 bytes)
./build/bin/tb 8 300000 8 256 75

# Include slightly larger small objects
./build/bin/tb 8 300000 8 512 75

# Scale threads
./build/bin/tb 16 400000 8 256 75
```

---
## Features (inspired by tcmalloc)

- Three-layer design: **ThreadCache (tc)** → **CentralCache (cc)** → **PageCache (pc)**, mirroring the simplified tcmalloc layout
- **Per-thread caches (TLS)** for small objects: tc handles requests up to ~**256 KB** before taking a slower path
- **CentralCache** uses hashed buckets with **per-bucket locks**, reducing global contention and enabling cross-thread rebalancing
- **PageCache** manages page-spans, performs **split/merge (coalescing)** of adjacent pages to reduce external fragmentation. (Pages default 8 KB here)
- **Size classes & alignment**: requests are rounded to the nearest class; batched refill/drain between tc↔cc amortizes locking
- **Fast pointer→span lookup** via a **radix-tree page map** (used to address MapObj→Span lookup bottlenecks)
- **Large allocations** (>~256 KB) bypass tc and take a separate path appropriate for big blocks

![mempool](<mempool.png>)