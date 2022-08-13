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

#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/options.h"
#include "rocksdb/c.h"
#include "rocksdb/statistics.h"
#include "rocksdb/utilities/options_util.h"

#include "libnuraft/log_store.hxx"
#include "libnuraft/logger.hxx"

#include <atomic>
#include <map>
#include <mutex>

using namespace nuraft;

namespace mybase {

class KvLogStore : public log_store
{
public:
    KvLogStore(const std::string& log_dir, ptr<logger> logger_=nullptr);
    ~KvLogStore();

    uint64_t next_slot() const;

    uint64_t start_index() const;

    ptr<log_entry> last_entry() const;

    uint64_t append(ptr<log_entry>& entry);

    void write_at(uint64_t index, ptr<log_entry>& entry);

    ptr<std::vector<ptr<log_entry>>> log_entries(uint64_t start, uint64_t end);

    ptr<std::vector<ptr<log_entry>>> log_entries_ext(
            uint64_t start, uint64_t end, uint64_t batch_size_hint_in_bytes = 0);

    ptr<log_entry> entry_at(uint64_t index);

    uint64_t term_at(uint64_t index);

    ptr<buffer> pack(uint64_t index, int32_t cnt);

    void apply_pack(uint64_t index, buffer& pack);

    bool compact(uint64_t last_log_index);

    bool flush() { return true; }

    void close();

private:
    bool open_database();
    bool open_sys_file();

__nocopy__(KvLogStore);

private:
    static ptr<log_entry> make_clone(const ptr<log_entry>& entry);

private:
    std::string dbPath;
    std::string sysFilePath;
    uint8_t* sysPtr{nullptr};
    uint64_t* startIdxPtr{nullptr};
    uint64_t* nextIdxPtr{nullptr};
    uint32_t sysFileSize{1024*1024};

    ptr<logger> l_{nullptr};
    std::map<uint64_t, ptr<log_entry>> logs_;
    mutable std::mutex logs_lock_;

    rocksdb::DB* db{nullptr};
    rocksdb::DBOptions dbOpts;
    std::vector<rocksdb::ColumnFamilyDescriptor> cfDescs;
    std::vector<rocksdb::ColumnFamilyHandle*> cfHandles;
};

}

