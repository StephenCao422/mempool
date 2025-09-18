#pragma once
#include <cstddef>
#include <new>
#include <cstdlib>
#include <algorithm>
#include <mutex>


template <class T>
class object_pool {
    private:
        char* _memory = nullptr;
        void* _freelist = nullptr;
    size_t _remnantBytes = 0;

    public:
        std::mutex _poolMtx; //prevent tc get nullptr

    public:
        T* New(){

            T* obj = nullptr;

            if(_freelist != nullptr) {
                void* next = *(void**)_freelist;//hold addr of 2nd blk
                obj = (T*)_freelist;
                _freelist = next; // pop from freelist
            }
            else {
                if(_remnantBytes < sizeof(T)){
                    _remnantBytes = 128*1024;
                    _memory = (char*)malloc(_remnantBytes);
                    if(_memory==nullptr){
                        throw std::bad_alloc();
                    }
                }

                obj = (T*)_memory;
                size_t objSize = sizeof(T)<sizeof(void*) ? sizeof(void*) : sizeof(T);
                _memory += objSize;
                _remnantBytes -= objSize;
            }
            new(obj)T;
            return obj;
        }

        void Delete(T* obj){ //head insert

            obj->~T();

            *(void**)obj = _freelist;
            _freelist = obj;
        }


};