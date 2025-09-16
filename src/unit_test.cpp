#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <cassert>
#include "../inc/ConcurrentAlloc.h"

static const size_t SMALL_N = 1000;

void AllocWorker(size_t allocSize, size_t count, std::vector<void*>& out) {
    out.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        void* p = ConcurrentAlloc(allocSize);
        if(!p) {
            std::cerr << "Allocation returned nullptr for size " << allocSize << "\n";
        }
        out.push_back(p);
    }
}

int main(){
    std::cout << "[unit] Starting concurrent allocation test" << std::endl;

    std::vector<void*> a,b,c;
    std::thread t1(AllocWorker, 8, SMALL_N, std::ref(a));
    std::thread t2(AllocWorker, 64, SMALL_N, std::ref(b));
    std::thread t3(AllocWorker, 128, SMALL_N, std::ref(c));

    t1.join();
    t2.join();
    t3.join();

    std::cout << "[unit] Allocated blocks: "
              << a.size() + b.size() + c.size() << std::endl;

    size_t nullCount = 0;
    for(auto p: a) if(!p) ++nullCount;
    for(auto p: b) if(!p) ++nullCount;
    for(auto p: c) if(!p) ++nullCount;
    std::cout << "[unit] Null pointer count: " << nullCount << std::endl;
    assert(nullCount == 0 && "Null allocations encountered");

    std::cout << "[unit] Test complete (no frees performed)." << std::endl;
    return 0;
}