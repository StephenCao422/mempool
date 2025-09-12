#pragma once

#include <iostream>
#include <vector>
#include <mutex>
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
    void* ptr = VirtualAlloc(0, kpage<<13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
#endif
    if(ptr == NULL){
        throw std::bad_alloc();
    }
    return ptr;
}

inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
#endif
}

static void*& ObjNext(void* obj){
    return *(void**)obj;
}

class FreeList
{
public:
    size_t Size(){
        return _size;
    }

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

    void PushRange(void* start, void* end, size_t size){ //head insert multiple nodes
        ObjNext(end) = _freeList;
        _freeList = start;
        _size += size;
    }

    bool Empty(){
        return _freeList == nullptr;
    }

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

    size_t& MaxSize(){
        return _maxSize;
    }

private:
    void* _freeList = nullptr;
    size_t _maxSize = 1; //cur freelist apply not reach max, default 1
    size_t _size = 0; //how many obj in freelist
}

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
    Span* PopFront(){
        Span* front = _head->next;
        Erase(front);
        return front;
    }

    bool Empty(){
        return _head->next == _head;
    }

    Span* Begin(){
        return _head->next;
    }
    Span* End(){
        return _head;
    }
    std::mutex _mtx;
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
}

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
