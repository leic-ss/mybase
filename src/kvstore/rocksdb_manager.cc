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

#include "rocksdb_manager.h"
#include "rocksdb_instance.h"

#include "public/config.h"
#include "public/common.h"

namespace mybase
{

RocksdbManager::RocksdbManager(mybase::BaseLogger* logger) : kvstore::KvEngine(logger), rdb_instance_(nullptr), scan_rdb_instance(nullptr)
{ }

RocksdbManager::~RocksdbManager()
{
    {
        std::unique_lock<std::mutex> l(mutex);
        DELETE(rdb_instance_);
    }
}

bool RocksdbManager::initialize()
{
    const char* data_db_path = sDefaultConfig.getString(sStorageSection, sDataDbPath, "data/datadb");
    const char* sys_db_path = sDefaultConfig.getString(sStorageSection, sSysDbPath, "data/sysdb");

    rdb_instance_ = new RocksdbInstance(myLog);
    if(!rdb_instance_->initialize(data_db_path, sys_db_path)) {
        _log_err(myLog, "rksdb instance initialize failed, db_path[%s] sys_db_path[%s]", data_db_path, sys_db_path);
        return false;
    }

    _log_warn(myLog, "rksdb instance initialize success, db_path[%s] sys_db_path[%s]", data_db_path, sys_db_path);
    return true;
}

int32_t RocksdbManager::put(int32_t ns, int32_t bucket_no, const std::string& key, const std::string& value,
                         bool version_care, int32_t expire_time_sec)
{
    // if( KV_SERVERFLAG_DUPLICATE == key.server_flag ) {
    //     if( false == m_validBucketMgn.valid(bucket_no) ) {
    //         _log_debug(myLog, "sync bucket num:%d, not valid,so ingore", bucket_no);
    //         return KV_RETURN_DUPLICATE_ROUTE_CHANGE_ERROR;
    //     }
    // }

    int32_t rc = rdb_instance_->put(ns, bucket_no, key, value, expire_time_sec);
    return rc;
}

int32_t RocksdbManager::get(int32_t ns, int32_t bucket_no, const std::string& key, KvEntry& entry)
{
    return rdb_instance_->get(ns, bucket_no, key, entry);
}

bool RocksdbManager::initBuckets(const CValidBucketMgn & valid_bucket_mgn)
{
    m_validBucketMgn = valid_bucket_mgn;

    std::vector<int32_t> valid_buckets;
    for (int32_t i = 0; i < (int32_t)bucket_count; i++) {
        if (m_validBucketMgn.valid(i)) {
            valid_buckets.push_back(i);
        }
    }
    _log_warn(myLog, "init valid buckets: %s curr ts[%lu]",
              UtilHelper::vecot2String(valid_buckets).c_str(), TimeHelper::currentSec());

    rdb_instance_->setValidBucketMgn(valid_bucket_mgn);
    return true;
}

int32_t RocksdbManager::remove(int32_t ns, int32_t bucket_no, const std::string& key)
{
    int32_t rc = rdb_instance_->remove(ns, bucket_no, key);
    return rc;
}

// int32_t RocksdbManager::qpush(int32_t bucket_number, rocksdb::Slice seq_key, rocksdb::Slice value,
//                           rocksdb::Slice item_key, rocksdb::Slice item, bool version_care,
//                           int32_t expire_time_sec)
// {
//     if( KV_SERVERFLAG_DUPLICATE == seq_key.server_flag ) {
//         if( false == m_validBucketMgn.valid(bucket_number) ) {
//             _log_debug(myLog, "sync bucket num:%d, not valid,so ingore", bucket_number);
//             return KV_RETURN_DUPLICATE_ROUTE_CHANGE_ERROR;
//         }
//     }

//     return rdb_instance_->qpush(bucket_number, seq_key, value, item_key, item, expire_time_sec);
// }

// int32_t RocksdbManager::qpop(int32_t bucket_number, rocksdb::Slice& seq_key, rocksdb::Slice& value,
//                          rocksdb::Slice& item_key, rocksdb::Slice& item, bool need_clear)
// {
//     if( KV_SERVERFLAG_DUPLICATE == seq_key.server_flag ) {
//         if( false == m_validBucketMgn.valid(bucket_number) ) {
//             _log_debug(myLog, "sync bucket num:%d, not valid,so ingore", bucket_number);
//             return KV_RETURN_DUPLICATE_ROUTE_CHANGE_ERROR;
//         }
//     }

//     return rdb_instance_->qpop(bucket_number, seq_key, value, item_key, item);
// }

int32_t RocksdbManager::clear(int32_t area)
{
    if (!rdb_instance_) {
        _log_err(myLog, "clear area failed! area[%d]", area);
        return OP_RETURN_FAILED;
    }

    int32_t rc = rdb_instance_->clearArea(area);
    if (OP_RETURN_SUCCESS != rc) {
        _log_err(myLog, "clear area failed! area[%d] rc[%d]", area, rc);
        return OP_RETURN_FAILED;
    }

    return OP_RETURN_SUCCESS;
}

// bool RocksdbManager::beginScan(kv::MigrateInfo& info)
// {
//     {
//         std::lock_guard<std::mutex> l(mutex);
//         if (scan_rdb_instance) {
//             _log_warn(myLog, "rocksdb manager scan is running, bucket[%d]", info.dbId);
//             return false;
//         }
//         scan_rdb_instance = rdb_instance_;
//     }

//     if (!scan_rdb_instance->beginScan(info)) {
//         _log_err(myLog, "begin scan bucket[%d] failed!", info.dbId);
//         scan_rdb_instance = nullptr;
//         return false;
//     }

//     _log_info(myLog, "begin scan bucket[%d] success.", info.dbId);
//     return true;
// }

// bool RocksdbManager::endScan(kv::MigrateInfo& info)
// {
//     std::lock_guard<std::mutex> l(mutex);
//     if (!scan_rdb_instance) {
//         _log_warn(myLog, "no need to end scan bucket[%u]!", info.dbId);
//         return false;
//     }

//     scan_rdb_instance->endScan(info);
//     scan_rdb_instance = nullptr;
//     return true;
// }

// bool RocksdbManager::getNextItems(kv::MigrateInfo& info, std::vector<kv::ItemDataInfo*>& list)
// {
//     if (!scan_rdb_instance) {
//         _log_err(myLog, "scan bucket not opened! bucket[%d]", info.dbId);
//         return false;
//     }

//     if (!scan_rdb_instance->getNextItems(list)) {
//         _log_err(myLog, "get next items failed! bucket[%d]", info.dbId);
//         return false;
//     }

//     _log_debug(myLog, "get next items success! bucket[%d] size[%d]", info.dbId, list.size());
//     return true;
// }

void RocksdbManager::statistics(std::string& info)
{
    info.append(rdb_instance_->getDbStatisticsInfo());
}

void RocksdbManager::dbstats(const std::string& type, std::string& info)
{
    info.append(rdb_instance_->dbstat(type));
}

void RocksdbManager::setBucketCount(uint32_t bucket_count)
{
    if (this->bucket_count != 0) return ;
    this->bucket_count = bucket_count;
}

void RocksdbManager::compact(const std::string& type, std::string& info)
{
    rdb_instance_->compact(type, info);
}
void RocksdbManager::compactMannually() { rdb_instance_->compactMannually(); }

int RocksdbManager::hash(int h)
{
    h += (h << 15) ^ 0xffffcd7d;
    h ^= (h >> 10);
    h += (h << 3);
    h ^= (h >> 6);
    h += (h << 2) + (h << 14);
    return h ^ (h >> 16);
}

}
