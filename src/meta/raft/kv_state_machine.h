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

#include "cclogger.h"
#include "base_sm.h"

#include "libnuraft/nuraft.hxx"
#include "libnuraft/tracer.hxx"

#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>

#include <sys/mman.h>

using namespace nuraft;

namespace mybase
{

class KvStateMachine : public state_machine
{
public:
    KvStateMachine(const std::string& log_dir, ptr<logger> logger_=nullptr);
    ~KvStateMachine();

    void setUserSM(ptr<BaseSM> base_sm) { baseSM = base_sm; }
    void setLogger(ptr<logger> logger_) { l_ = logger_; }

    ptr<nuraft::buffer> pre_commit(const uint64_t log_idx, buffer& data);

    ptr<nuraft::buffer> commit(const uint64_t log_idx, buffer& data);

    void rollback(const uint64_t log_idx, buffer& data);

    int32_t read_logical_snp_obj(snapshot& s, void*& user_snp_ctx,
                                 uint64_t obj_id, ptr<buffer>& data_out,
                                 bool& is_last_obj);

    void save_logical_snp_obj(snapshot& s, uint64_t& obj_id,
                              buffer& data, bool is_first_obj,
                              bool is_last_obj);

    bool apply_snapshot(snapshot& s);

    void free_user_snp_ctx(void*& user_snp_ctx) { }

    ptr<snapshot> last_snapshot();

    uint64_t last_commit_index() { return *lastCommitedIdxPtr; }

    void create_snapshot(snapshot& s, async_result<bool>::handler_type& when_done);

private:
    bool open_sys_file();

private:
    std::string sysFilePath;
    uint8_t* sysPtr{nullptr};
    uint64_t* lastCommitedIdxPtr{nullptr};
    uint32_t sysFileSize{512*1024};

    ptr<BaseSM> baseSM{nullptr};
    ptr<logger> l_{nullptr};

    // Last snapshot.
    ptr<snapshot> last_snapshot_;

    // Mutex for last snapshot.
    std::mutex last_snapshot_lock_;
};

}
