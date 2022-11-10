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

// #include "kv_log_store.h"

#include "raftcc/raftcc.hxx"
#include "walstore/walstore.h"

#include <iostream>
#include <fstream>
#include <sstream>

namespace raftcc {

class KvStateMgr: public state_mgr {
public:
    KvStateMgr(const std::string& base_path, uint32_t part_id, uint64_t srv_id, const std::string& endpoint, ptr<logger> logger_=nullptr)
        : my_id_(srv_id)
        , partid(part_id)
        , my_endpoint_(endpoint)
    {
        cur_log_store_ = cs_new<walstore::WalStore>(logger_);
        ptr<walstore::WalStore> store = std::dynamic_pointer_cast<walstore::WalStore>(cur_log_store_);
        store->initial(base_path + "/" + std::to_string(part_id));

        config_file_path_ = base_path + "/" + std::to_string(partid) + "/configfile";
        state_file_path_ = base_path + "/" + std::to_string(partid) + "/statefile";

        if (exists(config_file_path_)) {
            std::ifstream in(config_file_path_);
            std::ostringstream content;
            content << in.rdbuf();
            std::string str = content.str();

            ptr<buffer> buf = buffer::alloc(str.size());
            buf->writeRaw((const uint8_t*)str.data(), str.size());

            buf->pos(0);
            saved_config_ = cluster_config::deserialize(*buf);
            my_srv_config_ = saved_config_->get_server(srv_id);
        } else {
            my_srv_config_ = cs_new<srv_config>( srv_id, endpoint );

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
            buf->writeRaw((const uint8_t*)str.data(), str.size());

            buf->pos(0);
            saved_state_ = srv_state::deserialize(*buf);
        }
    }

    ~KvStateMgr() {}

    ptr<cluster_config> load_config() {
        // Just return in-memory data in this example.
        // May require reading from disk here, if it has been written to disk.
        return saved_config_;
    }

    bool exists(const std::string& file_path) {
        struct stat st;
        if (stat(file_path.c_str(), &st) != 0) return false;

        return true;
    }

    void save_config(const cluster_config& config) {
        // Just keep in memory in this example.
        // Need to write to disk here, if want to make it durable.
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

    void save_state(const srv_state& state) {
        // Just keep in memory in this example.
        // Need to write to disk here, if want to make it durable.
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

    ptr<srv_state> read_state() {
        // Just return in-memory data in this example.
        // May require reading from disk here, if it has been written to disk.
        return saved_state_;
    }

    ptr<log_store> load_log_store() {
        return cur_log_store_;
    }

    uint64_t server_id() {
        return my_id_;
    }

    uint32_t part_id() { return partid; }

    void system_exit(const int exit_code) {
    }

    ptr<srv_config> get_srv_config() const { return my_srv_config_; }

private:
    std::string config_file_path_;
    std::string state_file_path_;
    uint64_t my_id_;
    uint32_t partid;
    std::string my_endpoint_;
    ptr<log_store> cur_log_store_;
    ptr<srv_config> my_srv_config_;
    ptr<cluster_config> saved_config_;
    ptr<srv_state> saved_state_;
};

};

