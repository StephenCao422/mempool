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

void* FetchFromCentralCache(size_t size, size_t alignSize){
    return nullptr; 
}
