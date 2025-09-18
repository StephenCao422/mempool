#include "../include/page_cache.h"
#include <cassert>
#include <cstdint>

page_cache page_cache::_sInst; //Eager Singleton object

Span* page_cache::new_span(size_t k){
    assert(k>0);

    if (k > PAGE_NUM - 1) 
    {
        // void* ptr = system_alloc(k);
        void* ptr = system_alloc(k << PAGE_SHIFT);
        assert((((std::uintptr_t)ptr) & ((1ull << PAGE_SHIFT) - 1)) == 0 && "system_alloc must return 8KiB-aligned");
        Span* span = _spanPool.New();

        span->page_id_i = ((page_id)ptr) >> PAGE_SHIFT;
        span->_n = k;
        span->is_use = false;

        // map all pages to span
        // for(size_t i=0;i<span->_n;i++){
        //     _idSpanMap[span->page_id_i + i] = span;
        // }
        _pagemap.set_range(span->page_id_i, span->_n, span);
        return span;
    }

    //1. exact size list has span
    if(!_spanLists[k].Empty()){
        Span* span =  _spanLists[k].pop_front();
        // for(page_id i=0; i<span->_n; i++){
        //     _idSpanMap[span->page_id_i+i]= span;
        // }
        _pagemap.set_range(span->page_id_i, span->_n, span);
        return span;
    }
    //2. find larger span and split
    for(size_t i=k+1; i<PAGE_NUM; i++){
        if(!_spanLists[i].Empty()){
            Span* nSpan = _spanLists[i].pop_front();
            // split first k pages for kSpan
            Span* kSpan = _spanPool.New();
            kSpan->page_id_i = nSpan->page_id_i;
            kSpan->_n = k;
            kSpan->is_use = false;

            nSpan->page_id_i += k;
            nSpan->_n -= k;
            _spanLists[nSpan->_n].push_front(nSpan);

            // map pages
            // for(page_id j=0; j<kSpan->_n; j++){
            //     _idSpanMap[kSpan->page_id_i + j] = kSpan;
            // }
            _pagemap.set_range(kSpan->page_id_i, kSpan->_n, kSpan);
            // map leftover edges for buddy merging (head & tail)
            // for(page_id j=0; j<nSpan->_n; j++){
            //     _idSpanMap[nSpan->page_id_i + j] = nSpan;
            // }
            _pagemap.set_range(nSpan->page_id_i, nSpan->_n, nSpan);
            return kSpan;
        }
    }
    //3. allocate large block (PAGE_NUM-1) pages then retry
    // void* ptr = system_alloc(PAGE_NUM-1);
    void* ptr = system_alloc((PAGE_NUM - 1) << PAGE_SHIFT);
    assert((((std::uintptr_t)ptr) & ((1ull << PAGE_SHIFT) - 1)) == 0 && "system_alloc must return 8KiB-aligned");
    Span* bigSpan =  _spanPool.New();
    bigSpan->page_id_i = ((page_id)ptr)>>PAGE_SHIFT;
    bigSpan->_n = PAGE_NUM-1;
    bigSpan->is_use = false;
    _spanLists[bigSpan->_n].push_front(bigSpan);
    // for(page_id j=0;j<bigSpan->_n;j++){
    //     _idSpanMap[bigSpan->page_id_i + j] = bigSpan;
    // }
    _pagemap.set_range(bigSpan->page_id_i, bigSpan->_n, bigSpan);
    return new_span(k);
}

Span* page_cache::map_obj_to_span(void* obj){
    // page_id id = (page_id)obj >> PAGE_SHIFT;

    // std::unique_lock<std::mutex> lc(_pageMtx);

    // auto it = _idSpanMap.find(id);

    // if(it != _idSpanMap.end()){
    //     return it->second;
    // } else{
    //     fprintf(stderr, "[map_obj_to_span] Failed to map object %p pageID=%zu\n", obj, (size_t)id);
    //     assert(false && "map_obj_to_span: page id not found");
    //     return nullptr;
    // }
    page_id id = (page_id)obj >> PAGE_SHIFT;
    Span* s = _pagemap.get(id);
    if (s) return s;
    fprintf(stderr, "[map_obj_to_span] Failed to map object %p pageID=%zu\n", obj, (size_t)id);
    assert(false && "map_obj_to_span: page id not found");
    return nullptr;
}


void page_cache::release_span_to_page_cache(Span* span){
    if (span->_n > PAGE_NUM - 1)
	{
		void* ptr = (void*)(span->page_id_i << PAGE_SHIFT);
        system_free(ptr, span->_n << PAGE_SHIFT);
        // Erase mapping entries for big span pages
        // for(page_id i=0;i<span->_n;++i){
        //     _idSpanMap.erase(span->page_id_i + i);
        // }
        _pagemap.set_range(span->page_id_i, span->_n, nullptr);
        _spanPool.Delete(span);
        return;
	}


    while(1){
        // page_id leftID = span->page_id_i -1;
        // auto it = _idSpanMap.find(leftID);

        // if(it == _idSpanMap.end()){
        //     break;
        // }

        // Span* leftSpan = it->second;
        Span* leftSpan = _pagemap.get(span->page_id_i - 1);
         if (!leftSpan) break;

        if(leftSpan->is_use == true){
            break;
        }

        if(leftSpan->_n + span->_n > PAGE_NUM -1){
            break;
        }

        page_id newStart = leftSpan->page_id_i;
        size_t leftN = leftSpan->_n;

        // detach leftSpan from list
        _spanLists[leftSpan->_n].Erase(leftSpan);

        // update span to cover left + current
        span->page_id_i = newStart;
        span->_n += leftN;

        // remap all pages covered by leftSpan to span
        // for(page_id i=0;i<leftN;++i){
        //     _idSpanMap[newStart + i] = span;
        // }
        _pagemap.set_range(newStart, leftN, span);
        _spanPool.Delete(leftSpan);
    }
    while(1){
        // page_id rightID = span->page_id_i + span->_n;
        // auto it = _idSpanMap.find(rightID);

        // if(it == _idSpanMap.end()){
        //     break;
        // }

        // Span* rightSpan = it->second;
        Span* rightSpan = _pagemap.get(span->page_id_i + span->_n);
        if (!rightSpan) break;

        if(rightSpan->is_use == true){
            break;
        }

        if(rightSpan->_n + span->_n > PAGE_NUM -1){
            break;
        }

        size_t rightN = rightSpan->_n;

        _spanLists[rightSpan->_n].Erase(rightSpan);
        // extend span
        // for(page_id i=0;i<rightN;++i){
        //     _idSpanMap[rightSpan->page_id_i + i] = span; // remap pages
        // }
        _pagemap.set_range(rightSpan->page_id_i, rightN, span);
        span->_n += rightN;
        _spanPool.Delete(rightSpan);
    }
    _spanLists[span->_n].push_front(span);
	span->is_use = false;// back from cc to pc
    // Ensure mapping for boundary pages (already updated during merges but keep consistent)
    // for(page_id i=0;i<span->_n;++i){
    //     _idSpanMap[span->page_id_i + i] = span;
    // }
    _pagemap.set_range(span->page_id_i, span->_n, span);
}