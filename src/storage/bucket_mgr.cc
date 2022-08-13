#include "bucket_mgr.h"

#include "public/config.h"
#include "public/fileutils.h"
#include "common/defs.h"

#include "storage.pb.h"

namespace mybase
{

bool BucketMgr::initial(uint32_t bucket_no)
{
	const char* wal_log_dir = sDefaultConfig.getString(sStorageSection, sWalogPath, "data/wal");
	std::string wal_log_path = std::string(wal_log_dir) + "/" + std::to_string(bucket_no);

	// clear old

	if (!walog.initial(wal_log_path)) {
		return false;
	}

	bucketNo = bucket_no;
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
	walog.appendLog(log);

	requestAppendEntries();
	return true;
}

void BucketMgr::requestAppendEntries()
{
	for (auto& ele : peers) {
		requestAppendEntries(ele.second);
	}
}

void BucketMgr::requestAppendEntries(Peer::ptr p)
{
	uint64_t last_idx = walog.lastIndex();
	if (p->getNextLogIdx() == 0L) {
		p->setNextLogIdx( last_idx + 1 );
    }

    uint64_t last_log_idx = p->getNextLogIdx() - 1;
    uint64_t end_seq = last_idx;

   	uint64_t peer_last_sent_idx = p->getLastSentIdx();
   	auto vec = walog.logEntries(last_log_idx + 1, end_seq);

	KvRequestDump dump;
	dump.set_bucket(bucketNo);
	dump.set_peerid(p->peerid());
	dump.set_nextidx(last_log_idx + 1);
	dump.set_commitidx(quickCommitIdx);
	for (uint32_t i = 0; i < vec.size(); i++) {
		vec[i]->pos( sizeof(uint32_t) );
		dump.add_entries(vec[i]->curData(), vec[i]->curDataLen());
	}

	rpc::ClientCtx::ptr ctx = std::make_shared<rpc::ClientCtx>();
    ctx->req = mybase::Buffer::alloc(1024);
    ctx->sequence = nextSequence();

    ctx->req->writeInt32(0);
    ctx->req->writeInt32(ctx->sequence);
    ctx->req->writeInt32(KV_REQ_MESSAGE_DUMP);

    std::string value;
    dump.SerializeToString(&value);
    ctx->req->writeBytes(value.data(), value.size());
    ctx->req->fillInt32((uint8_t*)ctx->req->data(), ctx->req->dataLen());

    auto func = std::bind(&BucketMgr::rpcHandler, this, std::placeholders::_1);
    p->send(ctx, func, 100);
}

std::vector<std::shared_ptr<Buffer>> BucketMgr::createAppendEntriesReq(Peer::ptr p)
{
	std::vector<std::shared_ptr<Buffer>> vec;

	uint64_t last_idx = walog.lastIndex();
	if (p->getNextLogIdx() == 0L) {
		p->setNextLogIdx( last_idx + 1 );
    }

    uint64_t last_log_idx = p->getNextLogIdx() - 1;
    uint64_t end_seq = last_idx;

   	uint64_t peer_last_sent_idx = p->getLastSentIdx();
   	vec = walog.logEntries(last_log_idx + 1, end_seq);

	return vec;
}

void BucketMgr::commit(uint64_t idx)
{
	if (idx <= quickCommitIdx) {		
		return ;
	}

	auto func = [this, idx] (rpc::CThread* thr) {
		for (uint64_t i = quickCommitIdx + 1; i <= idx; i++) {
			_log_warn(myLog, "bucket: %lu commit idx: %lu %d", bucketNo, i, commitCB ? 1 : 0);
			Buffer::ptr data = walog.entryAt(i);
			if (commitCB) commitCB(data);
		}

		quickCommitIdx = idx;
	};

	commiter->dispatch(func);
}

void BucketMgr::rpcHandler(rpc::ContextX::ptr ctx_)
{
	if (ctx_->status != rpc::ContextX::Status::OK) {
        return ;
    }

    bool need_to_catchup = true;

    rpc::ClientCtx::ptr ctx = ctx_->convert<rpc::ClientCtx>();
    if (!ctx || !ctx->rsp) return ;

    Buffer buf((uint8_t*)ctx->rsp->data(), ctx->rsp->dataLen());
    uint32_t mtype = buf.readInt32();
    if (mtype != KV_RES_MESSAGE_DUMP_RETURN) {
        return ;
    }

    auto msg = std::make_shared<KvResponseDumpReturn>();
    if ( !msg->ParseFromArray(buf.curData(), buf.curDataLen()) ) {
        return ;
    }

    uint64_t peerid = msg->peerid();
    auto iter = peers.find(peerid);
    Peer::ptr p = iter->second;

    _log_warn(myLog, "bucket[%d] peerid[%lu] accept[%d] nextidx[%lu] lastidx[%lu]",
    		  bucketNo, peerid, msg->accept(), msg->nextidx(), walog.lastIndex());
    p->setNextLogIdx( msg->nextidx() );
    if ( !msg->accept() ) {
    	// requestAppendEntries(p);
    } else {
    	commit( msg->nextidx() - 1 );

    	need_to_catchup = (msg->nextidx() <= walog.lastIndex() ) || ( msg->commitidx() < (msg->nextidx() - 1));
    }

    if (role == Role::leader && need_to_catchup) {
        requestAppendEntries(p);
    }
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
	return walog.writeAt(idx, data);
}

}
