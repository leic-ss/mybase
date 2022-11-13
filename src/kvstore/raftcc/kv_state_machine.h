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

#include "raftcc/logger.hxx"
#include "raftcc/raftcc.hxx"

#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>
 
#include <sys/mman.h>

namespace raftcc {

class KvStateMachine : public raftcc::state_machine {
public:
    KvStateMachine(const std::string& log_dir, uint32_t part_id, int32_t srv_id,
                     std::shared_ptr<raftcc::logger> logger_=nullptr);
    ~KvStateMachine();

    void setLogger(std::shared_ptr<raftcc::logger> logger_) { myLog = logger_; }

    raftcc::state_machine::SResult pre_commit(const uint64_t log_idx, std::shared_ptr<raftcc::buffer> data);

    raftcc::state_machine::SResult commit(const uint64_t log_idx, std::shared_ptr<raftcc::buffer> data);

    void rollback(const uint64_t log_idx, raftcc::buffer& data);

    int read_logical_snp_obj(raftcc::snapshot& s,
                             void*& user_snp_ctx,
                             uint64_t obj_id,
                             std::shared_ptr<raftcc::buffer>& data_out,
                             bool& is_last_obj);

    void save_logical_snp_obj(raftcc::snapshot& s,
                              uint64_t& obj_id,
                              raftcc::buffer& data,
                              bool is_first_obj,
                              bool is_last_obj);

    bool apply_snapshot(raftcc::snapshot& s);

    void free_user_snp_ctx(void*& user_snp_ctx) { }

    std::shared_ptr<raftcc::snapshot> last_snapshot();

    uint64_t last_commit_index() {
        return *lastCommitedIdxPtr;
    }

    void create_snapshot(raftcc::snapshot& s, raftcc::async_result<bool>::handler_type& when_done);

private:
    bool open_sys_file();

private:
    std::string sysFilePath;
    uint8_t* sysPtr{nullptr};
    uint64_t* lastCommitedIdxPtr{nullptr};
    uint32_t sysFileSize{512*1024};

    std::shared_ptr<raftcc::logger> myLog{nullptr};

    // Last snapshot.
    std::shared_ptr<raftcc::snapshot> last_snapshot_;

    // Mutex for last snapshot.
    std::mutex last_snapshot_lock_;
};

}
