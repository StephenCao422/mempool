#pragma once
#include "thread_cache.h"
#include "page_cache.h"
#include "central_cache.h"

// thread allocate memory (tcmalloc)
inline void* concurrent_alloc(size_t size)
{
    if(size>MAX_BYTES){
        size_t k = (size + ((1u << PAGE_SHIFT) - 1)) >> PAGE_SHIFT;
		if (k == 0) k = 1;

		page_cache::get_instance()->_pageMtx.lock();
		Span* span = page_cache::get_instance()->new_span(k);
		span->obj_size = size;
		page_cache::get_instance()->_pageMtx.unlock();

		void* ptr = (void*)(span->page_id_i << PAGE_SHIFT);
		return ptr;
    }
    else {
        if(pTLSThreadCache == nullptr){
            // pTLSThreadCache = new thread_cache;
            static object_pool<thread_cache> objPool;
            objPool._poolMtx.lock();
            pTLSThreadCache = objPool.New();
            objPool._poolMtx.unlock();
        }
        return pTLSThreadCache->Allocate(size);
    }

}

// thread free memory
inline void concurrent_free(void* ptr)
{
	assert(ptr);
	Span* span = page_cache::get_instance()->map_obj_to_span(ptr);
	size_t size = span->obj_size;

	if (size > MAX_BYTES)
	{
		page_cache::get_instance()->_pageMtx.lock();
		page_cache::get_instance()->release_span_to_page_cache(span);
		page_cache::get_instance()->_pageMtx.unlock();
	}
	else
	{
		pTLSThreadCache->Deallocate(ptr, size);
	}
}
