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

#include "storage.h"

#include <inttypes.h>

namespace mybase
{

void StorageServer::heartbeatRun()
{
    std::vector<const char*> ip_list = sDefaultConfig.getStringList(sPublicSection, sMetaServer);
    if (ip_list.empty()) {
        _log_err(myLog, "miss config item %s:%s", sPublicSection, sMetaServer);
        return;
    }

    for (auto ip_str : ip_list) {
        uint64_t srv_id = NetHelper::str2Addr(ip_str);
        _log_warn(myLog, "found meta server %s", ip_str);

        if (srv_id == 0) continue;

        std::lock_guard<std::mutex> lk(mtx);
        metaServers.insert(srv_id);
        metaLeader = srv_id;
    }

    auto callback = std::bind(&StorageServer::hbCallback, this, std::placeholders::_1);

    while (isRunning) {

        auto client = clirpc->createClient(metaLeader);

        rpc::ClientCtx::ptr ctx = std::make_shared<rpc::ClientCtx>();
        {
            ctx->req = Buffer::alloc(1024);
            ctx->sequence = nextSeq++;

            ctx->req->writeInt32(0);
            ctx->req->writeInt32(ctx->sequence);
            ctx->req->writeInt32(KV_REQ_HEARTBEAT_MESSAGE);

            KvRequestHeartbeat message;
            message.set_serverid(NetHelper::sLocalServerAddr);
            message.set_metaversion(0);
            message.set_nscapacityversion(0);

            std::string value;
            message.SerializeToString(&value);
            ctx->req->writeBytes(value.data(), value.size());
            ctx->req->fillInt32((uint8_t*)ctx->req->data(), ctx->req->dataLen());
        }

        client->request(ctx, callback, 100);

        KVSLEEP(isRunning, 1);
    }
}

void StorageServer::hbCallback(rpc::ContextX::ptr ctx_)
{
    auto ctx = ctx_->convert<rpc::ClientCtx>();
    if (ctx->status != rpc::ContextX::Status::OK) {
        _log_err(myLog, "heartbeat failed! status[%d]", ctx->status);
        return ;
    }

    std::shared_ptr<mybase::Buffer> rsp = ctx->rsp;

    Buffer buf((uint8_t*)rsp->data(), rsp->dataLen());
    uint32_t mtype = buf.readInt32();
    if (mtype != KV_RES_HEARTBEAT_MESSAGE) {
        _log_err(myLog, "unexpected message type! mtype[%u]", mtype);
        return ;
    }

    auto msg = std::make_shared<KvResponseHeartbeat>();
    if (!msg->ParseFromArray(buf.curData(), buf.curDataLen())) {
        _log_err(myLog, "ParseFromArray failed! mtype[%u]", mtype);
        return ;
    }

    bucketCount = msg->bucketcount();
    copyCount = msg->copycount();

    do {
        if (bucketMgr) break;

        bucketMgr = new BucketMgr[bucketCount];

        for (uint32_t i = 0; i < bucketCount; i++) {
            bucketMgr[i].setRpcMgr(peerpc);
            bucketMgr[i].setCommiter(commiter);
            bucketMgr[i].setLogger(myLog);
        }
    } while (false);

    do {
        if (msg->serverversion() <= metaVersion) break;

        _log_warn(myLog, "meta version changed! %d -> %d", metaVersion, msg->serverversion());

        metaVersion = msg->serverversion();

        // update table
        uint64_t* table = (uint64_t*)(msg->tablebytes().data());

        std::unordered_set<uint32_t> holdMasterBuckets;
        std::unordered_set<uint32_t> holdAllBuckets;
        for (uint32_t i = 0; i < bucketCount; i++) {

            if (NetHelper::sLocalServerAddr == table[i]) {
                holdMasterBuckets.insert(i);
                bucketMgr[i].initial(i);
                bucketMgr[i].registCommitCB(std::bind(&StorageServer::commit, this, std::placeholders::_1));
            }

            for (uint32_t j = 1; j < copyCount; j++) {
                if (NetHelper::sLocalServerAddr == table[i]) {
                    uint64_t srv_id = table[i + j*bucketCount];
                    bucketMgr[i].addPeer(srv_id);
                }

                if (NetHelper::sLocalServerAddr != table[i + j*bucketCount]) continue;

                bucketMgr[i].initial(i);
                bucketMgr[i].registCommitCB(std::bind(&StorageServer::commit, this, std::placeholders::_1));
                holdAllBuckets.insert(i);
            }
        }

        _log_info(myLog, "hold master buckets count[%d] all buckets count[%d]",
                  holdMasterBuckets.size(), holdAllBuckets.size());

        updateServerTable(table, msg->tablebytes().size()/sizeof(uint64_t));
    } while (false);

    // _log_info(myLog, "heartbeat success! mtype[%u] bucketCount[%d] copyCount[%d]", mtype, bucketCount, copyCount);
}

void StorageServer::updateServerTable(uint64_t *table, uint32_t table_size)
{
    for (uint32_t i = 0; i < copyCount*bucketCount; i++) {
        if (NetHelper::sLocalServerAddr == table[i]) {

            if (i < bucketCount && table_size > copyCount*bucketCount) {
                calculateMigrates(table, i);
            }
        }
    }

    _log_warn(myLog, "add migration buckets! count: %d", migrates.size());
    for (auto& ele : migrates) {
        uint32_t index = ele.first;
        std::vector<uint64_t>& servers = ele.second;

        std::vector<std::string> srvs;
        for (auto& srvid : servers) {
            srvs.push_back( NetHelper::addr2String(srvid) );
        }

        _log_warn(myLog, "add migration! index: %d servers: %s", index, UtilHelper::vecot2String(srvs).c_str());
    }

    if (!migrates.empty()) {
        migwaiter.invoke();
    }
}

void StorageServer::calculateMigrates(uint64_t *table, uint32_t index)
{
    uint64_t* psource_table = table;
    uint64_t* pdest_table = table + bucketCount*copyCount;
    for (uint32_t i = 0; i < copyCount; ++i) {
        bool need_migrate = true;
        uint64_t dest_dataserver = pdest_table[index + bucketCount * i];

        for (uint32_t j = 0; j < copyCount; ++j) {
            if (dest_dataserver == psource_table[index + bucketCount * j]) {
                need_migrate = false;
                break;
            }
        }

        if (need_migrate) {
            migrates[index].push_back(dest_dataserver);
        }
    }
}

}

//////////////////
