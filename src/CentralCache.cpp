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
    return nullptr;
}
