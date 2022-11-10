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

#include "kv_engine.h"
#include "rocksdb_instance.h"
// #include "public/nlohmann/json.hpp"

#include <mutex>
#include <thread>

namespace mybase
{

class RocksdbInstance;

class RocksdbManager : public kvstore::KvEngine
{
public:
    RocksdbManager(mybase::BaseLogger* logger=nullptr);
    virtual ~RocksdbManager();

    bool initialize();

    int32_t put(int32_t ns, int32_t bucket_no, const std::string& key, const std::string& value,
                bool version_care, int32_t expire_time_sec);
    int32_t get(int32_t ns, int32_t bucket_no, const std::string& key, KvEntry& entry);
    int32_t remove(int32_t ns, int32_t bucket_no, const std::string& key);

    // int32_t qpush(int32_t ns, int32_t bucket_no, rocksdb::Slice seq_key, rocksdb::Slice value,
    //               rocksdb::Slice item_key, rocksdb::Slice item, bool version_care, int32_t expire_time_sec);
    // int32_t qpop(int32_t ns, int32_t bucket_no, rocksdb::Slice seq_key, rocksdb::Slice value,
    //              rocksdb::Slice item_key, rocksdb::Slice item);

    int32_t clear(int32_t area);

    bool initBuckets(const std::vector<int32_t>& buckets) { return true; }
    void closeBuckets(const std::vector<int32_t>& buckets) { }

    bool initBuckets(const CValidBucketMgn& valid_bucket_mgn);

    void statistics(std::string& info);
    void dbstats(const std::string& type, std::string& info);
    void setMaxFullScanSpeed(uint64_t speed, std::string& info);
    void setBucketCount(uint32_t bucket_count);

    void compactMannually();
	void compact(const std::string& type, std::string& info);

private:
    static int hash(int bucket_number);

private:
    RocksdbInstance* rdb_instance_;
    RocksdbInstance* scan_rdb_instance;
    std::mutex mutex;
};

}
