#pragma once
#include "common.h"

class CentralCache
{
public:
    static CentralCache* GetInstance(){
        return &_sInst;
    }

private:
    CentralCache(){}
    CentralCache(const CentralCache& copy) = delete;
    CentralCache& operator=(const CentralCache& copy) = delete;

private:
    SpanList _spanList[FREE_LIST_NUM];
    static CentralCache _sInst; 

};