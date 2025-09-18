#pragma once
#include <cstddef>
#include <atomic>
#include <new>
#include <cstring>

struct Span;
using PageID = std::size_t;

// 2-level radix tree: 16-bit root -> 18-bit leaves (covers 47 bits of VA with 8KiB pages)
struct PageMap {
  static constexpr std::size_t ROOT_BITS = 16;
  static constexpr std::size_t LEAF_BITS = 18;
  static constexpr std::size_t ROOT_SIZE = 1ull << ROOT_BITS;
  static constexpr std::size_t LEAF_SIZE = 1ull << LEAF_BITS;
  static constexpr std::size_t LEAF_MASK = LEAF_SIZE - 1;

  using Entry = std::atomic<Span*>;
  Entry** root_;  // root_[i1] -> Entry[LEAF_SIZE]

  PageMap() {
    root_ = static_cast<Entry**>(::operator new[](ROOT_SIZE * sizeof(Entry*)));
    std::memset(root_, 0, ROOT_SIZE * sizeof(Entry*));
  }
  ~PageMap() {
    for (std::size_t i = 0; i < ROOT_SIZE; ++i) if (root_[i]) ::operator delete[](root_[i]);
    ::operator delete[](root_);
  }

  inline Span* get(PageID id) const noexcept {
    const std::size_t i1 = id >> LEAF_BITS;
    const std::size_t i2 = id &  LEAF_MASK;
    Entry* leaf = root_[i1];
    return leaf ? leaf[i2].load(std::memory_order_acquire) : nullptr;
  }

  // writers must hold PageCache::_pageMtx
  inline void set(PageID id, Span* s) {
    const std::size_t i1 = id >> LEAF_BITS;
    const std::size_t i2 = id &  LEAF_MASK;
    Entry* leaf = root_[i1];
    if (!leaf) {
      leaf = static_cast<Entry*>(::operator new[](LEAF_SIZE * sizeof(Entry)));
      for (std::size_t i = 0; i < LEAF_SIZE; ++i)
        leaf[i].store(nullptr, std::memory_order_relaxed);
      root_[i1] = leaf;
    }
    leaf[i2].store(s, std::memory_order_release);
  }
  inline void set_range(PageID start, std::size_t n, Span* s) {
    for (std::size_t i = 0; i < n; ++i) set(start + i, s);
  }
};
