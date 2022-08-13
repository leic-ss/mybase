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

#include "storage/kv_engine.h"

#include "dataentry.h"
#include "fdb_define.h"
#include "validbucket.h"
#include "config.h"

#include <libjungle/jungle.h>

#include <mutex>
#include <atomic>

#include <stdint.h>

namespace fdb
{

class FdbInstance
{
public:
    FdbInstance(mybase::BaseLogger* logger);
    explicit FdbInstance(int32_t index, bool db_version_care = false);
    ~FdbInstance();

    bool initBuckets(const std::vector<int32_t> buckets) { return true; }
    void setValidBucketMgn(const CValidBucketMgn& valid_bucket_mgn);
    void closeBuckets(const std::vector<int32_t> buckets) { }

    bool initialize(const std::string& db_path);
    void destroy();

    bool openDataDB();
    bool openSysDB();

    int32_t clearArea(int32_t area);
    int32_t put(int32_t bucket_number, rocksdb::Slice& key, rocksdb::Slice& value, uint32_t expire_time);
    int32_t get(int32_t bucket_number, rocksdb::Slice& key, rocksdb::Slice& value);
    int32_t remove(int32_t bucket_number, rocksdb::Slice& key, bool version_care);

    int32_t qpush(int32_t bucket_number, rocksdb::Slice& seq_key, rocksdb::Slice& value,
                  rocksdb::Slice& item_key, rocksdb::Slice& item, uint32_t expire_time);
    int32_t qpop(int32_t bucket_number, rocksdb::Slice& seq_key, rocksdb::Slice& value,
                 rocksdb::Slice& item_key, rocksdb::Slice& item);

    bool beginScan(kv::MigrateInfo& info);
    bool endScan(kv::MigrateInfo& info);
    bool getNextItems(std::vector<kv::ItemDataInfo*>& list);

	void compact(const std::string& type, std::string& info);
    void compactMannually();

    std::string getDbStatisticsInfo();
    std::string dbstat(const std::string& type);

public:
    static int32_t to_kvstore_code(jungle::Status& status);

private:
    int32_t do_get(FdbKey& rksdb_key, std::string& rksdb_item);
    int32_t do_put(FdbKey& rksdb_key, FdbItem& rksdb_item);
    int32_t do_remove(FdbKey& rksdb_key);

    uint64_t getAreaExpiredTime(int32_t area);

    bool loadAreaExpiredSec();

private:
    jungle::DB* dataDb{nullptr};
    jungle::Iterator key_itr;

    // the bucket migrating
    std::atomic<int32_t> scanBucket{-1};
    bool still_have_{true};

    int32_t maxMigrateBatchSize{0};
    int32_t maxMigrateBatchCount{0};

    mybase::BaseLogger* myLog{nullptr};

    ReadWriteLock locker;
    CValidBucketMgn validBucketMgn;
};

}
