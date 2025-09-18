#pragma once
#include "common.h"

class central_cache
{
public:
    static central_cache* get_instance(){
        return &_sInst;
    }

    // cc provide mem from self _spanLists for tc applied mem
    /*  start, end: cc provided mem begin and end
        n: mem size needed for tc
        size: single mem block needed for tc
        return actural mem provided by cc
    */
    size_t fetch_range_obj(void*& start, void*& end, size_t n, size_t size);
    Span* get_one_span(span_list& list, size_t size);

    void release_list_to_spans(void* start, size_t size);

private:
    central_cache(){}
    central_cache(const central_cache& copy) = delete;
    central_cache& operator=(const central_cache& copy) = delete;

private:
    span_list _spanLists[FREE_LIST_NUM]; //hash bucket each have span
    static central_cache _sInst;//Eager Singleton

};