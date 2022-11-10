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

#include "public/common.h"
#include "common/defs.h"
#include "public/cast_helper.h"
#include "rocksdb/utilities/options_util.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb_instance.h"

#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#include <stdlib.h>
#include <string>

namespace mybase
{

static const std::string areaExpiredPrefix = "area_expired_";

template <class T>
static void DeleteEntry(const rocksdb::Slice& key, void* value) {
    T* typed_value = reinterpret_cast<T*>(value);
    delete typed_value;
}

bool ExpiredFilter::Filter(int32_t level,
                           const rocksdb::Slice& key,
                           const rocksdb::Slice& value,
                           std::string* new_value,
                           bool* value_changed) const
{
    RocksdbKey rdb_key;
    RocksdbItem rdb_item;

    rdb_key.assign(_CCC(key.data()), key.size());
    rdb_item.assign(_CCC(value.data()), value.size());

    int32_t area = rdb_key.area();
    int32_t actual_bucket = rdb_key.getBucketNumber();

    if (area < 0 || area >= (int32_t)sMaxNamespaceCount) {
        log_warn("invalid area, do filter! area[%d] key[%.*s]", area, rdb_key.keySize(), rdb_key.key());
        return true;
    }

    uint64_t area_expired_sec = 0;
    auto iter = areaExpiredSec.find(area);
    if (iter != areaExpiredSec.end()) {
        area_expired_sec = iter->second;
    }

    if ( (rdb_item.edate() != 0 && rdb_item.edate() < fillterCheckTime) ||
         (rdb_item.mdate() < area_expired_sec) ||
         (!validBucketMgn.valid(actual_bucket)) ) {
        log_debug("filter expired data, level[%d] area[%d] key[%.*s] edate[%u] mdate[%u] areaes[%lu] bucket[%d] valid[%d] chksec[%lu]",
                   level, area, rdb_key.keySize(), rdb_key.key(), rdb_item.edate(), rdb_item.mdate(), area_expired_sec,
                   actual_bucket, validBucketMgn.valid(actual_bucket), fillterCheckTime);
        return true;
    }

    return false;
}

const char* ExpiredFilter::Name() const { return "ExpiredFilter"; }

std::unique_ptr<rocksdb::CompactionFilter> ExpiredFilterFactory::CreateCompactionFilter(
                                            const rocksdb::CompactionFilter::Context& context)
{
    std::unique_lock<std::mutex> l(mutex);
    auto ptr = std::unique_ptr<rocksdb::CompactionFilter>(new ExpiredFilter(areaExpiredSec, validBucketMgn));
    return std::move(ptr);
}

void ExpiredFilterFactory::setAreaExpiredSec(const std::unordered_map<int32_t, uint64_t>& area_expired_sec)
{
    std::unique_lock<std::mutex> l(mutex);
    areaExpiredSec = area_expired_sec;
}

void ExpiredFilterFactory::setValidBucketMgn(const CValidBucketMgn& valid_bucket_mgn)
{
    std::unique_lock<std::mutex> l(mutex);
    validBucketMgn = valid_bucket_mgn;
}

const char* ExpiredFilterFactory::Name() const { return "ExpiredFilterFactory"; }

RocksdbInstance::RocksdbInstance(mybase::BaseLogger* logger) : dataDb(nullptr)
                                                  , sysDb(nullptr)
                                                  , scan_it_(nullptr)
                                                  , scan_bucket_(-1)
                                                  , still_have_(true)
                                                  , filterFactory(new ExpiredFilterFactory())
                                                  , maxMigrateBatchSize(0)
                                                  , maxMigrateBatchCount(0)
                                                  , myLog(logger)
{}

RocksdbInstance::RocksdbInstance(int32_t index, bool db_version_care)
                            : dataDb(nullptr)
                            , sysDb(nullptr)
                            , scan_it_(nullptr)
                            , scan_bucket_(-1)
                            , still_have_(true)
                            , filterFactory(new ExpiredFilterFactory())
                            , maxMigrateBatchSize(0)
                            , maxMigrateBatchCount(0)
{}

// TODO: optimize
bool RocksdbInstance::initialize(const std::string& db_path, const std::string& sys_db_path)
{
    if ( !openSysDB() ) {
        return false;
    }

    if ( !loadAreaExpiredSec() ) {
        return false;
    }

    if(filterFactory) filterFactory->setAreaExpiredSec(areaExpiredSec);

    if (!openDataDB() ) {
        return false;
    }

    maxMigrateBatchSize = sDefaultConfig.getInt(sStorageSection, sMigrateBatchSize, 1048576); // 1M default
    maxMigrateBatchCount = sDefaultConfig.getInt(sStorageSection, sMigrateBatchCount, 2000); // 2000 default
    return true;
}

bool RocksdbInstance::openSysDB()
{
    rocksdb::DBOptions sys_db_opts;
    const char* sys_db_path = sDefaultConfig.getString(sStorageSection, sSysDbPath, "data/sysdb");
    rocksdb::Status s = rocksdb::LoadLatestOptions(sys_db_path, rocksdb::Env::Default(), &sys_db_opts, &sysCfDescs, false);

    if (s.ok()) {
        _log_warn(myLog, "load sys db option success! sys db path[%s]", sys_db_path);
    } else if (s.IsNotFound()) {
        _log_warn(myLog, "sys db option not found, create one! sys db path[%s]", sys_db_path);

        sys_db_opts.create_if_missing = true;
        sys_db_opts.create_missing_column_families = true;

        rocksdb::ColumnFamilyOptions cfOpts;
        cfOpts.OptimizeForSmallDb();
        sysCfDescs.emplace_back(rocksdb::ColumnFamilyDescriptor(rocksdb::kDefaultColumnFamilyName, cfOpts));
    } else {
        _log_err(myLog, "load sys db option failed! sys path[%s] err[%s]", sys_db_path, s.ToString().c_str());
        return false;
    }

    if (sysCfDescs.empty()) {
        _log_err(myLog, "open sys db failed! sys path[%s] err[%s]", sys_db_path, s.ToString().c_str());
        return false;
    }

    s = rocksdb::DB::Open(sys_db_opts, sys_db_path, sysCfDescs, &sysCfHandles, &sysDb);
    if (!s.ok()) {
        _log_err(myLog, "open sys db failed! sys db path[%s] err[%s]", sys_db_path, s.ToString().c_str());
        return false;
    }

    _log_warn(myLog, "open sys db success! sys db path[%s]", sys_db_path);
    return true;
}

bool RocksdbInstance::openDataDB()
{
    const char* data_db_path = sDefaultConfig.getString(sStorageSection, sDataDbPath, "data/datadb");
    rocksdb::Status s = rocksdb::LoadLatestOptions(data_db_path, rocksdb::Env::Default(), &dataDbOpts, &dataCfDescs, false);
    if (s.ok()) {
        _log_warn(myLog, "load db option success! path[%s]", data_db_path);
    } else if (s.IsNotFound()) {
        _log_warn(myLog, "db option not found, create one! path[%s]", data_db_path);

        dataDbOpts.IncreaseParallelism();
        dataDbOpts.create_if_missing = true;
        dataDbOpts.create_missing_column_families = true;

        rocksdb::ColumnFamilyOptions cfOpts;
        cfOpts.OptimizeLevelStyleCompaction();
        dataCfDescs.emplace_back(rocksdb::ColumnFamilyDescriptor(rocksdb::kDefaultColumnFamilyName, cfOpts));
    } else {
        _log_err(myLog, "load db option failed! path[%s] err[%s]", data_db_path, s.ToString().c_str());
        return false;
    }

    if (dataCfDescs.empty()) {
        _log_err(myLog, "initialize db failed! path[%s] err[%s]", data_db_path, s.ToString().c_str());
        return false;
    }

    dataDbOpts.statistics = rocksdb::CreateDBStatistics();
    dataCfDescs[0].options.compaction_filter_factory = filterFactory;

    // rate limiter from read in compaction
    int32_t rate_limit_mbps = sDefaultConfig.getInt(sStorageSection, "sRdbRateLimitMbPerSec", 50);
    rateLimiter = rocksdb::NewGenericRateLimiter((int64_t)rate_limit_mbps * 1024 * 1024,
                                        100 * 1000, 10, rocksdb::RateLimiter::Mode::kReadsOnly);
    dataDbOpts.rate_limiter.reset(rateLimiter);

    // add lru cache
    int32_t block_cache_size = sDefaultConfig.getInt(sStorageSection, sRdbBlockCacheSizeMB, 100);
    int32_t block_cache_shard_num = sDefaultConfig.getInt(sStorageSection, sRdbBlockCacheShardNum, 8);
    if (block_cache_size <= 0 || block_cache_shard_num <= 0) {
        _log_warn(myLog, "invalid config, block_cache_size[%d] block_cache_shard_num[%d]", block_cache_size, block_cache_shard_num);
        return false;
    }
    _log_warn(myLog, "valid config, block_cache_size[%d] block_cache_shard_num[%d]", block_cache_size, block_cache_shard_num);

    rocksdb::BlockBasedTableOptions table_options;
    if (dataCfDescs[0].options.table_factory) {
        table_options = *(rocksdb::BlockBasedTableOptions*)( dataCfDescs[0].options.table_factory->GetOptions() );
    }

    table_options.block_cache = rocksdb::NewLRUCache((size_t)block_cache_size * 1024 * 1024, block_cache_shard_num);
    dataCfDescs[0].options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));

	s = rocksdb::DB::Open(dataDbOpts, data_db_path, dataCfDescs, &dataCfHandles, &dataDb);
    if (!s.ok()) {
        _log_err(myLog, "open data db failed! data db path[%s] err[%s]", data_db_path, s.ToString().c_str());
        return false;
    }

    _log_warn(myLog, "open data db success! path[%s]", data_db_path);
    return true;
}

