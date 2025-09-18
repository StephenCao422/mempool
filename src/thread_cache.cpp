#include "../include/thread_cache.h"
#include "../include/central_cache.h"
#include "../include/page_cache.h"
#include <algorithm>
thread_local ThreadCache* pTLSThreadCache = nullptr;

void* ThreadCache::Allocate(size_t size){
    assert(size <= MAX_BYTES);

    size_t alignSize = SizeClass::RoundUp(size);
    size_t index = SizeClass::Index(alignSize);
    assert(index < FREE_LIST_NUM && "size class index out of range");

    if(!_freeLists[index].Empty()){
        return _freeLists[index].Pop();
    }
    else {
        return FetchFromCentralCache(index, alignSize);
    }
}

void ThreadCache::Deallocate(void* obj, size_t size){
    assert(obj);
    assert(size <= MAX_BYTES);

    size_t index = SizeClass::Index(size);
    assert(index < FREE_LIST_NUM && "size class index out of range");

    _freeLists[index].Push(obj);
    // Optional: if list too long return some to central
}

/*  ThreadCache fetch memory from CentralCache (Slow Start like TCP conjection control)
    Control current alignSize meomry for tc by MaxSize and NumMoveSize
    MaxSize: The max applied memory when FreeList once apply in index that not reach maximum
    NumMoveSize: max aligned memory of tc once apply for cc
    alignSize=8B, MaxSize 1B, NumMoveSize 512B, then batchNum=8B
    no limit then batchNum=MaxNum, reach limit then batchNum = NumMoveSize
*/
void* ThreadCache::FetchFromCentralCache(size_t index, size_t alignSize){
    size_t batchNum = std::min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(alignSize));
    if(batchNum == _freeLists[index].MaxSize()){
        _freeLists[index].MaxSize()++;
    }
    void* start = nullptr;
    void* end = nullptr;
    size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, alignSize);
    assert(actualNum >= 1);
    if(actualNum == 1){
        return start;
    } else {
        _freeLists[index].PushRange(ObjNext(start), end, actualNum-1);
        return start;
    }
}

void ThreadCache::ListTooLong(FreeList& list, size_t size){
    void* start = nullptr;
    void* end = nullptr;
    size_t batch = std::min(list.MaxSize(), list.Size());
    if(batch == 0) return;
    list.PopRange(start, end, batch);
    CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}
