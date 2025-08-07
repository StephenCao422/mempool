#pragma once

#include <iostream>
using std::cout;
using std::endl;

template <class T>
class ObjectPool {
    private:
        char* _memory = nullptr;
        void* _freelist = nullptr;

    public:
        T* new(){
            T* obj = nullptr;

            if(_memory==nullptr){
                _memory = (char*)malloc(128*1024);
                if(_memory==nullptr){
                    throw std::bad_alloc();
                }
            }
            obj = (T*)_memory;
            _memory += sizeof(T);

            return obj;
        }

};