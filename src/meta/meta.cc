/************************************************************************
Copyright 2020 ~ 2021
Author: zhanglei

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
 
    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**************************************************************************/

#include "meta.h"

#include "public/cast_helper.h"
#include "meta.pb.h"
#include "storage.pb.h"

namespace mybase
{

MetaServer::MetaServer()
{
}

MetaServer::~MetaServer()
{
}

void MetaServer::setLogger(mybase::BaseLogger* logger)
{
    myLog = logger;
}

void MetaServer::process(const CloudConnHead& rsp_head, char* buf, uint32_t len)
{
    Buffer tmpbuf((uint8_t*)buf, len);
    uint32_t real_len = tmpbuf.readInt32();
    assert(real_len == len);

    uint32_t channelid = tmpbuf.readInt32();
    uint32_t mtype = tmpbuf.readInt32();

    uint64_t reqtime = rsp_head.request.tv_sec * 1000 * 1000 + rsp_head.request.tv_usec;
    uint64_t timeout_sec = reqtime + 1000 * 1000;
    uint64_t nowtimesec = TimeHelper::currentUs();
    if ( nowtimesec >= timeout_sec) {
        _log_err(myLog, "droped timeout! seq[%u] mtype[%d]", channelid, mtype);
        return ;
    }

    std::shared_ptr<google::protobuf::Message> rsp_msg;
    uint32_t rsp_type = 0;
    switch (mtype) {
        case KV_REQ_MESSAGE_GET_META:
        {
            rsp_type = KV_RES_MESSAGE_GET_META;

            std::shared_ptr<KvRequestGetMeta> msg = std::make_shared<KvRequestGetMeta>();
            if (!msg->ParseFromArray(tmpbuf.curData(), tmpbuf.curDataLen())) {
                return ;
            }

            rsp_msg = std::make_shared<KvResponseGetMeta>();
            KvResponseGetMeta* resp = _RC(KvResponseGetMeta*, rsp_msg.get());
            const char* client_bytes = sysMgr->getHashTableDeflateData(0);
            int32_t client_bytes_size = sysMgr->getHashTableDeflateSize(0);
            resp->set_tablebytes(client_bytes, client_bytes_size);
            resp->set_metaversion(sysMgr->getClientVersion());
            resp->set_bucketcount(sysMgr->getBucketCount());
            resp->set_copycount(sysMgr->getCopyCount());

            break;
        }
        case KV_REQ_HEARTBEAT_MESSAGE:
        {
            rsp_type = KV_RES_HEARTBEAT_MESSAGE;

            std::shared_ptr<KvRequestHeartbeat> msg = std::make_shared<KvRequestHeartbeat>();
            if (!msg->ParseFromArray(tmpbuf.curData(), tmpbuf.curDataLen())) {
                return ;
            }

            bool rc = sysMgr->doHeartbeat(msg->serverid());

            rsp_msg = std::make_shared<KvResponseHeartbeat>();

            KvResponseHeartbeat* resp = _RC(KvResponseHeartbeat*, rsp_msg.get());
            resp->set_code(rc ? 0 : OP_RETURN_FAILED);
            resp->set_clientversion(sysMgr->getClientVersion());
            resp->set_serverversion(sysMgr->getServerVersion());
            resp->set_bucketcount(sysMgr->getBucketCount());
            resp->set_copycount(sysMgr->getCopyCount());
            resp->set_nscapacityversion(sysMgr->getNsCapacityVersion());

            auto namespaces = sysMgr->getNamespaceCapacityInfo();
            auto nmap = resp->mutable_nscapacity();
            for (auto& ele : namespaces) {
                (*nmap)[ele.first] = ele.second;
            }

            resp->set_serverid(NetHelper::sLocalServerAddr);
            resp->add_metaservers(NetHelper::sLocalServerAddr);

            const char* storage_bytes = sysMgr->getHashTableDeflateData(1);
            int32_t storage_bytes_size = sysMgr->getHashTableDeflateSize(1);
            resp->set_tablebytes(storage_bytes, storage_bytes_size);

            break;
        }
        case KV_REQ_MESSAGE_FINISH_MIGRATE:
        {
            rsp_type = KV_RES_MESSAGE_RETURN;

            std::shared_ptr<KvRequestFinishMigration> msg = std::make_shared<KvRequestFinishMigration>();
            if (!msg->ParseFromArray(tmpbuf.curData(), tmpbuf.curDataLen())) {
                return ;
            }

            _log_warn(myLog, "recv finish migrate packet! bucket[%d] server[%s]",
                      msg->bucketid(), NetHelper::addr2String(msg->serverid()).c_str());

            sysMgr->setMigratingHashtable(msg->bucketid(), msg->serverid());
            rsp_msg = std::make_shared<KvResponseReturn>();
            KvResponseReturn* resp = _RC(KvResponseReturn*, rsp_msg.get());
            resp->set_metaversion(sysMgr->getClientVersion());

            break;
        }
        default:
        {
            _log_warn(myLog, "unknown request mtype[%d]", mtype);
            break;
        }
    }

    do {
        if (!rsp_msg) {
            break;
        }

        std::string value;
        rsp_msg->SerializeToString(&value);

        auto buff = Buffer::alloc(1024);
        buff->writeInt32(0);
        buff->writeInt32(channelid);
        buff->writeInt32(rsp_type);
        buff->writeBytes(value.data(), value.size());
        buff->fillInt32((uint8_t*)buff->data(), buff->dataLen());

        sendResponse(rsp_head, buff->data(), buff->dataLen());
    } while(false);
}

bool MetaServer::initServer()
{
    const char *dev_name = sDefaultConfig.getString(sMetaSection, sDevName, "");
    uint32_t local_ip = NetHelper::getLocalAddr(dev_name);
    int32_t local_port = sDefaultConfig.getInt(sMetaSection, sServerPort, sDefaultServerPort);
    NetHelper::sLocalServerAddr = NetHelper::ipAndPort(local_ip, local_port);

    sysMgr = std::make_shared<SysMgr>(myLog);
    sysMgr->loadConfig();
    sysMgr->openSysDB();
    sysMgr->loadSysData();

    int32_t admin_port = sDefaultConfig.getInt(sMetaSection, sAdminPort, sDefaultAdminPort);
	if (!adminServer.initialize(admin_port)) {
		return false;
	}
	registerHttpCallbacks();

    return true;
}

void MetaServer::registerHttpCallbacks()
{
    adminServer.regHandler("/api/v1/cstMonitor",
                std::bind(&MetaServer::handleCstMonitor, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/cstMonitor2",
                std::bind(&MetaServer::handleCstMonitor2, this, std::placeholders::_1));

    // TODO: 
    adminServer.regHandler("/api/v1/statInfo",
                std::bind(&MetaServer::handleStatInfo, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/getLogLevel",
                std::bind(&MetaServer::handleGetLogLevel, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/setLogLevel",
                std::bind(&MetaServer::handleSetLogLevel, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/meta/addsrv",
                std::bind(&MetaServer::handleAddMetaServer, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/meta/rmvsrv",
                std::bind(&MetaServer::handleRmvMetaServer, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/meta/getcluster",
                std::bind(&MetaServer::handleGetCluster, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/namespace/create",
                std::bind(&MetaServer::handleNameSpaceCreate, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/namespace/list",
                std::bind(&MetaServer::handleListNameSpace, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/locate",
                std::bind(&MetaServer::handleLocate, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/stat",
                std::bind(&MetaServer::handleStat, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/put",
                std::bind(&MetaServer::handlePut, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/get",
                std::bind(&MetaServer::handleGet, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/del",
                std::bind(&MetaServer::handleDel, this, std::placeholders::_1));

    adminServer.regHandler("/api/v1/storage/add",
                std::bind(&MetaServer::handleAddStorageServer, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/storage/list",
                std::bind(&MetaServer::handleListStorageServer, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/storage/rmv",
                std::bind(&MetaServer::handleRmvStorageServer, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/storage/rebalance",
                std::bind(&MetaServer::handleRebalance, this, std::placeholders::_1));
}

void MetaServer::httpError(struct evhttp_request *req, int32_t code, const std::string& msg)
{
    std::string mime_type("application/json; charset=UTF-8");
    evhttp_add_header(evhttp_request_get_output_headers(req),  "Content-Type", mime_type.c_str());

    nlohmann::json resp;
    resp["code"] = code;
    resp["message"] = msg;
    resp["debugInfo"] = "";
    mybase::AdminServer::httpOk(req, 200, resp.dump());
}

void MetaServer::httpOk(struct evhttp_request *req, const std::string& msg)
{
    std::string mime_type("application/json; charset=UTF-8");
    evhttp_add_header(evhttp_request_get_output_headers(req),  "Content-Type", mime_type.c_str());

    mybase::AdminServer::httpOk(req, 200, msg);
}

void MetaServer::httpPlainOk(struct evhttp_request *req, const std::string& msg)
{
    std::string mime_type("application/json; charset=UTF-8");
    evhttp_add_header(evhttp_request_get_output_headers(req),  "Content-Type", mime_type.c_str());

    nlohmann::json resp;
    resp["code"] = 200;
    resp["message"] = msg;
    resp["debugInfo"] = "";
    mybase::AdminServer::httpOk(req, 200, resp.dump());
}

// void MetaServer::handleServerInfo(struct evhttp_request *req)
// {
// 	std::string group_name = srvConfThread.getGroupName();
//     server_info_map& servers = *(srvConfThread.getServerInfoMap());

//     nlohmann::json resp_obj = nlohmann::json::array();
//     for (auto& item : servers) {
//         nlohmann::json j;
//         j["addr"] = NetHelper::addr2String(item.second->serverId);
//         j["lastTime"] = TimeHelper::timeConvert(item.second->lastTime);
//         j["status"] = (item.second->status == ServerInfo::ALIVE) ? "alive" : "down";
//         resp_obj.push_back(j);
//     }

//     nlohmann::json resp;
//     resp["code"] = 200;
//     resp["message"] = "success!";
//     resp["debugInfo"] = "";
//     resp["data"]["list"] = resp_obj;
//     resp["data"]["total"] = resp_obj.size();

//     httpOk(req, resp.dump());
// 	return ;
// }

void MetaServer::handleCstMonitor(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    std::string cst_monitor_info;
    sysMgr->tableMgr.getTableInfo(cst_monitor_info);
    httpOk(req, cst_monitor_info);
}

void MetaServer::handleCstMonitor2(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    const char* buf = sysMgr->getHashTableDeflateData();
    std::string cst_monitor_info = sysMgr->tableMgr.getHashTableStr((const uint64_t*)buf);

    httpOk(req, cst_monitor_info);
}

void MetaServer::handleStatInfo(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");
    std::string rsp_body = getFormatStat();
    mybase::AdminServer::httpOk(req, 200, rsp_body);
}

void MetaServer::handleGetLogLevel(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    nlohmann::json obj;
    obj["level"] = "uninitialized!";
    if (myLog) obj["level"] = myLog->getLogLevelStr();
    mybase::AdminServer::httpOk(req, 200, obj.dump());
}

void MetaServer::handleSetLogLevel(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    if (req->type != evhttp_cmd_type::EVHTTP_REQ_POST) {
        httpError(req, 400, "not a post request!");
        return ;
    }

    std::string content = mybase::AdminServer::readContent(req);
    if (content.empty()) {
        httpError(req, 400, "empty content in post request!");
        return ;
    }

    nlohmann::json json_obj;
    if (!jsonParse(content, json_obj)) {
        httpError(req, 400, "invalid json format!");
        return ;
    }

    if (json_obj["level"].is_null()) {
        httpError(req, 400, "missing log level!");
        return ;
    }

    nlohmann::json obj;
    try {
        std::string level = json_obj["level"].get<std::string>();
        if (myLog) {
            obj["desc"] = "success!";
            obj["old"] = myLog->getLogLevelStr();
            myLog->setLogLevel(level.c_str());
            obj["new"] = myLog->getLogLevelStr();
        } else {
            obj["desc"] = "not success!";
        }
    } catch (std::exception& e) {
        httpError(req, 400, "exception: " + std::string(e.what()));
        return ;
    }

    mybase::AdminServer::httpOk(req, 200, obj.dump());
}

void MetaServer::handleAddMetaServer(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    if (req->type != evhttp_cmd_type::EVHTTP_REQ_POST) {
        httpError(req, 400, "not a post request!");
        return ;
    }

    std::string content = mybase::AdminServer::readContent(req);
    if (content.empty()) {
        httpError(req, 400, "empty content in post request!");
        return ;
    }

    nlohmann::json json_obj;
    if (!jsonParse(content, json_obj)) {
        httpError(req, 400, "invalid json format!");
        return ;
    }

    int32_t srvid;
    std::string raft_addr;
    std::string srv_addr;
    std::string role;

    if (json_obj["srvid"].is_null()) {
        httpError(req, 400, "missing srvid!");
        return ;
    }
    if (json_obj["raft_addr"].is_null()) {
        httpError(req, 400, "missing raft_addr!");
        return ;
    }
    if (json_obj["srv_addr"].is_null()) {
        httpError(req, 400, "missing srv_addr!");
        return ;
    }

    try {
        srvid = json_obj["srvid"].get<int32_t>();
        raft_addr = json_obj["raft_addr"].get<std::string>();
        srv_addr = json_obj["srv_addr"].get<std::string>();
        role = json_obj.value("role", "learner");
    } catch (std::exception& e) {
        httpError(req, 400, "exception: " + std::string(e.what()));
    }

    // auto server = srvConfThread.getRaftServer();
    // if (!server) {
    //     httpError(req, 500, "add server failed!");
    //     return ;
    // }

    // std::string message;
    // if (role == "voter") {
    //     nuraft::srv_config config(srvid, 0, raft_addr, srv_addr, false);
    //     auto ret = server->add_srv(config);
    //     message = "add srv id: " + std::to_string(srvid) +" code: " + std::to_string(ret->get_result_code()) + " desc: " + ret->get_result_str();
    // } else if (role == "learner") {
    //     nuraft::srv_config config(srvid, 0, raft_addr, srv_addr, true);
    //     auto ret = server->add_srv(config);
    //     message = "add srv id: " + std::to_string(srvid) +" code: " + std::to_string(ret->get_result_code()) + " desc: " + ret->get_result_str();
    // } else {
    //     httpError(req, 400, "role: " + role + " not support");
    //     return ;
    // }

    // httpPlainOk(req, message);
}

void MetaServer::handleRmvMetaServer(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    if (req->type != evhttp_cmd_type::EVHTTP_REQ_POST) {
        httpError(req, 400, "not a post request!");
        return ;
    }

    std::string content = mybase::AdminServer::readContent(req);
    if (content.empty()) {
        httpError(req, 400, "empty content in post request!");
        return ;
    }

    nlohmann::json json_obj;
    if (!jsonParse(content, json_obj)) {
        httpError(req, 400, "invalid json format!");
        return ;
    }

    if (json_obj["srvid"].is_null()) {
        httpError(req, 400, "missing srvid!");
        return ;
    }

    int32_t srvid;
    try {
        srvid = json_obj["srvid"].get<int32_t>();
    } catch (std::exception& e) {
        httpError(req, 400, "exception: " + std::string(e.what()));
    }
}

void MetaServer::handleGetCluster(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    // auto server = srvConfThread.getRaftServer();
    // if (!server) {
    //     httpError(req, 500, "add server failed!");
    //     return ;
    // }

    // int32_t leader_id = server->get_leader();

    // ptr<nuraft::cluster_config> cluster_cfg = server->get_config();
    // std::list<ptr<nuraft::srv_config>> srvs = cluster_cfg->get_servers();

    // nlohmann::json resp_obj = nlohmann::json::array();
    // for (auto item : srvs) {
    //     nlohmann::json srv;
    //     srv["id"] = item->get_id();
    //     srv["raft_addr"] = item->get_endpoint();
    //     srv["srv_addr"] = item->get_aux();
    //     srv["role"] = (item->get_id() == leader_id) ? "leader" : ( item->is_learner() ? "leaner" : "follower");

    //     resp_obj.push_back(srv);
    // }
    // mybase::AdminServer::httpOk(req, 200, resp_obj.dump());
}

void MetaServer::handleNameSpaceCreate(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    if (req->type != evhttp_cmd_type::EVHTTP_REQ_POST) {
        httpError(req, 400, "not a post request!");
        return ;
    }

    std::string content = mybase::AdminServer::readContent(req);
    if (content.empty()) {
        httpError(req, 400, "empty content in post request!");
        return ;
    }

    nlohmann::json json_obj;
    if (!jsonParse(content, json_obj)) {
        httpError(req, 400, "invalid json format!");
        return ;
    }

    if (json_obj["ns"].is_null()) {
        httpError(req, 400, "missing ns!");
        return ;
    }
    if (json_obj["capacity"].is_null()) {
        httpError(req, 400, "missing capacity!");
        return ;
    }

    uint32_t ns = 0;
    uint64_t capacity = 0;
    try {
        ns = json_obj["ns"].get<int32_t>();
        capacity = json_obj["capacity"].get<int64_t>();
    } catch (std::exception& e) {
        httpError(req, 400, "exception: " + std::string(e.what()));
        return ;
    }

    bool ret = sysMgr->createNameSpace(ns, capacity);

    nlohmann::json resp_obj;
    resp_obj["code"] = ret ? 0 : OP_RETURN_FAILED;
    resp_obj["namespace"] = ns;
    std::string msg = "create namespace success!";
    if( !ret ) {
        msg = "create namespace failed!";
    }
    resp_obj["message"] = msg;
    mybase::AdminServer::httpOk(req, 200, resp_obj.dump());
}

void MetaServer::handleListNameSpace(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    nlohmann::json resp_obj;
    // nlohmann::json arr_obj = nlohmann::json::array();
    nlohmann::json arr_obj;
    const std::map<uint32_t, uint64_t>& nmap = sysMgr->getNamespaceCapacityInfo();
    for (auto& ele : nmap) {
        arr_obj.emplace(std::to_string(ele.first), ele.second);
    }

    resp_obj["code"] = 0;
    resp_obj["namespaces"] = arr_obj;
    mybase::AdminServer::httpOk(req, 200, resp_obj.dump());
}

void MetaServer::handleLocate(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    std::string key;
    mybase::AdminServer::HttpMap params = mybase::AdminServer::parseParams(req);
    if (params.find("key") != params.end()) {
        key = params["key"];
    }
    if (key.empty()) {
        httpError(req, 400, "missing key!");
        return ;
    }

    // KvRequestGetGroup request;
    // std::shared_ptr<KvResponseGetGroup> resp( new KvResponseGetGroup() );

    // memcpy(request.group_name, srvConfThread.getGroupName().data(), srvConfThread.getGroupName().size());
    // srvConfThread.findGroupHost(&request, resp.get());

    // uint32_t bucketCount = resp->bucket_count;
    // uint32_t copyCount = resp->copy_count;
    // uint64_t *server_list = resp->getServerList(bucketCount, copyCount);
    // // TODO: error

    // uint32_t hash = UtilHelper::murMurHash(key.data(), key.size());
    // hash %= bucketCount;

    // std::vector<uint64_t> servers;
    // for (uint32_t i = 0; i < copyCount; ++i) {
    //     uint64_t server_id = server_list[hash + i * bucketCount];
    //     if (server_id != 0) {
    //         servers.push_back(server_id);
    //     }
    // }

    // nlohmann::json resp_obj;
    // nlohmann::json arr_obj = nlohmann::json::array();
    // for (auto& srvid : servers) {
    //     arr_obj.push_back( NetHelper::addr2String(srvid) );
    // }

    // resp_obj["code"] = 0;
    // resp_obj["addrs"] = arr_obj;
    // mybase::AdminServer::httpOk(req, 200, resp_obj.dump());
}

void MetaServer::handleStat(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    // nlohmann::json resp_obj;
    // {
    //     KvPacketRequestQueryInfo request;
    //     std::shared_ptr<KvPacketResponseQueryInfo> resp( new KvPacketResponseQueryInfo() );
    //     request.groupName = srvConfThread.getGroupName();
    //     request.queryType = KvPacketRequestQueryInfo::Q_DATA_SEVER_INFO;
    //     request.serverId = 0;
    //     srvConfThread.doQueryInfoPacket(&request, resp.get());

    //     std::map<std::string, std::string> out_info = resp->map_k_v;
    //     nlohmann::json arr_obj;
    //     for (auto& ele : out_info) {
    //         arr_obj[ele.first] = ele.second;
    //     }

    //     resp_obj["status"] = arr_obj;
    // }

    // {
    //     auto group_info_found = srvConfThread.getGroupInfo();
    //     if (!group_info_found) {
    //         httpError(req, 400, "not ready!");
    //         return ;
    //     }
    //     NodeStatInfo node_info;
    //     group_info_found->getStatInfo(0, node_info);

    //     nlohmann::json arr_obj;
    //     do {
    //         auto data_holder = node_info.getStatData();
    //         auto it = data_holder.begin();

    //         for(; it != data_holder.end(); it++) {
    //             nlohmann::json ns_arr_obj;

    //             std::map<std::string, int64_t> m_k_v;
    //             it->second.format_detail(m_k_v);
    //             for (auto& ele : m_k_v) {
    //                 ns_arr_obj[ ele.first ] = ele.second;
    //             }

    //             arr_obj[ std::to_string(it->first) ] = ns_arr_obj;
    //         }

    //         if (!data_holder.empty()) break;

    //         auto& nmap = group_info_found->getNamespaceCapacityInfo();
    //         for (auto& ele : nmap) {
    //             arr_obj[ std::to_string(ele.first) ] = ele.second;
    //         }
    //     } while (false);

    //     resp_obj["namespace"] = arr_obj;
    // }

    // resp_obj["code"] = 0;
    // mybase::AdminServer::httpOk(req, 200, resp_obj.dump());
}

void MetaServer::handlePut(struct evhttp_request* req)
{
    // std::string mime_type("application/json; charset=utf-8");
    // evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    // evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    // if (req->type != evhttp_cmd_type::EVHTTP_REQ_POST) {
    //     httpError(req, 400, "not a post request!");
    //     return ;
    // }

    // std::string content = mybase::AdminServer::readContent(req);
    // if (content.empty()) {
    //     httpError(req, 400, "empty content in post request!");
    //     return ;
    // }

    // nlohmann::json json_obj;
    // if (!jsonParse(content, json_obj)) {
    //     httpError(req, 400, "invalid json format!");
    //     return ;
    // }

    // int32_t ns = 0;
    // std::string key;
    // std::string value;
    // uint32_t ttl = 0;

    // if (json_obj["ns"].is_null()) {
    //     httpError(req, 400, "missing ns!");
    //     return ;
    // }
    // if (json_obj["key"].is_null()) {
    //     httpError(req, 400, "missing key!");
    //     return ;
    // }
    // if (json_obj["value"].is_null()) {
    //     httpError(req, 400, "missing value!");
    //     return ;
    // }

    // try {
    //     ns = json_obj["ns"].get<int32_t>();
    //     key = json_obj["key"].get<std::string>();
    //     value = json_obj["value"].get<std::string>();
    //     ttl = json_obj.value("ttl", 0);
    // } catch (std::exception& e) {
    //     httpError(req, 400, "exception: " + std::string(e.what()));
    //     return ;
    // }

    // KvRequestGetGroup request;
    // std::shared_ptr<KvResponseGetGroup> resp( new KvResponseGetGroup() );

    // memcpy(request.group_name, srvConfThread.getGroupName().data(), srvConfThread.getGroupName().size());
    // srvConfThread.findGroupHost(&request, resp.get());

    // uint32_t bucketCount = resp->bucket_count;
    // uint32_t copyCount = resp->copy_count;
    // uint64_t *server_list = resp->getServerList(bucketCount, copyCount);
    // // TODO: error

    // uint32_t hash = UtilHelper::murMurHash(key.data(), key.size());
    // hash %= bucketCount;

    // uint64_t srv_id = server_list ? server_list[hash] : 0;

    // kv::KvRequestPut put_packet;
    // put_packet.area = ns;
    // put_packet.version = 0;
    // put_packet.expired = ttl;
    // put_packet.key.setData(key.data(), key.size());
    // put_packet.data.setData(value.data(), value.size());

    // auto tpacket = syncNetPacketMgn.sendAndRecv(srv_id, &put_packet);
    // if (!tpacket) {
    //     httpError(req, 400, "failed! " + NetHelper::addr2String(srv_id));
    //     return ;
    // }

    // KvResponseReturn* response_put = _SC(KvResponseReturn*, tpacket.get());
    // int32_t ret = response_put->getCode();

    // nlohmann::json resp_obj;
    // resp_obj["code"] = ret;
    // mybase::AdminServer::httpOk(req, 200, resp_obj.dump());
}

void MetaServer::handleGet(struct evhttp_request* req)
{
    // std::string mime_type("application/json; charset=utf-8");
    // evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    // evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    // std::string key;
    // int32_t ns = 0;
    // mybase::AdminServer::HttpMap params = mybase::AdminServer::parseParams(req);
    // if (params.find("key") != params.end()) {
    //     key = params["key"];
    // }
    // if (params.find("ns") != params.end()) {
    //     ns = atoi(params["ns"].c_str());
    // }
    // if (key.empty()) {
    //     httpError(req, 400, "missing key!");
    //     return ;
    // }

    // KvRequestGetGroup request;
    // std::shared_ptr<KvResponseGetGroup> resp( new KvResponseGetGroup() );

    // memcpy(request.group_name, srvConfThread.getGroupName().data(), srvConfThread.getGroupName().size());
    // srvConfThread.findGroupHost(&request, resp.get());

    // uint32_t bucketCount = resp->bucket_count;
    // uint32_t copyCount = resp->copy_count;
    // uint64_t *server_list = resp->getServerList(bucketCount, copyCount);
    // // TODO: error

    // uint32_t hash = UtilHelper::murMurHash(key.data(), key.size());
    // hash %= bucketCount;

    // uint64_t srv_id = server_list ? server_list[hash] : 0;

    // kv::KvRequestGet get_packet;
    // get_packet.area = ns;
    // get_packet.addKey(key.data(), key.size());

    // auto tpacket = syncNetPacketMgn.sendAndRecv(srv_id, &get_packet);
    // if (!tpacket) {
    //     httpError(req, 400, "failed! " + NetHelper::addr2String(srv_id));
    //     return ;
    // }

    // KvResponseGet* response_get = _SC(KvResponseGet*, tpacket.get());
    // int32_t ret = response_get->getCode();
    // nlohmann::json resp_obj;
    // resp_obj["code"] = ret;

    // for (auto& ele : response_get->keyValueMap) {
    //     DataEntry* key = ele.first;
    //     DataEntry* val = ele.second;

    //     std::string data;
    //     data.assign(val->getData(), val->getSize());

    //     resp_obj["value"] = data;
    //     resp_obj["mdate"] = key->data_meta.mdate;
    //     resp_obj["edate"] = key->data_meta.edate;
    //     break;
    // }

    // mybase::AdminServer::httpOk(req, 200, resp_obj.dump());
}

void MetaServer::handleDel(struct evhttp_request* req)
{
    // std::string mime_type("application/json; charset=utf-8");
    // evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    // evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    // if (req->type != evhttp_cmd_type::EVHTTP_REQ_POST) {
    //     httpError(req, 400, "not a post request!");
    //     return ;
    // }

    // std::string content = mybase::AdminServer::readContent(req);
    // if (content.empty()) {
    //     httpError(req, 400, "empty content in post request!");
    //     return ;
    // }

    // nlohmann::json json_obj;
    // if (!jsonParse(content, json_obj)) {
    //     httpError(req, 400, "invalid json format!");
    //     return ;
    // }

    // int32_t ns = 0;
    // std::string key;

    // if (json_obj["ns"].is_null()) {
    //     httpError(req, 400, "missing ns!");
    //     return ;
    // }
    // if (json_obj["key"].is_null()) {
    //     httpError(req, 400, "missing key!");
    //     return ;
    // }

    // try {
    //     ns = json_obj["ns"].get<int32_t>();
    //     key = json_obj["key"].get<std::string>();
    // } catch (std::exception& e) {
    //     httpError(req, 400, "exception: " + std::string(e.what()));
    //     return ;
    // }

    // KvRequestGetGroup request;
    // std::shared_ptr<KvResponseGetGroup> resp( new KvResponseGetGroup() );

    // memcpy(request.group_name, srvConfThread.getGroupName().data(), srvConfThread.getGroupName().size());
    // srvConfThread.findGroupHost(&request, resp.get());

    // uint32_t bucketCount = resp->bucket_count;
    // uint32_t copyCount = resp->copy_count;
    // uint64_t *server_list = resp->getServerList(bucketCount, copyCount);
    // // TODO: error

    // uint32_t hash = UtilHelper::murMurHash(key.data(), key.size());
    // hash %= bucketCount;

    // uint64_t srv_id = server_list ? server_list[hash] : 0;

    // kv::KvRequestRemove remove_packet;
    // remove_packet.area = ns;
    // remove_packet.addKey(key.data(), key.size());

    // auto tpacket = syncNetPacketMgn.sendAndRecv(srv_id, &remove_packet);
    // if (!tpacket) {
    //     httpError(req, 400, "failed! " + NetHelper::addr2String(srv_id));
    //     return ;
    // }

    // KvResponseReturn* response_remove = _SC(KvResponseReturn*, tpacket.get());
    // int32_t ret = response_remove->getCode();

    // nlohmann::json resp_obj;
    // resp_obj["code"] = ret;
    // mybase::AdminServer::httpOk(req, 200, resp_obj.dump());
}

void MetaServer::handleAddStorageServer(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    if (req->type != evhttp_cmd_type::EVHTTP_REQ_POST) {
        httpError(req, 400, "not a post request!");
        return ;
    }

    std::string content = mybase::AdminServer::readContent(req);
    if (content.empty()) {
        httpError(req, 400, "empty content in post request!");
        return ;
    }

    nlohmann::json json_obj;
    if (!jsonParse(content, json_obj)) {
        httpError(req, 400, "invalid json format!");
        return ;
    }

    uint64_t srvid;

    if (json_obj["addr"].is_null()) {
        httpError(req, 400, "missing addr!");
        return ;
    }

    try {
        srvid = NetHelper::str2Addr(json_obj["addr"].get<std::string>());
    } catch (std::exception& e) {
        httpError(req, 400, "exception: " + std::string(e.what()));
    }

    bool ret = sysMgr->addStorageServer(srvid);

    nlohmann::json resp_obj;
    resp_obj["code"] = 0;
    resp_obj["desc"] = ret ? "success" : "failed";
    httpOk(req, resp_obj.dump());
}

void MetaServer::handleListStorageServer(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    uint64_t srvid = 0;
    mybase::AdminServer::HttpMap params = mybase::AdminServer::parseParams(req);
    if (params.find("addr") != params.end()) {
        srvid = NetHelper::str2Addr( params["addr"] );
    }

    nlohmann::json resp_obj;
    nlohmann::json arr_obj = nlohmann::json::array();

    auto servers = sysMgr->getStorageServer(srvid);
    for (auto& ele : servers) {
        nlohmann::json obj;
        obj["addr"] = NetHelper::addr2String(ele.serverid());
        obj["lasttime"] = ele.lasttime();
        obj["status"] = ele.status();

        arr_obj.push_back( obj );
    }

    resp_obj["code"] = 0;
    resp_obj["servers"] = arr_obj;
    httpOk(req, resp_obj.dump());
}

void MetaServer::handleRmvStorageServer(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    if (req->type != evhttp_cmd_type::EVHTTP_REQ_POST) {
        httpError(req, 400, "not a post request!");
        return ;
    }

    std::string content = mybase::AdminServer::readContent(req);
    if (content.empty()) {
        httpError(req, 400, "empty content in post request!");
        return ;
    }

    nlohmann::json json_obj;
    if (!jsonParse(content, json_obj)) {
        httpError(req, 400, "invalid json format!");
        return ;
    }

    uint64_t srvid;

    if (json_obj["addr"].is_null()) {
        httpError(req, 400, "missing addr!");
        return ;
    }

    try {
        srvid = NetHelper::str2Addr(json_obj["addr"].get<std::string>());
    } catch (std::exception& e) {
        httpError(req, 400, "exception: " + std::string(e.what()));
    }

    bool ret = sysMgr->rmvStorageServer(srvid);

    nlohmann::json resp_obj;
    resp_obj["code"] = 0;
    resp_obj["desc"] = ret ? "success" : "failed";
    httpOk(req, resp_obj.dump());
}

void MetaServer::handleRebalance(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    if (req->type != evhttp_cmd_type::EVHTTP_REQ_POST) {
        httpError(req, 400, "not a post request!");
        return ;
    }

    sysMgr->rebuild();

    nlohmann::json resp_obj;
    resp_obj["code"] = 0;
    resp_obj["desc"] = "success";
    httpOk(req, resp_obj.dump());
}

bool MetaServer::jsonParse(const std::string& json_str, nlohmann::json& json_obj)
{
	bool success = true;
	try {
    	json_obj = nlohmann::json::parse(json_str);
    } catch (std::exception& e) {
    	log_warn("catch exception: %s", e.what());
    	success = false;
    }

    return success;
}

void MetaServer::formatStat(uint32_t report_timestamp)
{
    const char* project = sDefaultConfig.getString(sMonitorStat, sStatProject, "");
    const char* city = sDefaultConfig.getString(sMonitorStat, sStatCity, "");
    const char* groupname = sDefaultConfig.getString(sMonitorStat, sStatGroupName, "");

    std::ostringstream common_tag_str;
    common_tag_str << "code=0,project=" << project << ",city=" << city << ",groupname=" << groupname
                   << ",ip_port=" << NetHelper::addr2String(NetHelper::sLocalServerAddr);

    nlohmann::json stat_obj = nlohmann::json::array();
    nlohmann::json metric_obj;
    metric_obj["endpoint"] = NetHelper::addr2IpStr( (NetHelper::sLocalServerAddr & 0xFFFFFFFF) );
    metric_obj["step"] = 60;
    metric_obj["counterType"] = "GAUGE";
    metric_obj["timestamp"] = report_timestamp;

    // group_info_map* p_group_info_map_data = srvConfThread.getGroupInfoMap();
    // do {
    //     if (!p_group_info_map_data) break;

    //     for(auto it = p_group_info_map_data->begin(); it != p_group_info_map_data->end(); it++) {
    //         if(it->first.empty()) continue;
    //         if(it->second == nullptr) continue;

    //         SysMgr* group_info_found = it->second;
    //         if (group_info_found) {
    //             auto& capacity_info = group_info_found->getNamespaceCapacityInfo();
    //             metric_obj["metric"] = "pv";
    //             metric_obj["value"] = capacity_info.size();
    //             metric_obj["tags"] = common_tag_str.str() + ",module=cs_area_size";
    //             stat_obj.push_back(metric_obj);
    //         }

    //         std::vector<std::pair<uint64_t, uint32_t>> mig_machine_info;
    //         if(group_info_found && group_info_found->isMigrating()) {
    //             group_info_found->getMigratingMachines(mig_machine_info);

    //             metric_obj["metric"] = "pv";
    //             metric_obj["value"] = mig_machine_info.size();
    //             metric_obj["tags"] = common_tag_str.str() + ",module=cs_migrate_machine_num";
    //             stat_obj.push_back(metric_obj);
    //         }

    //         int32_t alive_ds_num = 0;
    //         int32_t dead_ds_num = 0;
    //         if (group_info_found) {
    //             srvConfThread.serverInfoRwLockerRdlock();
    //             NodeInfoSet nodeInfo = group_info_found->getNodeInfo();
    //             for(auto node_it = nodeInfo.begin(); node_it != nodeInfo.end(); ++node_it) {
    //                 if((*node_it)->server && (*node_it)->server->status == ServerInfo::ALIVE) {
    //                     alive_ds_num++;
    //                 } else {
    //                     dead_ds_num++;
    //                 }
    //             }
    //             srvConfThread.serverInfoRwLockerUnlock();

    //             {
    //                 metric_obj["metric"] = "pv";
    //                 metric_obj["value"] = alive_ds_num;
    //                 metric_obj["tags"] = common_tag_str.str() + ",module=cs_alive_ds_num";
    //                 stat_obj.push_back(metric_obj);
    //             }

    //             {
    //                 metric_obj["metric"] = "pv";
    //                 metric_obj["value"] = dead_ds_num;
    //                 metric_obj["tags"] = common_tag_str.str() + ",module=cs_dead_ds_num";
    //                 stat_obj.push_back(metric_obj);
    //             }

    //             {
    //                 metric_obj["metric"] = "pv";
    //                 metric_obj["value"] = alive_ds_num + dead_ds_num;
    //                 metric_obj["tags"] = common_tag_str.str() + ",module=cs_total_ds_num";
    //                 stat_obj.push_back(metric_obj);
    //             }
    //         }

    //         if (group_info_found) {
    //             int32_t migrate_bucket_num = group_info_found->getMigrateBucketNum();

    //             metric_obj["metric"] = "pv";
    //             metric_obj["value"] = migrate_bucket_num + 1;
    //             metric_obj["tags"] = common_tag_str.str() + ",module=cs_migrate_bucket_num";
    //             stat_obj.push_back(metric_obj);
    //         }
    //     }
    // } while(false);

    {
        std::lock_guard<std::mutex> lk(mtx);
        lastFormatStat = stat_obj.dump();
    }

    return ;
}

void MetaServer::formatStatRun()
{
    while(isRunning) {
        uint64_t nowtime = time(nullptr);
        if (nowtime % 60 == 0) {
            uint64_t last_timestamp = nowtime - 60;
            formatStat(last_timestamp);
        }

        KVSLEEP(isRunning, 1);
    }
}

std::string MetaServer::getFormatStat()
{
    std::lock_guard<std::mutex> lk(mtx);
    return lastFormatStat;
}

bool MetaServer::startServer()
{
	if (!adminServer.start()) {
		return false;
	}

    sysMgr->start();

    monitorThread = std::thread(&MetaServer::formatStatRun, this);

    return true;
}

bool MetaServer::stopServer()
{
    isRunning = false;

	if (!adminServer.stop()) return false;
    adminServer.wait();

    sysMgr->stop();

    if (monitorThread.joinable()) monitorThread.join();

    // srvConfThread.stop();
    // srvConfThread.wait();

    return true;
}

}
