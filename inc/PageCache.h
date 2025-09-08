#pragma once

#include "common.h"

class PageCache
{
    public:
        static PageCache* GetInstance(){
            return &_sInst;
        }

    private: 
        SpanList _spanLists[PAGE_NUM]; //hash bucket each have span
        std::mutex _pageMtx;
    private:
        PageCache(){}
        PageCache(const PageCache& pc) = delete;
        PageCache& operator=(const PageCache& pc) = delete;

        static PageCache _sInst;//Eager Singleton
};

