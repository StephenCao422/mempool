#include "../inc/CentralCache.h"

CentralCache CentralCache::_sInst; //Eager Singleton object

size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size){
    size_t index = SizeClass::Index(size); //get size corresponding to which SpanList

    _spanLists[index]._mtx.lock();//add lock when cc operate on _spanLists

    Span* span = GetOnceSpan(_spanLists[index], size);//get one non-empty span from spanlist
    assert(span);
    assert(span->_freeList);

    start = end = span->_freeList;
    size_t actualNum = 1; //actual num of mem block provided by cc

    size_t i=0;
    while(i<batchNum-1 && ObjNext(end) != nullptr){
        end = ObjNext(end);
        actualNum++;
        i++;
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

    list._mtx.unlock(); //temp unlock cc mtx (pc can get mtx)

    //cc have no non-empty span,bring a new span from pc
    size_t k = SizeClass::NumMovePage(size); //num of page need to bring from pc

    //avoid deadlock in recursive
    PageCache:: GetInstance()._pageMtx.lock();
    Span* span = PageCache::GetInstance()->NewSpan(k);
    span->_isUse = true; //mark span is in cc
    PageCache:: GetInstance()._pageMtx.unlock();
    
    char* start = (char*)(span->_pageID << PAGE_SHIFT);
    char* end = (char*)(start+ (span->_n << PAGE_SHIFT));

    //cut the span into small block
    span->freeList = start;
    void* tail - start;
    start += size;
    
    int i =0;

    while(start<end){
        i++;
        ObjNext(tail) = start;
        start += size;
        tail = ObjNext(tail);
    }
    ObjNext(tail) = nullptr;

    list._mtx.lock(); //lock cc mtx back after pc mtx unlock

    //insert the new span into spanlist
    list.PushFront(span);
    return span;
}

void ReleaseListToSpans(void* start, size_t size){
    size_t index = SizeClass::Index(size);

    _spanLists[index]._mtx.lock();

    _spanLists[index]._mtx.unlock();

    while(start){
        void* next = ObjNext(start);
        
        Span* span = PageCache::GetInstance()->MapObjToSpan(start);

        ObjNext(start) = span->_freeList;
        span->_freeList = start;

        span->use_count--;

        if(span->use_count == 0){
            //give span to pc to manage
            _spanLists[index].Erase(span);
            span->_freeList = nullptr;
            span->_next = nullptr;
            span->_prev = nullptr;

            _spanLists[index]._mtx.unlock();

            PageCache::GetInstance()->_pageMtx.lock();
            PageCache::GetInstance()->ReleaseSpanToPageCache(span);
            PageCache::GetInstance()->_pageMtx.unlock();

            _spanLists[index]._mtx.lock();
        }

        start = next;
    }
}
