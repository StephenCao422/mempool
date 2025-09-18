#include "../inc/CentralCache.h"
#include "../inc/PageCache.h"
#include <cassert>

CentralCache CentralCache::_sInst; //Eager Singleton object

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size){
    size_t index = SizeClass::Index(size); //get size corresponding to which SpanList
    assert(index < FREE_LIST_NUM && "size class index out of range");

    _spanLists[index]._mtx.lock();
    Span* span = GetOneSpan(_spanLists[index], size);//get one non-empty span from spanlist
    assert(span);
    assert(span->_freeList);

    start = end = span->_freeList;
    size_t actualNum = 1; //actual num of mem block provided by cc

    while(actualNum < batchNum && ObjNext(end) != nullptr){
        end = ObjNext(end);
        actualNum++;
    }
    span->_freeList = ObjNext(end);
    span->use_count += actualNum;
    ObjNext(end) = nullptr; //cut off the link

    _spanLists[index]._mtx.unlock();
    return actualNum;
}

Span* CentralCache::GetOneSpan(SpanList& list, size_t size){
    //find cc have non-empty span
    Span* it = list.Begin();
    while(it != list.End()){
        if(it->_freeList != nullptr){
            return it;
        }
        else{
            it = it->_next;
        }
    }

    // (Removed manual unlock of list mutex to avoid double unlock issues)

    //cc have no non-empty span, bring a new span from pc
    size_t k = SizeClass::NumMovePage(size); //num of page need to bring from pc

    // Lock page cache while getting a span (ordering: central bucket lock already held)
    PageCache::GetInstance()->_pageMtx.lock();
    Span* span = PageCache::GetInstance()->NewSpan(k);
    span->_isUse = true; //mark span is in cc
    span->_objSize = size;
    PageCache::GetInstance()->_pageMtx.unlock();
    
    char* start = (char*)(span->_pageID << PAGE_SHIFT);
    char* end = (char*)(start+ (span->_n << PAGE_SHIFT));

    //cut the span into small block
    span->_freeList = start;
    void* tail = start;
    start += size;
    
    int i =0;

    CheckCanStorePtr(size);

    while(start<end){
        i++;
        ObjNext(tail) = start;
        start += size;
        tail = ObjNext(tail);
    }
    ObjNext(tail) = nullptr;

    //insert the new span into spanlist (central cache mutex still held by caller)
    list.PushFront(span);
    return span;
}

void CentralCache::ReleaseListToSpans(void* start, size_t size){
    size_t index = SizeClass::Index(size);
    std::unique_lock<std::mutex> lk(_spanLists[index]._mtx);

    while(start){
        void* next = ObjNext(start);
        Span* span = PageCache::GetInstance()->MapObjToSpan(start);
        ObjNext(start) = span->_freeList;
        span->_freeList = start;
        span->use_count--;

        if(span->use_count == 0){
            _spanLists[index].Erase(span);
            span->_freeList = nullptr;
            span->_next = nullptr;
            span->_prev = nullptr;

            lk.unlock();
            PageCache::GetInstance()->_pageMtx.lock();
            PageCache::GetInstance()->ReleaseSpanToPageCache(span);
            PageCache::GetInstance()->_pageMtx.unlock();
            lk.lock();
        }
        start = next;
    }
}
