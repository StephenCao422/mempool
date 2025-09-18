#pragma once
#include "common.h"
#include "central_cache.h"


class ThreadCache 
{
public:
    void* Allocate(size_t size);
    void  Deallocate(void* obj, size_t size);
    void* FetchFromCentralCache(size_t index, size_t alignSize);
    void  ListTooLong(FreeList& list, size_t size);

private:
    FreeList _freeLists[FREE_LIST_NUM]; //hash table of freelist 
};

extern thread_local ThreadCache* pTLSThreadCache;
