#pragma once

#include <iostream>
#include <vector>
#include <mutex>
#include <cassert>
#include <unordered_map>
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif
using std::vector; 
using std::cout;
using std::endl;

static const size_t FREE_LIST_NUM = 208; // num of free lists in hash table
static const size_t MAX_BYTES     = 256*1024; //tc max alloate byte once
static const size_t PAGE_NUM     = 129; //max manage page num of span
static const size_t PAGE_SHIFT   = 13;  //8KB page

#ifdef _WIN32
    #include <windows.h>
#else

#endif // _WIN32

inline static void* SystemAlloc(size_t kpage){
#ifdef _WIN32
    void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if(ptr == nullptr) throw std::bad_alloc();
    return ptr;
#else
    size_t bytes = kpage << PAGE_SHIFT;
    void* ptr = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(ptr == MAP_FAILED) throw std::bad_alloc();
    return ptr;
#endif
}

inline static void SystemFree(void* ptr, size_t kpage = 0)
{
#ifdef _WIN32
    (void)kpage;
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    if(ptr){
        if(kpage){
            munmap(ptr, kpage << PAGE_SHIFT);
        }
        // If kpage not provided we cannot safely munmap size; caller should supply.
    }
#endif
}

static void*& ObjNext(void* obj){
    return *(void**)obj;
}

class FreeList
{
public:
    size_t Size() const { return _size; }

    void PopRange(void*&start, void*& end, size_t n){
        assert(n<=_size);
        start = _freeList;
        end = _freeList;
        for(size_t i=0; i<n-1; i++){
            end = ObjNext(end);
        }
        _freeList = ObjNext(end);
        ObjNext(end) = nullptr;
        _size -= n;
    }

    void PushRange(void* start, void* end, size_t count){ //head insert multiple nodes
        ObjNext(end) = _freeList;
        _freeList = start;
        _size += count;
    }

    bool Empty() const { return _freeList == nullptr; }

    void Push(void* obj){
        assert(obj);
        ObjNext(obj) = _freeList;
        _freeList = obj;
        _size++;
    }

    void* Pop(){
        assert(_freeList);
        void* obj = _freeList;
        _freeList = ObjNext(_freeList);
        _size--;
        return obj;
    }

    size_t& MaxSize(){ return _maxSize; }

private:
    void* _freeList = nullptr;
    size_t _maxSize = 1; //cur freelist apply not reach max, default 1
    size_t _size = 0; //how many obj in freelist
};

using PageID = size_t;

struct Span
{
    PageID _pageID = 0;
    size_t _n = 0;          //current page number manager by span
    size_t _objSize = 0;   //size of single block memory alloc by span

    void* _freeList = nullptr;  // head node of small space under each span
    size_t use_count = 0;   // number of block memory alloc by current span

    Span* _prev = nullptr;
    Span* _next = nullptr;

    bool _isUse = false; //whether span is in cc or pc
};

class SpanList
{
public:
    SpanList(){
        _head = new Span;
        _head->_next = _head;
        _head->_prev = _head;
    }

    ~SpanList(){ delete _head; }

    Span* PopFront(){
        Span* front = _head->_next;
        if(front == _head) return nullptr;
        Erase(front);
        return front;
    }

    bool Empty() const { return _head->_next == _head; }
    Span* Begin() const { return _head->_next; }
    Span* End()   const { return _head; }

    void PushFront(Span* span){ Insert(_head->_next, span); }

    void Insert(Span* pos, Span* ptr){
        assert(pos);
        assert(ptr);
        Span* prev = pos->_prev;
        prev->_next = ptr;
        ptr->_prev = prev;
        ptr->_next = pos;
        pos->_prev = ptr;
    }

    void Erase(Span* pos){
        assert(pos);
        if(pos == _head) return; // do not remove head sentinel
        Span* prev = pos->_prev;
        Span* next = pos->_next;
        prev->_next = next;
        next->_prev = prev;
    }

    std::mutex _mtx; // public for external locking patterns

private:
    Span* _head;
};

class SizeClass
{
public:

    static size_t _RoundUp(size_t size, size_t alignNum){
        return ((size+alignNum-1) & ~(alignNum-1)); // efficient use binary
    }


    static size_t RoundUp(size_t size){
        if(size <= 128){
            return _RoundUp(size, 8);   //[1,128] 8B
        }
        else if(size <= 1024){
            return _RoundUp(size, 16);  //[128+1,1024] 16B
        }
        else if(size <= 8*1024){
            return _RoundUp(size, 128); //[1024+1,8*1024] 128B
        }
        else if(size <= 64*1024){
            return _RoundUp(size, 1024); //[8*1024+1,64*1024] 1024B
        }
        else if(size <= 256*1024){
            return _RoundUp(size, 8*1024);//[64*1024+1,256*1024] 8*1024B
        }else{
            return _RoundUp(size, 8*1024); // >256KB, system alloc
        }
    }


    static inline size_t _Index(size_t size, size_t align_shift){
        return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
    }
    

    static inline size_t Index(size_t size)
    {
        assert(size <= MAX_BYTES);

        static int group_array[4] = { 16, 56, 56, 56 };

        if (size <= 128){
            return _Index(size, 3);
        }
        else if (size <= 1024){
            return _Index(size - 128, 4) + group_array[0];
        }
        else if (size <= 8 * 1024){
            return _Index(size - 1024, 7) + group_array[1] + group_array[0];
        }
        else if (size <= 64 * 1024){
            return _Index(size - 8 * 1024, 10) + group_array[2] + group_array[1]
                + group_array[0];
        }
        else if (size <= 256 * 1024){
            return _Index(size - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
        }
        else{
            assert(false);
        }
        return -1;
    }

    static size_t NumMoveSize(size_t size){
        assert(size >0); //cannot apply 0 byte

        int num = MAX_BYTES / size; // MAX_BYTES is max space of single block 256KB

        if(num>512){
            num = 512; 
        }
        if(num<2){
            num = 2;
        }

        return num;
    }

    static size_t NumMovePage(size_t size){
        size_t num = NumMoveSize(size);
        size_t npage = num*size;
        npage >>=PAGE_SHIFT;
        if(npage == 0){
            npage = 1;
        }
        return npage;
    }


};