RocksdbInstance::~RocksdbInstance()
{
    for (auto& handle : dataCfHandles) {
        delete handle;
    }

    if (scan_it_) {
        delete scan_it_;
        scan_it_ = nullptr;
    }

    if ( dataDb ) {
        delete dataDb;
        dataDb = nullptr;
    }

    for (auto& handle : sysCfHandles) {
        delete handle;
    }

    if (sysDb) {
        delete sysDb;
        sysDb = nullptr;
    }
}

void RocksdbInstance::destroy() { }

int32_t RocksdbInstance::put(int32_t ns, int32_t bucket_no, const std::string& key, const std::string& value, uint32_t expire_time)
{
    uint32_t mdate = TimeHelper::currentSec();
    uint32_t edate = (expire_time != 0) ? (mdate + expire_time) : 0;

    _log_info(myLog, "bucket_no[%d] mdate = %u expire_time = %u edate = %u",
               bucket_no, mdate, expire_time, edate);

    RocksdbKey rdb_key(key.data(), key.size(), bucket_no, ns);

    RocksdbItem rdb_item;
    // rdb_item.meta().flag = value.data_meta.flag;
    rdb_item.meta().mdate = mdate;
    rdb_item.meta().edate = edate;

    rdb_item.set(value.data(), value.size());

    return do_put(rdb_key, rdb_item);
}

