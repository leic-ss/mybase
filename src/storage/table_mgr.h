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

#include "bitmap.h"
#include "validbucket.h"
#include "dlog.h"
#include "common.h"
#include "defs.h"

#include <unordered_map>
#include <mutex>
#include <set>
#include <vector>

namespace mybase
{

using BucketServerMap = std::unordered_map<int32_t, std::vector<uint64_t>>;
using BucketServerMapIt = std::unordered_map<int32_t, std::vector<uint64_t>>::iterator;

class TableMgr
{
public:
    TableMgr();
    ~TableMgr();

    bool isMaster(int32_t bucket_number, int32_t server_flag);

    std::vector<uint64_t> getSlaves(int32_t bucket_number, bool is_migrating = false) ;
    std::vector<uint64_t> getAllDs(int32_t bucket_number, bool is_migrating = false);
    uint64_t getMigrateTarget(int32_t bucket_number) ;

    void computeAllSlaveServerIds();
    void computeAllProxyServerIds();

    void initMigrateDoneSet(CBitMap& migrate_done_set, const std::vector<uint64_t> &current_state_table);
    void doUpdateTable(uint64_t *new_server_table, size_t size, uint32_t table_version, uint32_t copy_count, uint32_t bucket_count, bool fromMaster);
    std::vector<int32_t> getHoldingBuckets() const { return holding_buckets; }
    std::vector<int32_t> getPaddingBuckets() const { return padding_buckets; }
    std::vector<int32_t> getReleaseBuckets() const { return release_buckets; }
    BucketServerMap getMigrates() const { return migrates; }
    std::set<uint64_t> getAvailableServerIds() const { return available_server_ids; }

    uint32_t getVersion() const { return table_version; }
    uint32_t getCopyCount() const { return copy_count; }
    uint32_t getBucketCount() const { return bucket_count; }
    uint32_t getHashTableSize() const { return bucket_count * copy_count; }
    void clearAvailableServer() { available_server_ids.clear(); }
    void setTableForLocalmode() { copy_count = bucket_count = 1u; }

    const CValidBucketMgn& getValidBucketMgn() const { return m_validBucketMgn; }
    const std::set<uint64_t>& getAllSlaveServerIds() const { return m_allSlaveServerIds; }
    const std::set<uint64_t>& getAllProxyServerIds() const { return m_allProxyServerIds; }

private:
    void calculateReleaseBucket(std::vector<int32_t> &newHoldingBucket);
    void calculateMigrates(uint64_t *table, int32_t index);

private:
    uint64_t *server_table{nullptr};
    uint32_t table_version;
    uint32_t copy_count;
    uint32_t bucket_count;

    mybase::BaseLogger* myLog{nullptr};

    RWSimpleLock locker;

    std::vector<int32_t> holding_buckets;
    std::vector<int32_t> padding_buckets; //目的对照表中本ds要处理的bucket(主备都算)
    std::vector<int32_t> release_buckets;
    std::set<uint64_t> available_server_ids;

    std::set<uint64_t> m_allSlaveServerIds;
    std::set<uint64_t> m_allProxyServerIds;

    BucketServerMap migrates;
    CValidBucketMgn m_validBucketMgn;
};

}