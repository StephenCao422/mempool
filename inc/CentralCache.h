#pragma once
#include "common.h"

class CentralCache
{
public:
    static CentralCache* GetInstance(){
        return &_sInst;
    }

    // cc provide mem from self _spanLists for tc applied mem
    /*  start, end: cc provided mem begin and end
        n: mem size needed for tc
        size: single mem block needed for tc
        return actural mem provided by cc
    */
    size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t size);
    Span* GetOneSpan(SpanList& list, size_t size);

private:
    CentralCache(){}
    CentralCache(const CentralCache& copy) = delete;
    CentralCache& operator=(const CentralCache& copy) = delete;

private:
    SpanList _spanList[FREE_LIST_NUM]; //hash bucket each have span
    static CentralCache _sInst;//Eager Singleton

};