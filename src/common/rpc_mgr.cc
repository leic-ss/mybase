#include "rpc_mgr.h"

namespace mybase
{

RpcMgr::RpcMgr(uint32_t thread_count) : threadCount(thread_count)
{
}

RpcMgr::~RpcMgr()
{
	for (auto ele : clients) {
		ele.second->close();
	}

    for (auto thr : thrs) {
        thr->stop();
    }
    for (auto thr : thrs) {
        delete thr;
    }

    thrs.clear();
    clients.clear();
}

bool RpcMgr::initialize(const std::string& prefix)
{
    for (uint32_t i = 0; i < threadCount; i++) {
        auto thr = new rpc::CThread(prefix + "_" + std::to_string(i), (rpc::CThread::callback)nullptr, myLog);

        thrs.push_back(thr);
    }

    return true;
}

rpc::Client::ptr RpcMgr::createClient(uint64_t srv_id)
{
    rpc::Client::ptr client;
    static uint32_t thread_idx = 0;
    uint32_t idx = thread_idx++ % threadCount;

    {
        rwlock.readLock();
        auto iter = clients.find(srv_id);
        if (iter != clients.end()) {
            client = iter->second;
        }
        rwlock.readUnlock();
    }

    if (client) return client;

    std::string host = NetHelper::addr2Ip(srv_id);
    uint16_t port = NetHelper::addr2Port(srv_id);

    client = std::make_shared<rpc::Client>(host, port);
    client->setLogger(myLog);
    client->attachNio(thrs[idx]);
    client->attachWk(thrs[idx]);

    {
        rwlock.writeLock();
        auto iter = clients.find(srv_id);
        if (iter != clients.end()) {
            client->close();
            client = iter->second;
        } else {
            clients.emplace(srv_id, client);
        }
        rwlock.writeUnlock();
    }

    return client;
}

}
