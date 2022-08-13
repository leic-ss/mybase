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

#include "kv_log_store.h"

#include "libnuraft/nuraft.hxx"
#include "libnuraft/state_mgr.hxx"

#include <iostream>
#include <fstream>
#include <sstream>

using namespace nuraft;

namespace mybase {

class KvStateMgr: public state_mgr {
public:
    KvStateMgr(int32_t srv_id, const std::string& endpoint, const std::string& aux, const std::string& base_path, ptr<logger> logger_=nullptr);
    ~KvStateMgr() {}

    ptr<cluster_config> load_config() { return saved_config_; }

    bool exists(const std::string& file_path);

    void save_config(const cluster_config& config);

    void save_state(const srv_state& state);

    ptr<srv_state> read_state() { return saved_state_; }

    ptr<log_store> load_log_store() { return cur_log_store_; }

    int32_t server_id() { return my_id_; }

    void system_exit(const int32_t exit_code) { }

    ptr<srv_config> get_srv_config() const { return my_srv_config_; }

private:
    std::string config_file_path_;
    std::string state_file_path_;
    int32_t my_id_;
    std::string my_endpoint_;
    ptr<KvLogStore> cur_log_store_;
    ptr<srv_config> my_srv_config_;
    ptr<cluster_config> saved_config_;
    ptr<srv_state> saved_state_;
};

};
