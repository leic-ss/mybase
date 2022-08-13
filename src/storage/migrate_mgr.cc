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

void StorageServer::migrateRun()
{
    while (isRunning) {
        migwaiter.wait();

        std::map<uint32_t, std::vector<uint64_t>> tmp_migrates;
        tmp_migrates.swap(migrates);

        if (tmp_migrates.empty()) continue;

        for (auto& ele : tmp_migrates) {
            uint32_t bucket_no = ele.first;
            std::vector<uint64_t>& dest_servers = ele.second;

            _log_warn(myLog, "begin migrate bucket: %d", bucket_no);

            do {
                if (dest_servers.empty()) {
                    break;
                }

                // migrate one bucket, finish/cancel


            } while (false);

            _log_warn(myLog, "finish migrate bucket: %d", bucket_no);

            // finish migrate

            auto client = clirpc->createClient(metaLeader);

            rpc::ClientCtx::ptr ctx = std::make_shared<rpc::ClientCtx>();
            {
                ctx->req = Buffer::alloc(1024);
                ctx->sequence = nextSeq++;

                ctx->req->writeInt32(0);
                ctx->req->writeInt32(ctx->sequence);
                ctx->req->writeInt32(KV_REQ_MESSAGE_FINISH_MIGRATE);

                KvRequestFinishMigration message;
                message.set_serverid(NetHelper::sLocalServerAddr);
                message.set_metaversion(0);
                message.set_bucketid(bucket_no);

                std::string value;
                message.SerializeToString(&value);
                ctx->req->writeBytes(value.data(), value.size());
                ctx->req->fillInt32((uint8_t*)ctx->req->data(), ctx->req->dataLen());
            }

            client->request(ctx, 1000);

            if (ctx->status != rpc::ContextX::Status::OK) {
                _log_err(myLog, "send finish migrate message failed! bucket[%d] status[%d]", bucket_no, ctx->status);
                continue ;
            }

            _log_warn(myLog, "send finish migrate message success! bucket[%d]", bucket_no);
        }
    }
}

}