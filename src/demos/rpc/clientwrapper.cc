#include "rpc_mgr.h"
#include "dlog.h"
#include "packet_streamer.h"
#include "packet_factory.h"
#include "kv_packet_put.h"

#include <vector>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

int32_t main(int32_t argc, char* argv[])
{
    sDefLogger->setLogLevel("debg");

    std::string key;
    key.assign(10, 'k');
    std::string value;
    value.assign(1024, 'c');

    kv::KvRequestPut packet_put;
    packet_put.area = 1;
    packet_put.key.setData(key.data(), key.size());
    packet_put.data.setData(value.data(), value.size());
    packet_put.setChannelId(1);

    kv::KvPacketStreamer streamer;
    nds::DataBuffer output;
    if (!streamer.encode(&packet_put, &output)) {
        _log_err(nds::CCLogger::instance(), "streamer encode failed!");
    }

    nds::RpcClientWrapper client_wrapper;
	client_wrapper.setLogger(sDefLogger);
    client_wrapper.setAnsThreadNum(3);

    for (uint32_t i = 0; i < 100000000; i++) {
        std::set<uint64_t> srv_ids;
        uint64_t srv_id = 0;
        if (i % 2 == 0) {
            srv_id = NetHelper::str2Addr("192.168.56.101:8000");
            srv_ids.insert(srv_id);
        } else {
            srv_id = NetHelper::str2Addr("192.168.56.101:8001");
            srv_ids.insert(srv_id);
        }

        client_wrapper.init(srv_ids);
        client_wrapper.start();

        client_wrapper.send(srv_id, &packet_put);

        usleep(10);
        _log_info(sDefLogger, "i = %d", i);
    }

    sleep(300);

    return 0;
}
