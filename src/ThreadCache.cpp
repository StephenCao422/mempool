#include "ThreadCache.h"

void* ThreadCache::Allocate(size_t size){
    assert(size <= MAX_BYTES);

    size_t allignSize = SizeClass::RoundUp(size);
    size_t index = SizeClass::Index(allignSize);

    if(!_freeLists[index].Empty()){
        return _freeLists[index].Pop();
    }
    else {
        return FetchFromCentralCache(index, allignSize);
    }
}

void* ThreadCache::Deallocate(void* obj, size_t size){
    assert(obj);
    assert(size <= MAX_BYTES);

    size_t index = SizeClass::Index(size);
    _freeLists[index].Push(obj);
}

/*  ThreadCache fetch memory from CentralCache (Slow Start like TCP conjection control)
    Control current alignSize meomry for tc by MaxSize and NumMoveSize
    MaxSize: The max applied memory when FreeList once apply in index that not reach maximum
    NumMoveSize: max aligned memory of tc once apply for cc
    alignSize=8B, MaxSize 1B, NumMoveSize 512B, then batchNum=8B
    no limit then batchNum=MaxNum, reach limit then batchNum = NumMoveSize
*/
void* FetchFromCentralCache(size_t index, size_t alignSize){
    size_t batchNum = std::min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(alignSize));

    if(batchNum == _freeLists[index].MaxSize()){
        //if not reach max limit, next apply for this memory can apply one more 
        _freeList[index].MaxSize()++;
    }
    void* start = nullptr;
    void* end = nullptr;

    size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, alignSize);

    //FetchRangeObj can gruantee actualNum>=1
    assert(actualNum >= 1);
    
    if(actualNum ==1){
        assert(start==end);
        return start;//actual num =1, return start;
    } else{
        _freeLists[index].PushRange(ObjNext(start), end);
        return start;
    }
}

