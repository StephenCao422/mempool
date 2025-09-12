#include "../inc/PageCache.h"

PageCache PageCache::_sInst; //Eager Singleton object

Span* NewSpan(size_t k){\
    assert(k>0 && k<=PAGE_NUM);

    // GetInstance()._pageMtx.lock();

    //1. kth bucket has span
    if(!_spanLists[k].Empty()){
        return _spanLists[k].PopFront();
    }
    //2. kth bucket not have span, but other bucket behide have span
    for(int i=k+1; i<PAGE_NUM; i++){
        if(!_spanLists[i].Empty()){
            Span* nSpan = _spanLists[i].PopFront();

            Span* kSpan = new Span;

            kSpan->_pageID = nSpan->_pageID;
            kSpan->_n = k;

            _spanLists[nSpan->n].PushFront(nSpan);
            return kSpan;
        }
        

    }
    //3. kth bucket and other bucket not have span
    void* ptr = SystemAlloc(PAGE_NUM-1);
    Span* bigSpan = new Span;
    
    bigSpan->_pageID = ((PageID)ptr)>>PAGE_SHIFT;
    bigSpan->_n = PAGE_NUM-1;

    _spanLIsts[PAGE_NUM-1].PushFront(bigSpan);
    return NewSpan(k);

}