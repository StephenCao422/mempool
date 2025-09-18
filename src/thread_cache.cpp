#include "../include/thread_cache.h"
#include "../include/central_cache.h"
#include "../include/page_cache.h"
#include <algorithm>
thread_local thread_cache* pTLSThreadCache = nullptr;

void* thread_cache::Allocate(size_t size){
    assert(size <= MAX_BYTES);

    size_t alignSize = size_class::round_up(size);
    size_t index = size_class::index(alignSize);
    assert(index < FREE_LIST_NUM && "size class index out of range");

    if(!free_lists_i[index].Empty()){
        return free_lists_i[index].Pop();
    }
    else {
        return fetch_from_cc(index, alignSize);
    }
}

void thread_cache::Deallocate(void* obj, size_t size){
    assert(obj);
    assert(size <= MAX_BYTES);

    size_t index = size_class::index(size);
    assert(index < FREE_LIST_NUM && "size class index out of range");

    free_lists_i[index].Push(obj);
    // Optional: if list too long return some to central
}

/*  thread_cache fetch memory from central_cache (Slow Start like TCP conjection control)
    Control current alignSize meomry for tc by MaxSize and num_move_size
    MaxSize: The max applied memory when free_list once apply in index that not reach maximum
    num_move_size: max aligned memory of tc once apply for cc
    alignSize=8B, MaxSize 1B, num_move_size 512B, then batchNum=8B
    no limit then batchNum=MaxNum, reach limit then batchNum = num_move_size
*/
void* thread_cache::fetch_from_cc(size_t index, size_t alignSize){
    size_t batchNum = std::min(free_lists_i[index].MaxSize(), size_class::num_move_size(alignSize));
    if(batchNum == free_lists_i[index].MaxSize()){
        free_lists_i[index].MaxSize()++;
    }
    void* start = nullptr;
    void* end = nullptr;
    size_t actualNum = central_cache::get_instance()->fetch_range_obj(start, end, batchNum, alignSize);
    assert(actualNum >= 1);
    if(actualNum == 1){
        return start;
    } else {
        free_lists_i[index].PushRange(obj_next(start), end, actualNum-1);
        return start;
    }
}

void thread_cache::list_too_long(free_list& list, size_t size){
    void* start = nullptr;
    void* end = nullptr;
    size_t batch = std::min(list.MaxSize(), list.Size());
    if(batch == 0) return;
    list.PopRange(start, end, batch);
    central_cache::get_instance()->release_list_to_spans(start, size);
}
