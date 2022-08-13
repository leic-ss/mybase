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

#include "public/bitmap.h"
#include "common/defs.h"
#include "common/validbucket.h"
#include "public/dlog.h"
#include "storage.pb.h"

#include <map>

#include <stdint.h>

__attribute__((unused)) static const int ITEM_HEAD_LENGTH = 2;

namespace mybase
{

class MigrateInfo
{
public:
    MigrateInfo() { }

public:
    uint32_t hashIndex{0};
    uint32_t dbId{0};
    bool isMigrate{false};
    int32_t ns{-1};
    CBitMap bucketSet{sMaxBucketNumber};
};

namespace kvstore
{

class KvEngine
{
public:
    KvEngine(mybase::BaseLogger* logger=nullptr) : bucket_count(0), m_needLoadFromLocal(false), myLog(logger) { }
    virtual ~KvEngine() { }

    virtual bool initialize() { return false; }

    virtual int32_t put(int32_t ns, int32_t bucket_number, const std::string& key, const std::string& value,
                        bool version_care, int32_t expire_time) = 0;
    virtual int32_t get(int32_t ns, int32_t bucket_number, const std::string& key, KvEntry& entry) = 0;
    virtual int32_t remove(int32_t ns, int32_t bucket_number, const std::string& key) = 0;

    virtual int32_t clear(int32_t ns) = 0;

    virtual bool initBuckets(const std::vector<int32_t> &buckets) = 0;
    virtual void closeBuckets(const std::vector<int32_t> &buckets) = 0;

    // virtual bool getNextItems(MigrateInfo& info, std::vector<ItemDataInfo*> &list) = 0;
    // virtual bool beginScan(MigrateInfo & info) = 0;
    // virtual bool endScan(MigrateInfo & info) = 0;

    virtual void setBucketCount(uint32_t bucket_count)
    {
        if(this->bucket_count != 0) return;
        this->bucket_count = bucket_count;
        return;
    }

    virtual void displayStatics() { return; }
    virtual void setUpdateStatusOk() { return; }

    bool checkNeedLoadFromLocal() const { return m_needLoadFromLocal; }
    bool initBuckets(const CValidBucketMgn& validBucketMgn)
    {
        m_validBucketMgn = validBucketMgn;
        return true;
    }
    void setLogger(mybase::BaseLogger* logger) { myLog = logger; }

    virtual void statistics(std::string& info) { }
    virtual void dbstats(const std::string& type, std::string& info) { }

protected:
    uint32_t bucket_count;
    CValidBucketMgn m_validBucketMgn;
    bool m_needLoadFromLocal;
    mybase::BaseLogger* myLog{nullptr};
};

}

}
