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

#include "kv_state_mgr.h"
#include <sys/stat.h>

namespace mybase
{

KvStateMgr::KvStateMgr(int srv_id, const std::string& endpoint, const std::string& aux, const std::string& base_path, ptr<logger> logger_)
        : my_id_(srv_id), my_endpoint_(endpoint), cur_log_store_( cs_new<KvLogStore>(base_path, logger_) )
{
    config_file_path_ = base_path + "/sys_configfile";
    state_file_path_ = base_path + "/sys_statefile";

    if (exists(config_file_path_)) {
        std::ifstream in(config_file_path_);
        std::ostringstream content;
        content << in.rdbuf();
        std::string str = content.str();

        ptr<buffer> buf = buffer::alloc(str.size());
        buf->put_raw((const uint8_t*)str.data(), str.size());

        buf->pos(0);
        saved_config_ = cluster_config::deserialize(*buf);
        my_srv_config_ = saved_config_->get_server(srv_id);
    } else {
        my_srv_config_ = cs_new<srv_config>( srv_id, 0, endpoint, aux, false);

        // Initial cluster config: contains only one server (myself).
        saved_config_ = cs_new<cluster_config>();
        saved_config_->get_servers().push_back(my_srv_config_);

        save_config(*saved_config_);
    }

    if (exists(state_file_path_)) {
        std::ifstream in(state_file_path_);
        std::ostringstream content;
        content << in.rdbuf();
        std::string str = content.str();

        ptr<buffer> buf = buffer::alloc(str.size());
        buf->put_raw((const uint8_t*)str.data(), str.size());

        buf->pos(0);
        saved_state_ = srv_state::deserialize(*buf);
    }
}

bool KvStateMgr::exists(const std::string& file_path)
{
    struct stat st;
    if (stat(file_path.c_str(), &st) != 0) return false;

    return true;
}

void KvStateMgr::save_config(const cluster_config& config)
{
    ptr<buffer> buf = config.serialize();

    std::string config_file_path_bak = config_file_path_ + ".bak";
    std::ofstream outFile(config_file_path_bak.c_str(), std::ios::out | std::ios::binary);

    buf->pos(0);
    outFile.write((const char*)buf->data(), buf->size());
    outFile.close();

    if (exists(config_file_path_)) {
        ::remove(config_file_path_.c_str());
    }
    ::rename(config_file_path_bak.c_str(), config_file_path_.c_str());

    saved_config_ = cluster_config::deserialize(*buf);
}

void KvStateMgr::save_state(const srv_state& state)
{
    ptr<buffer> buf = state.serialize();
    buf->pos(0);
    saved_state_ = srv_state::deserialize(*buf);

    std::string state_file_path_bak = state_file_path_ + ".bak";
    std::ofstream outFile(state_file_path_bak.c_str(), std::ios::out | std::ios::binary);

    buf->pos(0);
    outFile.write((const char*)buf->data(), buf->size());
    outFile.close();

    if (exists(state_file_path_)) {
        ::remove(state_file_path_.c_str());
    }
    ::rename(state_file_path_bak.c_str(), state_file_path_.c_str());
}

}

