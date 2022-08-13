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

#include "common.h"
#include "defs.h"
#include "cast_helper.h"
#include "fdb_instance.h"

#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#include <stdlib.h>
#include <string>

namespace fdb
{

static const std::string areaExpiredPrefix = "area_expired_";

FdbInstance::FdbInstance(mybase::BaseLogger* logger) : myLog(logger)
{}

FdbInstance::FdbInstance(int32_t index, bool db_version_care)
{}

// TODO: optimize
bool FdbInstance::initialize(const std::string& db_path)
{
    if ( !loadAreaExpiredSec() ) {
        return false;
    }

    if (!openDataDB() ) {
        return false;
    }

    maxMigrateBatchSize = sDefaultConfig.getInt(sDataServerSection, sMigrateBatchSize, 1048576); // 1M default
    maxMigrateBatchCount = sDefaultConfig.getInt(sDataServerSection, sMigrateBatchCount, 2000); // 2000 default
    return true;
}

bool FdbInstance::openDataDB()
{
    const char* data_db_path = sDefaultConfig.getString(sDataServerSection, sDataDbPath, "data/datadb");
    const char* log_dir = sDefaultConfig.getString(sDataServerSection, sLogDir, "logs");

    _log_warn(myLog, "trying to open data db! path[%s]", data_db_path);

    jungle::GlobalConfig global_config;
    global_config.globalLogPath = log_dir;

    jungle::init(global_config);
    jungle::DB::open(&dataDb, data_db_path, jungle::DBConfig());

    if (!dataDb) {
        _log_err(myLog, "open data db failed! path[%s]", data_db_path);
        return false;
    }

    _log_warn(myLog, "open data db success! path[%s]", data_db_path);
    return true;
}

FdbInstance::~FdbInstance()
{
    if ( dataDb ) {
        jungle::DB::close(dataDb);
        dataDb = nullptr;
    }
}

void FdbInstance::destroy() { }

int32_t FdbInstance::put(int32_t ns, int32_t bucket_number, rocksdb::Slice key, rocksdb::Slice value, uint32_t expire_time)
{
    uint32_t mdate = TimeHelper::currentSec();
    uint32_t edate = mdate + expire_time;

    _log_debug(myLog, "bucket_number[%d] key.data_meta.mdate = %u expire_time = %u edate = %u",
               bucket_number, key.data_meta.mdate, expire_time, edate);

    FdbKey fdb_key(key.getData(), key.getSize(), bucket_number, ns);

    FdbItem fdb_item;
    fdb_item.meta().flag = value.data_meta.flag;
    fdb_item.meta().mdate = mdate;
    fdb_item.meta().edate = edate;
    fdb_item.meta().version = key.data_meta.version;

    fdb_item.set(value.getData(), value.getSize());

    return do_put(fdb_key, fdb_item);
}

int32_t FdbInstance::get(int32_t ns, int32_t bucket_number, rocksdb::Slice key, std::string& value)
{
    int32_t area = key.decodeArea();
    FdbKey fdb_key(key.getData(), key.getSize(), bucket_number, area);
    FdbItem fdb_item;

    std::string fdb_value;
    int32_t rc = do_get(fdb_key, fdb_value);
    if ( rc != OP_RETURN_SUCCESS ) {
        return rc;
    }
    fdb_item.assign((char*)fdb_value.data(), fdb_value.size());

    uint64_t current_sec = TimeHelper::currentSec();
    uint64_t area_expired_sec = getAreaExpiredTime(area);
    if ( (fdb_item.meta().edate != 0 &&  fdb_item.meta().edate < current_sec) ||
         (fdb_item.meta().mdate < area_expired_sec) || !validBucketMgn.valid(bucket_number) ) {
        _log_debug(myLog, "data expired, key[%.*s] edate[%u] current_sec[%lu] area_expired_sec[%lu] bucket_number[%d] valid[%d]",
                   key.getSize(), key.getData(), fdb_item.meta().edate, current_sec, area_expired_sec,
                   bucket_number, validBucketMgn.valid(bucket_number));
        return KV_RETURN_DATA_EXPIRED;
    }

    key.setVersion(fdb_item.meta().version);
    key.data_meta.mdate = fdb_item.meta().mdate;
    key.data_meta.edate = fdb_item.meta().edate;
    key.data_meta.flag = fdb_item.meta().flag;
    key.data_meta.keysize = key.getSize();

    value.setData(fdb_item.value(), fdb_item.valueSize());
    value.setVersion(fdb_item.meta().version);
    value.data_meta.mdate = fdb_item.meta().mdate;
    value.data_meta.edate = fdb_item.meta().edate;
    value.data_meta.valsize = fdb_item.valueSize();
    value.data_meta.flag = fdb_item.meta().flag;

    return OP_RETURN_SUCCESS;
}

int32_t FdbInstance::remove(int32_t bucket_number, rocksdb::Slice& key, bool version_care)
{
    int32_t area = key.decodeArea();
    FdbKey fdb_key(key.getData(), key.getSize(), bucket_number, area);

    int32_t rc = do_remove(fdb_key);
    if (rc != OP_RETURN_SUCCESS) {
        return rc;
    }

    return OP_RETURN_SUCCESS;
}

bool FdbInstance::beginScan(kv::MigrateInfo& info)
{
    int32_t bucket_number = info.dbId;
    if (bucket_number < 0) {
        _log_warn(myLog, "bucket number invalid! bucket[%d]", bucket_number);
        return false;
    }

    int32_t expected = -1;
    if (!scanBucket.compare_exchange_strong(expected, bucket_number)) {
        _log_err(myLog, "begin scan failed! scanBucket[%d]!", scanBucket.load());
        return false;
    }

    char scan_key_start[sFdbKeyBucketSize] = {0};
    FdbKey::encodeBucketNumber(scan_key_start, bucket_number);
    char scan_key_end[sFdbKeyBucketSize] = {0};
    FdbKey::encodeBucketNumber(scan_key_end, bucket_number + 1);

    key_itr.init(dataDb, jungle::SizedBuf(sFdbKeyBucketSize, scan_key_start),
                 jungle::SizedBuf(sFdbKeyBucketSize, scan_key_end));
    
    still_have_ = true;

    _log_warn(myLog, "begin scan success! scanBucket[%d]!", scanBucket.load());
    return true;
}

// TODO: filter expired data
bool FdbInstance::getNextItems(std::vector<kv::ItemDataInfo*>& list)
{
    list.clear();

    if (!still_have_) {
        return false;
    }

    FdbKey fdb_key;       // reuse is ok.
    FdbItem fdb_item;

    int32_t batch_size = 0, batch_count = 0;
    int32_t meta_size = sFdbKeyBucketSize + sFdbKeyAreaSize;

    uint64_t current_sec = TimeHelper::currentSec();
    while ( (batch_size < maxMigrateBatchSize) && (batch_count < maxMigrateBatchCount) ) {
        jungle::Record rec_out;
        jungle::Record::Holder h(rec_out); // Auto free.

        if (!key_itr.get(rec_out).ok()) break;

        // match bucket
        int32_t actual_bucket = FdbKey::decodeBucketNumber((const char*)rec_out.kv.key.data);
        if ( (scanBucket.load() != -1) && (scanBucket.load() != actual_bucket) ) {
            break;
        }

        fdb_key.assign((char*)rec_out.kv.key.data, (int32_t)rec_out.kv.key.size);
        fdb_item.assign((char*)rec_out.kv.value.data, (int32_t)rec_out.kv.value.size);

        int32_t key_size = fdb_key.mergedKeySize();
        int32_t value_size = fdb_item.valueSize();
        if ( (key_size >= sMaxKeySizeWithArea) || (key_size < 1) || (value_size >= sMaxDataSize) || (value_size < 1)) {
            _log_err(myLog, "kv size invalid: k: %.*s, v: %d [%d %d] [%x %x %s]",
                      fdb_key.keySize(), fdb_key.key(), value_size, rec_out.kv.key.size,
                      rec_out.kv.value.size, *(fdb_key.key()),
                      *(fdb_key.key()+1), fdb_key.key() + 2);

            if (!key_itr.next().ok()) break;
            continue;
        }

        uint64_t area_expired_sec = getAreaExpiredTime(fdb_key.area());
        if ( (fdb_item.meta().edate != 0 &&  fdb_item.meta().edate < current_sec) ||
             (fdb_item.meta().mdate < area_expired_sec) || !validBucketMgn.valid(actual_bucket) ) {
            _log_debug(myLog, "data expired, key[%.*s] edate[%u] current_sec[%lu] area_expired_sec[%lu] bucket[%d] valid[%d]",
                      fdb_key.keySize(), fdb_key.key(), fdb_item.edate(), current_sec, area_expired_sec,
                      actual_bucket, validBucketMgn.valid(actual_bucket));
            
            if (!key_itr.next().ok()) break;
            continue;
        }

        int32_t total_size = key_size + value_size + sizeof(kv::ItemDataInfo);
        kv::ItemDataInfo* data = (kv::ItemDataInfo*) calloc(1, total_size);
        //data->header.magic = MAGIC_ITEM_META_LDB_PREFIX;
        data->header.keysize = key_size;
        data->header.version = fdb_item.version();
        data->header.valsize = value_size;
        data->header.flag = fdb_item.flag();
        data->header.prefixsize = 0;
        data->header.mdate = fdb_item.mdate();
        data->header.edate = fdb_item.edate();
        data->header.bucketId = actual_bucket;

        memcpy(data->m_data, fdb_key.mergedKey(), key_size);
        memcpy(data->m_data + key_size, fdb_item.value(), value_size);

        list.push_back(data);
        if (!key_itr.next().ok()) break;

        batch_size += total_size;
        batch_count++;
    }
    _log_debug(myLog, "get next items count[%d] size[%d]", list.size(), batch_size);

    if (list.empty()) {
        still_have_ = false;
    }

    return still_have_;
}

bool FdbInstance::endScan(kv::MigrateInfo& info)
{
    int32_t expected = info.dbId;
    if (!scanBucket.compare_exchange_strong(expected, -1)) {
        _log_err(myLog, "end scan failed! scan_bucket[%d] expect_bucket[%d]!",
                 scanBucket.load(), (int32_t)info.dbId);
        return false;
    }

    key_itr.close();

    _log_warn(myLog, "end scan success! scanBucket[%d]!", info.dbId);
    return true;
}

int32_t FdbInstance::do_put(FdbKey& fdb_key, FdbItem& fdb_item)
{
    if (!dataDb) return KV_ROCKSDB_kNotSupported;

    jungle::KV rec(fdb_key.size(), fdb_key.data(), fdb_item.size(), fdb_item.data());
    jungle::Status status = dataDb->set( rec );

    if (!status.ok()) {
        _log_err(myLog, "put failed! area[%d] key[%.*s] err[%s]",
                 fdb_key.area(), fdb_key.keySize(), fdb_key.key(), status.toString().c_str());
        return to_kvstore_code(status);
    }

    _log_debug(myLog, "put success! area[%d] key[%.*s]",
               fdb_key.area(), fdb_key.keySize(), fdb_key.key());
    return OP_RETURN_SUCCESS;
}

int32_t FdbInstance::do_get(FdbKey& fdb_key, std::string& value)
{
    if (!dataDb) return KV_ROCKSDB_kNotSupported;

    jungle::SizedBuf value_out;
    jungle::SizedBuf key(fdb_key.size(), fdb_key.data());
    jungle::Status s = dataDb->get( key, value_out );
    if (!s.ok()) {
        _log_debug(myLog, "get failed! area[%d] bucket[%d] key[%.*s] err[%s]",
                  fdb_key.area(), fdb_key.getBucketNumber(), fdb_key.keySize(),
                  fdb_key.key(), s.toString().c_str());
        return to_kvstore_code(s);
    }

    value = std::move( value_out.toString() );
    // Should free the memory of value after use.
    value_out.free();

    _log_debug(myLog, "rocksdb get success! area[%d] bucket[%d] key[%.*s] vsize[%d]",
               fdb_key.area(), fdb_key.getBucketNumber(), fdb_key.keySize(),
               fdb_key.key(), value.size());
    return OP_RETURN_SUCCESS;
}

int32_t FdbInstance::do_remove(FdbKey& fdb_key)
{
    if (!dataDb) return KV_ROCKSDB_kNotSupported;

    jungle::SizedBuf key(fdb_key.size(), fdb_key.data());
    jungle::Status s = dataDb->del( key );

    if (!s.ok()) {
        _log_debug(myLog, "get failed! area[%d] bucket[%d] key[%.*s] err[%s]",
                  fdb_key.area(), fdb_key.getBucketNumber(), fdb_key.keySize(),
                  fdb_key.key(), s.toString().c_str());
        return to_kvstore_code(s);
    }

    log_debug("rocksdb remove success! key[%.*s]", fdb_key.keySize(), fdb_key.key());
    return OP_RETURN_SUCCESS;
}

bool FdbInstance::loadAreaExpiredSec()
{
    return true;
}

void FdbInstance::setValidBucketMgn(const CValidBucketMgn& valid_bucket_mgn)
{
    locker.writeLock();
    validBucketMgn = valid_bucket_mgn;
    locker.writeUnlock();
}

int32_t FdbInstance::clearArea(int32_t area)
{

    return OP_RETURN_SUCCESS;
}

uint64_t FdbInstance::getAreaExpiredTime(int32_t area)
{
    uint64_t expired_ts = 0;

    return expired_ts;
}

int32_t FdbInstance::to_kvstore_code(jungle::Status& status)
{
    int32_t rc = OP_RETURN_SUCCESS;
    switch (status.getValue()) {
        case jungle::Status::OK: {
            rc = OP_RETURN_SUCCESS;
            break;
        }
        case jungle::Status::KEY_NOT_FOUND: {
            rc = KV_RETURN_DATA_NOT_EXIST;
            break;
        }
        default: {
            rc = KV_FORESTDB_kMinCode + status.getValue();
            break;
        }
    }

    return rc;
}

std::string FdbInstance::getDbStatisticsInfo()
{
    return std::string();
}

std::string FdbInstance::dbstat(const std::string& type)
{
    std::string stats;
    return std::move(stats);
}

}
