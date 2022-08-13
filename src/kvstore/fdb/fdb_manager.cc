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

#include "fdb_manager.h"
#include "fdb_instance.h"

#include "common.h"

namespace fdb
{

FdbManager::FdbManager(mybase::BaseLogger* logger) : fdb_instance_(nullptr), scan_fdb_instance(nullptr), kv::storage::KvEngine(logger)
{ }

FdbManager::~FdbManager()
{
    {
        std::unique_lock<std::mutex> l(mutex);
        DELETE(fdb_instance_);
    }
}

bool FdbManager::initialize()
{
    const char* data_db_path = sDefaultConfig.getString(sDataServerSection, sDataDbPath, "data/datadb");

    fdb_instance_ = new FdbInstance(myLog);
    if(!fdb_instance_->initialize(data_db_path)) {
        _log_err(myLog, "fdb instance initialize failed, db_path[%s]", data_db_path);
        return false;
    }

    _log_warn(myLog, "fdb instance initialize success, db_path[%s]", data_db_path);
    return true;
}

int32_t FdbManager::put(int32_t bucket_number, rocksdb::Slice& key, rocksdb::Slice& value,
                         bool version_care, int32_t expire_time_sec, bool need_clear)
{
    if( KV_SERVERFLAG_DUPLICATE == key.server_flag ) {
        if( false == m_validBucketMgn.valid(bucket_number) ) {
            _log_debug(myLog, "sync bucket num:%d, not valid,so ingore", bucket_number);
            return KV_RETURN_DUPLICATE_ROUTE_CHANGE_ERROR;
        }
    }

    int32_t rc = fdb_instance_->put(bucket_number, key, value, expire_time_sec);
    if (rc == OP_RETURN_SUCCESS) {
        int32_t area = key.getArea();
    }
    return rc;
}

int32_t FdbManager::get(int32_t bucket_number, rocksdb::Slice& key, rocksdb::Slice& value, bool need_clear)
{
    return fdb_instance_->get(bucket_number, key, value);
}

bool FdbManager::initBuckets(const CValidBucketMgn & valid_bucket_mgn)
{
    m_validBucketMgn = valid_bucket_mgn;

    std::vector<int32_t> valid_buckets;
    for (int32_t i = 0; i < bucket_count; i++) {
        if (m_validBucketMgn.valid(i)) {
            valid_buckets.push_back(i);
        }
    }
    _log_warn(myLog, "init valid buckets: %s curr ts[%lu]",
              UtilHelper::vecot2String(valid_buckets).c_str(), TimeHelper::currentSec());

    fdb_instance_->setValidBucketMgn(valid_bucket_mgn);
    return true;
}

int32_t FdbManager::remove(int32_t bucket_number, rocksdb::Slice& key, bool version_care, bool need_clear)
{
    int32_t rc = fdb_instance_->remove(bucket_number, key, version_care);
    if (rc == OP_RETURN_SUCCESS) {
        int32_t area = key.getArea();
    }
    return rc;
}

int32_t FdbManager::clear(int32_t area)
{
    if (!fdb_instance_) {
        _log_err(myLog, "clear area failed! area[%d]", area);
        return OP_RETURN_FAILED;
    }

    int32_t rc = fdb_instance_->clearArea(area);
    if (OP_RETURN_SUCCESS != rc) {
        _log_err(myLog, "clear area failed! area[%d] rc[%d]", area, rc);
        return OP_RETURN_FAILED;
    }

    return OP_RETURN_SUCCESS;
}

bool FdbManager::beginScan(kv::MigrateInfo& info)
{
    {
        std::lock_guard<std::mutex> l(mutex);
        if (scan_fdb_instance) {
            _log_warn(myLog, "fdb manager scan is running, bucket[%d]", info.dbId);
            return false;
        }
        scan_fdb_instance = fdb_instance_;
    }

    if (!scan_fdb_instance->beginScan(info)) {
        _log_err(myLog, "begin scan bucket[%d] failed!", info.dbId);
        scan_fdb_instance = nullptr;
        return false;
    }

    _log_info(myLog, "begin scan bucket[%d] success.", info.dbId);
    return true;
}

bool FdbManager::endScan(kv::MigrateInfo& info)
{
    std::lock_guard<std::mutex> l(mutex);
    if (!scan_fdb_instance) {
        _log_warn(myLog, "no need to end scan bucket[%u]!", info.dbId);
        return false;
    }

    scan_fdb_instance->endScan(info);
    scan_fdb_instance = nullptr;
    return true;
}

bool FdbManager::getNextItems(kv::MigrateInfo& info, std::vector<kv::ItemDataInfo*>& list)
{
    if (!scan_fdb_instance) {
        _log_err(myLog, "scan bucket not opened! bucket[%d]", info.dbId);
        return false;
    }

    if (!scan_fdb_instance->getNextItems(list)) {
        _log_err(myLog, "get next items failed! bucket[%d]", info.dbId);
        return false;
    }

    _log_debug(myLog, "get next items success! bucket[%d] size[%d]", info.dbId, list.size());
    return true;
}

void FdbManager::getStats(AreaStat* stat)
{
    if (!stat || !fdb_instance_) return ;

    // TODO

    return ;
}

void FdbManager::statistics(int32_t db, std::string& info)
{
    info.append(fdb_instance_->getDbStatisticsInfo());
}

void FdbManager::dbstat(const std::string& type, std::string& info)
{
    info.append(fdb_instance_->dbstat(type));
}

void FdbManager::setBucketCount(uint32_t bucket_count)
{
    if (this->bucket_count != 0) return ;
    this->bucket_count = bucket_count;
}

int FdbManager::hash(int h)
{
    h += (h << 15) ^ 0xffffcd7d;
    h ^= (h >> 10);
    h += (h << 3);
    h ^= (h >> 6);
    h += (h << 2) + (h << 14);
    return h ^ (h >> 16);
}

}