int32_t RocksdbInstance::get(int32_t ns, int32_t bucket_no, const std::string& key, KvEntry& entry)
{
    RocksdbKey rdb_key(key.data(), key.size(), bucket_no, ns);
    RocksdbItem rdb_item;

    std::string rdb_value;
    int32_t rc = do_get(rdb_key, rdb_value);
    if ( rc != OP_RETURN_SUCCESS ) {
        return rc;
    }
    rdb_item.assign((char*)rdb_value.data(), rdb_value.size());

    uint64_t current_sec = TimeHelper::currentSec();
    uint64_t area_expired_sec = getAreaExpiredTime(ns);
    if ( (rdb_item.meta().edate != 0 &&  rdb_item.meta().edate < current_sec) ||
         (rdb_item.meta().mdate < area_expired_sec) ) { // || !validBucketMgn.valid(bucket_no) ) {
        _log_info(myLog, "data expired, key[%.*s] edate[%u] current_sec[%lu] area_expired_sec[%lu] bucket_no[%d] valid[%d]",
                   key.size(), key.data(), rdb_item.meta().edate, current_sec, area_expired_sec,
                   bucket_no, validBucketMgn.valid(bucket_no));
        return KV_RETURN_DATA_EXPIRED;
    }

    entry.set_key(key);
    entry.set_val(rdb_item.value(), rdb_item.valueSize());
    entry.set_mtime(rdb_item.meta().mdate);
    entry.set_etime(rdb_item.meta().edate);

    return OP_RETURN_SUCCESS;
}

int32_t RocksdbInstance::remove(int32_t ns, int32_t bucket_no, const std::string& key)
{
    RocksdbKey rdb_key(key.data(), key.size(), bucket_no, ns);

    int32_t rc = do_remove(rdb_key);
    if (rc != OP_RETURN_SUCCESS) return rc;

    return OP_RETURN_SUCCESS;
}

