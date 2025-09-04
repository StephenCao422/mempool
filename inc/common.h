#pragma once

#include <iostream>
#include <vector>
#include <mutex>
using std::vector; 
using std::cout;
using std::endl;

static const size_t FREE_LIST_NUM = 208; // num of free lists in hash table
static const size_t MAX_BYTES     = 256*1024; //tc max alloate byte once

static void*& ObjNext(void* obj){
    return *(void**)obj;
}

class FreeList
{
public:

    bool Empty(){
        return _freeList == nullptr;
    }

    void Push(void* obj){
        assert(obj);
        ObjNext(obj) = _freeList;
        _freeList = obj;
    }

    void* Pop(){
        assert(_freeList);
        void* obj = _freeList;
        _freeList = ObjNext(_freeList);
        return obj;
    }
private:
    void* _freeList = nullptr;
}

struct Span
{
    PageID _pageID = 0;
    size_t _n = 0;          //current page number manager by span

    void* _freeList = nullptr;  // head node of small space under each span
    size_t use_count = 0;   // number of block memory alloc by current span

    Span* _prev = nullptr;
    Span* _next = nullptr;
};

class SpanList
{
public:
    SpanList(){
        _head = new Span;
        _head->next = _head;
        _head->prev = _head;
    }

    void Insert(Span* pos, Span* ptr){
        //insert ptr before pos
        assert(pos);
        assert(ptr); //cannot be empty

        Span* prev = pos->_prev;
        prev->next = ptr;
        ptr->_prev = prev;

        ptr->next = pos;
        pos->prev = ptr;
    }

    void Erase(Span* pos){
        assert(pos);
        assert(ptr);

        Span* prev = pos->_prev;
        Span* next = pos->_next;

        prev->_next = next;
        next->_prev = prev; 
        //pos node not need delete,because pos node's Span need to recycle
    }

private:
    Span* _head;
    std::mutex _mtx;
}
