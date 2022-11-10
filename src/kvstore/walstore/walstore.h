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

#include "buffer.h"
#include "fileutils.h"

#include "raftcc/log_store.hxx"
#include "raftcc/logger.hxx"

#include <string>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <atomic>
#include <map>
#include <cstdlib>
#include <thread>

#include <unordered_map>

#ifndef __WAL_STORE_WAL_H__
#define __WAL_STORE_WAL_H__

namespace walstore {

struct Writer {
    Writer() {}

    int32_t ret{-1};
    std::shared_ptr<Buffer> data;

    bool sync{false};
    bool done{false};
    uint64_t seq{0};

    std::condition_variable cv;
};

class ReadWriteLock
{
public:
    ReadWriteLock()
    {}
 
    void readLock();
    void readUnlock();
    void writeLock();
    void writeUnlock();

private:
    std::mutex mtx;
    std::condition_variable cond;
    // == 0 no lock, > 0 read lock count, < 0 write lock count
    int32_t count{0};
    int32_t writeWaitCount{0};
};

class RWLock {
public:
    RWLock(const RWLock&) = delete;
    RWLock& operator=(const RWLock&) = delete;
    virtual ~RWLock() = default;

    RWLock(bool write_first=false);

public:
    int32_t readLock();
    int32_t readUnlock();
    int32_t writeLock();
    int32_t writeUnlock();

private:
    const bool writeFirst;

    std::thread::id writeThreadId;
    std::atomic<int32_t> lockCount{0};
    std::atomic<int32_t> writeWaitCount{0};
};

class WalFileInfo final {
public:
    enum Status : int32_t {
        CLEAN        = 1,
        LOADING      = 2,
        NORMAL       = 3,
    };

public:
    WalFileInfo(std::string path, int64_t start_idx);
    ~WalFileInfo();

    const char* path() const { return fullPath.c_str(); }
    std::string getFullPath() const { return fullPath; }
    int32_t getFd() { return fd; }

    int64_t startIdx() const { return startIndex; }
    int64_t lastIdx() const { return lastIndex; }

    uint32_t rollback(uint64_t idx);

    std::shared_ptr<Buffer> buildIndex();
    bool insert(uint64_t seq, uint64_t size);
    uint64_t getIvtIndex(uint64_t seq);

    void clean();
    bool loadInfo();
    bool isLoadFull() { return loadFull; }

    void setNormal() { status = NORMAL; }
    void setClean() { status = CLEAN; }

    bool isNormal() { return status == NORMAL; }
    bool isClean() { return status == CLEAN; }

private:
    const std::string fullPath;
    int64_t startIndex{0};
    int64_t lastIndex{0};
    int32_t fd{-1};

    Status status{CLEAN};
    bool loadFull{false};
    RWLock rwlock;
    std::unordered_map<uint64_t, uint64_t> ivts;
};

using WalFileInfoPtr = std::shared_ptr<WalFileInfo>;

class WalStore : public raftcc::log_store {
public:
    WalStore(std::shared_ptr<raftcc::logger> logger_=nullptr);
    ~WalStore();

    bool initial(const std::string& dir);

    uint64_t next_slot() const { return lastIdx.load() + 1; }

    uint64_t start_index() const { return startIdx.load(); }

    std::shared_ptr<raftcc::log_entry> last_entry() const;

    uint64_t append(std::shared_ptr<raftcc::log_entry> entry);

    void write_at(uint64_t index, std::shared_ptr<raftcc::log_entry> entry);

    std::shared_ptr<std::vector<std::shared_ptr<raftcc::log_entry>>> log_entries(uint64_t start, uint64_t end);

    std::shared_ptr<std::vector<std::shared_ptr<raftcc::log_entry>>> log_entries_ext(
            uint64_t start, uint64_t end, uint64_t batch_size_hint_in_bytes = 0);

    std::shared_ptr<raftcc::log_entry> entry_at(uint64_t index);

    uint64_t term_at(uint64_t index);

    std::shared_ptr<raftcc::buffer> pack(uint64_t index, int32_t cnt);

    void apply_pack(uint64_t index, raftcc::buffer& pack);

    bool compact(uint64_t last_log_index);

    bool flush();

protected:
    // Append one log message to the WAL
    uint64_t appendLog(Buffer::ptr data);

    Buffer::ptr entryAt(const uint64_t idx) const;

    std::vector<Buffer::ptr> logEntries(uint64_t sidx, uint64_t eidx);

    bool writeAt(uint64_t idx, Buffer::ptr data);

private:
    bool load();
    void rollback(uint64_t last_idx);

    Buffer::ptr buildBatchGroup(Writer** last_writer);
    bool prepareNewFile(uint64_t startLogId);

    bool scanAllWalFiles();

    std::string toString();

private:
    std::mutex cvLock;

    mutable std::mutex walFileInfoLock;
    std::map<uint64_t, WalFileInfoPtr> walFiles;

    WalFileInfoPtr curWalFile{nullptr};

    std::atomic<uint64_t> startIdx{0};
    std::atomic<uint64_t> lastIdx{0};
    std::atomic<uint64_t> nextIdx{1};

    uint32_t curWalPos{0};
    Buffer::ptr idxBuf;

    std::string walDir;
    std::shared_ptr<raftcc::logger> myLog{nullptr};

    int32_t wFd{-1};
    std::deque<Writer*> writers;
};

} // namespace walstore

#endif  // WAL_WAL_H_