// int32_t RocksdbInstance::qpush(int32_t bucket_number, rocksdb::Slice& seq_key, rocksdb::Slice& value,
//                            rocksdb::Slice& item_key, rocksdb::Slice& item, uint32_t expire_time)
// {
//     RocksdbKey rdb_seq_key;
//     RocksdbItem rdb_seq_item;
//     {
//         uint32_t mdate = 0, edate = 0;
//         if (seq_key.data_meta.mdate == 0) {
//             mdate = TimeHelper::currentSec();
//             if(expire_time > 0) {
//                 edate = mdate + expire_time;
//             }

//             if (expire_time >= mdate) {
//                 edate = expire_time;
//             }

//             seq_key.data_meta.mdate = mdate;
//             seq_key.data_meta.edate = edate;
//         } else {
//             mdate = seq_key.data_meta.mdate;
//             edate = seq_key.data_meta.edate;
//         }

//         int32_t area = seq_key.decodeArea();
//         rdb_seq_key.set(seq_key.getData(), seq_key.getSize(), bucket_number, area);

//         rdb_seq_item.meta().flag = value.data_meta.flag;
//         rdb_seq_item.meta().mdate = mdate;
//         rdb_seq_item.meta().edate = edate;
//         rdb_seq_item.meta().version = seq_key.data_meta.version;

//         rdb_seq_item.set(value.getData(), value.getSize());
//     }
    
//     RocksdbKey rdb_item_key;
//     RocksdbItem rdb_item_item;
//     {
//         uint32_t mdate = 0, edate = 0;
//         if (item_key.data_meta.mdate == 0) {
//             mdate = TimeHelper::currentSec();
//             if(expire_time > 0) {
//                 edate = mdate + expire_time;
//             }

//             if (expire_time >= mdate) {
//                 edate = expire_time;
//             }

//             item_key.data_meta.mdate = mdate;
//             item_key.data_meta.edate = edate;
//         } else {
//             mdate = item_key.data_meta.mdate;
//             edate = item_key.data_meta.edate;
//         }

//         int32_t area = item_key.decodeArea();
//         rdb_item_key.set(item_key.getData(), item_key.getSize(), bucket_number, area);

//         rdb_item_item.meta().flag = item.data_meta.flag;
//         rdb_item_item.meta().mdate = mdate;
//         rdb_item_item.meta().edate = edate;
//         rdb_item_item.meta().version = item_key.data_meta.version;

//         rdb_item_item.set(item.getData(), item.getSize());
//     }

//     rocksdb::WriteBatch batch;
//     batch.Put(dataCfHandles[0], rocksdb::Slice(rdb_seq_key.data(), rdb_seq_key.size()), rocksdb::Slice(rdb_seq_item.data(), rdb_seq_item.size()));
//     batch.Put(dataCfHandles[0], rocksdb::Slice(rdb_item_key.data(), rdb_item_key.size()), rocksdb::Slice(rdb_item_item.data(), rdb_item_item.size()));

//     rocksdb::Status status = dataDb->Write(writeOpts, &batch);
//     if (!status.ok()) {
//         _log_err(myLog, "rocksdb qpush failed! area[%d] key[%.*s] err[%s]",
//                  rdb_item_key.area(), rdb_item_key.keySize(), rdb_item_key.key(), status.ToString().c_str());
//         return to_kvstore_code(status);
//     }

//     return OP_RETURN_SUCCESS;
// }

// int32_t RocksdbInstance::qpop(int32_t bucket_number, rocksdb::Slice& seq_key, rocksdb::Slice& value,
//                           rocksdb::Slice& item_key, rocksdb::Slice& item)
// {
//     int32_t area = item_key.decodeArea();
//     RocksdbKey rdb_key(item_key.getData(), item_key.getSize(), bucket_number, area);

//     std::string data;
//     rocksdb::Status status = dataDb->Get(rocksdb::ReadOptions(), dataCfHandles[0], rocksdb::Slice(rdb_key.data(), rdb_key.size()), &data);
//     if (!status.ok()) {
//         _log_err(myLog, "rocksdb qpop failed! area[%d] key[%.*s] err[%s]",
//                   area, item_key.getSize(), item_key.getData(), status.ToString().c_str());
//         return to_kvstore_code(status);
//     }

//     {
//         RocksdbItem rdb_item;
//         rdb_item.assign((char*)data.data(), data.size());
//         item.setData(rdb_item.value(), rdb_item.valueSize(), true);
//     }

//     RocksdbKey rdb_seq_key;
//     RocksdbItem rdb_seq_item;
//     {
//         int32_t area = seq_key.decodeArea();
//         rdb_seq_key.set(seq_key.getData(), seq_key.getSize(), bucket_number, area);

