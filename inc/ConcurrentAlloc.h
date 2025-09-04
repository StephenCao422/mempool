#pragma once
#include "ThreadCache.h"

// thread allocate memory (tcmalloc)
void* ConcurrentAlloc(size_t size)
{
    if(pTLSThreadCache == nullptr){
        pTLSThreadCache = new ThreadCache;
    }
    return pTLSThreadCache->Allocate(size);
}

// thread free memory
void* ConcurrentFree(void* ptr)
{

}
