#pragma once
#include <cstddef>
#include <new>
#include <cstdlib>
#include <algorithm>


template <class T>
class ObjectPool {
    private:
        char* _memory = nullptr;
        void* _freelist = nullptr;
        size_t _remanetBytes = 0;

    public:
        T* New(){

            T* obj = nullptr;

            if(_freelist != nullptr) {
                void* next = *(void**)_freelist;//hold addr of 2nd blk
                obj = (T*)_freelist;
                _freelist = next; // pop from freelist
            }
            else {
                if(_remanetBytes < sizeof(T)){
                    _remanetBytes = 128*1024;
                    _memory = (char*)malloc(_remanetBytes);
                    if(_memory==nullptr){
                        throw std::bad_alloc();
                    }
                }

                obj = (T*)_memory;
                size_t objSize = sizeof(T)<sizeof(void*) ? sizeof(void*) : sizeof(T);
                _memory += objSize;
                _remanetBytes -= objSize;
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