
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
    return 0;
}

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



        KVSLEEP(running, 1);
    }
}

void SysMgr::getStatInfo(uint64_t server_id, NodeStatInfo& node_info)
{
    statInfoRwLocker.wrlock();
    if(server_id == 0) {
        for(auto it = statInfo.begin(); it != statInfo.end(); it++) {
            if (availableServer.find(it->first) != availableServer.end()) {
                node_info.update_stat_info(it->second);
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
    return nullptr;
}

const char *SysMgr::getHashTableDeflateData(int32_t mode) const
{
    return nullptr;
}

int32_t SysMgr::getHashTableDeflateSize(int32_t mode) const
{
    return 0;
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

    rocksdb::WriteBatch batch;
    batch.Put(sNamespaceKey, data);
    rocksdb::Status s = sysDb->Write(rocksdb::WriteOptions(), &batch);
    if (!s.ok()) {
        _log_err(myLog, "add storage server failed! srv[%s] err[%s]",
                 NetHelper::addr2String(srv_id).c_str(), s.ToString().c_str());
        return false;
    }

    return true;
}

bool SysMgr::rmvStorageServer(uint64_t srv_id)
{
    auto node_list = sysData.mutable_nodelist();
    auto iter = (*node_list).find(srv_id);
    if ( iter != (*node_list).end() ) {
        (*node_list).erase(iter);
    } else {
        return true;
    }

    std::string data;
    if (!sysData.SerializeToString(&data)) {
        _log_err(myLog, "add storage server failed! srv[%s]", NetHelper::addr2String(srv_id).c_str());
        return false;
    }

    rocksdb::WriteBatch batch;
    batch.Put(sNamespaceKey, data);
    rocksdb::Status s = sysDb->Write(rocksdb::WriteOptions(), &batch);
    if (!s.ok()) {
        _log_err(myLog, "add storage server failed! srv[%s] err[%s]",
                 NetHelper::addr2String(srv_id).c_str(), s.ToString().c_str());
        return false;
    }

    return true;
}

std::vector<ServerInfo> SysMgr::getStorageServer(uint64_t srv_id)
{
    std::vector<ServerInfo> servers;

    auto& nmap = sysData.nodelist();
    if (srv_id != 0) {
        auto iter = nmap.find(srv_id);
        if (iter != nmap.end()) {
            servers.push_back(iter->second);
        }

        return servers;
    }

    for (auto& ele : nmap) {
        servers.push_back(ele.second);
    }

    return std::move(servers);
}

bool SysMgr::loadConfig()
{
    bucket_count = sDefaultConfig.getInt(sMetaServer, sGroupDataBucketNumber, sDefaultBucketNumber);
    copy_count = sDefaultConfig.getInt(sMetaServer, sStrCopyCount, sDefaultCopyCountNumber);

    for (int32_t i = 0; i < copy_count; i++) {
        std::vector< std::pair<uint64_t, uint32_t> > vec(bucket_count, {0, 0});
        curSysTable[i] = vec;
    }

    return true;
}

void SysMgr::getUpNode(std::vector<ServerInfo>& upnode_list)
{
    availableServer.clear();

    auto& nmap = sysData.nodelist();
    for(auto& ele : nmap) {
        // if(ele.second.status() != ServerInfo::Alive) {
        //     _log_info(myLog, "skip inactive node: <%s>", NetHelper::addr2String(ele.second.serverid()).c_str());
        //     continue;
        // }

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

    TableBuilder p_table_builder(bucket_count, copy_count);

    p_table_builder.setLogger(myLog);
    p_table_builder.setAvailableServer(upnode_list);
	p_table_builder.printAvailableServer();

    SysTableType quick_table_tmp = curSysTable;
    SysTableType dest_hash_table_for_builder_tmp = quick_table_tmp;
    int32_t ret = p_table_builder.rebuildTableNew(quick_table_tmp, dest_hash_table_for_builder_tmp);

    if( BUILD_NO_CHANGE == ret ) {
        return;
    } else if(ret == 0) {
        _log_err(myLog, "build table fail. fatal error.");

        return;
    }

    /*for (int32_t i = 0; i < bucket_count; i++) {

        std::string str;
        for (int32_t j = 0; j < copy_count; j++) {
            auto vec = dest_hash_table_for_builder_tmp[j];

            str += NetHelper::addr2String( vec[i].first ) + " " + std::to_string( vec[i].second ) + ", ";
        }

        _log_info(myLog, "partition %u -> %s", i, str.substr(0, str.size() - 1).c_str());
    }*/

    for (int32_t i = 0; i < bucket_count; i++) {

        /*auto leader_vec = dest_hash_table_for_builder_tmp[0];
        uint64_t leader = leader_vec[i].first;

        for (int32_t j = 1; j < copy_count; j++) {
            auto follower_vec = dest_hash_table_for_builder_tmp[j];
            uint64_t follower = follower_vec[i];


        }*/

        for (int32_t j = 0; j < copy_count; j++) {
            auto& vec = dest_hash_table_for_builder_tmp[j];
            uint64_t node = vec[i].first;

            
        }
    }

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
    auto nmap = sysData.mutable_namespaces();
    (*nmap)[ns] = capacity;

    std::string data;
    if (!sysData.SerializeToString(&data)) {
        _log_err(myLog, "createNameSpace failed! ns[%u] capacity[%lu]", ns, capacity);
        return false;
    }

    rocksdb::WriteBatch batch;
    batch.Put(sNamespaceKey, data);
    rocksdb::Status s = sysDb->Write(rocksdb::WriteOptions(), &batch);
    if (!s.ok()) {
        _log_err(myLog, "write namespace failed! ns[%u] capacity[%lu] err[%s]",
                 ns, capacity, s.ToString().c_str());
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
    if(metaVersion > (*value)) {
        (*value) = metaVersion;
    }
}

void SysMgr::incrVersion(const uint32_t inc_step)
{
    incrVersion(tableMgr.clientVersion, inc_step);
    incrVersion(tableMgr.serverVersion, inc_step);

    tableMgr.produceMd5();
}

}
