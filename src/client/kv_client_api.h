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
#include "public/common.h"
#include "public/awaiter.h"
#include "public/cast_helper.h"

#include "common/defs.h"
#include "common/rpc_mgr.h"

#include <unordered_map>
#include <functional>
#include <set>

#include <stdint.h>
#include <stdio.h>
#include <stdint.h>

namespace mybase {

struct GetOption {
    uint32_t mdate{0};
    uint32_t edate{0};
};

class KvClientApi
{
public:
using AsyncHandler = std::function<void(uint32_t seq, int32_t pcode, int32_t result, void* data)>;

public:
    KvClientApi() { }
    ~KvClientApi();

    void setTimeout(uint32_t timeout_ms) { timeoutMs = timeout_ms; }
    void setLogger(mybase::BaseLogger* logger) { myLog = logger; }
    void setAsyncHandler(const AsyncHandler& handler) { asyncHandler = handler; }

    void setIoThreadNum(const uint32_t& thread_num) { ioThreadNum = thread_num; }
    void setAnsThreadNum(const uint32_t& thread_num) { ansThreadNum = thread_num; }

    bool startup(const std::vector<std::string>& meta_server_lis);

    int32_t asyncPut(uint32_t seq, int32_t ns, const std::string& key,
                     const std::string& data, uint32_t expire_sec, uint16_t version = 0);
    int32_t asyncGet(uint32_t seq, int32_t ns, const std::string& key);

    int32_t asyncRemove(uint32_t seq, int32_t ns, const std::string& key);

    int32_t asyncClearNamespace(uint32_t seq, int32_t ns, uint64_t server_id);

    int32_t asyncPing(uint32_t seq, uint64_t server_id);

    int32_t put(int32_t ns, const std::string& key, const std::string& data,
                uint32_t expire_sec, uint16_t version);
    int32_t get(int32_t ns, const std::string& key, std::string& data, GetOption* opt);
    int32_t remove(int32_t ns, const std::string& key);

    void clearNamespace(int32_t ns);
    int32_t clearNamespace(uint64_t srv_id, int32_t ns);

    void close();

    bool getServerId(const std::string& key, std::vector<uint64_t>& servers);

    void getServers(std::set<uint64_t>& servers);

protected:
    bool retrieveServerAddr(bool is_update=false);

protected:
    uint32_t nextSequence() { return nextSeq.fetch_add(1); }

private:
    void updateConfig();

private:
    bool checkKey(const std::string& key);
    bool checkData(const std::string& data);

private:
    AsyncHandler asyncHandler;
    bool initialized{false};
    std::atomic<bool> running{false};
    std::vector<uint64_t> metaServerList;

    std::thread updater;
    std::vector<uint64_t> myServerList;

    uint32_t ioThreadNum{1};
    uint32_t ansThreadNum{3};
    mybase::BaseLogger* myLog{nullptr};

    RpcMgr rpcli;

    uint32_t timeoutMs{0};
    uint32_t bucketCount{0};
    uint32_t copyCount{0};
    uint32_t metaVersion{0};
    uint32_t newMetaVersion{0};
    std::atomic<uint32_t> failCount{0};
    std::atomic<bool> direct{false};

    std::atomic<uint32_t> nextSeq{1};
};

}