/************************************************************************
Copyright 2020 ~ 2021
Author: zhanglei
Email: shanshenshi@126.com

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

#include "public/common.h"
#include "rpc/client.h"

namespace mybase
{

class RpcMgr
{
public:
	using ptr = std::shared_ptr<RpcMgr>;

public:
	RpcMgr(uint32_t thread_count=3);
	~RpcMgr();

public:
	void setLogger(BaseLogger* logger) { myLog = logger; }
	bool initialize(const std::string& prefix);

	rpc::Client::ptr createClient(uint64_t srv_id);

private:
	BaseLogger* myLog{nullptr};

    RWLock rwlock;
    std::unordered_map<uint64_t, std::shared_ptr<rpc::Client>> clients;

    uint32_t threadCount{3};
    std::vector<rpc::CThread*> thrs;
};

}
