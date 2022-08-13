#include "netserver.h"
#include "cclient.h"
#include "packet_streamer.h"
#include "packet_factory.h"
#include "kv_packet_put.h"
#include "kv_packet_response_return.h"

#include "dlog.h"

#include <vector>

#include <stdio.h>
#include <stdint.h>

using namespace mybase;
class WorkerServerSample : public IwServer
{
public:
	WorkerServerSample() : streamer(&factory) {}
	~WorkerServerSample() {}

	void process(const CloudConnHead& rsp_head, char* buf, uint32_t len) override
	{
		// log_info("%d %u %d", rsp_head.sock_index, rsp_head.src_ip, rsp_head.src_port);

		uint64_t reqtime = TimeHelper::getUSec(&(rsp_head.request));

		if( len < 4*sizeof(int32_t) ) {
			log_error("unexpected message! len[%u]", len);
	        return ;
	    }

	    int32_t flag = ntohl(*(int32_t*)buf);
	    PacketHeader packetHd;
	    packetHd._chid = ntohl( *(int32_t*)(buf + sizeof(int32_t)) );
	    packetHd._pcode = ntohl( *(int32_t*)(buf + 2 * sizeof(int32_t)) );
	    packetHd._dataLen = ntohl( *(int32_t*)(buf + 3 * sizeof(int32_t) ) );

	    if (flag != streamer.getPacketFlag() || packetHd._dataLen != (uint32_t)( len - 4 * sizeof(int32_t)) ) {
	        log_error("get from ip:%s:%d,stream error: %x<>%x,chid:%u,code:%d or dataLen: %d != bufdatalen:%u",
	                  NetHelper::addr2IpStr(rsp_head.src_ip).c_str(), rsp_head.src_port, flag,
	                  streamer.getPacketFlag(), packetHd._chid, packetHd._pcode,
	                  packetHd._dataLen, (uint32_t)(len - 4 * sizeof(int32_t)));
	        return ;
	    } else {
	        log_debug("get from ip:%s:%d,stream  %x==%x,chid:%u,code:%d and dataLen: %d == bufdatalen:%u",
	                  NetHelper::addr2IpStr(rsp_head.src_ip).c_str(), rsp_head.src_port, flag,
	                  streamer.getPacketFlag(), packetHd._chid,packetHd._pcode,
	                  packetHd._dataLen, (uint32_t)(len - 4 * sizeof(int32_t)));
	    }

	    uint64_t timeout_sec = reqtime + 1000 * 1000;
	    uint64_t nowtimesec = TimeHelper::currentUs();
	    if ( nowtimesec >= timeout_sec) {
	        log_error("timeout! seq[%u] total[%u] queue[%u]", packetHd._chid, nowtimesec - reqtime, nowtimesec - reqtime);
	        return ;
	    }

	    DataBuffer input((unsigned char*)buf + 4*sizeof(int), len - 4*sizeof(int));
	    input.pourData(len - 4*sizeof(int));

	    std::shared_ptr<BasePacket> packet( streamer.decode(&input, &packetHd) );
	    if( !packet ) {
	        log_error("chid:%u,code:%d or dataLen: %d ,streamer.decode fail",
	                  packetHd._chid, packetHd._pcode, packetHd._dataLen);
	        return ;
	    } else {
	        packet->setPacketHeader(&packetHd);
	        // todo: handle
	    }

	    // DELETE(packet);

	    DataBuffer output;
	    kv::KvResponseReturn return_packet;
	    return_packet.setChannelId(packetHd._chid);
	    if( false == streamer.encode(&return_packet, &output) ) {
	        log_error("encode failed!");
	    }

	    sendResponse(rsp_head, (const char*)output.getData(), output.getDataLen());

	    uint64_t curtimeusec = TimeHelper::currentUs();
	    log_info("handler! seq[%u] total[%u] queue[%u] req[%lu] que[%lu] cur[%lu]",
	    		 packetHd._chid, curtimeusec - reqtime, nowtimesec - reqtime, reqtime, nowtimesec, curtimeusec);
	}

private:
	kv::KvPacketFactory factory;
	kv::KvPacketStreamer streamer;
};

void sign_handler(int sig)
{
    log_warn("recv sig: %d", sig);
}

int32_t main(int32_t argc, char* argv[])
{
	sDefLogger->setFileName("tmp.log");
	sDefLogger->setLogLevel("warn");

	for (uint32_t i=0; i<64; i++) {
        if ( (i == 9) || (i == SIGINT) || (i == SIGTERM) || (i == 40) ) {
        	continue;
        }
        signal(i, SIG_IGN);
    }
    // signal(SIGINT, sign_handler);
    signal(SIGTERM, sign_handler);
    signal(40, sign_handler);
    signal(41, sign_handler);
    signal(42, sign_handler);
    signal(43, sign_handler);
    signal(44, sign_handler);
    signal(45, sign_handler);
    signal(46, sign_handler);
    signal(47, sign_handler); 
    signal(48, sign_handler); 
    signal(50, sign_handler);
    signal(51, sign_handler);
    signal(52, sign_handler);
    signal(53, sign_handler);

	mybase::CCloudAddrArgu arg;
	arg.m_protocal = mybase::CModelProtocalTcp;
	snprintf(arg.m_bind, sizeof(arg.m_bind), "%s", "127.0.0.1");
	arg.m_port = atoi(argv[1]);

	WorkerServerSample worker_server;
	worker_server.setLogger(sDefLogger);
	worker_server.initialize(arg);
	worker_server.start();

	worker_server.wait();

	worker_server.stop();

	return 0;
}