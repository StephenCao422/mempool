#pragma once
#include "common.h"
#include "central_cache.h"


class thread_cache 
{
public:
    void* Allocate(size_t size);
    void  Deallocate(void* obj, size_t size);
    void* fetch_from_cc(size_t index, size_t alignSize);
    void  list_too_long(free_list& list, size_t size);

private:
    free_list free_lists_i[FREE_LIST_NUM]; //hash table of freelist 
};

extern thread_local thread_cache* pTLSThreadCache;
