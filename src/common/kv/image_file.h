/************************************************************************
Copyright 2020 ~ 2021
Author: zhanglei

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
 
    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**************************************************************************/

#pragma once

#include <string>
#include <stdint.h>
#include <string.h> 
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>

#include "dataentry.h"
#include "dlog.h"

namespace mybase
{

typedef struct ImageEntry
{
    uint32_t len;
    uint32_t endSyncTag;

    union {
        struct {
            uint16_t ver;
        }s;
        char dummy[12];
    } u;

    ItemMetaInfo m_metaInfo;
    uint32_t tag;
    char keyvaluebuf[(1024*1024+3)&(~3)];

    ImageEntry();

    bool init(ItemDataInfo* dinfo);
    
    static uint32_t getMinLen();

    inline int32_t getArea() {
        // return (uint16_t)( (keyvaluebuf[1] & 0xFF) << 8 ) | (keyvaluebuf[0] & 0xFF);
        return *(uint16_t*)keyvaluebuf;
    }

    void toKeyValue(DataEntry& key, DataEntry& value);
    
    const static uint32_t EndSyncTag = 0x3abfce25;
} ImageEntry;

struct CImageActHeader
{
    int32_t type;
    uint32_t num;
    uint64_t pos;
    uint64_t len;
    union {
        char dummy[64];
        struct {
            int32_t  m_jobid;
            uint32_t m_begin;
            uint32_t m_end;
        }s;
    }u;

    std::string toString();
};

struct CImageFileHeader
{
    uint32_t imageMagic;
    int32_t headlen;
    int32_t ver;
    char binlogname[256];
    uint64_t pos;
    int32_t acctcount;

    CImageActHeader actHeader[1];

    void init(int32_t acctnum);

    static int32_t getHeadLen(int32_t num) {
        return  sizeof(CImageFileHeader) + num*sizeof(CImageActHeader);
    }

    CImageActHeader* getActHeader(int32_t type);

    std::string toString();

	bool checkHeaderValid(std::string& err);

    const static uint32_t CImageFileHeaderMagix = 0x48ab16cd;
};

class CImageLoadDeal
{
public:
    CImageLoadDeal(uint32_t rate = 0) : m_rate(rate) { }
    ~CImageLoadDeal();

    bool init(const char* imagefile);
    bool deal();

    void setHandler(std::function<void(ImageEntry* entry)> func) { handler = func; }
    void setRate(uint32_t rate) { m_rate = rate; }
    void setDoneHandler(std::function<void()> func) { doneHandler = func; }

    void setLogger(mybase::BaseLogger* logger) { myLog = logger; }
	CImageFileHeader* getHeader() { return m_header; }
    inline uint64_t getRecordNum() const { return totalRecordNum; }

private:
    CImageFileHeader* m_header{nullptr};
    mybase::BaseLogger* myLog{nullptr};

    std::string m_imagefile;
    uint64_t totalRecordNum{0};

    int32_t m_fd{-1};
    uint32_t m_rate;

    std::function<void(ImageEntry* entry)> handler;     
    std::function<void()> doneHandler; 

    bool dealHeader(CImageActHeader * hd );
};

}
