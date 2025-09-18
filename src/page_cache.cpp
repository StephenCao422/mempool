#include "../include/page_cache.h"
#include <cassert>
#include <cstdint>

PageCache PageCache::_sInst; //Eager Singleton object

Span* PageCache::NewSpan(size_t k){
    assert(k>0);

    if (k > PAGE_NUM - 1) 
    {
        // void* ptr = SystemAlloc(k);
        void* ptr = SystemAlloc(k << PAGE_SHIFT);
        assert((((std::uintptr_t)ptr) & ((1ull << PAGE_SHIFT) - 1)) == 0 && "SystemAlloc must return 8KiB-aligned");
        Span* span = _spanPool.New();

        span->_pageID = ((PageID)ptr) >> PAGE_SHIFT;
        span->_n = k;
        span->_isUse = false;

        // map all pages to span
        // for(size_t i=0;i<span->_n;i++){
        //     _idSpanMap[span->_pageID + i] = span;
        // }
        _pagemap.set_range(span->_pageID, span->_n, span);
        return span;
    }

    //1. exact size list has span
    if(!_spanLists[k].Empty()){
        Span* span =  _spanLists[k].PopFront();
        // for(PageID i=0; i<span->_n; i++){
        //     _idSpanMap[span->_pageID+i]= span;
        // }
        _pagemap.set_range(span->_pageID, span->_n, span);
        return span;
    }
    //2. find larger span and split
    for(size_t i=k+1; i<PAGE_NUM; i++){
        if(!_spanLists[i].Empty()){
            Span* nSpan = _spanLists[i].PopFront();
            // split first k pages for kSpan
            Span* kSpan = _spanPool.New();
            kSpan->_pageID = nSpan->_pageID;
            kSpan->_n = k;
            kSpan->_isUse = false;

            nSpan->_pageID += k;
            nSpan->_n -= k;
            _spanLists[nSpan->_n].PushFront(nSpan);

            // map pages
            // for(PageID j=0; j<kSpan->_n; j++){
            //     _idSpanMap[kSpan->_pageID + j] = kSpan;
            // }
            _pagemap.set_range(kSpan->_pageID, kSpan->_n, kSpan);
            // map leftover edges for buddy merging (head & tail)
            // for(PageID j=0; j<nSpan->_n; j++){
            //     _idSpanMap[nSpan->_pageID + j] = nSpan;
            // }
            _pagemap.set_range(nSpan->_pageID, nSpan->_n, nSpan);
            return kSpan;
        }
    }
    //3. allocate large block (PAGE_NUM-1) pages then retry
    // void* ptr = SystemAlloc(PAGE_NUM-1);
    void* ptr = SystemAlloc((PAGE_NUM - 1) << PAGE_SHIFT);
    assert((((std::uintptr_t)ptr) & ((1ull << PAGE_SHIFT) - 1)) == 0 && "SystemAlloc must return 8KiB-aligned");
    Span* bigSpan =  _spanPool.New();
    bigSpan->_pageID = ((PageID)ptr)>>PAGE_SHIFT;
    bigSpan->_n = PAGE_NUM-1;
    bigSpan->_isUse = false;
    _spanLists[bigSpan->_n].PushFront(bigSpan);
    // for(PageID j=0;j<bigSpan->_n;j++){
    //     _idSpanMap[bigSpan->_pageID + j] = bigSpan;
    // }
    _pagemap.set_range(bigSpan->_pageID, bigSpan->_n, bigSpan);
    return NewSpan(k);
}

Span* PageCache::MapObjToSpan(void* obj){
    // PageID id = (PageID)obj >> PAGE_SHIFT;

    // std::unique_lock<std::mutex> lc(_pageMtx);

    // auto it = _idSpanMap.find(id);

    // if(it != _idSpanMap.end()){
    //     return it->second;
    // } else{
    //     fprintf(stderr, "[MapObjToSpan] Failed to map object %p pageID=%zu\n", obj, (size_t)id);
    //     assert(false && "MapObjToSpan: page id not found");
    //     return nullptr;
    // }
    PageID id = (PageID)obj >> PAGE_SHIFT;
    Span* s = _pagemap.get(id);
    if (s) return s;
    fprintf(stderr, "[MapObjToSpan] Failed to map object %p pageID=%zu\n", obj, (size_t)id);
    assert(false && "MapObjToSpan: page id not found");
    return nullptr;
}


void PageCache::ReleaseSpanToPageCache(Span* span){
    if (span->_n > PAGE_NUM - 1)
	{
		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
        SystemFree(ptr, span->_n << PAGE_SHIFT);
        // Erase mapping entries for big span pages
        // for(PageID i=0;i<span->_n;++i){
        //     _idSpanMap.erase(span->_pageID + i);
        // }
        _pagemap.set_range(span->_pageID, span->_n, nullptr);
        _spanPool.Delete(span);
        return;
	}


    while(1){
        // PageID leftID = span->_pageID -1;
        // auto it = _idSpanMap.find(leftID);

        // if(it == _idSpanMap.end()){
        //     break;
        // }

        // Span* leftSpan = it->second;
        Span* leftSpan = _pagemap.get(span->_pageID - 1);
         if (!leftSpan) break;

        if(leftSpan->_isUse == true){
            break;
        }

        if(leftSpan->_n + span->_n > PAGE_NUM -1){
            break;
        }

        PageID newStart = leftSpan->_pageID;
        size_t leftN = leftSpan->_n;

        // detach leftSpan from list
        _spanLists[leftSpan->_n].Erase(leftSpan);

        // update span to cover left + current
        span->_pageID = newStart;
        span->_n += leftN;

        // remap all pages covered by leftSpan to span
        // for(PageID i=0;i<leftN;++i){
        //     _idSpanMap[newStart + i] = span;
        // }
        _pagemap.set_range(newStart, leftN, span);
        _spanPool.Delete(leftSpan);
    }
    while(1){
        // PageID rightID = span->_pageID + span->_n;
        // auto it = _idSpanMap.find(rightID);

        // if(it == _idSpanMap.end()){
        //     break;
        // }

        // Span* rightSpan = it->second;
        Span* rightSpan = _pagemap.get(span->_pageID + span->_n);
        if (!rightSpan) break;

        if(rightSpan->_isUse == true){
            break;
        }

        if(rightSpan->_n + span->_n > PAGE_NUM -1){
            break;
        }

        size_t rightN = rightSpan->_n;

        _spanLists[rightSpan->_n].Erase(rightSpan);
        // extend span
        // for(PageID i=0;i<rightN;++i){
        //     _idSpanMap[rightSpan->_pageID + i] = span; // remap pages
        // }
        _pagemap.set_range(rightSpan->_pageID, rightN, span);
        span->_n += rightN;
        _spanPool.Delete(rightSpan);
    }
    _spanLists[span->_n].PushFront(span);
	span->_isUse = false;// back from cc to pc
    // Ensure mapping for boundary pages (already updated during merges but keep consistent)
    // for(PageID i=0;i<span->_n;++i){
    //     _idSpanMap[span->_pageID + i] = span;
    // }
    _pagemap.set_range(span->_pageID, span->_n, span);
}