//         rdb_seq_item.meta().flag = value.data_meta.flag;
//         rdb_seq_item.meta().mdate = seq_key.data_meta.mdate;
//         rdb_seq_item.meta().edate = seq_key.data_meta.edate;
//         rdb_seq_item.meta().version = seq_key.data_meta.version;

//         rdb_seq_item.set(value.getData(), value.getSize());
//     }

//     rocksdb::WriteBatch batch;
//     batch.Put(dataCfHandles[0], rocksdb::Slice(rdb_seq_key.data(), rdb_seq_key.size()), rocksdb::Slice(rdb_seq_item.data(), rdb_seq_item.size()));
//     batch.Delete(dataCfHandles[0], rocksdb::Slice(rdb_key.data(), rdb_key.size()));

//     status = dataDb->Write(writeOpts, &batch);
//     if (!status.ok()) {
//         _log_err(myLog, "rocksdb qpop failed! area[%d] key[%.*s] err[%s]",
//                   area, item_key.getSize(), item_key.getData(), status.ToString().c_str());
//         return to_kvstore_code(status);
//     }

//     return OP_RETURN_SUCCESS;
// }

// bool RocksdbInstance::beginScan(kv::MigrateInfo& info)
// {
//     if (scan_it_) {
//         _log_warn(myLog, "scan_it_ already exist, delete it first. bucket[%d]", info.dbId);
//         return false;
//     }

//     int32_t bucket_number = info.dbId;
//     bool ret = true;

//     rocksdb::ReadOptions scan_read_options = readOpts;
//     scan_read_options.fill_cache = false; // not fill cache
//     scan_it_ = dataDb->NewIterator(scan_read_options, dataCfHandles[0]);
//     if (nullptr == scan_it_) {
//         _log_err(myLog, "new iterator for begin scan failed! bucket[%d]", bucket_number);
//         ret = false;
//     } else if (bucket_number != -1){
//         char scan_key[sRdbKeyBucketSize] = {0};
//         RocksdbKey::encodeBucketNumber(scan_key, bucket_number);
//         scan_it_->Seek(rocksdb::Slice(scan_key, sizeof(scan_key)));
//     } else {
//         scan_it_->Seek(rocksdb::Slice());
//     }

//     _log_info(myLog, "new iterator for begin scan success! bucket[%d]", bucket_number);
//     if (ret) {
//         scan_bucket_ = bucket_number;
//         still_have_ = true;
//     }

//     return ret;
// }

// // TODO: filter expired data
// bool RocksdbInstance::getNextItems(std::vector<kv::ItemDataInfo*>& list)
// {
//     list.clear();

//     if (!scan_it_) {
//         _log_err(myLog, "scan is not begin!");
//         return false;
//     }

//     if (!still_have_) {
//         return false;
//     }

//     RocksdbKey rdb_key;       // reuse is ok.
//     RocksdbItem rdb_item;

//     int32_t batch_size = 0, batch_count = 0;

//     uint64_t current_sec = TimeHelper::currentSec();
//     while ( (batch_size < maxMigrateBatchSize) && (batch_count < maxMigrateBatchCount) && scan_it_->Valid()) {
//         // match bucket
//         int32_t actual_bucket = RocksdbKey::decodeBucketNumber(scan_it_->key().data());
//         if ( (scan_bucket_ != -1) && (scan_bucket_ != actual_bucket) ) {
//             break;
//         }

//         rdb_key.assign(const_cast<char*>(scan_it_->key().data()), scan_it_->key().size());
//         rdb_item.assign(const_cast<char*>(scan_it_->value().data()), scan_it_->value().size());

//         int32_t key_size = rdb_key.mergedKeySize();
//         int32_t value_size = rdb_item.valueSize();
//         if ( (key_size >= (int32_t)sMaxKeySizeWithArea) || (key_size < 1) || (value_size >= (int32_t)sMaxDataSize) || (value_size < 1)) {
//             _log_err(myLog, "kv size invalid: k: %.*s, v: %d [%d %d] [%x %x %s]",
//                       rdb_key.keySize(), rdb_key.key(), value_size, scan_it_->key().size(),
//                       scan_it_->value().size(), *(rdb_key.key()),
//                       *(rdb_key.key()+1), rdb_key.key() + 2);
//             scan_it_->Next();
//             continue;
//         }

