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

#include "table_mgr.h"

#include <algorithm>
#include <sstream>

namespace mybase
{

static const int32_t MISECONDS_ASSURANCE_TIME = 30;

TableMgr::TableMgr()
{
    server_table = nullptr;
    table_version = 0u;
    copy_count = 0u;
    bucket_count = 0u;
}

TableMgr::~TableMgr()
{
    if (server_table != nullptr) {
        delete[] server_table;
        server_table = nullptr;
    }
}

bool TableMgr::isMaster(int32_t bucket_number, int32_t server_flag)
{
    locker.rdlock();
    int index = bucket_number;
    if (server_flag == KV_SERVERFLAG_PROXY) {
        index += getHashTableSize();
    }

    bool rc = (server_table[index] == NetHelper::sLocalServerAddr);
    locker.unlock();

    return rc;
}

void TableMgr::doUpdateTable(uint64_t *new_table, size_t size, uint32_t table_version,
                                 uint32_t copy_count, uint32_t bucket_count, bool fromMaster)
{
    this->copy_count = copy_count;
    this->bucket_count = bucket_count;
    assert((size % getHashTableSize()) == 0);

    _log_debug(myLog, "list a new table size = %d ", size);
    for (size_t i = 0; i < size; ++i) {
        _log_debug(myLog, "serverTable[%d] = %s", i, NetHelper::addr2String(new_table[i]).c_str() );
    }

    uint64_t* temp_table = new uint64_t[size];
    memcpy(temp_table, new_table, size * sizeof(uint64_t));

    {
        locker.wrlock();
        uint64_t *old_table = server_table;
        server_table = temp_table;
        if (old_table != nullptr) {
            usleep(MISECONDS_ASSURANCE_TIME);
            delete[] old_table;
        }
        locker.unlock();
    }

    padding_buckets.clear();
    release_buckets.clear();
    migrates.clear();
    available_server_ids.clear();

    std::vector<int32_t> temp_holding_buckets;

    for (size_t i = 0; i < getHashTableSize(); i++) {
        available_server_ids.insert(new_table[i]);
        if (new_table[i] == NetHelper::sLocalServerAddr) {
            _log_debug(myLog, "take bucket: %d", (i % this->bucket_count) );
            temp_holding_buckets.push_back(i % this->bucket_count);

            if (i < this->bucket_count && size > getHashTableSize()) {
                calculateMigrates(new_table, i);
            }
        }
    }

    _log_info(myLog, "caculate migrate ok size = %d", migrates.size());
    for (auto it = migrates.begin(); it != migrates.end(); ++it) {
        for(auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
            _log_info(myLog, "bucket id:%d will migrate to server id %s ", it->first, NetHelper::addr2String(*it2).c_str());
        }
    }

    if (size > getHashTableSize()) {
        for (size_t i = getHashTableSize(); i < 2 * getHashTableSize(); i++) {
            available_server_ids.insert(new_table[i]);
            if (new_table[i] == NetHelper::sLocalServerAddr) {
                padding_buckets.push_back(i % this->bucket_count);
            }
        }
    }

    calculateReleaseBucket(temp_holding_buckets);

    holding_buckets = temp_holding_buckets;

    this->table_version = table_version;

    for (size_t i=0; i<holding_buckets.size(); i++) {
        _log_debug(myLog, "holding bucket: %d", holding_buckets[i]);
    }

    m_validBucketMgn.clean();

    m_validBucketMgn.add(holding_buckets);
    m_validBucketMgn.add(padding_buckets);

    m_validBucketMgn.setFromMaster(fromMaster);

    computeAllSlaveServerIds();
    computeAllProxyServerIds();
}

std::vector<uint64_t> TableMgr::getSlaves(int32_t bucket_number, bool is_migrating)
{
    locker.rdlock();
    assert(server_table != NULL);

    std::vector<uint64_t> slaves;

    int32_t end = getHashTableSize();
    int32_t index = bucket_number + this->bucket_count;
    if (is_migrating) {
        index += getHashTableSize();
        end += getHashTableSize();
    }

    if (server_table[index - this->bucket_count] != NetHelper::sLocalServerAddr) {
        locker.unlock();
        return slaves;
    }

    while (index < end)
    {
        uint64_t sid = server_table[index];
        if (sid != 0) {
            _log_debug(myLog, "add slave: %s", NetHelper::addr2String(sid).c_str());
            slaves.push_back(sid);
        }
        index += this->bucket_count;
    }

    locker.unlock();
    return slaves;
}

std::vector<uint64_t> TableMgr::getAllDs(int32_t bucket_number, bool is_migrating) //获取某行所有的节点
{
    locker.rdlock();
    assert(server_table != NULL);

    std::vector<uint64_t> result;

    int32_t end = getHashTableSize();
    int32_t index = bucket_number;
    if (is_migrating) {
        index += getHashTableSize();
        end += getHashTableSize();
    }

    do
    {
        uint64_t sid = server_table[index];
        if (sid != 0) {
            result.push_back(sid);
        }
        index += this->bucket_count;
    } while (index < end);
    locker.unlock();

    return result;
}


uint64_t TableMgr::getMigrateTarget(int32_t bucket_number)
{
    locker.rdlock();
    assert(server_table != NULL);
    uint64_t number = server_table[bucket_number + getHashTableSize()];
    locker.unlock();

    return number;
}

void TableMgr::computeAllSlaveServerIds()
{
    m_allSlaveServerIds.clear();

    for(uint32_t i = 0 ; i < bucket_count; ++i) {
        std::vector<uint64_t> vec = getSlaves(i, false);
        for(auto iter = vec.begin(); iter != vec.end(); ++iter) {
            m_allSlaveServerIds.insert(*iter);
        }

        vec = getSlaves(i, true);//目标表
        for(auto iter = vec.begin(); iter != vec.end(); ++iter) {
            m_allSlaveServerIds.insert(*iter);
        }
    }
}

void TableMgr::computeAllProxyServerIds()
{
    m_allProxyServerIds.clear();

    for(uint32_t i = 0 ; i < bucket_count; ++i)
    {
        std::vector<uint64_t> quick = getAllDs(i, false);
        std::vector<uint64_t> target = getAllDs(i, true);

        bool hold = false;
        for(auto iter = quick.begin(); iter != quick.end(); ++iter) {
            if( *iter == NetHelper::sLocalServerAddr ) {
                hold = true;
                break;
            }
        }

        if (!hold) continue;

        std::vector<uint64_t> quickSort(quick);
        std::vector<uint64_t> targetSort(target);

        std::sort(quickSort.begin(), quickSort.end());
        std::sort(targetSort.begin(), targetSort.end());

        if( quickSort != targetSort ) {
            if( target.size() >= 0 && target[0] != 0 ) {
                m_allProxyServerIds.insert(target[0]);
            }
        }
    }

    return ;
}

void TableMgr::calculateReleaseBucket(std::vector<int32_t>& new_holding_bucket)
{
    std::sort(new_holding_bucket.begin(), new_holding_bucket.end());
    std::sort(holding_buckets.begin(), holding_buckets.end());

    std::set_difference(holding_buckets.begin(), holding_buckets.end(),
                        new_holding_bucket.begin(), new_holding_bucket.end(),
                        std::inserter(release_buckets, release_buckets.end()));

    std::stringstream ss;
    ss << "release_buckets list: ";
    bool first = true;
    for(auto iter = release_buckets.begin(); iter != release_buckets.end(); ++iter) {
        if (first) {
            ss << *iter;
            first = false;
        } else {
            ss << ", " << *iter;
        }
    }

    _log_info(myLog, "%s", ss.str().c_str());
}

void TableMgr::calculateMigrates(uint64_t *table, int32_t index)
{
    uint64_t* psource_table = table;
    uint64_t* pdest_table = table + getHashTableSize();
    for (size_t i = 0; i < this->copy_count; ++i) {
        bool need_migrate = true;
        uint64_t dest_dataserver = pdest_table[index + this->bucket_count * i];

        for (size_t j = 0; j < this->copy_count; ++j) {
            if (dest_dataserver == psource_table[index + this->bucket_count * j]) {
                need_migrate = false;
                break;
            }
        }

        if (need_migrate) {
            _log_debug(myLog, "add migrate item: bucket[%d] => %s", index, NetHelper::addr2String(dest_dataserver).c_str());
            migrates[index].push_back(dest_dataserver);
        }
    }

    auto it = migrates.find(index);
    if (it == migrates.end()) {
        if (psource_table[index] != pdest_table[index]) {
            // do nothing
        }
    }
}

void TableMgr::initMigrateDoneSet( CBitMap& migrate_done_set, const std::vector<uint64_t> &current_state_table)
{
    _log_warn(myLog, "run init_migrate_done_set");

    locker.rdlock();

    int32_t bucket_number = 0;
    for (size_t i = 0; i < current_state_table.size(); i += this->copy_count)
    {
        bucket_number = (int32_t) current_state_table[i++]; // skip the bucket_number
        bool has_migrated = false;
        for (size_t j = 0; j < this->copy_count; ++j)
        {
            //迁移表与快表中只要存在某个ds不一样，说明迁移完成
            if (current_state_table[i + j] != server_table[bucket_number + j * this->bucket_count])
            {
                has_migrated = true;
                break;
            }
        }
        if (has_migrated) {
            migrate_done_set.set(bucket_number);
            _log_debug(myLog, "bucket[%d] has migrated", bucket_number);
        }
    }

    locker.unlock();
}

}
