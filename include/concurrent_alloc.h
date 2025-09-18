#pragma once
#include "thread_cache.h"
#include "page_cache.h"
#include "central_cache.h"

// thread allocate memory (tcmalloc)
inline void* ConcurrentAlloc(size_t size)
{
    if(size>MAX_BYTES){
        size_t k = (size + ((1u << PAGE_SHIFT) - 1)) >> PAGE_SHIFT;
		if (k == 0) k = 1;

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(k);
		span->_objSize = size;
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		return ptr;
    }
    else {
        if(pTLSThreadCache == nullptr){
            // pTLSThreadCache = new ThreadCache;
            static ObjectPool<ThreadCache> objPool;
            objPool._poolMtx.lock();
            pTLSThreadCache = objPool.New();
            objPool._poolMtx.unlock();
        }
        return pTLSThreadCache->Allocate(size);
    }

}

// thread free memory
inline void ConcurrentFree(void* ptr)
{
	assert(ptr);
	Span* span = PageCache::GetInstance()->MapObjToSpan(ptr);
	size_t size = span->_objSize;

	if (size > MAX_BYTES)
	{
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else
	{
		pTLSThreadCache->Deallocate(ptr, size);
	}
}
