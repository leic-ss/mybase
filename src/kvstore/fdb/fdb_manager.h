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

#include <malloc.h>
#ifdef WITH_TCMALLOC
#include <google/malloc_extension.h>
#endif

#include "storage/kv_engine.h"
#include "dataentry.h"
#include "config.h"

#include "fdb_instance.h"
#include "nlohmann/json.hpp"

#include <mutex>
#include <thread>


namespace fdb
{

class FdbManager : public kv::storage::KvEngine
{
public:
    FdbManager(mybase::BaseLogger* logger=nullptr);
    virtual ~FdbManager();

    bool initialize();

    int32_t put(int32_t bucket_number, rocksdb::Slice& key, rocksdb::Slice& value,
                bool version_care, int32_t expire_time_sec, bool need_clear = false);
    int32_t get(int32_t bucket_number, rocksdb::Slice& key, rocksdb::Slice& value, bool need_clear = false);
    int32_t remove(int32_t bucket_number, rocksdb::Slice& key, bool version_care, bool need_clear = false);

    int32_t clear(int32_t area);

    bool initBuckets(const std::vector<int32_t>& buckets) { return true; }
    void closeBuckets(const std::vector<int32_t>& buckets) { }

    bool initBuckets(const CValidBucketMgn& valid_bucket_mgn);

    bool beginScan(kv::MigrateInfo& info);
    bool endScan(kv::MigrateInfo& info);
    bool getNextItems(kv::MigrateInfo& info, std::vector<kv::ItemDataInfo*>& list);

    void getStats(AreaStat* stat);

    void statistics(int32_t db, std::string& info);
    void dbstat(const std::string& type, std::string& info);
    void setMaxFullScanSpeed(uint64_t speed, std::string& info);
    void setBucketCount(uint32_t bucket_count);

private:
    static int hash(int bucket_number);

private:
    FdbInstance* fdb_instance_;
    FdbInstance* scan_fdb_instance;
    std::mutex mutex;
};

}