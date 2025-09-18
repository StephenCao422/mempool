#pragma once

#include <iostream>
#include <vector>
#include <mutex>
#include <cassert>
#include <unordered_map>
#include "os_alloc.h"

using std::vector; 
using std::cout;
using std::endl;

static const size_t FREE_LIST_NUM = 208; // num of free lists in hash table
static const size_t MAX_BYTES     = 256*1024; //tc max alloate byte once
static const size_t PAGE_NUM     = 129; //max manage page num of span
static const size_t PAGE_SHIFT   = 13;  //8KB page


static inline void*& obj_next(void* obj){
    return *(void**)obj;
}

static inline void check_can_store_ptr(size_t block_size) {
    assert(block_size >= sizeof(void*) && "Block too small to store 'next' pointer");
    assert((block_size % 8) == 0 && "Small object must be >=8-byte aligned");
}

class free_list
{
public:
    size_t Size() const { return _size; }

    void PopRange(void*&start, void*& end, size_t n){
        assert(n<=_size);
        start = free_list_i;
        end = free_list_i;
        for(size_t i=0; i<n-1; i++){
            end = obj_next(end);
        }
        free_list_i = obj_next(end);
        obj_next(end) = nullptr;
        _size -= n;
    }

    void PushRange(void* start, void* end, size_t count){ //head insert multiple nodes
        obj_next(end) = free_list_i;
        free_list_i = start;
        _size += count;
    }

    bool Empty() const { return free_list_i == nullptr; }

    void Push(void* obj){
        assert(obj);
        obj_next(obj) = free_list_i;
        free_list_i = obj;
        _size++;
    }

    void* Pop(){
        assert(free_list_i);
        void* obj = free_list_i;
        free_list_i = obj_next(free_list_i);
        _size--;
        return obj;
    }

    size_t& MaxSize(){ return _maxSize; }

private:
    void* free_list_i = nullptr;
    size_t _maxSize = 1; //cur freelist apply not reach max, default 1
    size_t _size = 0; //how many obj in freelist
};

using page_id = size_t;

struct Span
{
    page_id page_id_i = 0;
    size_t _n = 0;          //current page number manager by span
    size_t obj_size = 0;   //size of single block memory alloc by span

    void* free_list_i = nullptr;  // head node of small space under each span
    size_t use_count = 0;   // number of block memory alloc by current span

    Span* _prev = nullptr;
    Span* _next = nullptr;

    bool is_use = false; //whether span is in cc or pc
};

class span_list
{
public:
    span_list(){
        _head = new Span;
        _head->_next = _head;
        _head->_prev = _head;
    }

    ~span_list(){ delete _head; }

    Span* pop_front(){
        Span* front = _head->_next;
        if(front == _head) return nullptr;
        Erase(front);
        return front;
    }

    bool Empty() const { return _head->_next == _head; }
    Span* Begin() const { return _head->_next; }
    Span* End()   const { return _head; }

    void push_front(Span* span){ Insert(_head->_next, span); }

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

class size_class
{
public:

    static size_t round_up_h(size_t size, size_t alignNum){
        return ((size+alignNum-1) & ~(alignNum-1)); // efficient use binary
    }


    static size_t round_up(size_t size){
        if(size <= 128){
            return round_up_h(size, 8);   //[1,128] 8B
        }
        else if(size <= 1024){
            return round_up_h(size, 16);  //[128+1,1024] 16B
        }
        else if(size <= 8*1024){
            return round_up_h(size, 128); //[1024+1,8*1024] 128B
        }
        else if(size <= 64*1024){
            return round_up_h(size, 1024); //[8*1024+1,64*1024] 1024B
        }
        else if(size <= 256*1024){
            return round_up_h(size, 8*1024);//[64*1024+1,256*1024] 8*1024B
        }else{
            return round_up_h(size, 8*1024); // >256KB, system alloc
        }
    }


    static inline size_t index_h(size_t size, size_t align_shift){
        return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
    }
    

    static inline size_t index(size_t size)
    {
        assert(size <= MAX_BYTES);

        static int group_array[4] = { 16, 56, 56, 56 };

        if (size <= 128){
            return index_h(size, 3);
        }
        else if (size <= 1024){
            return index_h(size - 128, 4) + group_array[0];
        }
        else if (size <= 8 * 1024){
            return index_h(size - 1024, 7) + group_array[1] + group_array[0];
        }
        else if (size <= 64 * 1024){
            return index_h(size - 8 * 1024, 10) + group_array[2] + group_array[1]
                + group_array[0];
        }
        else if (size <= 256 * 1024){
            return index_h(size - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
        }
        else{
            assert(false);
        }
        return -1;
    }

    static size_t num_move_size(size_t size){
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

    static size_t num_move_page(size_t size){
        size_t num = num_move_size(size);
        size_t npage = num*size;
        npage >>=PAGE_SHIFT;
        if(npage == 0){
            npage = 1;
        }
        return npage;
    }


};
