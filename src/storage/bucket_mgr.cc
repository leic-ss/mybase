#include "bucket_mgr.h"

#include "public/config.h"
#include "public/fileutils.h"
#include "common/defs.h"

#include "storage.pb.h"

namespace mybase
{

bool BucketMgr::initial(uint32_t bucket_no)
{
	
	return true;
}

bool BucketMgr::addPeer(uint64_t srv_id)
{
	auto iter = peers.find(srv_id);
	if (iter != peers.end()) {
		return false;
	}

	auto client = rpcMgr->createClient(srv_id);
	auto hb_handler = [srv_id, this] () {
		// _log_info(myLog, "heartbeat %lu", bucketNo);

		auto cli = rpcMgr->createClient(srv_id);

		BucketHeartbeat	hb;
		hb.set_bucketid(bucketNo);
		hb.set_commitidx(quickCommitIdx);

		rpc::ClientCtx::ptr ctx = std::make_shared<rpc::ClientCtx>();
	 	ctx->req = mybase::Buffer::alloc(10240);
	 	ctx->sequence = nextSequence();

	 	ctx->req->writeInt32(0);
	 	ctx->req->writeInt32(ctx->sequence);
	 	ctx->req->writeInt32(KV_REQ_BUCKET_HEARTBEAT);

		std::string value;
		hb.SerializeToString(&value);
		ctx->req->writeBytes(value.data(), value.size());
		ctx->req->fillInt32((uint8_t*)ctx->req->data(), ctx->req->dataLen());

		auto func = std::bind(&BucketMgr::hbHandler, this, std::placeholders::_1);
		cli->request(ctx, func, 100);
	};
	// timer->registerTimer(bucketNo, hb_handler, 1000*1000, true);

	Peer::ptr p = std::make_shared<Peer>(client, srv_id);
	peers.emplace(srv_id, p);

	return true;
}

bool BucketMgr::rmvPeer(uint64_t srv_id)
{
	auto iter = peers.find(srv_id);
	if (iter == peers.end()) {
		return false;
	}

	peers.erase(iter);
	return true;
}

bool BucketMgr::appendEntris(std::shared_ptr<Buffer> log)
{
	requestAppendEntries();
	return true;
}

void BucketMgr::requestAppendEntries()
{
	
}

void BucketMgr::requestAppendEntries(Peer::ptr p)
{
	
}

std::vector<std::shared_ptr<Buffer>> BucketMgr::createAppendEntriesReq(Peer::ptr p)
{
	std::vector<std::shared_ptr<Buffer>> vec;

	return vec;
}

void BucketMgr::commit(uint64_t idx)
{
	
}

void BucketMgr::rpcHandler(rpc::ContextX::ptr ctx_)
{
	
}

void BucketMgr::hbHandler(rpc::ContextX::ptr ctx_)
{
	if (ctx_->status != rpc::ContextX::Status::OK) {
        return ;
    }

    rpc::ClientCtx::ptr ctx = ctx_->convert<rpc::ClientCtx>();
    if (!ctx || !ctx->rsp) return ;

    Buffer buf((uint8_t*)ctx->rsp->data(), ctx->rsp->dataLen());
    uint32_t mtype = buf.readInt32();
    if (mtype != KV_RES_MESSAGE_RETURN) {
        return ;
    }

    auto msg = std::make_shared<KvResponseReturn>();
    if ( !msg->ParseFromArray(buf.curData(), buf.curDataLen()) ) {
        return ;
    }
}

bool BucketMgr::writeAt(uint64_t idx, std::shared_ptr<Buffer> data)
{
	return true;
}

}
