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

#pragma once

#include "common/netserver.h"
#include "public/common.h"
#include "common/adminserver.h"
#include "public/nlohmann/json.hpp"
#include "public/dlog.h"
#include "public/config.h"
#include "common/defs.h"
#include "public/cast_helper.h"
#include "sys_mgr.h"

#include <memory>
#include <atomic>
#include <mutex>

#include <stdio.h>
#include <stdint.h>

namespace mybase
{

class MetaServer : public mybase::IwServer
{
public:
	MetaServer();
	~MetaServer();

public:
	void process(const CloudConnHead& rsp_head, char* buf, uint32_t len) override;
	void setLogger(mybase::BaseLogger* logger);

protected:
	bool initServer();
    bool startServer();
    bool stopServer();

protected:
	void registerHttpCallbacks();
	bool jsonParse(const std::string& json_str, nlohmann::json& json_obj);

protected:
	void httpError(struct evhttp_request *req, int32_t code, const std::string& msg);
	void httpOk(struct evhttp_request *req, const std::string& msg);
	void httpPlainOk(struct evhttp_request *req, const std::string& msg);

	void handleCstMonitor(struct evhttp_request* req);
	void handleCstMonitor2(struct evhttp_request* req);
	void handleStatInfo(struct evhttp_request* req);
	void handleGetLogLevel(struct evhttp_request* req);
	void handleSetLogLevel(struct evhttp_request* req);
	void handleAddMetaServer(struct evhttp_request* req);
	void handleRmvMetaServer(struct evhttp_request* req);
	void handleGetCluster(struct evhttp_request* req);
	void handleNameSpaceCreate(struct evhttp_request* req);
	void handleNameSpaceList(struct evhttp_request* req);
	void handleLocate(struct evhttp_request* req);
	void handleStat(struct evhttp_request* req);
	void handlePut(struct evhttp_request* req);
	void handleGet(struct evhttp_request* req);
	void handleDel(struct evhttp_request* req);

	void handleAddStorageServer(struct evhttp_request* req);
	void handleBuildTable(struct evhttp_request* req);

protected:
	void formatStatRun();
	void formatStat(uint32_t report_timestamp);
	std::string getFormatStat();

private:
    std::mutex mtx;
    std::string lastFormatStat;

    mybase::AdminServer adminServer;
    std::thread monitorThread;

    SysMgr* sysMgr{nullptr};
};

}
