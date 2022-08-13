#include "public/dlog.h"
#include "common/rpc/client.h"

#include <vector>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

int32_t main(int32_t argc, char* argv[])
{
    sDefLogger->setLogLevel("debg");

    auto iothr = new rpc::CThread("nio 1", (rpc::CThread::callback)nullptr, sDefLogger);
    auto wkthr = new rpc::CThread("wk 1", (rpc::CThread::callback)nullptr, sDefLogger);

    for (int32_t i = 0; i < 1000000; i++)
    {
        auto client = std::make_shared<rpc::Client>("192.168.56.103", 8000);
        client->setLogger(sDefLogger);
        client->attachNio(iothr);
        client->attachWk(wkthr);

        rpc::ClientCtx::ptr ctx = std::make_shared<rpc::ClientCtx>();
        ctx->req = mybase::Buffer::alloc(64);
        std::string str;
        str.assign(1024, 'c');

        ctx->sequence = i;

        ctx->req->writeInt32(4 + 4 + str.size());
        ctx->req->writeInt32(ctx->sequence);
        ctx->req->writeString(str);

        auto callback = [sDefLogger] (rpc::ContextX::ptr ctx_) {
            if (ctx_->status != rpc::ContextX::Status::OK) {
                _log_info(sDefLogger, "callback failed! seq[%u] status[%d]", ctx_->sequence, ctx_->status);
                return ;
            }

            rpc::ClientCtx::ptr cctx = ctx_->convert<rpc::ClientCtx>();
            std::string str;
            cctx->rsp->readString(str);

            _log_info(sDefLogger, "recv seq[%u] str[%.*s]", ctx_->sequence, str.size(), (char*)str.data());
        };

        client->request(ctx, callback, 100);
        usleep(500);
        client->close();
        client.reset();
    }

    sleep(300);

    return 0;
}