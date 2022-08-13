#include "peer.h"

namespace mybase
{

void Peer::send(rpc::ContextX::ptr ctx, rpc::ContextX::cb callback, uint32_t timeout_ms)
{
	clirpc->request(ctx, callback, timeout_ms);
}

}