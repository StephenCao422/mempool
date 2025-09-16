#pragma once

#include "common.h"
#include "ObjectPool.h"

class PageCache
{
    public:
        static PageCache* GetInstance(){
            return &_sInst;
        }

        Span* NewSpan(size_t k); //pc bring a k page span from _spanLists

        Span* MapObjToSpan(void* obj); //map obj to span

        void ReleaseSpanToPageCache(Span* span); //release span to pc

        std::mutex _pageMtx;

    private: 
        SpanList _spanLists[PAGE_NUM]; //hash bucket each have span

        std::unordered_map<PageID, Span*> _idSpanMap; //hash map record pageID to span

        ObjectPool<Span> _spanPool; //span object pool

    private:
        PageCache(){}
        PageCache(const PageCache& pc) = delete;
        PageCache& operator=(const PageCache& pc) = delete;

        static PageCache _sInst;//Eager Singleton
};

