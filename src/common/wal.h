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

#include "public/buffers.h"
#include "public/fileutils.h"
#include "public/common.h"

#include <string>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <atomic>
#include <map>
#include <cstdlib>
#include <thread>

#include <unordered_map>

#ifndef __WAL_LOG_WAL_H__
#define __WAL_LOG_WAL_H__

namespace mybase {

namespace wal {

class WriteBatch {
public:

public:
    std::string batch;
};

struct Writer {
    Writer() {}

    int32_t ret{-1};
    std::shared_ptr<Buffer> data;

    bool sync{false};
    bool done{false};
    uint64_t seq{0};

    std::condition_variable cv;
};

class WalFileInfo final {
public:
    enum Status : int32_t {
        CLEAN        = 1,
        LOADING      = 2,
        NORMAL       = 3,
    };

public:
    WalFileInfo(std::string path, int32_t fd_, int64_t start_idx)
               : fullPath(path)
               , fd(fd_)
               , startIndex(start_idx) { }

    ~WalFileInfo() { }

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

class Wal {
public:
    Wal();
    ~Wal();

    bool initial(const std::string& dir);

    // Append one log message to the WAL
    uint64_t appendLog(Buffer::ptr data);

    Buffer::ptr entryAt(uint64_t idx);

    std::vector<Buffer::ptr> logEntries(uint64_t sidx, uint64_t eidx);

    bool writeAt(uint64_t idx, Buffer::ptr data);

    uint64_t startIndex() { return startIdx.load(); }

    uint64_t lastIndex() { return lastIdx.load(); }

    uint64_t nextIndex() { return nextIdx.load(); }

private:
    bool load();
    void rollback(uint64_t last_idx);

    Buffer::ptr buildBatchGroup(Writer** last_writer);
    bool prepareNewFile(uint64_t startLogId);

    bool scanAllWalFiles();

    std::string toString();

private:
    std::mutex cvLock;

    std::mutex walFileInfoLock;
    std::map<uint64_t, WalFileInfoPtr> walFiles;

    WalFileInfoPtr curWalFile{nullptr};

    std::atomic<uint64_t> startIdx{0};
    std::atomic<uint64_t> lastIdx{0};
    std::atomic<uint64_t> nextIdx{1};

    uint32_t curWalPos{0};
    Buffer::ptr idxBuf;

    std::string walDir;

    int32_t wFd{-1};

    std::deque<Writer*> writers;
};

} // namespace wal

} // namespace mybase

#endif  // WAL_WAL_H_
