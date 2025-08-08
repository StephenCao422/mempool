#pragma once

#include <iostream>
#include <vector>
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