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

#include "kv_state_machine.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>
 
#include <sys/mman.h>

namespace raftcc {

KvStateMachine::KvStateMachine(const std::string& log_dir, uint32_t part_id, int32_t srv_id,
                 std::shared_ptr<raftcc::logger> logger_)
    : sysFilePath(log_dir + "/" + std::to_string(part_id) + "/sys_state_machine")
    , myLog(logger_)
{
    assert(open_sys_file());
}

KvStateMachine::~KvStateMachine()
{
    if ( (sysPtr != MAP_FAILED) && (munmap(sysPtr, sysFileSize)) < 0) {
        _log_err(myLog, "munmap sys file failed! file[%s] err[%s]", sysFilePath.c_str(), strerror(errno));
    }
}

raftcc::state_machine::SResult KvStateMachine::pre_commit(const uint64_t log_idx, std::shared_ptr<raftcc::buffer> data)
{
    raftcc::BufferHelper bs(*data);
    std::string str = bs.readStr();

    return state_machine::SResult(0, nullptr);
}

raftcc::state_machine::SResult KvStateMachine::commit(const uint64_t log_idx, std::shared_ptr<raftcc::buffer> data)
{
    raftcc::BufferHelper bs(*data);
    std::string str = bs.readStr();

    _log_info(myLog, "commit %lu : %.*s", log_idx, str.size(), str.data());

    *lastCommitedIdxPtr = log_idx;
    return state_machine::SResult(0, nullptr);
}

void KvStateMachine::rollback(const uint64_t log_idx, raftcc::buffer& data)
{
    raftcc::BufferHelper bs(data);
    std::string str = bs.readStr();

    _log_info(myLog, "rollback %lu : %.*s", log_idx, str.size(), str.data());
}

int32_t KvStateMachine::read_logical_snp_obj(raftcc::snapshot& s,
                                               void*& user_snp_ctx,
                                               uint64_t obj_id,
                                               std::shared_ptr<raftcc::buffer>& data_out,
                                               bool& is_last_obj)
{
    data_out = raftcc::buffer::alloc( sizeof(int32_t) );
    raftcc::BufferHelper bs(data_out);
    bs.writeInt32(0);

    is_last_obj = true;
    return 0;
}

void KvStateMachine::save_logical_snp_obj(raftcc::snapshot& s,
                                            uint64_t& obj_id,
                                            raftcc::buffer& data,
                                            bool is_first_obj,
                                            bool is_last_obj)
{
    obj_id++;
}

bool KvStateMachine::apply_snapshot(raftcc::snapshot& s)
{
    {   std::lock_guard<std::mutex> l(last_snapshot_lock_);
        std::shared_ptr<raftcc::buffer> snp_buf = s.serialize();
        last_snapshot_ = raftcc::snapshot::deserialize(*snp_buf);
    }
    return true;
}

std::shared_ptr<raftcc::snapshot> KvStateMachine::last_snapshot()
{
    std::lock_guard<std::mutex> l(last_snapshot_lock_);
    return last_snapshot_;
}

void KvStateMachine::create_snapshot(raftcc::snapshot& s,
                                       raftcc::async_result<bool>::handler_type& when_done)
{
    std::cout << "create snapshot " << s.get_last_log_idx()
              << " term " << s.get_last_log_term() << std::endl;
    // Clone snapshot from `s`.
    {   std::lock_guard<std::mutex> l(last_snapshot_lock_);
        std::shared_ptr<raftcc::buffer> snp_buf = s.serialize();
        last_snapshot_ = raftcc::snapshot::deserialize(*snp_buf);
    }
    std::shared_ptr<std::exception> except(nullptr);
    bool ret = true;
    when_done(ret, except);
}

bool KvStateMachine::open_sys_file()
{
    int32_t fd = open(sysFilePath.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        _log_err(myLog, "open sys file failed! file[%s]", sysFilePath.c_str());
        return false;
    }
    ftruncate(fd, sysFileSize);

    sysPtr = (uint8_t*)mmap(0, sysFileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (sysPtr == MAP_FAILED) {
        _log_err(myLog, "mmap sys file failed! file[%s] err[%s]", sysFilePath.c_str(), strerror(errno));
        return false;
    }

    ::close(fd);

    lastCommitedIdxPtr = (uint64_t*)sysPtr;
    // if (*lastCommitedIdxPtr == 0) *lastCommitedIdxPtr = 1;

    return true;
}

}
