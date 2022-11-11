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

#include "public/dlog.h"
#include "public/config.h"
#include "common/defs.h"
#include "public/common.h"
#include "table_mgr.h"
#include "stat_info.h"
#include "meta.pb.h"

#include "rocksdb/db.h"
#include "rocksdb/slice.h"

#include <string>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

namespace mybase
{

struct ServerInfoCmp {
    bool operator()(const ServerInfo& left, const ServerInfo& right) const { 
        return left.serverid() < right.serverid();
    }
};

class SysMgr
{
public:
    using SysTableType = std::map<int32_t, std::vector< std::pair<uint64_t, uint32_t> >>;

public:
    SysMgr(mybase::BaseLogger* logger=nullptr);
    ~SysMgr();

    bool openSysDB();
    bool loadSysData();

    bool start();
    bool stop();
    void serverCheck();

    void setLogger(mybase::BaseLogger* logger) { myLog = logger; }

    bool addStorageServer(uint64_t srv_id);
    bool rmvStorageServer(uint64_t srv_id);
    std::vector<ServerInfo> getStorageServer(uint64_t srv_id);

    uint32_t getNsCapacityVersion() const { return *tableMgr.namespaceCapacityVersion; }
    std::map<uint32_t, uint64_t> getNamespaceCapacityInfo();
    bool createNameSpace(uint32_t ns, uint64_t capacity);

    uint32_t getClientVersion() const { return *(tableMgr.clientVersion); }
    uint32_t getServerVersion() const { return *(tableMgr.serverVersion); }
    uint32_t getPluginsVersion() const { return *(tableMgr.pluginsVersion); }

    const uint64_t* getHashTable(int32_t mode = 0) const;
    const char *getHashTableDeflateData(int32_t mode = 0) const;
    int32_t getHashTableDeflateSize(int32_t mode = 0) const;

    const char *getServerTableData() const { return (const char *) tableMgr.mMapFile.mData(); }
    int32_t getServerTableSize() const { return tableMgr.mMapFile.getSize(); }
    bool isMigrating() const { return (*tableMgr.migrateBlockCount) != -1; }

    uint32_t getBucketCount() const { return tableMgr.getBucketCount(); }
    uint32_t getCopyCount() const { return tableMgr.getCopyCount(); }
    uint32_t getHashTableSize() const { return tableMgr.getHashTableSize(); }
    uint32_t getHashTableByteSize() const { return tableMgr.getHashTableByteSize(); }

    bool loadConfig();
    void rebuild();
    void rebuildQuickTable();
    bool doFinishMigrate(uint64_t server_id, uint32_t server_version, int32_t bucket_id);
    bool checkMigrateComplete();
    void setMigratingHashtable(uint32_t bucket_no, uint64_t server_id);
    void hardCheckMigrateComplete();

    void setStatInfo(uint64_t server_id, const NodeStatInfo&);
    void getStatInfo(uint64_t server_id, NodeStatInfo&);

    std::set<uint64_t> getAvailableServerId() const { return availableServer; }
    void getMigratingMachines(std::vector<std::pair<uint64_t, uint32_t>>& vec_server_id_count) const;

	int32_t getMigrateBucketNum() const { return *tableMgr.migrateBlockCount; }
    void incrVersion(const uint32_t inc_step = 1);

    bool doHeartbeat(uint64_t srv_id);

private:
    SysMgr(const SysMgr &);
    SysMgr & operator=(const SysMgr &);
    int32_t fillMigrateMachine();

    void deflateHashTable() { tableMgr.deflateHashTable(); }

    void getUpNode(std::vector<ServerInfo>& up_node_list);
    void printServerCount();
    void incrVersion(uint32_t* value, const uint32_t inc_step = 1);

private:
    mybase::BaseLogger* myLog{nullptr};

public:
    TableMgr tableMgr;

private:
    std::set<uint64_t> availableServer;

    uint32_t metaVersion{sMinMetaVersion};

    std::map<uint64_t, int32_t> migrateMachine;

    rocksdb::DB* sysDb{nullptr};
    std::vector<rocksdb::ColumnFamilyDescriptor> sysCfDescs;
    std::vector<rocksdb::ColumnFamilyHandle*> sysCfHandles;

    int32_t bucket_count;
    int32_t copy_count;

    SysData sysData;
    SysTableType curSysTable;
    SysTableType dstSysTable;

    std::map<uint64_t, NodeStatInfo> statInfo;
    RWSimpleLock statInfoRwLocker;

    std::thread chkThread;
    std::atomic<bool> running{false};

    std::mutex hashTableSetLock;
};

}