//         uint64_t area_expired_sec = getAreaExpiredTime(rdb_key.area());
//         if ( (rdb_item.meta().edate != 0 &&  rdb_item.meta().edate < current_sec) ||
//              (rdb_item.meta().mdate < area_expired_sec) || !validBucketMgn.valid(actual_bucket) ) {
//             _log_debug(myLog, "data expired, key[%.*s] edate[%u] current_sec[%lu] area_expired_sec[%lu] bucket[%d] valid[%d]",
//                       rdb_key.keySize(), rdb_key.key(), rdb_item.edate(), current_sec, area_expired_sec,
//                       actual_bucket, validBucketMgn.valid(actual_bucket));
//             scan_it_->Next();
//             continue;
//         }

//         int32_t total_size = key_size + value_size + sizeof(kv::ItemDataInfo);
//         kv::ItemDataInfo* data = (kv::ItemDataInfo*) calloc(1, total_size);
//         //data->header.magic = MAGIC_ITEM_META_LDB_PREFIX;
//         data->header.keysize = key_size;
//         data->header.version = rdb_item.version();
//         data->header.valsize = value_size;
//         data->header.flag = rdb_item.flag();
//         data->header.prefixsize = 0;
//         data->header.mdate = rdb_item.mdate();
//         data->header.edate = rdb_item.edate();
//         data->header.bucketId = actual_bucket;

//         memcpy(data->m_data, rdb_key.mergedKey(), key_size);
//         memcpy(data->m_data + key_size, rdb_item.value(), value_size);

//         list.push_back(data);
//         scan_it_->Next();

//         batch_size += total_size;
//         batch_count++;
//     }
//     _log_debug(myLog, "get next items count[%d] size[%d]", list.size(), batch_size);

//     if (list.empty()) {
//         still_have_ = false;
//     }

//     return still_have_;
// }

// bool RocksdbInstance::endScan(kv::MigrateInfo& info)
// {
//     if (scan_it_) {
//         _log_info(myLog, "delete iterator for end scan success! bucket[%d]", info.dbId);
//         delete scan_it_;
//         scan_it_ = nullptr;
//         scan_bucket_ = -1;
//         still_have_ = false;
//     } else {
//         _log_warn(myLog, "no need delete iterator for end scan! bucket[%d]", info.dbId);
//         return false;
//     }

//     return true;
// }

int32_t RocksdbInstance::do_put(RocksdbKey& rdb_key, RocksdbItem& rdb_item)
{
    if (!dataDb) return KV_ROCKSDB_kNotSupported;

    rocksdb::Slice cache_key(rdb_key.data(), rdb_key.size());
    rocksdb::Slice cache_data(rdb_item.data(), rdb_item.size());
    rocksdb::Status status = dataDb->Put(writeOpts, dataCfHandles[0], cache_key, cache_data);

    if (!status.ok()) {
        _log_err(myLog, "rocksdb put failed! area[%d] key[%.*s] err[%s]",
                  rdb_key.area(), rdb_key.keySize(), rdb_key.key(), status.ToString().c_str());
        return to_kvstore_code(status);
    }

    /*if (lruRowCache) {
        std::string* str = new std::string(cache_data.data(), cache_data.size());
        size_t charge = cache_data.size();
        lruRowCache->Insert(cache_key, (void*)str, charge, &DeleteEntry<std::string>);
    }*/

    _log_debug(myLog, "rocksdb put success! area[%d] key[%.*s]", rdb_key.area(), rdb_key.keySize(), rdb_key.key());
    return OP_RETURN_SUCCESS;
}

int32_t RocksdbInstance::do_get(RocksdbKey& rdb_key, std::string& value)
{
    if (!dataDb) return KV_ROCKSDB_kNotSupported;

    rocksdb::Slice cache_key(rdb_key.data(), rdb_key.size());

    // TODO: try read LRU CACHE
    /*if (lruRowCache) {
        rocksdb::Cache::Handle* handle = lruRowCache->Lookup(cache_key);

        if (handle != nullptr) {
            std::string* str = (std::string*)lruRowCache->Value(handle);
            value.assign(str->data(), str->size());

            lruRowCache->Release(handle);
            return OP_RETURN_SUCCESS;
        }
    }*/

    rocksdb::ReadOptions read_opt;
    rocksdb::Status status = dataDb->Get(read_opt, dataCfHandles[0], cache_key, &value);

    if (!status.ok()) {
        _log_debug(myLog, "rocksdb get failed! area[%d] bucket[%d] key[%.*s] err[%s]",
                  rdb_key.area(), rdb_key.getBucketNumber(), rdb_key.keySize(), rdb_key.key(), status.ToString().c_str());
        return to_kvstore_code(status);
    }

    // TODO: put lru cache
    /*if (lruRowCache) {
        std::string* str = new std::string(value);
        size_t charge = value.size();
        lruRowCache->Insert(cache_key, (void*)str, charge, &DeleteEntry<std::string>);
    }*/

    _log_debug(myLog, "rocksdb get success! area[%d] bucket[%d] key[%.*s] vsize[%d]",
               rdb_key.area(), rdb_key.getBucketNumber(), rdb_key.keySize(), rdb_key.key(), value.size());
    return OP_RETURN_SUCCESS;
}

