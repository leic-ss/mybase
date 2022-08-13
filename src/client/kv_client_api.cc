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

#include "kv_client_api.h"

#include "meta.pb.h"
#include "storage.pb.h"

#include "public/cast_helper.h"
#include "common/defs.h"
#include "public/murmurhash3.h"

namespace mybase {

bool KvClientApi::startup(const std::vector<std::string>& meta_server_list)
{
    if (meta_server_list.empty()) {
        _log_err(myLog, "startup failed as no metas! metas[%s]", UtilHelper::vecot2String(meta_server_list).c_str());
        return false;
    }

    if (initialized) {
        _log_err(myLog, "startup failed! metas[%s] initialized[%d]",
                 UtilHelper::vecot2String(meta_server_list).c_str(), initialized);
        return false;
    }

    running = true;

    rpcli.setLogger(myLog);

    if (!rpcli.initialize("rpc")) {
        _log_err(myLog, "startup failed! metas[%s] initialized[%d]",
                 UtilHelper::vecot2String(meta_server_list).c_str(), initialized);
        return false;
    }

    for (auto server : meta_server_list) {
        uint64_t srv_id = NetHelper::str2Addr(server.c_str());
        metaServerList.push_back(srv_id);
    }

    if ( !retrieveServerAddr() ) {
        _log_err(myLog, "retrieve_server_addr %s failed!", UtilHelper::vecot2String(meta_server_list).c_str());
        // close();
        return false;
    }

    if (metaVersion != newMetaVersion) {
        metaVersion = newMetaVersion;
    }

    updater = std::thread(&KvClientApi::updateConfig, this);

    initialized = true;

    _log_info(myLog, "startup success! servers[%s]", UtilHelper::vecot2String(meta_server_list).c_str());

    return true;
}

bool KvClientApi::retrieveServerAddr(bool is_update)
{
    if (metaServerList.size() == 0U) {
        _log_err(myLog, "retrieveServerAddr failed! meta server list is empty.");
        return false;
    }

    std::shared_ptr<Buffer> res;
    for (uint64_t srv_id : metaServerList) {
        auto client = rpcli.createClient(srv_id);

        rpc::ClientCtx::ptr ctx = std::make_shared<rpc::ClientCtx>();
        ctx->req = mybase::Buffer::alloc(1024);
        ctx->sequence = nextSequence();

        ctx->req->writeInt32(0);
        ctx->req->writeInt32(ctx->sequence);
        ctx->req->writeInt32(KV_REQ_MESSAGE_GET_META);

        KvRequestGetMeta req_msg;
        req_msg.set_metaversion(metaVersion);
        req_msg.set_type(KvRequestGetMeta::Client);

        std::string value;
        req_msg.SerializeToString(&value);
        ctx->req->writeBytes(value.data(), value.size());
        ctx->req->fillInt32((uint8_t*)ctx->req->data(), ctx->req->dataLen());

        client->request(ctx, 100);
        if (ctx->status == rpc::ContextX::Status::OK) {
            res = ctx->rsp;
            break;
        }
    }

    if (!res) return false;

    Buffer buf((uint8_t*)res->data(), res->dataLen());
    uint32_t mtype = buf.readInt32();
    if (mtype != KV_RES_MESSAGE_GET_META) {
        _log_err(myLog, "unexpected message type! mtype[%u]", mtype);
        return false;
    }

    auto msg = std::make_shared<KvResponseGetMeta>();
    if (!msg->ParseFromArray(buf.curData(), buf.curDataLen())) {
        return false;
    }

    if (msg->metaversion() <= 0) {
        return false;
    }

    bucketCount = msg->bucketcount();
    copyCount = msg->copycount();

    if (bucketCount <= 0 || copyCount <= 0) {
        _log_warn(myLog, "bucket or copy count doesn't correct, Be sure you are Authrized! "
                  "bucketCount[%d] copyCount[%d]", bucketCount, copyCount);
        return false;
    }

    uint32_t server_list_count = bucketCount * copyCount;
    _log_info(myLog, "server_list_count: %d, is_update[%d]", server_list_count, is_update);

    if (server_list_count != bucketCount * copyCount) {
        _log_err(myLog, "server table is wrong, server_list_count: %u, bucketCount: %u, copyCount: %u",
                 server_list_count, bucketCount, copyCount);
        return false;
    }

    uint64_t* tables = (uint64_t*)(msg->tablebytes().data());
    for (uint32_t i = 0; i < server_list_count; i++) {
        _log_debug(myLog, "server table: [%d] => [%s]", i, NetHelper::addr2String(tables[i]).c_str());
        if (!is_update) {
            myServerList.push_back(tables[i]);
        } else {
            myServerList[i] = tables[i];
        }
    }

    newMetaVersion = msg->metaversion();
    return true;
}

bool KvClientApi::checkKey(const std::string& key)
{
    if ( key.empty() ) {
        return false;
    }
    if ( (key.size() == 1) && (*(key.data()) == '\0') ) {
        return false;
    }
    if ( key.size() >= sMaxKeySize ) {
        return false;
    }
    return true;
}

bool KvClientApi::checkData(const std::string& data)
{
    if ( data.empty() ) {
        return false;
    }
    if ( data.size() >= sMaxDataSize) {
        return false;
    }
    return true;
}

bool KvClientApi::getServerId(const std::string& key, std::vector<uint64_t>& servers)
{
    if (this->direct) {
        // servers.push_back(this->data_server);
        return true;
    }

    uint32_t hash = UtilHelper::murMurHash(key.data(), key.size());
    servers.clear();

    if (myServerList.empty()) return false;

    hash %= bucketCount;
    for (uint32_t i = 0; i < copyCount && i < myServerList.size(); ++i) {
        uint64_t server_id = myServerList[hash + i * bucketCount];
        if (server_id != 0) {
            servers.push_back(server_id);
        }
    }

    return servers.empty() ? false : true;
}

void KvClientApi::getServers(std::set<uint64_t>& servers)
{
    std::set<uint64_t> tmp(myServerList.begin(), myServerList.end());
    servers.swap(tmp);
}

int32_t KvClientApi::asyncPut(uint32_t seq, int32_t area, const std::string& key,
                                  const std::string& data, uint32_t expire_sec, uint16_t version)
{
    if (!(checkKey(key)) || (!checkData(data))) {
        return KV_RETURN_ITEMSIZE_ERROR;
    }

    if ( (key.size() + data.size()) > (sMaxKeySize + sMaxDataSize)) {
        return KV_RETURN_ITEMSIZE_ERROR;
    }

    if (area < 0 || area >= (int32_t)sMaxNamespaceCount || expire_sec < 0) {
        return KV_RETURN_INVALID_ARGUMENT;
    }

    std::vector<uint64_t> server_list;
    if (!getServerId(key, server_list)) {
        _log_err(myLog, "can not find serverId, return false");
        return -1;
    }

    auto client = rpcli.createClient(server_list[0]);

    return OP_RETURN_SUCCESS;
}

int32_t KvClientApi::asyncGet(uint32_t seq, int32_t area, const std::string& key)
{
    if (!(checkKey(key))) {
        return KV_RETURN_ITEMSIZE_ERROR;
    }

    if (area < 0 || area >= (int32_t)sMaxNamespaceCount) {
        return KV_RETURN_INVALID_ARGUMENT;
    }

    std::vector<uint64_t> server_list;
    if (!getServerId(key, server_list)) {
        _log_err(myLog, "can not find serverId, return false");
        return OP_RETURN_FAILED;
    }

    return OP_RETURN_SUCCESS;
}

int32_t KvClientApi::asyncRemove(uint32_t seq, int32_t area, const std::string& key)
{
    if (!(checkKey(key))) {
        return KV_RETURN_ITEMSIZE_ERROR;
    }

    if (area < 0 || area >= (int32_t)sMaxNamespaceCount) {
        return KV_RETURN_INVALID_ARGUMENT;
    }

    std::vector<uint64_t> server_list;
    if (!getServerId(key, server_list)) {
        _log_err(myLog, "can not find serverId, return false");
        return OP_RETURN_FAILED;
    }

    return OP_RETURN_SUCCESS;
}

int32_t KvClientApi::asyncClearNamespace(uint32_t seq, int32_t area, uint64_t server_id)
{
    if (area < 0 || area >= (int32_t)sMaxNamespaceCount) {
        return KV_RETURN_INVALID_ARGUMENT;
    }

    return OP_RETURN_SUCCESS;
}

int32_t KvClientApi::asyncPing(uint32_t seq, uint64_t server_id)
{

    return OP_RETURN_SUCCESS;
}

int32_t KvClientApi::put(int32_t ns, const std::string& key, const std::string& data, uint32_t expire_sec, uint16_t version)
{
    if (!(checkKey(key)) || (!checkData(data))) {
        return KV_RETURN_ITEMSIZE_ERROR;
    }

    if ( (key.size() + data.size()) > (sMaxKeySize + sMaxDataSize)) {
        return KV_RETURN_ITEMSIZE_ERROR;
    }

    if (ns < 0 || ns >= (int32_t)sMaxNamespaceCount || expire_sec < 0) {
        return KV_RETURN_INVALID_ARGUMENT;
    }

    std::vector<uint64_t> server_list;
    if (!getServerId(key, server_list)) {
        _log_err(myLog, "can not find serverId, return false");
        return -1;
    }

    auto client = rpcli.createClient(server_list[0]);

    rpc::ClientCtx::ptr ctx = std::make_shared<rpc::ClientCtx>();
    ctx->req = mybase::Buffer::alloc(1024);
    ctx->sequence = nextSequence();

    ctx->req->writeInt32(0);
    ctx->req->writeInt32(ctx->sequence);
    ctx->req->writeInt32(KV_REQ_MESSAGE_PUT);

    KvRequestPut req_msg;
    req_msg.set_ns(ns);
    req_msg.set_key(key);
    req_msg.set_val(data);
    req_msg.set_expired(expire_sec);
    req_msg.set_version(0);

    std::string value;
    req_msg.SerializeToString(&value);
    ctx->req->writeBytes(value.data(), value.size());
    ctx->req->fillInt32((uint8_t*)ctx->req->data(), ctx->req->dataLen());

    client->request(ctx, 100);
    if (ctx->status != rpc::ContextX::Status::OK) {
        return (int32_t)ctx->status;
    }

    if (!ctx->rsp) return OP_RETURN_FAILED;

    Buffer buf((uint8_t*)ctx->rsp->data(), ctx->rsp->dataLen());
    uint32_t mtype = buf.readInt32();
    if (mtype != KV_RES_MESSAGE_RETURN) {
        _log_err(myLog, "unexpected message type! mtype[%u]", mtype);
        return OP_RETURN_FAILED;
    }

    auto msg = std::make_shared<KvResponseReturn>();
    if (!msg->ParseFromArray(buf.curData(), buf.curDataLen())) {
        return OP_RETURN_FAILED;
    }

    return msg->code();
}

int32_t KvClientApi::get(int32_t ns, const std::string& key, std::string& data, GetOption* opt)
{
    if (!(checkKey(key))) {
        return KV_RETURN_ITEMSIZE_ERROR;
    }

    if (ns < 0 || ns >= (int32_t)sMaxNamespaceCount) {
        return KV_RETURN_INVALID_ARGUMENT;
    }

    std::vector<uint64_t> server_list;
    if (!getServerId(key, server_list)) {
        _log_err(myLog, "can not find serverId, return false");
        return OP_RETURN_FAILED;
    }

    auto client = rpcli.createClient(server_list[0]);

    rpc::ClientCtx::ptr ctx = std::make_shared<rpc::ClientCtx>();
    ctx->req = mybase::Buffer::alloc(1024);
    ctx->sequence = nextSequence();

    ctx->req->writeInt32(0);
    ctx->req->writeInt32(ctx->sequence);
    ctx->req->writeInt32(KV_REQ_MESSAGE_GET);

    KvRequestGet req_msg;
    req_msg.set_ns(ns);
    req_msg.set_key(key);
    req_msg.set_clientversion(metaVersion);

    std::string value;
    req_msg.SerializeToString(&value);
    ctx->req->writeBytes(value.data(), value.size());
    ctx->req->fillInt32((uint8_t*)ctx->req->data(), ctx->req->dataLen());

    client->request(ctx, 100);
    if (ctx->status != rpc::ContextX::Status::OK) {
        return (int32_t)ctx->status;
    }

    if (!ctx->rsp) return OP_RETURN_FAILED;

    Buffer buf((uint8_t*)ctx->rsp->data(), ctx->rsp->dataLen());
    uint32_t mtype = buf.readInt32();
    if (mtype != KV_RES_MESSAGE_GET) {
        _log_err(myLog, "unexpected message type! mtype[%u]", mtype);
        return OP_RETURN_FAILED;
    }

    auto msg = std::make_shared<KvResponseGet>();
    if (!msg->ParseFromArray(buf.curData(), buf.curDataLen())) {
        return OP_RETURN_FAILED;
    }

    data = msg->kv().val();
    opt->mdate = msg->kv().mtime();
    opt->edate = msg->kv().etime();
    return msg->code();
}

int32_t KvClientApi::remove(int32_t area, const std::string& key)
{
    if (!(checkKey(key))) {
        return KV_RETURN_ITEMSIZE_ERROR;
    }

    if (area < 0 || area >= (int32_t)sMaxNamespaceCount) {
        return KV_RETURN_INVALID_ARGUMENT;
    }

    std::vector<uint64_t> server_list;
    if (!getServerId(key, server_list)) {
        _log_err(myLog, "can not find serverId, return false");
        return OP_RETURN_FAILED;
    }

    return OP_RETURN_SUCCESS;
}

void KvClientApi::clearNamespace(int32_t ns)
{
    std::set<uint64_t> srvids;
    for (auto srvid : myServerList) {
        srvids.insert(srvid);
    }

    return;
}

int32_t KvClientApi::clearNamespace(uint64_t srv_id, int32_t ns)
{

    return OP_RETURN_SUCCESS;
}

void KvClientApi::updateConfig()
{
    while ( running ) {
        
        uint32_t count = 100;
        while (running) {
            usleep(10*1000);

            if ((--count) == 0) break;
        }

        if (!running) break;

        // if (configVersion < sConfigMinVersion) {
        //     // need to update
        // } else if (configVersion == newConfigVersion && failCount < sUpdateServerTableInterval) {
        //     continue;
        // }

        // _log_info(myLog, "configVersion[%d] newConfigVersion[%d] failCount[%d]", configVersion, newConfigVersion, failCount.load());
        // if ( !retrieveServerAddr(true) ) {
        //     _log_err(myLog, "retrieve_server_addr failed!");
        //     // close();
        //     continue;
        // }

        // if (configVersion != newConfigVersion) {
        //     if (!reInitKvClient()) {
        //         _log_err(myLog, "reinit kv client failed!");
        //         continue;
        //     }

        //     _log_warn(myLog, "configVersion[%d] -> newConfigVersion[%d]", configVersion, newConfigVersion);
        //     configVersion = newConfigVersion;
        // } else {
        //     _log_warn(myLog, "skip updateConfig after retrieveServerAddr! configVersion[%d] newConfigVersion[%d]", configVersion, newConfigVersion);
        // }

        // failCount = 0;
    }
}

KvClientApi::~KvClientApi()
{
    if (running) close();
}

void KvClientApi::close()
{
    running = false;

    if (updater.joinable()) {
        updater.join();
    }

    initialized = false;
}

}