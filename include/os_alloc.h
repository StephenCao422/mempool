#pragma once
#include <cstddef>
#include <cstdint>
#include <sys/mman.h>
#include <unistd.h>

// Our allocator uses 8 KiB "pages"
static constexpr std::size_t kAllocPageShift = 13;
static constexpr std::size_t kAllocPageSize  = 1ull << kAllocPageShift;

// Returns an 8 KiB–aligned mapping of exactly `bytes` length.
static inline void* SystemAlloc(std::size_t bytes) {
    if (bytes == 0) bytes = kAllocPageSize;

    // Over-map by one 8 KiB page so we can align up to 8 KiB.
    const std::size_t req = bytes + kAllocPageSize;

    void* base = ::mmap(nullptr, req, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) return nullptr;

    const std::uintptr_t addr    = (std::uintptr_t)base;
    const std::uintptr_t aligned = (addr + (kAllocPageSize - 1)) & ~(std::uintptr_t)(kAllocPageSize - 1);

    // Trim head
    const std::size_t head = aligned - addr;
    if (head) ::munmap((void*)addr, head);

    // Trim tail so final length == bytes
    const std::uintptr_t map_end   = addr + req;
    const std::uintptr_t tail_from = aligned + bytes;
    if (tail_from < map_end) {
        const std::size_t tail = map_end - tail_from;
        ::munmap((void*)tail_from, tail);
    }

    return (void*)aligned; // guaranteed 8 KiB–aligned
}

static inline void SystemFree(void* p, std::size_t bytes) {
    if (!p) return;
    ::munmap(p, bytes);
}
