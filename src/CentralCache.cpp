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
    //cc have no non-empty span,bring a new span from pc
    size_t k = SizeClass::NumMovePage(size); //num of page need to bring from pc
    Span* span = PageCache::GetInstance()->NewSpan(k);
    
    char* start = (char*)(span->_pageID << PAGE_SHIFT);
    char* end = (char*)(start+ (span->_n << PAGE_SHIFT));

    //cut the span into small block
    span->freeList = start;
    void* tail - start;
    start += size;

    while(start<end){
        ObjNext(tail) = start;
        start += size;
        tail = ObjNext(tail);
    }
    ObjNext(tail) = nullptr;

    //insert the new span into spanlist
    list.PushFront(span);
    return span;
}
