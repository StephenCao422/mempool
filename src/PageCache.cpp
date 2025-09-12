#include "../inc/PageCache.h"

PageCache PageCache::_sInst; //Eager Singleton object

Span* NewSpan(size_t k){\
    assert(k>0);

    // GetInstance()._pageMtx.lock();

    if (k > PAGE_NUM - 1) 
	{
		void* ptr = SystemAlloc(k);
		Span* span = _spanPool.New();
		
		span->_pageID = ((PageID)ptr >> PAGE_SHIFT);
		span->_n = k;

		_idSpanMap.set(span->_pageID, span);

		return span;
	}

    //1. kth bucket has span
    if(!_spanLists[k].Empty()){
        Span* span =  _spanLists[k].PopFront();

        for(PageID i=0; i<span->_n; i++){
            _idSpanMap[span->_pageID+i]= span;
        }
        return span;
    }
    //2. kth bucket not have span, but other bucket behide have span
    for(int i=k+1; i<PAGE_NUM; i++){
        if(!_spanLists[i].Empty()){
            Span* nSpan = _spanLists[i].PopFront();

            Span* kSpan = _spanPool.New(); //new Span object from pool

            kSpan->_pageID = nSpan->_pageID;
            kSpan->_n = k;

            _spanLists[nSpan->n].PushFront(nSpan);

            // n-k page span map to span
            _idSpanMap[nSpan->_pageID] = nSpan;
            _idSpanMap[nSpan->_pageID + nSpan->_n -1] = nSpan;

            for(PageID i=0; i<kSpan->_n; i++){
                _idSpanMap[kSpan->_pageID+i]= kSpan;
            }

            return kSpan;
        }
        

    }
    //3. kth bucket and other bucket not have span
    void* ptr = SystemAlloc(PAGE_NUM-1);
    Span* bigSpan =  _spanPool.New();
    
    bigSpan->_pageID = ((PageID)ptr)>>PAGE_SHIFT;
    bigSpan->_n = PAGE_NUM-1;

    _spanLIsts[PAGE_NUM-1].PushFront(bigSpan);
    return NewSpan(k);

}

Span* PageCache::MapObjToSpan(void* obj){
    PageID id = (PageID)obj >> PAGE_SHIFT;

    std::unique_lock<std::mutex> lc(_pageMtx);

    auto it = _idSpanMap.find(id);

    if(it != _idSpanMap.end()){
        return it->second;
    } else{
        assert(false);
        return nullptr;
    }
}


void ReleaseSpanToPageCache(Span* span){
    if (span->_n > PAGE_NUM - 1)
	{
		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		SystemFree(ptr);
		_spanPool.Delete(span);

		return;
	}


    while(1){
        PageID leftID = span->_pageID -1;
        auto it = _idSpanMap.find(leftID);

        if(it == _idSpanMap.end()){
            break;
        }

        Span* leftSpan = it->second;

        if(leftSpan->_isUsed == true){
            break;
        }

        if(leftSpan->_n + span->_n > PAGE_NUM -1){
            break;
        }

        span->_pageID = leftSpan->_pageID;
        span->_n += leftSpan->_n;

        _spanLists[leftSpan->_n].Erase(leftSpan);
        // delete leftSpan;?
        _spanPool.Delete(leftSpan);
    }
    while(1){
        PageID rightID = span->_pageID + span->_n;
        auto it = _idSpanMap.find(rightID);

        if(it == _idSpanMap.end()){
            break;
        }

        Span* rightSpan = it->second;

        if(rightSpan->_isUsed == true){
            break;
        }

        if(rightSpan->_n + span->_n > PAGE_NUM -1){
            break;
        }

        span->_n += rightSpan->_n;

        _spanLists[rightSpan->_n].Erase(rightSpan);
        // delete rightSpan;
        _spanPool.Delete(rightSpan);
    }
    _spanLists[span->_n].PushFront(span);
	span->_isUse = false;// back from cc to pc

	/*_idSpanMap[span->_pageID] = span; 
	_idSpanMap[span->_pageID + span->_n - 1] = span;*/
	_idSpanMap.set(span->_pageID, span);
	_idSpanMap.set(span->_pageID + span->_n - 1, span);
}