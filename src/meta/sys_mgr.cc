
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

#include "sys_mgr.h"
#include "table_builder.h"
#include "public/common.h"
#include "rocksdb/utilities/options_util.h"

#include <unordered_map>

#include <inttypes.h>

static const std::string sNamespaceKey = "namespace_k";

namespace mybase
{

SysMgr::SysMgr(mybase::BaseLogger* logger) : myLog(logger)
{
    char server_table_file_name[256];
    const char *sz_data_dir = sDefaultConfig.getString(sMetaSection, sDataDir, sDefaultDataDir);
    snprintf(server_table_file_name, 256, "%s/server_table", sz_data_dir);

    tableMgr.setLogger(logger);
    if(tableMgr.open(server_table_file_name)) {
        if((*tableMgr.migrateBlockCount) > 0) {
            _log_info(myLog, "restore migrate machine list, size: %d", *tableMgr.migrateBlockCount);
            fillMigrateMachine();
        }
        deflateHashTable();

        _log_info(myLog, "open %s success!", server_table_file_name);
    } else {
        _log_info(myLog, "open %s success!", server_table_file_name);
    }
}

SysMgr::~SysMgr()
{
    for (auto& handle : sysCfHandles) {
        delete handle;
    }

    if (sysDb) {
        delete sysDb;
        sysDb = nullptr;
    }
}

bool SysMgr::loadSysData()
{
    std::string value;
    rocksdb::Status status = sysDb->Get(rocksdb::ReadOptions(), sNamespaceKey, &value);

    do {
        if (status.IsNotFound()) break;

        if (!status.ok()) {
            _log_err(myLog, "read sys data failed!");
            return false;
        }

        if (!sysData.ParseFromArray(value.data(), value.size())) {
            _log_err(myLog, "parse sys data failed!");
            return false;
        }
    } while (false);

    uint32_t curtime = TimeHelper::currentSec();

    auto nmap = sysData.mutable_nodelist();
    for (auto& ele : *nmap) {
        if (ele.second.status() != ServerInfo::Alive) {
            continue;
        }

        ele.second.set_lasttime(curtime);
    }

    _log_info(myLog, "load sys data success!");
    return true;
}

bool SysMgr::start()
{
    bool expect = false;
    if (!running.compare_exchange_strong(expect, true)) {
        return false;
    }

    chkThread = std::thread(&SysMgr::serverCheck, this);

    return true;
}

bool SysMgr::stop()
{
    bool expect = true;
    if (!running.compare_exchange_strong(expect, false)) {
        return false;
    }

    if (chkThread.joinable()) {
        chkThread.join();
    }

    return true;
}

int32_t SysMgr::fillMigrateMachine()
{
    int32_t migrate_count = 0;
    migrateMachine.clear();

    for(uint32_t i = 0; i < tableMgr.getBucketCount(); i++) {
        if (tableMgr.mHashTable[i] == 0) {
            _log_err(myLog, "fill_migrate_machine should not run here,bucket: %u", i);
            continue;
        }

        bool migrate_this_bucket = false;
        do {
            if(tableMgr.mHashTable[i] != tableMgr.dHashTable[i]) {
                migrate_this_bucket = true;
                break;
            }

            for(uint32_t j = 1; j < tableMgr.getCopyCount(); j++) {
                bool found = false;
                for(uint32_t k = 1; k < tableMgr.getCopyCount(); k++) {
                    if(tableMgr.dHashTable[j * tableMgr.getBucketCount() + i]
                            == tableMgr.mHashTable[k * tableMgr.getBucketCount() + i]) {
                        found = true;
                        break;
                    }
                }

                if(found == false) {
                    migrate_this_bucket = true;
                    break;
                }
            }
        } while(false);

        if(migrate_this_bucket) {
            migrateMachine[tableMgr.mHashTable[i]]++;
            migrate_count++;
            _log_warn(myLog, "added migrating bucket[%d] machine[%s]",
                      i, NetHelper::addr2String(tableMgr.mHashTable[i]).c_str());
        }
    }

    return migrate_count;
}

// void SysMgr::setMigratingHashtable(uint32_t bucket_no, uint64_t srv_id)
// {
//     bool ret = false;
//     if(bucket_no > tableMgr.getBucketCount()) {
//         _log_err(myLog, "invalid bucket %d", bucket_no);
//         return;
//     }

//     if(tableMgr.mHashTable[bucket_no] != srv_id) {
//         _log_err(myLog, "invalid message! bucket_no: %u, m_server: %s, server: %s",
//                  bucket_no, NetHelper::addr2String(tableMgr.mHashTable[bucket_no]).c_str(),
//                  NetHelper::addr2String(srv_id).c_str());
//         return;
//     }

//     for(uint32_t i = 0; i < tableMgr.getCopyCount(); i++) {
//         uint32_t idx = i * tableMgr.getBucketCount() + bucket_no;
//         if(tableMgr.mHashTable[idx] != tableMgr.dHashTable[idx])
//         {
//             tableMgr.mHashTable[idx] = tableMgr.dHashTable[idx];
//             ret = true;
//         }
//     }

//     if(ret == true)
//     {
//         (*tableMgr.migrateBlockCount)--;
//         auto it = migrateMachine.find(srv_id);
//         assert(it != migrateMachine.end());
//         it->second--;
//         if(it->second == 0) {
//             migrateMachine.erase(it);
//         }

//         _log_info(myLog, "setMigratingHashtable bucketNo %d server: %s, finish migrate this bucket",
//                   bucket_no, NetHelper::addr2String(srv_id).c_str());
//         _log_info(myLog, "waiting migrate dataserver num: %d", migrateMachine.size());

//         for( auto iter = migrateMachine.begin(); iter != migrateMachine.end(); ++iter )
//         {
//             _log_info(myLog, "waiting migrate dataserver count: %s:%d",
//                       NetHelper::addr2String(iter->first).c_str(), iter->second);
//         }
//     }
// }

void SysMgr::setStatInfo(uint64_t server_id, const NodeStatInfo& node_info)
{
    if(server_id == 0) return;

    statInfoRwLocker.wrlock();
    statInfo[server_id] = node_info;
    statInfoRwLocker.unlock();
    return;
}

void SysMgr::serverCheck()
{
    while (running) {

        uint32_t curtime = TimeHelper::currentSec();

        auto node_list = sysData.mutable_nodelist();
        for (auto& node : *node_list) {
            auto& server_info = node.second;
            uint32_t lasttime = server_info.lasttime();
            int32_t status = server_info.status();

            if ( (status == ServerInfo_Status_Alive) && (curtime > lasttime) &&
                 ((curtime - lasttime) >= 4) ) {
                _log_warn(myLog, "server %s is down! lasttime: %u",
                          NetHelper::addr2String(server_info.serverid()).c_str(), lasttime);

                server_info.set_status(ServerInfo_Status_Down);
                rebuildQuickTable();
            }
        }

        checkMigrateComplete();

        KVSLEEP(running, 1);
    }
}

void SysMgr::getStatInfo(uint64_t server_id, NodeStatInfo& node_info)
{
    statInfoRwLocker.wrlock();
    if(server_id == 0) {
        // _log_info(myLog, "getStatInfo: ");
        for(auto it = statInfo.begin(); it != statInfo.end(); it++) {
            // _log_info(myLog, "getStatInfo: %lu", it->first);
            if (availableServer.find(it->first) != availableServer.end()) {
                // _log_info(myLog, "getStatInfo 2: %lu, size = %d", it->first, it->second.getStatData().size());
                node_info.update_stat_info(it->second);
                // _log_info(myLog, "getStatInfo 2: %lu, size = %d", it->first, node_info.getStatData().size());
            }
        }
    } else {
        auto it = statInfo.find(server_id);
        if(it != statInfo.end()) {
            node_info.update_stat_info(it->second);
        }
    }
    statInfoRwLocker.unlock();
    return;
}

const uint64_t* SysMgr::getHashTable(int32_t mode) const
{
    if(mode == 0) {
        return tableMgr.hashTable;
    } else if(mode == 1) {
        return tableMgr.mHashTable;
    }
    return tableMgr.dHashTable;
}

const char *SysMgr::getHashTableDeflateData(int32_t mode) const
{
    if(mode == 0) {
        return tableMgr.hashTableDeflateDataForClient;
    }
    return tableMgr.hashTableDeflateDataForDataServer;
}

int32_t SysMgr::getHashTableDeflateSize(int32_t mode) const
{
    if(mode == 0) {
        return tableMgr.hashTableDeflateDataForClientSize;
    }
    return tableMgr.hashTableDeflateDataForDataServerSize;
}

bool SysMgr::openSysDB()
{
    rocksdb::DBOptions sys_db_opts;
    const char* sys_db_path = sDefaultConfig.getString(sMetaServer, sSysDbPath, "data/sysdb");
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

bool SysMgr::addStorageServer(uint64_t srv_id)
{
    auto& nmap = sysData.nodelist();
    auto iter = nmap.find(srv_id);
    if (iter != nmap.end()) {
        _log_warn(myLog, "existing server: %s", NetHelper::addr2String(srv_id).c_str());
        return false;
    }

    ServerInfo sinfo;
    sinfo.set_serverid(srv_id);
    sinfo.set_lasttime(TimeHelper::currentSec());
    sinfo.set_status(ServerInfo::Initial);

    auto node_list = sysData.mutable_nodelist();
    (*node_list)[srv_id] = sinfo;

    std::string data;
    if (!sysData.SerializeToString(&data)) {
        _log_err(myLog, "add storage server failed! srv[%s]", NetHelper::addr2String(srv_id).c_str());
        return false;
    }

    rocksdb::Status s = sysDb->Put(rocksdb::WriteOptions(), sNamespaceKey, data);
    if (!s.ok()) {
        _log_err(myLog, "add storage server failed! srv[%s] err[%s]",
                 NetHelper::addr2String(srv_id).c_str(), s.ToString().c_str());
        return false;
    }

    return true;
}

bool SysMgr::loadConfig()
{
    int32_t bucket_count = sDefaultConfig.getInt(sMetaServer, sGroupDataBucketNumber, sDefaultBucketNumber);
    int32_t copy_count = sDefaultConfig.getInt(sMetaServer, sStrCopyCount, sDefaultCopyCountNumber);
    if( !tableMgr.isFileOpened() ) {
        char file_name[256];
        const char *sz_data_dir = sDefaultConfig.getString(sMetaServer, sDataDir, sDefaultDataDir);
        snprintf(file_name, 256, "%s/server_table", sz_data_dir);

        bool tmp_ret = tableMgr.create(file_name, bucket_count, copy_count);
        (void)(tmp_ret);

        _log_info(myLog, "set bucket count = %u ok", tableMgr.getBucketCount());
        _log_info(myLog, "set copy count = %u ok", tableMgr.getCopyCount());
    }

    _log_debug(myLog, "loadconfig, lastVersion: %d",
               (tableMgr.lastLoadConfigTime ? *tableMgr.lastLoadConfigTime : 0) );

    minConfigVersion = sDefaultConfig.getInt(sMetaServer, sStrMinConfigversion, sMinConfigVersion);
    if(minConfigVersion < sMinConfigVersion) {
        _log_err(myLog, "%s must large than %d. set it to %d", sStrMinConfigversion, sMinConfigVersion, sMinConfigVersion);
        minConfigVersion = sMinConfigVersion;
    }

    uint64_t old_pos_mask = posMask;
    posMask = strtoll(sDefaultConfig.getString(sMetaServer, sStrPosMask, "0"), nullptr, 10);
    if(posMask == 0) posMask = sDefaultPosMask;
    _log_info(myLog, "%s = %llX", sStrPosMask, posMask);

    return true;
}

void SysMgr::getUpNode(std::vector<ServerInfo>& upnode_list)
{
    availableServer.clear();

    auto& nmap = sysData.nodelist();
    for(auto& ele : nmap) {
        if(ele.second.status() != ServerInfo::Alive) {
            _log_info(myLog, "skip inactive node: <%s>", NetHelper::addr2String(ele.second.serverid()).c_str());
            continue;
        }

        _log_info(myLog, "get up node: <%s>", NetHelper::addr2String(ele.second.serverid()).c_str());
        upnode_list.push_back(ele.second);
        availableServer.insert(ele.second.serverid());
    }
}

void SysMgr::rebuildQuickTable()
{
    std::vector<ServerInfo> upnode_list;
    bool first_run = true;

    getUpNode(upnode_list);

    size_t size = upnode_list.size();
    _log_info(myLog, "upnodeList.size = %d", size);
    for(uint32_t i = 0; i < tableMgr.getHashTableSize(); i++) {
        if(tableMgr.mHashTable[i] != 0) {
            first_run = false;
            break;
        }
    }

    TableBuilder table_builder(tableMgr.getBucketCount(), tableMgr.getCopyCount());

    table_builder.setLogger(myLog);
    table_builder.setAvailableServer(upnode_list);
    table_builder.printAvailableServer();

    TableBuilder::hash_table_type hash_table_for_builder_tmp;
    table_builder.loadHashTable(hash_table_for_builder_tmp, tableMgr.mHashTable);

    TableBuilder::hash_table_type quick_table_tmp = hash_table_for_builder_tmp;
    if( (tableMgr.getCopyCount() > 1) && (first_run == false) ) {
        _log_info(myLog, "will build quick table");
        table_builder.printHashTable(quick_table_tmp);

        if( !( table_builder.buildQuickTable(quick_table_tmp) ) ) {

            incrVersion(tableMgr.clientVersion);
            incrVersion(tableMgr.serverVersion);

            _log_err(myLog, "build quich table error!");

            // APPEND RAFT LOG
            return;
        }

        _log_info(myLog, "quick table build ok,new quick hashtable");
        table_builder.printHashTable(quick_table_tmp);
    }

    table_builder.writeHashTable(quick_table_tmp, tableMgr.mHashTable);
    table_builder.writeHashTable(quick_table_tmp, tableMgr.hashTable);
    table_builder.writeHashTable(quick_table_tmp, tableMgr.dHashTable);

    incrVersion(tableMgr.clientVersion);
    incrVersion(tableMgr.serverVersion);
    incrVersion(tableMgr.namespaceCapacityVersion);

    _log_info(myLog, "version changed: clientVersion: %u serverVersion: %u namespaceCapacityVersion: %u",
              *tableMgr.clientVersion, *tableMgr.serverVersion, *tableMgr.namespaceCapacityVersion);

    deflateHashTable();

    return ;
}

void SysMgr::rebuild()
{
    std::vector<ServerInfo> upnode_list;
    bool first_run = true;

    getUpNode(upnode_list);

    size_t size = upnode_list.size();
    _log_info(myLog, "upnodeList.size = %d", size);
    for(uint32_t i = 0; i < tableMgr.getHashTableSize(); i++) {
        if(tableMgr.mHashTable[i] != 0) {
            first_run = false;
            break;
        }
    }

    TableBuilder* p_table_builder = new TableBuilder(tableMgr.getBucketCount(), tableMgr.getCopyCount());

    p_table_builder->setLogger(myLog);
    p_table_builder->setAvailableServer(upnode_list);
	p_table_builder->printAvailableServer();

    TableBuilder::hash_table_type hash_table_for_builder_tmp;
    p_table_builder->loadHashTable(hash_table_for_builder_tmp, tableMgr.mHashTable);

    TableBuilder::hash_table_type quick_table_tmp = hash_table_for_builder_tmp;
    if( (tableMgr.getCopyCount() > 1) &&
        first_run == false ) {
        _log_info(myLog, "will build quick table");
        p_table_builder->printHashTable(quick_table_tmp);

        if( !( p_table_builder->buildQuickTable(quick_table_tmp) ) ) {
            delete p_table_builder;

            incrVersion(tableMgr.clientVersion);
            incrVersion(tableMgr.serverVersion);

            _log_err(myLog, "build quich table error!");

            // APPEND RAFT LOG
            return;
        }

        _log_info(myLog, "quick table build ok,new quick hashtable");
        p_table_builder->printHashTable(quick_table_tmp);
    }

    TableBuilder::hash_table_type dest_hash_table_for_builder_tmp;
    int32_t ret = p_table_builder->rebuildTableNew(quick_table_tmp, dest_hash_table_for_builder_tmp);

    if( BUILD_NO_CHANGE == ret ) {
        delete p_table_builder;
        return;
    } else if(ret == 0) {
        _log_err(myLog, "build table fail. fatal error.");

        delete p_table_builder;
        return;
    }

    p_table_builder->writeHashTable(dest_hash_table_for_builder_tmp, tableMgr.dHashTable);

    if( !first_run ) {
        _log_info(myLog, "need migrate write quick table to _p_hashTable");
        p_table_builder->writeHashTable(quick_table_tmp, tableMgr.mHashTable);
        p_table_builder->writeHashTable(quick_table_tmp, tableMgr.hashTable);

        if(tableMgr.getCopyCount() <= 1) {
            uint32_t total_count = tableMgr.getBucketCount() * tableMgr.getCopyCount();
            for(uint32_t i = 0; i < total_count; i++) {
                if(availableServer.find(tableMgr.mHashTable[i])== availableServer.end()) {
                    tableMgr.mHashTable[i] = tableMgr.dHashTable[i];
                    tableMgr.hashTable[i] = tableMgr.dHashTable[i];
                }
            }
        }
    } else {
        _log_info(myLog, "need`t migrate write original table to _p_hashTable");
        p_table_builder->writeHashTable(dest_hash_table_for_builder_tmp, tableMgr.mHashTable);
        p_table_builder->writeHashTable(dest_hash_table_for_builder_tmp, tableMgr.hashTable);

        tableMgr.produceMd5();
    }

    _log_info(myLog, "_confServerTableManager._p_hashTable");

    tableMgr.logPrintTable(tableMgr.getHashTable());

    _log_info(myLog, "_mhashTable");
    tableMgr.logPrintTable(tableMgr.getMHashTable());

    _log_info(myLog, "_dhashTable");
    tableMgr.logPrintTable(tableMgr.getDHashTable());

    int32_t diff_count = 0;
    if(first_run == false) {
        diff_count = fillMigrateMachine();
    }

    (*tableMgr.migrateBlockCount) = diff_count > 0 ? diff_count : -1;
    incrVersion(tableMgr.clientVersion);
    incrVersion(tableMgr.serverVersion);
    incrVersion(tableMgr.namespaceCapacityVersion);
    _log_info(myLog, "version changed: clientVersion: %u serverVersion: %u namespaceCapacityVersion: %u migrateBlockCount: %d",
              *tableMgr.clientVersion, *tableMgr.serverVersion, *tableMgr.namespaceCapacityVersion, diff_count);
    deflateHashTable();

    do {
        diff_count = 0;
        for(uint32_t i = 0; i < tableMgr.getBucketCount(); i++) {
            for(uint32_t j = 0; j < tableMgr.getCopyCount(); j++) {
                bool migrate_it = true;
                for(uint32_t k = 0; k < tableMgr.getCopyCount(); ++k) {
                    if(tableMgr.dHashTable[i +tableMgr.getBucketCount() *j ] ==
                            tableMgr.mHashTable[i +tableMgr.getBucketCount() *k]) {
                        migrate_it = false;
                        break;
                    }
                }

                if(migrate_it) {
                    diff_count++;
                }
            }
        }
    } while (false);

    _log_warn(myLog, "different ratio is : migrateCount:%d/%d=%.2f%%",
              diff_count, tableMgr.getBucketCount() * tableMgr.getCopyCount(),
              ( 100.0 * diff_count / (tableMgr.getBucketCount() * tableMgr.getCopyCount()) ) );

    if(diff_count > 0) {
        _log_warn(myLog, "need migrate, count: %d", diff_count);
    }

    printServerCount();

    delete p_table_builder;
    return;
}

bool SysMgr::doFinishMigrate(uint64_t server_id, uint32_t server_version, int32_t bucket_id)
{
    _log_info(myLog, "migrate finish: from[%s] bucket[%d] base version: [%d], now version: [%d] ",
              NetHelper::addr2String(server_id).c_str(), bucket_id, server_version, *tableMgr.serverVersion);
    if(server_version != *tableMgr.serverVersion) {
        _log_err(myLog, "migrate version conflict, ds version: %d, cs version: %d",
                 server_version, *tableMgr.serverVersion);
        return false;
    }

    setMigratingHashtable(bucket_id, server_id);
    return true;
}

bool SysMgr::createNameSpace(uint32_t ns, uint64_t capacity)
{
    incrVersion(tableMgr.namespaceCapacityVersion);
    tableMgr.produceMd5();

    auto nmap = sysData.mutable_namespaces();
    (*nmap)[ns] = capacity;

    std::string data;
    if (!sysData.SerializeToString(&data)) {
        _log_err(myLog, "createNameSpace failed! ns[%u] capacity[%lu]", ns, capacity);
        return false;
    }

    rocksdb::Status s = sysDb->Put(rocksdb::WriteOptions(), sNamespaceKey, data);
    if (!s.ok()) {
        _log_err(myLog, "write namespace failed! ns[%u] capacity[%lu] err[%s]", ns, capacity, s.ToString().c_str());
        return false;
    }

    _log_warn(myLog, "createNameSpace success! ns[%u] capacity[%lu]", ns, capacity);
    return true;
}

std::map<uint32_t, uint64_t> SysMgr::getNamespaceCapacityInfo()
{
    std::map<uint32_t, uint64_t> namespaces_capacity;
    {
        auto& m = sysData.namespaces();
        for (auto& ele : m) {
             namespaces_capacity[ele.first] = ele.second;
        }
    }

    return namespaces_capacity;
}

bool SysMgr::checkMigrateComplete()
{
    if( (*tableMgr.migrateBlockCount) == -1) {
        return true;
    }

    if(migrateMachine.size() > 0U) {
        for(auto it = migrateMachine.begin(); it != migrateMachine.end(); ++it) {
            auto& nmap = sysData.nodelist();
            auto iter = nmap.find(it->first);
            if(iter == nmap.end() || iter->second.status() == ServerInfo::Down) {
                _log_info(myLog, "migrate machine %s down", NetHelper::addr2String(it->first).c_str());
            }
        }
        _log_info(myLog, "still have %d data server are in migrating.", migrateMachine.size());

        for(auto it = migrateMachine.begin(); it != migrateMachine.end(); it++) {
            _log_info(myLog, "server %s still have %d buckets waiting to be migrated",
                      NetHelper::addr2String(it->first).c_str(), it->second);
        }

        return false;
    }

    assert( memcmp(tableMgr.mHashTable,
                   tableMgr.dHashTable,
                   tableMgr.getHashTableByteSize()) == 0 );
    incrVersion(tableMgr.clientVersion);
    incrVersion(tableMgr.serverVersion);

    _log_info(myLog, "version changed: clientVersion: %u",
              *tableMgr.clientVersion, *tableMgr.serverVersion);
    memcpy((void *) tableMgr.hashTable,
           (const void *) tableMgr.dHashTable,
           tableMgr.getHashTableByteSize());
    deflateHashTable();
    (*tableMgr.migrateBlockCount) = -1;

    _log_info(myLog, "migrate all done");

    printServerCount();
    return true;
}

void SysMgr::printServerCount()
{
    std::unordered_map<uint64_t, int32_t> m;
    for(uint32_t i = 0; i < tableMgr.getBucketCount(); i++) {
        m[ tableMgr.hashTable[i] ]++;
    }

    for(auto it = m.begin(); it != m.end(); ++it) {
        _log_info(myLog, "%s => %d",
                  NetHelper::addr2String(it->first).c_str(), it->second);
    }
}

void SysMgr::setMigratingHashtable(uint32_t bucket_id, uint64_t server_id)
{
    bool ret = false;
    if(bucket_id > tableMgr.getBucketCount() || isMigrating() == false) {
        return;
    }

    if(tableMgr.mHashTable[bucket_id] != server_id) {
        _log_err(myLog, "where you god this ? old hashtable? bucket_id: %u, m_server: %s, server: %s",
                 bucket_id, NetHelper::addr2String(tableMgr.mHashTable[bucket_id]).c_str(),
                 NetHelper::addr2String(server_id).c_str());
        return;
    }

    std::lock_guard<std::mutex> l(hashTableSetLock);

    for(uint32_t i = 0; i < tableMgr.getCopyCount(); i++) {
        uint32_t idx = i * tableMgr.getBucketCount() + bucket_id;
        if(tableMgr.mHashTable[idx] != tableMgr.dHashTable[idx]) {
            tableMgr.mHashTable[idx] = tableMgr.dHashTable[idx];
            ret = true;
        }
    }

    if(ret == true) {
        (*tableMgr.migrateBlockCount)--;
        auto it = migrateMachine.find(server_id);
        it->second--;
        if(it->second == 0) {
            migrateMachine.erase(it);
            _log_warn(myLog, "server_id %s finished to migrate", NetHelper::addr2String(server_id).c_str());
        }
        _log_info(myLog, "setMigratingHashtable bucketNo %d serverId %s, finish migrate this bucket.",
                  bucket_id, NetHelper::addr2String(server_id).c_str());

        for( auto iter = migrateMachine.begin(); iter != migrateMachine.end(); ++iter ) {
            _log_info(myLog, "waiting migrate dataserver:count %s:%d",
                      NetHelper::addr2String(iter->first).c_str(), iter->second);
        }
    }

    tableMgr.backup();
    return;
}

void SysMgr::hardCheckMigrateComplete()
{
    if( memcmp(tableMgr.mHashTable, tableMgr.dHashTable,
               tableMgr.getHashTableByteSize()) == 0 ) {
        migrateMachine.clear();
    }
}

void SysMgr::getMigratingMachines(std::vector<std::pair<uint64_t, uint32_t>>& vec_server_id_count) const
{
    _log_debug(myLog, "machine size = %d", migrateMachine.size());
    for(auto it = migrateMachine.begin(); it != migrateMachine.end(); ++it) {
        vec_server_id_count.push_back(std::make_pair(it->first, it->second));
    }
}

static const char* serverStatus(int32_t status)
{
    switch (status) {
        case ServerInfo::Alive:
        {
            return "Alive";
        }
        case ServerInfo::Down:
        {
            return "Down";
        }
        case ServerInfo::ForceDown:
        {
            return "ForceDown";
        }
        case ServerInfo::Initial:
        {
            return "Initial";
        }
        default:
        {
            return "unknow";
        }
    }

    return "unknow";
}

bool SysMgr::doHeartbeat(uint64_t srv_id)
{
    // TODO: lock
    auto nmap = sysData.mutable_nodelist();
    auto iter = nmap->find(srv_id);
    if (iter == nmap->end()) {
        _log_warn(myLog, "server[%s] not in server info map, drop!", NetHelper::addr2String(srv_id).c_str());
        return false;
    }

    if (iter->second.status() != ServerInfo::Alive) {
        _log_warn(myLog, "update server[%s] status, [%s -> %s]",
                  NetHelper::addr2String(srv_id).c_str(),
                  serverStatus(iter->second.status()),
                  serverStatus(ServerInfo::Alive));
        iter->second.set_status(ServerInfo::Alive);
    }
    iter->second.set_lasttime(TimeHelper::currentSec());

    // _log_info(myLog, "update success! server[%s] lastTime[%u]",
    //           NetHelper::addr2String(srv_id).c_str(), iter->second.lasttime());
    return true;
}

void SysMgr::incrVersion(uint32_t* value, const uint32_t inc_step)
{
    (*value) += inc_step;
    if(minConfigVersion > (*value)) {
        (*value) = minConfigVersion;
    }
}

void SysMgr::incrVersion(const uint32_t inc_step)
{
    incrVersion(tableMgr.clientVersion, inc_step);
    incrVersion(tableMgr.serverVersion, inc_step);

    tableMgr.produceMd5();
}

}
