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

#include "kv_engine.h"

#include "public/common.h"
#include "public/config.h"
#include "rocksdb_defs.h"
#include "common/validbucket.h"

#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/table.h"
#include "rocksdb/options.h"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/statistics.h"
#include "rocksdb/cache.h"
#include "rocksdb/rate_limiter.h"

#include <mutex>
#include <atomic>

#include <stdint.h>

namespace mybase
{

class ExpiredFilter : public rocksdb::CompactionFilter {
public:
    ExpiredFilter(const std::unordered_map<int32_t, uint64_t>& area_expired_sec,
                  const CValidBucketMgn& valid_bucket_mgn)
                  : fillterCheckTime(TimeHelper::currentSec())
                  , areaExpiredSec(area_expired_sec)
                  , validBucketMgn(valid_bucket_mgn) {}

    bool Filter(int32_t level,
                const rocksdb::Slice& key,
                const rocksdb::Slice& value,
                std::string* new_value,
                bool* value_changed) const override;
    const char* Name() const override;

private:
    uint64_t fillterCheckTime;
    std::unordered_map<int32_t, uint64_t> areaExpiredSec;
    CValidBucketMgn validBucketMgn;
};

class ExpiredFilterFactory : public rocksdb::CompactionFilterFactory {
public:
    explicit ExpiredFilterFactory() {}

    void setAreaExpiredSec(const std::unordered_map<int32_t, uint64_t>& area_expired_sec);
    void setValidBucketMgn(const CValidBucketMgn& valid_bucket_mgn);

    std::unique_ptr<rocksdb::CompactionFilter> CreateCompactionFilter(
        const rocksdb::CompactionFilter::Context& context) override;
    const char* Name() const override;

private:
    std::mutex mutex;
    std::unordered_map<int32_t, uint64_t> areaExpiredSec;
    CValidBucketMgn validBucketMgn;
};

class RocksdbInstance
{
public:
    RocksdbInstance(mybase::BaseLogger* logger);
    explicit RocksdbInstance(int32_t index, bool db_version_care = false);
    ~RocksdbInstance();

    bool initBuckets(const std::vector<int32_t> buckets) { return true; }
    void setValidBucketMgn(const CValidBucketMgn& valid_bucket_mgn);
    void closeBuckets(const std::vector<int32_t> buckets) { }

    bool initialize(const std::string& db_path, const std::string& sys_db_path);
    void destroy();

    bool openDataDB();
    bool openSysDB();

    int32_t clearArea(int32_t area);
    int32_t put(int32_t ns, int32_t bucket_no, const std::string& key, const std::string& value, uint32_t expire_time);
    int32_t get(int32_t ns, int32_t bucket_no, const std::string& key, KvEntry& entry);
    int32_t remove(int32_t ns, int32_t bucket_no, const std::string& key);

    // int32_t qpush(int32_t bucket_number, rocksdb::Slice seq_key, rocksdb::Slice value,
    //               rocksdb::Slice item_key, rocksdb::Slice item, uint32_t expire_time);
    // int32_t qpop(int32_t bucket_number, rocksdb::Slice seq_key, std::string& value,
    //              rocksdb::Slice item_key, std::string& item);

    // bool beginScan(kv::MigrateInfo& info);
    // bool endScan(kv::MigrateInfo& info);
    // bool getNextItems(std::vector<kv::ItemDataInfo*>& list);

	void compact(const std::string& type, std::string& info);
    void compactMannually();

    void setRateLimit(int32_t mbps) {
        if (rateLimiter) rateLimiter->SetBytesPerSecond((int64_t)mbps * 1024 * 1024);
    }

    std::string getDbStatisticsInfo();
    std::string dbstat(const std::string& type);

public:
    static int32_t to_kvstore_code(rocksdb::Status& status);

private:
    int32_t do_get(RocksdbKey& rksdb_key, std::string& rksdb_item);
    int32_t do_put(RocksdbKey& rksdb_key, RocksdbItem& rksdb_item);
    int32_t do_remove(RocksdbKey& rksdb_key);

    uint64_t getAreaExpiredTime(int32_t area);

    bool loadAreaExpiredSec();

private:
    rocksdb::DB* dataDb;
    rocksdb::DBOptions dataDbOpts;
    std::vector<rocksdb::ColumnFamilyDescriptor> dataCfDescs;
    std::vector<rocksdb::ColumnFamilyHandle*> dataCfHandles;

    rocksdb::DB* sysDb;
    std::vector<rocksdb::ColumnFamilyDescriptor> sysCfDescs;
    std::vector<rocksdb::ColumnFamilyHandle*> sysCfHandles;

    rocksdb::RateLimiter* rateLimiter{nullptr};

    // for scan, MUST single-bucket everytime
    rocksdb::Iterator* scan_it_;
    // std::shared_ptr<rocksdb::Cache> lruRowCache;

    // the bucket migrating
    int32_t scan_bucket_;
    bool still_have_;

    std::shared_ptr<ExpiredFilterFactory> filterFactory;

    rocksdb::ReadOptions readOpts;
    rocksdb::WriteOptions writeOpts;

    int32_t maxMigrateBatchSize;
    int32_t maxMigrateBatchCount;

    mybase::BaseLogger* myLog{nullptr};

    ReadWriteLock locker;
    std::unordered_map<int32_t, uint64_t> areaExpiredSec;
    CValidBucketMgn validBucketMgn;
};

}
