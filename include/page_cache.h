#pragma once

#include "common.h"
#include "object_pool.h"
#include "page_map.h"

class page_cache
{
    public:
        static page_cache* get_instance(){
            return &_sInst;
        }

        Span* new_span(size_t k); //pc bring a k page span from _spanLists

        Span* map_obj_to_span(void* obj); //map obj to span

        void release_span_to_page_cache(Span* span); //release span to pc

        std::mutex _pageMtx;

    private: 
        span_list _spanLists[PAGE_NUM]; //hash bucket each have span

        // std::unordered_map<page_id, Span*> _idSpanMap; //hash map record pageID to span
        page_map _pagemap;

        object_pool<Span> _spanPool; //span object pool

    private:
        page_cache(){}
        page_cache(const page_cache& pc) = delete;
        page_cache& operator=(const page_cache& pc) = delete;

        static page_cache _sInst;//Eager Singleton
};