int32_t RocksdbInstance::do_remove(RocksdbKey& rdb_key)
{
    if (!dataDb) return KV_ROCKSDB_kNotSupported;

    std::string cache_key(rdb_key.data(), rdb_key.size());

    /*if (lruRowCache) {
        lruRowCache->Erase(cache_key);
    }*/

    std::string value;
    rocksdb::Status status = dataDb->Get(readOpts,
                                         dataCfHandles[0],
                                         rocksdb::Slice(rdb_key.data(), rdb_key.size()),
                                         &value);
    if (status.IsNotFound()) {
        _log_err(myLog, "rocksdb remove failed! key[%.*s] err[%s]",
                  rdb_key.keySize(), rdb_key.key(), status.ToString().c_str());
        return to_kvstore_code(status);
    }

    status = dataDb->Delete(writeOpts, dataCfHandles[0], cache_key);
    if (!status.ok()) {
        _log_err(myLog, "rocksdb remove failed! key[%.*s] err[%s]",
                  rdb_key.keySize(), rdb_key.key(), status.ToString().c_str());
        return to_kvstore_code(status);
    }

    _log_debug(myLog, "rocksdb remove success! key[%.*s]", rdb_key.keySize(), rdb_key.key());
    return OP_RETURN_SUCCESS;
}

bool RocksdbInstance::loadAreaExpiredSec()
{
    std::string start_key = areaExpiredPrefix + std::string(":");
    std::string end_key = areaExpiredPrefix + std::string(";");
    rocksdb::Slice startKey(start_key);
    rocksdb::Slice endKey(end_key);

    rocksdb::ReadOptions scan_read_options;
    scan_read_options.fill_cache = false; // not fill cache
    scan_read_options.iterate_lower_bound = &startKey;
    scan_read_options.iterate_upper_bound = &endKey;

    rocksdb::Iterator* scan_itr = sysDb->NewIterator(scan_read_options);
    if (nullptr == scan_itr) {
        _log_err(myLog, "load area expired sec failed! new iterator for begin scan error.");
        return false;
    } else {
        scan_itr->SeekToFirst();
    }

    while (scan_itr->Valid()) {
        std::string k = scan_itr->key().ToString();
        std::string v = scan_itr->value().ToString();

        _log_info(myLog, "k[%s] v[%s]", k.c_str(), v.c_str());
        std::vector<std::string> vec = StringHelper::tokenize(k, ":");
        if (vec.size() != 2) {
            scan_itr->Next();
            continue;
        }
        int32_t area = std::stoi(vec[1]);
        uint64_t expired_ts = std::stoul(v);
        _log_warn(myLog, "load area expired sec success! area[%d] expired_ts[%lu]", area, expired_ts);

        areaExpiredSec.insert({area, expired_ts});
        scan_itr->Next();
    }

    delete scan_itr;
    return true;
}

void RocksdbInstance::setValidBucketMgn(const CValidBucketMgn& valid_bucket_mgn)
{
    locker.writeLock();
    validBucketMgn = valid_bucket_mgn;
    filterFactory->setValidBucketMgn(valid_bucket_mgn);
    locker.writeUnlock();
}

int32_t RocksdbInstance::clearArea(int32_t area)
{
    std::string key = areaExpiredPrefix + std::string(":") + std::to_string(area);
    uint64_t expired_ts = TimeHelper::currentSec();
    std::string val = std::to_string(expired_ts);

    rocksdb::Status status = sysDb->Put(rocksdb::WriteOptions(),
                                        rocksdb::Slice(key.data(), key.size()),
                                        rocksdb::Slice(val.data(), val.size()));

    if (!status.ok()) {
        _log_err(myLog, "clear area failed! key[%.*s] val[%.*s] err[%s]",
                  key.size(), key.data(), val.size(), val.data(), status.ToString().c_str());
        return RocksdbInstance::to_kvstore_code(status);
    }

    locker.writeLock();
    areaExpiredSec[area] = expired_ts;
    filterFactory->setAreaExpiredSec(areaExpiredSec);
    locker.writeUnlock();

    _log_warn(myLog, "clear area success! area[%d] expired_ts[%lu]", area, expired_ts);
    return OP_RETURN_SUCCESS;
}

