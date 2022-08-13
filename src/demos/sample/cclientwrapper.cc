#include "cclient_wrapper.h"

#include "common.h"
#include "packet_streamer.h"
#include "packet_factory.h"
#include "dlog.h"
#include "asynclimit.h"
#include "kv_packet_put.h"

#include <vector>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

int32_t main(int32_t argc, char* argv[])
{
    sDefLogger->setLogLevel("debg");

    mybase::CAsyncLimit limit(5000);
    {
    	mybase::CClientWrapper client;
    	client.setLogger(sDefLogger);
    	std::set<uint64_t> ids;
    	ids.insert(NetHelper::str2Addr("127.0.0.1:8090"));
    	client.init( ids );
    	client.start();

    	std::set<uint64_t> ids2;
    	ids2.insert(NetHelper::str2Addr("127.0.0.1:8091"));
    	client.init( ids2 );
    	client.start();

        client.wait(100, false);

        std::string key;
        key.assign(10, 'k');
        std::string value;
        value.assign(10*1024, 'c');

        for (uint32_t i = 0; i < 100000000; i++) {
            limit.limitrate();

            kv::KvRequestPut packet_put;
            packet_put.area = 1;
            packet_put.key.setData(key.data(), key.size());
            packet_put.data.setData(value.data(), value.size());
            packet_put.setChannelId(i);

            kv::KvPacketStreamer streamer;
            mybase::DataBuffer output;
            if (!streamer.encode(&packet_put, &output)) {
                _log_err(mybase::CCLogger::instance(), "streamer encode failed!");
            }

            client.send(NetHelper::str2Addr("127.0.0.1:8091"), output.getData(), output.getDataLen());
        }

        sleep(100);
    }

    sleep(1);

    return 0;
}