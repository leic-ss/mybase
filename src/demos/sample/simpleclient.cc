#include "cclient.h"
#include "common.h"
#include "packet_streamer.h"
#include "packet_factory.h"
#include "dlog.h"
#include "simpletcp.h"

#include <vector>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static void cclient_demo()
{
    mybase::CSimpleTcp simple_tcp;
    simple_tcp.setLogger(sDefLogger);

    // svc::SvcRequestPutPacket put_packet;
    // put_packet.area = 1;
    // put_packet.expired = 100;
    // put_packet.key = "key test";
    // put_packet.data = "data value";

    // svc::SvcPacketStreamer streamer;
    // mybase::DataBuffer output;
    // if (!streamer.encode(&put_packet, &output)) {
    //     _log_err(mybase::CCLogger::instance(), "streamer encode failed!");
    // }

    // int32_t sock = simple_tcp.sendTo("127.0.0.1", 8090, output.getData(), output.getDataLen());
    // if (sock < 0) {
    //     return ;
    // }

    // int32_t headLen = 4*sizeof(int32_t);
    // mybase::NBuffer buffer;
    // buffer.expand(headLen);

    // int32_t len = simple_tcp.read(sock, buffer.getBuff(), headLen);
    // if (len < 0) {
    //     simple_tcp.errorfd(sock);
    //     return ;
    // }

    // log_info("len = %d", len);
    // mybase::PacketHeader packetHd;
    // packetHd._chid = ntohl(*(int*)(buffer.getBuff() + sizeof(int)) );
    // packetHd._pcode = ntohl(*(int*)(buffer.getBuff() + sizeof(int) + sizeof(int)));
    // packetHd._dataLen = ntohl(*(int*)(buffer.getBuff() + sizeof(int) + sizeof(int) + sizeof(int)));

    // buffer.expand(packetHd._dataLen);
    // if (packetHd._dataLen != simple_tcp.read(sock, buffer.getBuff(), packetHd._dataLen)) {
    //     simple_tcp.errorfd(sock);
    //     return ;
    // }

    // log_info("datalen = %d", packetHd._dataLen);
    return ;
}

int32_t main(int32_t argc, char* argv[])
{
    sDefLogger->setLogLevel("debg");

    cclient_demo();

    sleep(300);

    return 0;
}