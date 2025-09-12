#include <common.h>


class ThreadCache 
{

public:
    void* Allocate(size_t size);
    void* Deallocate(void* obj, size_t size);
    void* FetchFromCentralCache(size_t size, size_t alignSize);
    void  LisrTooLong(FreeList& list, size_t size);

private:
    FreeList _freeLists[FREE_LIST_NUM]; //hash table of freelist 
};

static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
