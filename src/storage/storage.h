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

#include "common/netserver.h"
#include "public/common.h"
#include "common/adminserver.h"
#include "public/nlohmann/json.hpp"
#include "public/dlog.h"
#include "public/config.h"
#include "common/defs.h"
#include "public/cast_helper.h"
#include "common/rpc_mgr.h"
#include "kvstore/kv_engine.h"
#include "public/awaiter.h"

#include "raftcc/raftcc.hxx"
#include "raftcc/raft_mgr.h"

#include "storage.pb.h"

#include <memory>
#include <atomic>
#include <mutex>
#include <set>
#include <thread>

#include <stdio.h>
#include <stdint.h>

namespace mybase
{

class StorageServer : public mybase::IwServer
{
public:
	StorageServer();
	~StorageServer();

	void setConfigFile(const std::string& file) { configFile = file; }
	std::string getConfigFile() { return configFile; }

public:
	void process(const CloudConnHead& rsp_head, char* buf, uint32_t len) override;

protected:
	void updateServerTable(uint64_t *table, uint32_t table_size);
    void calculateMigrates(uint64_t *table, uint32_t index);
    uint32_t getBucketNumber(const std::string& key);

protected:
    void process(KvRequestPut* msg, KvResponseReturn* resp);
    void process(KvRequestGet* msg, KvResponseGet* resp);

protected:
    void commit(Buffer::ptr data);

protected:
	bool initServer();
    bool startServer();
    bool stopServer();

protected:
    void heartbeatRun();
    void migrateRun();
    void hbCallback(rpc::ContextX::ptr ctx);

protected:
	bool jsonParse(const std::string& json_str, nlohmann::json& json_obj);
	void httpError(struct evhttp_request *req, int32_t code, const std::string& msg);
	void httpOk(struct evhttp_request *req, const std::string& msg);
	void httpPlainOk(struct evhttp_request *req, const std::string& msg);

	void registerHttpCallbacks();
	void handleServerInfo(struct evhttp_request *req);

protected:
    void handleDbStats(struct evhttp_request* req);
    void handleGetLogLevel(struct evhttp_request* req);
    void handleSetLogLevel(struct evhttp_request* req);
    void handleOpenfalconReport(struct evhttp_request* req);

    void handleInitRaft(struct evhttp_request* req);
    void handleRaftStatus(struct evhttp_request* req);

protected:
	void formatStatRun();
	void formatStat(uint32_t report_timestamp);
	std::string getFormatStat();

private:
    AdminServer adminServer;
    std::set<uint64_t> metaServers;
    uint64_t metaLeader{0};

    std::mutex mtx;
    std::string lastFormatStat;

    RpcMgr::ptr clirpc{nullptr};

    std::string configFile;

    rpc::CThread* commiter{nullptr};
    std::thread monitorMgr;
    std::thread heartbeatMgr;
    std::thread migrateMgr;

    std::shared_ptr<raftcc::RaftMgr> raft_mgr{nullptr};

    std::unordered_map<uint32_t, std::shared_ptr<raftcc::raft_server>> servers;

    AWaiter migwaiter;

    std::atomic<uint32_t> nextSeq{1};
    int32_t m_validArea[sMaxNamespaceCount];

    uint32_t metaVersion{0};

    std::shared_ptr<kvstore::KvEngine> kvengine{nullptr};

    uint32_t bucketCount{0};
    uint32_t copyCount{0};

    // table builder
    RWSimpleLock locker;
    std::vector<int32_t> holding_buckets;
    std::vector<int32_t> padding_buckets;
    std::vector<int32_t> release_buckets;
    std::set<uint64_t> available_server_ids;

    std::set<uint64_t> slaveServerIds;
    std::set<uint64_t> proxyServerIds;

    std::map<uint32_t, std::vector<uint64_t>> migrates;

    std::mutex update_server_table_mtx;
};

}