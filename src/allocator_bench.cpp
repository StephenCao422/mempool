#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <algorithm>
#include <atomic>
#include <iomanip>
#include "../inc/ConcurrentAlloc.h"

using namespace std;

// Reference tests adapted from user example
void Alloc1(){ for(int i=0;i<5;++i) ConcurrentAlloc(6); }
void Alloc2(){ for(int i=0;i<5;++i) ConcurrentAlloc(7); }
void AllocTest(){ thread t1(Alloc1); t1.join(); thread t2(Alloc2); t2.join(); }

void ConcurrentAllocTest1(){
    void* ptrs[5];
    ptrs[0]=ConcurrentAlloc(5); ptrs[1]=ConcurrentAlloc(8); ptrs[2]=ConcurrentAlloc(4); ptrs[3]=ConcurrentAlloc(6); ptrs[4]=ConcurrentAlloc(3);
    for(void* p:ptrs) cout<<p<<"\n"; }

void ConcurrentAllocTest2(){
    for(int i=0;i<1024;++i){ void* ptr = ConcurrentAlloc(5); /* optionally store */ }
    void* ptr = ConcurrentAlloc(3); cout << "-----" << ptr << endl; }

void TestConcurrentFree1(){
    vector<void*> v; int sizes[]={5,8,4,6,3,3,3}; for(int s: sizes) v.push_back(ConcurrentAlloc(s));
    for(void* p: v) ConcurrentFree(p); }

void MultiThreadAlloc1(){ vector<void*> v; for(size_t i=0;i<7000;++i){ v.push_back(ConcurrentAlloc(6)); } for(void* p: v) ConcurrentFree(p);} 
void MultiThreadAlloc2(){ vector<void*> v; for(size_t i=0;i<7000;++i){ v.push_back(ConcurrentAlloc(16)); } for(void* p: v) ConcurrentFree(p);} 
void TestMultiThread(){ thread t1(MultiThreadAlloc1); thread t2(MultiThreadAlloc2); t1.join(); t2.join(); }

void TestAddressShift(){ PageID id1=2000,id2=2001; char* p1=(char*)(id1<<PAGE_SHIFT); char* p2=(char*)(id2<<PAGE_SHIFT); while(p1<p2){ cout<<(void*)p1<<":"<<((PageID)p1>>PAGE_SHIFT)<<"\n"; p1+=8; } }

void BigAlloc(){ void* p1=ConcurrentAlloc(257*1024); ConcurrentFree(p1); void* p2=ConcurrentAlloc(129*8*1024); ConcurrentFree(p2);} 

// Stress test: random small allocations across many threads
struct AllocRecord { void* ptr; size_t size; };

void StressWorker(size_t threadId, size_t iterations, vector<AllocRecord>& recBuf){
    std::mt19937_64 rng(threadId + 12345);
    std::uniform_int_distribution<size_t> distSize(1, 256); // within small allocator arena
    recBuf.reserve(iterations/2);
    for(size_t i=0;i<iterations;++i){
        size_t sz = distSize(rng);
        void* p = ConcurrentAlloc(sz);
        if((i & 3)==0){ // free roughly 25% immediately
            ConcurrentFree(p);
        } else {
            recBuf.push_back({p, sz});
        }
        if(recBuf.size()>8192){ // periodically free a batch
            for(size_t k=0;k<4096;++k){
                ConcurrentFree(recBuf.back().ptr);
                recBuf.pop_back();
            }
        }
    }
    // cleanup
    for(auto &r: recBuf) ConcurrentFree(r.ptr);
    recBuf.clear();
}

// Benchmark helper
template<class F>
long long TimeMs(F&& f){ auto start=chrono::steady_clock::now(); f(); auto end=chrono::steady_clock::now(); return chrono::duration_cast<chrono::milliseconds>(end-start).count(); }

void BenchmarkCustomVsMalloc(size_t threads, size_t iterationsPerThread){
    cout << "[bench] Threads="<<threads<<" iterations/thread="<<iterationsPerThread<<"\n";

    // Custom allocator benchmark
    long long customMs = TimeMs([&]{
        vector<thread> ts; ts.reserve(threads);
        vector<vector<AllocRecord>> buffers(threads);
        for(size_t t=0;t<threads;++t){ ts.emplace_back(StressWorker, t, iterationsPerThread, std::ref(buffers[t])); }
        for(auto &th: ts) th.join();
    });

    // Malloc benchmark (similar pattern)
    auto mallocWorker = [&](size_t threadId){
        std::mt19937_64 rng(threadId + 98765);
        std::uniform_int_distribution<size_t> dist(1,256);
        vector<void*> hold; hold.reserve(iterationsPerThread/2);
        for(size_t i=0;i<iterationsPerThread;++i){
            size_t sz = dist(rng);
            void* p = ::malloc(sz);
            if(!p){ cerr<<"malloc failed"<<endl; continue; }
            if((i & 3)==0){ ::free(p); } else { hold.push_back(p); }
            if(hold.size()>8192){ for(size_t k=0;k<4096;++k){ ::free(hold.back()); hold.pop_back(); } }
        }
        for(void* p: hold) ::free(p);
    };
    long long mallocMs = TimeMs([&]{ vector<thread> ts; for(size_t t=0;t<threads;++t) ts.emplace_back(mallocWorker, t); for(auto &th: ts) th.join(); });

    double speedup = customMs ? (double)mallocMs / (double)customMs : 0.0;
    cout << "[bench] custom(ms)="<<customMs<<" malloc(ms)="<<mallocMs
        << " speedup="<< fixed << setprecision(2) << speedup << "x\n";
}

int main(){
    cout << "Running reference functional tests..." << endl;
    AllocTest();
    ConcurrentAllocTest1();
    ConcurrentAllocTest2();
    TestConcurrentFree1();
    TestMultiThread();
    BigAlloc();

    cout << "Starting stress benchmark..." << endl;
    BenchmarkCustomVsMalloc(4, 200000); // adjust iterations as needed
    cout << "Done." << endl;
    return 0;
}
