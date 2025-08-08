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
            assert(false);
            return -1;
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


};