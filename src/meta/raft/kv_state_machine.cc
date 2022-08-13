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
#include "libnuraft/ptr.hxx"
#include "buffer_helper.h"

using namespace nuraft;

namespace mybase
{

KvStateMachine::KvStateMachine(const std::string& log_dir, ptr<logger> logger_)
    : sysFilePath(log_dir + "/sys_state_machine"), l_(logger_)
{
    assert(open_sys_file());
}

KvStateMachine::~KvStateMachine()
{
    if ( (sysPtr != MAP_FAILED) && (munmap(sysPtr, sysFileSize)) < 0) {
        p_er("munmap sys file failed! file[%s] err[%s]",
                 sysFilePath.c_str(), strerror(errno));
    }
}

ptr<nuraft::buffer> KvStateMachine::pre_commit(const uint64_t log_idx, buffer& data)
{
    // // Extract string from `data.
    // BufferHelper bs(data);
    // std::string str = bs.readStr();

    // // Just print.
    // std::cout << "pre_commit " << log_idx << ": "
    //           << str << std::endl;
    p_in("pre_commit %lu", log_idx);

    if (baseSM) baseSM->doPreCommit(log_idx);

    return nullptr;
}

ptr<nuraft::buffer> KvStateMachine::commit(const uint64_t log_idx, buffer& data)
{
    // // Extract string from `data.
    // BufferHelper bs(data);
    // std::string str = bs.readStr();

    p_in("commit %lu", log_idx);

    if (baseSM) baseSM->doCommit(log_idx, data);

    *lastCommitedIdxPtr = log_idx;

    return nullptr;
}

void KvStateMachine::rollback(const uint64_t log_idx, buffer& data)
{
    // Extract string from `data.
    BufferHelper bs(data);
    std::string str = bs.readStr();

    p_in("rollback %lu : %.*s", log_idx, str.size(), str.data());

    if (baseSM) baseSM->doRollback(log_idx);
}

int32_t KvStateMachine::read_logical_snp_obj(snapshot& s, void*& user_snp_ctx,
                                             uint64_t obj_id, ptr<buffer>& data_out,
                                             bool& is_last_obj)
{
    // Put dummy data.
    data_out = buffer::alloc( sizeof(int32_t) );
    BufferHelper bs(data_out);
    bs.writeInt32(0);

    is_last_obj = true;
    return 0;
}

void KvStateMachine::save_logical_snp_obj(snapshot& s, uint64_t& obj_id,
                                          buffer& data, bool is_first_obj,
                                          bool is_last_obj)
{
    p_in("save snapshot %lu term %lu object ID %lu",
              s.get_last_log_idx(), s.get_last_log_term(), obj_id);

    // Request next object.
    obj_id++;
}

bool KvStateMachine::apply_snapshot(snapshot& s)
{
    p_in("apply snapshot %lu term %lu",
              s.get_last_log_idx(), s.get_last_log_term());

    {   std::lock_guard<std::mutex> l(last_snapshot_lock_);
        ptr<buffer> snp_buf = s.serialize();
        last_snapshot_ = snapshot::deserialize(*snp_buf);
    }

    return true;
}

ptr<snapshot> KvStateMachine::last_snapshot()
{
    std::lock_guard<std::mutex> l(last_snapshot_lock_);
    return last_snapshot_;
}

void KvStateMachine::create_snapshot(snapshot& s, async_result<bool>::handler_type& when_done)
{
    p_in("create snapshot %lu term %lu", s.get_last_log_idx(), s.get_last_log_term());

    {   
        std::lock_guard<std::mutex> l(last_snapshot_lock_);
        ptr<buffer> snp_buf = s.serialize();
        last_snapshot_ = snapshot::deserialize(*snp_buf);
    }

    ptr<std::exception> except(nullptr);
    bool ret = true;
    when_done(ret, except);
}

bool KvStateMachine::open_sys_file()
{
    int32_t fd = open(sysFilePath.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        p_er("open sys file failed! file[%s]", sysFilePath.c_str());
        return false;
    }
    ftruncate(fd, sysFileSize);

    sysPtr = (uint8_t*)mmap(0, sysFileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (sysPtr == MAP_FAILED) {
        p_er("mmap sys file failed! file[%s] err[%s]", sysFilePath.c_str(), strerror(errno));
        return false;
    }

    ::close(fd);

    lastCommitedIdxPtr = (uint64_t*)sysPtr;
    // if (*lastCommitedIdxPtr == 0) *lastCommitedIdxPtr = 1;

    return true;
}

}