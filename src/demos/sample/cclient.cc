#include "cclient.h"
#include "common.h"
#include "packet_streamer.h"
#include "packet_factory.h"
#include "dlog.h"
#include "cclient_wrapper.h"

#include <vector>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

class CRespCommHandle : public mybase::CCRspHandle
{
public:
    CRespCommHandle()
    {
    }

    virtual void handle(const mybase::CCRspHead* rspHead, char* buf, uint32_t len)
    {
        if( len < 4*sizeof(int) )
        {
            return ;
        }

        log_info("len = %u", len);
    }
};

static void cclient_demo()
{
    std::vector<mybase::CModelHeadConf*> confVec;

    mybase::CWeightModelConf* modelConf = new mybase::CWeightModelConf;

    strcpy(modelConf->m_modelStr, "weight");
    modelConf->m_modelID = 0;
    modelConf->m_protocal = mybase::CModelProtocalTcp;
    modelConf->m_connTimeout = 5;
    modelConf->m_longConn = true;

    uint64_t server_id = NetHelper::str2Addr("127.0.0.1", 8090);
    snprintf(modelConf->m_name, sizeof(modelConf->m_name), "%lu", server_id);
    snprintf(modelConf->m_groupname, sizeof(modelConf->m_groupname), "%s", "cclient");

    modelConf->m_threadNum = 3;
    modelConf->m_checkRate = 0.7;
    modelConf->m_lockSec = 500;

    modelConf->m_decodeModel.m_package_type = mybase::CC_PACK_NORMAL;
    modelConf->m_decodeModel.m_field_len_size = 4;
    modelConf->m_decodeModel.m_field_len_pos = 12;

    modelConf->m_chunk = false;

    mybase::CWeightMachineConf machine_conf;
    snprintf(machine_conf.m_ip, sizeof(machine_conf.m_ip), "%s", NetHelper::addr2IpStr(server_id & 0xFFFFFFFF).c_str());
    machine_conf.m_port = (server_id >> 32) & 0xFFFF;
    machine_conf.m_weight = 100;
    modelConf->m_machineVec.push_back(machine_conf);

    confVec.push_back(modelConf);

    // mybase::CCRspHandle* handle = new CRespCommHandle();

    // mybase::CClientIpr client;
    // client.setLogger(mybase::CCLogger::instance());

    // if (!client.init(confVec)) {
    //     log_error("cclient init failed!");
    // }

    // client.registerGroupHandle(modelConf->m_groupname, handle);

    // if (!client.start()) {
    //     log_error("cclient start failed!");
    // }

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

    // sleep(1);

    // mybase::CCliReqHead reqHead;
    // snprintf(reqHead.m_name, sizeof(reqHead.m_name), "%lu", server_id);
    // if( false == client.send(&reqHead, output.getData(), output.getDataLen()) ) {
    //     _log_err(mybase::CCLogger::instance(), "client send message failed!");
    // }

    sleep(100);

    // client.stop();

    // delete here
    for (auto ele : confVec) {
        DELETE(ele);
    }

    return ;
}

int32_t main(int32_t argc, char* argv[])
{
    sDefLogger->setLogLevel("debg");

    // svc::SvcClientApiImpl api;

    // uint64_t server_id = NetHelper::str2Addr("127.0.0.1", 8090);
    // api.init({server_id});
    // api.start();
    // svc::SvcClientApiImpl::AsyncHandlerImpl func = [] (uint32_t seq, int32_t result, const std::string& data) {
    //     log_info("seq[%u] result[%d]", seq, result);
    // };
    // api.setAsyncHandler(func);

    // sleep(1);

    // api.asyncPut(0, 0, 1000, 0, "key111", "value", 100, 10);

    cclient_demo();

    sleep(1);

    return 0;
}