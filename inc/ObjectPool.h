#pragma once

#include <iostream>
using std::cout;
using std::endl;

template <class T>
class ObjectPool {
    private:
        char* _memory = nullptr;
        void* _freelist = nullptr;
        size_t _remanetBytes = 0;

    public:
        T* new(){

            T* obj = nullptr;

            if(_remaneBytes < sizeof(T)){
                _remanetBytes = 128*1024;
                _memory = (char*)malloc(_remanetBytes);
                if(_memory==nullptr){
                    throw std::bad_alloc();
                }
            }

            obj = (T*)_memory;
            _memory += sizeof(T);
            _remanetBytes -= sizeof(T);

            return obj;
        }

        void* delete(T* obj){

            if(_freelist ==nullptr){ //freelist is empty
                *(void**)obj = nullptr; //*(void**) get ptr size in diff sys
                _freelist = obj;
            }
            else{ //freelist not empty, head insert
                *(void**) obj = _freelist;
                _freelist = obj;
            }
        }

};