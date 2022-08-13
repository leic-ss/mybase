#include "common/rpc_mgr.h"

namespace mybase
{

class Peer
{
public:
	using ptr = std::shared_ptr<Peer>;

public:
	Peer(rpc::Client::ptr rpc_, uint64_t peer_id) : clirpc(rpc_), peerId(peer_id) { }

	void setMatchedIdx(uint64_t idx) { matchedIdx = idx; }
	uint64_t getMatchedIdx() { return matchedIdx; }

	void setNextLogIdx(uint64_t idx) { nextSendIdx = idx; }
	uint64_t getNextLogIdx() { return nextSendIdx; }

	void setLastSentIdx(uint64_t to)    { lastSentIdx = to; }
    uint64_t getLastSentIdx() const     { return lastSentIdx; }

	void send(rpc::ContextX::ptr ctx, rpc::ContextX::cb callback, uint32_t timeout_ms);

	bool makeBusy() {
        bool f = false;
        return busyFlag.compare_exchange_strong(f, true);
    }

    bool isBusy() { return busyFlag; }
    void setFree() { busyFlag.store(false); }

    uint64_t peerid() { return peerId; }

protected:
	uint64_t matchedIdx{0};
	uint64_t lastSendIdx{0};
	uint64_t nextSendIdx{0};

	uint64_t peerId{0};

	std::atomic<bool> busyFlag{false};

	uint64_t lastSentIdx{0};
	rpc::Client::ptr clirpc;
};

}