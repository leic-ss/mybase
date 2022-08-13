#include "common/wal.h"
#include "peer.h"
#include "common/rpc_mgr.h"

#include <unordered_map>

namespace mybase
{

class BucketMgr
{
public:
	enum Role : int32_t {
		leader,
		follow
	};

public:
	BucketMgr() { }
	~BucketMgr() { }

	void setRpcMgr(RpcMgr::ptr rpc_mgr) { rpcMgr = rpc_mgr; }
	void setCommiter(rpc::CThread* thr) { commiter = thr; }
	void setLogger(BaseLogger* logger) { myLog = logger; }

	bool initial(uint32_t bucket_no);
	bool close() { return true; }

	bool addPeer(uint64_t srv_id);
	bool rmvPeer(uint64_t srv_id);

	bool appendEntris(Buffer::ptr log);

	void requestAppendEntries();
	void requestAppendEntries(Peer::ptr p);

	void commit(uint64_t idx);
	void registCommitCB(std::function<void(Buffer::ptr)> cb) { commitCB = cb; }

	void rpcHandler(rpc::ContextX::ptr ctx);
	void hbHandler(rpc::ContextX::ptr ctx);

	bool writeAt(uint64_t idx, Buffer::ptr data);

	uint64_t nextIndex() { return walog.nextIndex(); }

protected:
	std::vector<Buffer::ptr> createAppendEntriesReq(Peer::ptr p);

	uint32_t nextSequence() { return sequence++; }

protected:
	uint32_t bucketNo{0};
	wal::Wal walog;

	uint64_t smCommitIdx{0};
	uint64_t quickCommitIdx{0};

	RpcMgr::ptr rpcMgr;
	std::unordered_map<uint64_t, Peer::ptr> peers;

	rpc::CThread* commiter{nullptr};
	Role role{Role::leader};

	std::function<void(Buffer::ptr)> commitCB;
	mybase::BaseLogger* myLog{nullptr};

	std::atomic<uint32_t> sequence{0};
};

}