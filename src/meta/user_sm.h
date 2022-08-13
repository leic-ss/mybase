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

#include "base_sm.h"
#include "dlog.h"

namespace mybase
{

class ServerConfThread;
class UserSM : public mybase::BaseSM
{
public:
	UserSM(ServerConfThread* srv_conf_thd=nullptr, mybase::BaseLogger* logger=nullptr);
	~UserSM();

public:
	void doPreCommit(const uint64_t log_idx);

    void doCommit(const uint64_t log_idx, nuraft::buffer& data);

    void doConfCommit(const uint64_t log_idx);

    void doRollback(const uint64_t log_idx);

private:
	ServerConfThread* srvConfThd{nullptr};
	mybase::BaseLogger* myLog{nullptr};
};

}
