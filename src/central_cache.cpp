#include "../include/central_cache.h"
#include "../include/page_cache.h"
#include <cassert>

central_cache central_cache::_sInst; //Eager Singleton object

size_t central_cache::fetch_range_obj(void*& start, void*& end, size_t batchNum, size_t size){
    size_t index = size_class::index(size); //get size corresponding to which span_list
    assert(index < FREE_LIST_NUM && "size class index out of range");

    _spanLists[index]._mtx.lock();
    Span* span = get_one_span(_spanLists[index], size);//get one non-empty span from spanlist
    assert(span);
    assert(span->free_list_i);

    start = end = span->free_list_i;
    size_t actualNum = 1; //actual num of mem block provided by cc

    while(actualNum < batchNum && obj_next(end) != nullptr){
        end = obj_next(end);
        actualNum++;
    }
    span->free_list_i = obj_next(end);
    span->use_count += actualNum;
    obj_next(end) = nullptr; //cut off the link

    _spanLists[index]._mtx.unlock();
    return actualNum;
}

Span* central_cache::get_one_span(span_list& list, size_t size){
    //find cc have non-empty span
    Span* it = list.Begin();
    while(it != list.End()){
        if(it->free_list_i != nullptr){
            return it;
        }
        else{
            it = it->_next;
        }
    }

    // (Removed manual unlock of list mutex to avoid double unlock issues)

    //cc have no non-empty span, bring a new span from pc
    size_t k = size_class::num_move_page(size); //num of page need to bring from pc

    // Lock page cache while getting a span (ordering: central bucket lock already held)
    page_cache::get_instance()->_pageMtx.lock();
    Span* span = page_cache::get_instance()->new_span(k);
    span->is_use = true; //mark span is in cc
    span->obj_size = size;
    page_cache::get_instance()->_pageMtx.unlock();
    
    char* start = (char*)(span->page_id_i << PAGE_SHIFT);
    char* end = (char*)(start+ (span->_n << PAGE_SHIFT));

    //cut the span into small block
    span->free_list_i = start;
    void* tail = start;
    start += size;
    
    int i =0;

    check_can_store_ptr(size);

    while(start<end){
        i++;
        obj_next(tail) = start;
        start += size;
        tail = obj_next(tail);
    }
    obj_next(tail) = nullptr;

    //insert the new span into spanlist (central cache mutex still held by caller)
    list.push_front(span);
    return span;
}

void central_cache::release_list_to_spans(void* start, size_t size){
    size_t index = size_class::index(size);
    std::unique_lock<std::mutex> lk(_spanLists[index]._mtx);

    while(start){
        void* next = obj_next(start);
        Span* span = page_cache::get_instance()->map_obj_to_span(start);
        obj_next(start) = span->free_list_i;
        span->free_list_i = start;
        span->use_count--;

        if(span->use_count == 0){
            _spanLists[index].Erase(span);
            span->free_list_i = nullptr;
            span->_next = nullptr;
            span->_prev = nullptr;

            lk.unlock();
            page_cache::get_instance()->_pageMtx.lock();
            page_cache::get_instance()->release_span_to_page_cache(span);
            page_cache::get_instance()->_pageMtx.unlock();
            lk.lock();
        }
        start = next;
    }
}
