#include <common.h>


class ThreadCache 
{

public:
    void* Allocate(size_t size);
    void* Deallocate(void* obj, size_t size);

private:
    FreeList _freeLists[FREE_LIST_NUM]; //hash table of freelist 


};

class SizeClass
{
public:

    static size_t _RoundUp(size_t size, size_t alignNum){
        size_t res = 0;
        if(size % alignNum != 0){
            res = (size / alignNum +1)*alignNum;
        } else{
            res = size;
        }
        return res;
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
            assert(false);
            return -1;
        }
    }
};