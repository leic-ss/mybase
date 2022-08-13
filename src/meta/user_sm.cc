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

#include "kv_state_machine.h"
#include "user_sm.h"
#include "kv_packet_conf_heartbeat.h"
#include "server_conf_thread.h"

namespace mybase
{

UserSM::UserSM(ServerConfThread* srv_conf_thd, mybase::BaseLogger* logger)
{
	srvConfThd = srv_conf_thd;
	myLog = nullptr;
}

UserSM::~UserSM()
{

}

void UserSM::doPreCommit(const uint64_t log_idx)
{

}

void UserSM::doCommit(const uint64_t log_idx, nuraft::buffer& data)
{
	data.pos(0);
	uint32_t type = data.get_int();
	uint32_t op = type & 0xFF;

	if ( ((type >> 24) & 0xFF) != 0xFF ) {
		int32_t len = data.get_int();
		uint8_t* buf = data.get_raw(len);

		mybase::DataBuffer input(buf, len);
		input.pourData(len);

		KvResponseConfHeartbeart packet;
		packet.decode(&input, nullptr);
		if (srvConfThd) srvConfThd->dealUpdateConfAndTable(&packet);
	} else if (op == 1) {
		int32_t len = data.get_int();
		uint8_t* buf = data.get_raw(len);

		mybase::DataBuffer input(buf, len);
		input.pourData(len);

		KvResponseConfHeartbeart packet;
		packet.decode(&input, nullptr);
		if (srvConfThd) srvConfThd->dealUpdateConfAndTable(&packet);
	} else if (op == 2) {
		uint32_t ns = (uint32_t)data.get_int();
		uint64_t capacity = data.get_ulong();

		auto group_info = srvConfThd ? srvConfThd->getGroupInfo() : nullptr;
        if (!group_info) {
            return ;
        }

        group_info->createNameSpace(ns, capacity);
	}
	
}

void UserSM::doConfCommit(const uint64_t log_idx)
{

}

void UserSM::doRollback(const uint64_t log_idx)
{

}

}