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

#include "storage.h"
#include "storage.pb.h"

#include "kvstore/raftcc/kv_state_mgr.h"
#include "kvstore/raftcc/kv_state_machine.h"
#include "kvstore/rocksdb_manager.h"
#include "public/common.h"

namespace mybase
{

StorageServer::StorageServer()
{
}

StorageServer::~StorageServer()
{
}

void StorageServer::process(const CloudConnHead& rsp_head, char* buf, uint32_t len)
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
        case KV_REQ_MESSAGE_PUT:
        {
            rsp_type = KV_RES_MESSAGE_RETURN;
            std::shared_ptr<KvRequestPut> msg = std::make_shared<KvRequestPut>();
            if (!msg->ParseFromArray(tmpbuf.curData(), tmpbuf.curDataLen())) {
                return ;
            }
            // rsp_msg = std::make_shared<KvResponseReturn>();
            // KvResponseReturn* resp = _RC(KvResponseReturn*, rsp_msg.get());

            // process(msg.get(), resp);

            uint32_t bucket_no = getBucketNumber( msg->key() );

            DumpEntry entry;
            // auto header = entry.mutable_head();
            // header->set_socket( rsp_head.sock_index );
            // header->set_socketcreate( rsp_head.sock_create.tv_sec );
            // header->set_request( rsp_head.request.tv_sec );
            // header->set_srcip( rsp_head.src_ip );
            // header->set_srcport( rsp_head.src_port );

            entry.set_channelid(channelid);
            entry.set_mtype(mtype);
            entry.set_key( msg->key() );
            entry.set_ns( msg->ns() );
            entry.set_val( msg->val() );
            entry.set_expired( msg->expired() );
            entry.set_version( msg->version() );

            uint32_t entry_size = entry.ByteSizeLong();
            uint32_t total_size = 4 * sizeof(uint32_t) + sizeof(uint16_t) + entry.ByteSizeLong();
            Buffer::ptr log = Buffer::alloc(total_size);
            log->writeInt32( rsp_head.sock_index );
            log->writeInt32( rsp_head.sock_create.tv_sec );
            log->writeInt32( rsp_head.request.tv_sec );
            log->writeInt32( rsp_head.src_ip );
            log->writeInt16( rsp_head.src_port );

            // ByteSizeLong
            // SerializeToArray
            entry.SerializeToArray( log->freeData(), entry_size);
            log->pourData( entry_size );
            _log_warn(myLog, "KV_REQ_MESSAGE_PUT bucket: %u size: %d", bucket_no, log->dataLen());

            break;
        }
        case KV_REQ_MESSAGE_GET:
        {
            rsp_type = KV_RES_MESSAGE_GET;
            rsp_msg = std::make_shared<KvResponseGet>();
            KvResponseGet* resp = _RC(KvResponseGet*, rsp_msg.get());

            std::shared_ptr<KvRequestGet> msg = std::make_shared<KvRequestGet>();
            if (!msg->ParseFromArray(tmpbuf.curData(), tmpbuf.curDataLen())) {
                break ;
            }

            process(msg.get(), resp);
            break;
        }
        case KV_REQ_MESSAGE_DUMP:
        {
            
            break;
        }
        case KV_REQ_BUCKET_HEARTBEAT:
        {
            rsp_type = KV_RES_MESSAGE_RETURN;
            std::shared_ptr<BucketHeartbeat> msg = std::make_shared<BucketHeartbeat>();
            if (!msg->ParseFromArray(tmpbuf.curData(), tmpbuf.curDataLen())) {
                return ;
            }
            rsp_msg = std::make_shared<KvResponseReturn>();
            KvResponseReturn* resp = _RC(KvResponseReturn*, rsp_msg.get());

            _log_warn(myLog, "KV_REQ_BUCKET_HEARTBEAT bucketid: %u idx: %lu", msg->bucketid(), msg->commitidx());
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

bool StorageServer::initServer()
{
	const char *dev_name = sDefaultConfig.getString(sStorageSection, sDevName, "");
    uint32_t local_ip = NetHelper::getLocalAddr(dev_name);
    int32_t local_port = sDefaultConfig.getInt(sStorageSection, sServerPort, sDefaultServerPort);
    NetHelper::sLocalServerAddr = NetHelper::ipAndPort(local_ip, local_port);

    int32_t admin_port = sDefaultConfig.getInt(sStorageSection, sAdminPort, sDefaultAdminPort);
	if (!adminServer.initialize(admin_port)) {
        _log_err(myLog, "admin server initialize failed!");
		return false;
	}

    clirpc = std::make_shared<RpcMgr>();
    clirpc->setLogger(myLog);
    if (!clirpc->initialize("cli")) {
        _log_err(myLog, "rpc initialize failed!");
        return false;
    }

    commiter = new rpc::CThread("commiter", (rpc::CThread::callback)nullptr, myLog);

    kvengine.reset( new RocksdbManager() );
    kvengine->setLogger(myLog);

    if (!kvengine->initialize()) {
        return false;
    }


    int32_t raft_port = sDefaultConfig.getInt(sRaftSection, sRaftPort, 6198);
    raft_mgr = std::make_shared<raftcc::RaftMgr>(nullptr, raft_port);
    if ( !raft_mgr->initial() ) {
        _log_err(myLog, "raft mgr initialize failed!");
        return false;
    }

    _log_info(myLog, "raft mgr initialize success!");

	registerHttpCallbacks();

    return true;
}

uint32_t StorageServer::getBucketNumber(const std::string& key)
{
    if (bucketCount == 0) return 0;

    uint32_t hashcode = UtilHelper::murMurHash(key.data(), key.size());
    // _log_debug(myLog, "hashcode: %u, bucket count: %d", hashcode, table_mgr->getBucketCount());
    return hashcode % bucketCount;
}

void StorageServer::registerHttpCallbacks()
{
	adminServer.regHandler("/api/v1/serverinfo",
				std::bind(&StorageServer::handleServerInfo, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/openfalconReport",
                std::bind(&StorageServer::handleOpenfalconReport, this, std::placeholders::_1));

    // curl 127.0.0.1:7191/api/v1/dbstats?type=rocksdb.dbstats
    adminServer.regHandler("/api/v1/dbStats",
                std::bind(&StorageServer::handleDbStats, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/getLogLevel",
                std::bind(&StorageServer::handleGetLogLevel, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/setLogLevel",
                std::bind(&StorageServer::handleSetLogLevel, this, std::placeholders::_1));

    adminServer.regHandler("/api/v1/raft/init",
                std::bind(&StorageServer::handleInitRaft, this, std::placeholders::_1));
    adminServer.regHandler("/api/v1/raft/status",
                std::bind(&StorageServer::handleRaftStatus, this, std::placeholders::_1));
}

void StorageServer::handleOpenfalconReport(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");
    std::string rsp_body = getFormatStat();
    mybase::AdminServer::httpOk(req, 200, rsp_body);
}

void StorageServer::handleInitRaft(struct evhttp_request* req)
{
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

    if (json_obj["id"].is_null()) {
        httpError(req, 400, "missing id!");
        return ;
    }

    if (json_obj["partid"].is_null()) {
        httpError(req, 400, "missing partid!");
        return ;
    }

    uint64_t id = 0;
    uint32_t partid = 0;
    try {
        id = json_obj["id"].get<uint64_t>();
        partid = json_obj["partid"].get<uint64_t>();
    } catch (std::exception& e) {
        httpError(req, 400, "exception: " + std::string(e.what()));
        return ;
    }

    raftcc::raft_params         rparams;     // your Raft parameters
    rparams.election_timeout_lower_bound_ = 2000;
    rparams.election_timeout_upper_bound_ = 3000;
    rparams.snapshot_distance_ = 10000;
    rparams.client_req_timeout_ = 1000;
    rparams.use_bg_thread_for_urgent_commit_ = false;
    rparams.return_method_ = raftcc::raft_params::async_handler;

    const char* raft_data_dir = sDefaultConfig.getString(sRaftSection, sRaftDataDir, "raft_data");
    int32_t raft_port = sDefaultConfig.getInt(sRaftSection, sRaftPort, 6198);
    std::string raft_addr = "0.0.0.0:" + std::to_string(raft_port);

    auto my_state_manager = std::make_shared<raftcc::kv_state_mgr>(raft_data_dir, partid, id, raft_addr.c_str(), nullptr );
    auto my_state_machine = std::make_shared<raftcc::KvStateMachine>(raft_data_dir, partid, id, nullptr);

    auto server = raft_mgr->create(my_state_machine, my_state_manager, rparams);

    servers.emplace(partid, server);

    nlohmann::json obj;
    obj["desc"] = "success";

    httpOk(req, obj.dump());
}

void StorageServer::handleRaftStatus(struct evhttp_request* req)
{
    AdminServer::HttpMap params = AdminServer::parseParams(req);

    uint32_t partid = 0;
    if (params.find("partid") != params.end()) {
        partid = atoi(params["partid"].c_str());
    }

    if (partid == 0) {
        httpError(req, 400, "missing partid!");
        return ;
    }

    if ( !raft_mgr->get(partid) ) {
        httpError(req, 400, "partid not exist!");
        return ;
    }

    nlohmann::json obj;
    if (!raft_mgr->get(partid)->is_initialized()) {
        obj["status"] = "uninitialized";
    } else {
        obj["status"] = "initialized";
    }

    httpOk(req, obj.dump());
}

void StorageServer::handleDbStats(struct evhttp_request* req)
{

}

void StorageServer::handleGetLogLevel(struct evhttp_request* req)
{
    std::string mime_type("application/json; charset=utf-8");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "0");
    evhttp_add_header(evhttp_request_get_output_headers(req), mime_type.c_str(), "1");

    nlohmann::json obj;
    obj["level"] = "uninitialized!";
    if (myLog) obj["level"] = myLog->getLogLevelStr();
    mybase::AdminServer::httpOk(req, 200, obj.dump());
}

void StorageServer::handleSetLogLevel(struct evhttp_request* req)
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

void StorageServer::formatStat(uint32_t report_timestamp)
{
    const char* project = sDefaultConfig.getString(sMonitorStat, sStatProject, "");
    const char* city = sDefaultConfig.getString(sMonitorStat, sStatCity, "");
    const char* groupname = sDefaultConfig.getString(sMonitorStat, sStatGroupName, "");
    uint32_t m_total_memory = sDefaultConfig.getInt(sStorageSection, sSlabMemSize, 0);

    std::ostringstream common_tag_str;
    common_tag_str << "project=" << project << ",city=" << city << ",groupname=" << groupname
                   << ",ip_port=" << NetHelper::addr2String(NetHelper::sLocalServerAddr);

    nlohmann::json stat_obj = nlohmann::json::array();
    nlohmann::json metric_obj;
    metric_obj["endpoint"] = NetHelper::addr2IpStr( (NetHelper::sLocalServerAddr & 0xFFFFFFFF) );
    metric_obj["step"] = 60;
    metric_obj["counterType"] = "GAUGE";
    metric_obj["timestamp"] = report_timestamp;

    std::vector<int64_t> area_data_size(sMaxNamespaceCount, 0);
    std::vector<int64_t> area_use_size(sMaxNamespaceCount, 0);
    std::vector<int64_t> area_item_count(sMaxNamespaceCount, 0);

    // AreaStat* area_stat = StatManager::statInstance.getStats();
    // if (area_stat) {
    //     for (int32_t area_id = 0; area_id < (int32_t)sMaxNamespaceCount; area_id++) {
    //         area_data_size[area_id] += area_stat[area_id].data_size_value;
    //         area_use_size[area_id] += area_stat[area_id].use_size_value;
    //         area_item_count[area_id] += area_stat[area_id].item_count_value;
    //     }
    // }

    // for (uint32_t areaid = 0; areaid < sMaxNamespaceCount; areaid++) {
    //     if (area_data_size[areaid] == 0) continue;

    //     metric_obj["metric"] = "ds_data_size";
    //     metric_obj["value"] = area_data_size[areaid]/1024/1024;
    //     metric_obj["tags"] = common_tag_str.str() + ",module=ds_server,areaid=" + std::to_string(areaid);
    //     stat_obj.push_back(metric_obj);
    // }

    // for (uint32_t areaid = 0; areaid < sMaxNamespaceCount; areaid++) {
    //     if (area_use_size[areaid] == 0) continue;

    //     metric_obj["metric"] = "ds_use_size";
    //     metric_obj["value"] = area_use_size[areaid]/1024/1024;
    //     metric_obj["tags"] = common_tag_str.str() + ",module=ds_server,areaid=" + std::to_string(areaid);
    //     stat_obj.push_back(metric_obj);
    // }

    // for (uint32_t areaid = 0; areaid < sMaxNamespaceCount; areaid++) {
    //     if (area_item_count[areaid] == 0) continue;

    //     metric_obj["metric"] = "ds_item_count";
    //     metric_obj["value"] = area_item_count[areaid];
    //     metric_obj["tags"] = common_tag_str.str() + ",module=ds_server,areaid=" + std::to_string(areaid);
    //     stat_obj.push_back(metric_obj);
    // }

    // {
    //     metric_obj["metric"] = "ds_total_size";
    //     metric_obj["value"] = m_total_memory;
    //     metric_obj["tags"] = common_tag_str.str() + ",module=ds_server";
    //     stat_obj.push_back(metric_obj);
    // }

    // std::map<DsServerMetricTag, int64_t> serverCountStatistic;
    // std::map<DsServerMetricTag, int64_t> serverTimecostStatistic;
    // CDsSrvOpCountStat::instance().getAndResetOpStat(serverCountStatistic, serverTimecostStatistic);

    // for (const auto &metric_iter : serverCountStatistic) {
    //     metric_obj["metric"] = "pv";
    //     metric_obj["value"] = metric_iter.second;
    //     metric_obj["tags"] = common_tag_str.str() + ",module=ds_server," + metric_iter.first.ToString();
    //     stat_obj.push_back(metric_obj);
    // }

    // for (const auto &metric_iter : serverTimecostStatistic) {
    //     metric_obj["metric"] = "consume_time";
    //     metric_obj["value"] = metric_iter.second;
    //     metric_obj["tags"] = common_tag_str.str() + ",module=ds_server," + metric_iter.first.ToString();
    //     stat_obj.push_back(metric_obj);
    // }

    {
        std::lock_guard<std::mutex> lk(mtx);
        lastFormatStat = stat_obj.dump();
    }

    return ;
}

void StorageServer::formatStatRun()
{
    while(isRunning) {
        uint64_t nowtime = time(NULL);
        if (nowtime % 60 == 0) {
            uint64_t last_timestamp = nowtime - 60;
            formatStat(last_timestamp);
        }

        KVSLEEP(isRunning, 1);
    }
}

std::string StorageServer::getFormatStat()
{
    std::lock_guard<std::mutex> lk(mtx);
    return lastFormatStat;
}

void StorageServer::httpError(struct evhttp_request *req, int32_t code, const std::string& msg)
{
    std::string mime_type("application/json; charset=UTF-8");
    evhttp_add_header(evhttp_request_get_output_headers(req),  "Content-Type", mime_type.c_str());

    nlohmann::json resp;
    resp["code"] = code;
    resp["message"] = msg;
    resp["debugInfo"] = "";
    mybase::AdminServer::httpOk(req, 200, resp.dump());
}

void StorageServer::httpOk(struct evhttp_request *req, const std::string& msg)
{
    std::string mime_type("application/json; charset=UTF-8");
    evhttp_add_header(evhttp_request_get_output_headers(req),  "Content-Type", mime_type.c_str());

    mybase::AdminServer::httpOk(req, 200, msg);
}

void StorageServer::httpPlainOk(struct evhttp_request *req, const std::string& msg)
{
    std::string mime_type("application/json; charset=UTF-8");
    evhttp_add_header(evhttp_request_get_output_headers(req),  "Content-Type", mime_type.c_str());

    nlohmann::json resp;
    resp["code"] = 200;
    resp["message"] = msg;
    resp["debugInfo"] = "";
    mybase::AdminServer::httpOk(req, 200, resp.dump());
}

void StorageServer::handleServerInfo(struct evhttp_request *req)
{
	httpPlainOk(req, "server info");
	return ;
}

bool StorageServer::jsonParse(const std::string& json_str, nlohmann::json& json_obj)
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

bool StorageServer::startServer()
{
	if (!adminServer.start()) {
		return false;
	}

    int32_t admin_port = sDefaultConfig.getInt(sStorageSection, sAdminPort, sDefaultAdminPort);
    _log_warn(myLog, "admin server start success! admin_port[%d]", admin_port);

    // heartbeatMgr = std::thread(&StorageServer::heartbeatRun, this);
    monitorMgr = std::thread(&StorageServer::formatStatRun, this);

    return true;
}

bool StorageServer::stopServer()
{
	if (!adminServer.stop()) {
		return false;
	}
    adminServer.wait();
    
    if (heartbeatMgr.joinable()) heartbeatMgr.join();
    if (monitorMgr.joinable()) monitorMgr.join();

    // StatManager::statInstance.stop();
    return true;
}

}
