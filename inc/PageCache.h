#pragma once

#include "common.h"

class PageCache
{
    public:
        static PageCache* GetInstance(){
            return &_sInst;
        }

        Span* NewSpan(size_t k); //pc bring a k page span from _spanLists

        std::mutex _pageMtx;

    private: 
        SpanList _spanLists[PAGE_NUM]; //hash bucket each have span

        std::unordered_map<PageID, Span*> _idSpanMap; //hash map record pageID to span

    private:
        PageCache(){}
        PageCache(const PageCache& pc) = delete;
        PageCache& operator=(const PageCache& pc) = delete;

        static PageCache _sInst;//Eager Singleton
};