uint64_t RocksdbInstance::getAreaExpiredTime(int32_t area)
{
    uint64_t expired_ts = 0;

    locker.readLock();
    auto iter = areaExpiredSec.find(area);
    if (iter != areaExpiredSec.end()) {
        expired_ts = iter->second;
    }
    locker.readUnlock();

    return expired_ts;
}

int32_t RocksdbInstance::to_kvstore_code(rocksdb::Status& status)
{
    int32_t rc = OP_RETURN_SUCCESS;
    switch (status.code()) {
        case rocksdb::Status::kOk: {
            rc = OP_RETURN_SUCCESS;
            break;
        }
        case rocksdb::Status::kNotFound: {
            rc = KV_RETURN_DATA_NOT_EXIST;
            break;
        }
        case rocksdb::Status::kCorruption: {
            rc = KV_ROCKSDB_kCorruption;
            break;
        }
        case rocksdb::Status::kNotSupported: {
            rc = KV_ROCKSDB_kNotSupported;
            break;
        }
        case rocksdb::Status::kInvalidArgument: {
            rc = KV_ROCKSDB_kInvalidArgument;
            break;
        }
        case rocksdb::Status::kIOError: {
            rc = KV_ROCKSDB_kIOError;
            break;
        }
        case rocksdb::Status::kMergeInProgress: {
            rc = KV_ROCKSDB_kMergeInProgress;
            break;
        }
        case rocksdb::Status::kIncomplete: {
            rc = KV_ROCKSDB_kIncomplete;
            break;
        }
        case rocksdb::Status::kShutdownInProgress: {
            rc = KV_ROCKSDB_kShutdownInProgress;
            break;
        }
        case rocksdb::Status::kTimedOut: {
            rc = KV_ROCKSDB_kTimedOut;
            break;
        }
        case rocksdb::Status::kAborted: {
            rc = KV_ROCKSDB_kAborted;
            break;
        }
        case rocksdb::Status::kBusy: {
            rc = KV_ROCKSDB_kBusy;
            break;
        }
        case rocksdb::Status::kExpired: {
            rc = KV_ROCKSDB_kExpired;
            break;
        }
        case rocksdb::Status::kTryAgain: {
            rc = KV_ROCKSDB_kTryAgain;
            break;
        }
        case rocksdb::Status::kCompactionTooLarge: {
            rc = KV_ROCKSDB_kCompactionTooLarge;
            break;
        }
        case rocksdb::Status::kColumnFamilyDropped: {
            rc = KV_ROCKSDB_kColumnFamilyDropped;
            break;
        }
        case rocksdb::Status::kMaxCode: {
            rc = KV_ROCKSDB_kMaxCode;
            break;
        }
        default: {
            break;
        }
    }

    return rc;
}

std::string RocksdbInstance::getDbStatisticsInfo()
{
    return dataDbOpts.statistics->ToString();
}

std::string RocksdbInstance::dbstat(const std::string& type)
{
    std::string stats;
    if ( dataDb && !dataDb->GetProperty(dataCfHandles[0], type, &stats) ) {
        stats = "No such stat type!";
    }
    return std::move(stats);
}

void RocksdbInstance::compactMannually()
{
    dataDb->CompactRange(rocksdb::CompactRangeOptions(), dataCfHandles[0], nullptr, nullptr);
}

void RocksdbInstance::compact(const std::string& type, std::string& info)
{
    rocksdb::Status s = rocksdb::Status::NotSupported("not supprted op type: " + type);

    if (type == "mannually") {
        // s = dataDb->CompactRange(rocksdb::CompactRangeOptions(), dataCfHandles[0], nullptr, nullptr);
    } else if (type == "auto") {
        // s = dataDb->CompactAuto();
    } else if (type == "status") {
        // s = dataDb->CompactStatus(info);
    }

    if( !s.ok() ) {
        info.clear();
        info.append(s.ToString());
    }

    if (info.empty()) {
        info.append(s.ToString());
    }

    return ;
}

}
