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

#include <chrono>
#include <ctime>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>
#include <atomic>
#include <string>
#include <mutex>
#include <condition_variable>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <net/if.h>
#include <inttypes.h>
#include <linux/unistd.h>
#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <assert.h>
#include <string.h>

#define DELETE(ptr) \
    do {    \
        if (!ptr) break;    \
        delete ptr;     \
        ptr = nullptr;   \
    } while(false)

#define FREE(ptr) \
    do {    \
        if (!ptr) break;    \
        free(ptr);     \
        ptr = nullptr;   \
    } while(false)

class CTimeInfo {
public:
    CTimeInfo() { }
    CTimeInfo(std::chrono::system_clock::time_point now);
    void now() { *this = CTimeInfo(std::chrono::system_clock::now()); }
    std::string toString();

public:
    int32_t year{0};
    int32_t month{0};
    int32_t day{0};
    int32_t hour{0};
    int32_t min{0};
    int32_t sec{0};
    int32_t msec{0};
    int32_t usec{0};
};

class GenericTimer {
public:
    GenericTimer(uint64_t _duration_sec = 0, bool fire_first_event = false)
        : duration_us(_duration_sec * 1000000)
        , firstEventFired(!fire_first_event)
    { reset(); }

    static std::chrono::system_clock::time_point now() { return std::chrono::system_clock::now(); }
    static void sleepUs(size_t us) { std::this_thread::sleep_for(std::chrono::microseconds(us)); }
    static void sleepMs(size_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
    static void sleepSec(size_t sec) { std::this_thread::sleep_for(std::chrono::seconds(sec)); }

    static void getTimeofDay(uint64_t& sec_out, uint32_t& us_out);
    static uint64_t getTimeofDay();
    inline bool timeout() const;
    inline bool timeoutAndReset();

    inline void reset() { start = std::chrono::system_clock::now(); }

    inline uint64_t getElapsedUs() const;
    inline uint64_t getElapsedMs() const;
    inline size_t getDurationSec() const { return duration_us / 1000000; }

    inline void resetDurationUs(uint64_t us) { duration_us = us; reset(); }
    inline void resetDurationMs(uint64_t ms) { duration_us = ms * 1000; reset(); }

protected:
    std::chrono::time_point<std::chrono::system_clock> start;
    uint64_t duration_us;
    mutable bool firstEventFired;
};

// Thread-safe version of GenericTimer.
class GenericTimerTs : public GenericTimer {
public:
    GenericTimerTs(uint64_t _duration_sec = 0, bool fire_first_event = false)
        : GenericTimer(_duration_sec, fire_first_event)
    {}

    bool timeout() const;
    bool timeoutAndReset();
    void reset();
    uint64_t getElapsedUs() const;
    uint64_t getElapsedMs() const;
    size_t getDurationSec() const;
    void resetDurationUs(uint64_t us);
    void resetDurationMs(uint64_t ms);

protected:
    mutable std::mutex lock;
};

class SpinLock {
public:
    SpinLock() : flag(false)
    {}

    void lock() {
        bool expect = false;
        while (!flag.compare_exchange_strong(expect, true)) {
            expect = false;
        }
    }
    void unlock() { flag.store(false); }

private:
    std::atomic<bool> flag;
};

class RWSimpleLock
{
public:
    enum ELockMode
    {
        NO_PRIORITY,
        WRITE_PRIORITY,
        READ_PRIORITY
    };

public:
    RWSimpleLock(ELockMode lockMode = NO_PRIORITY);
    ~RWSimpleLock() { pthread_rwlock_destroy(&_rwlock); }

public:
    int32_t rdlock() { return pthread_rwlock_rdlock(&_rwlock); } // 0: success
    int32_t wrlock() { return pthread_rwlock_wrlock(&_rwlock); } // 0: success
    int32_t tryrdlock() { return pthread_rwlock_tryrdlock(&_rwlock); } // 0: success
    int32_t trywrlock() { return pthread_rwlock_trywrlock(&_rwlock); } // 0: success
    int32_t unlock() { return pthread_rwlock_unlock(&_rwlock); } // 0: success

private:
    pthread_rwlock_t _rwlock;
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

class StringHelper {
public:
    // Replace all `before`s in `src_str` with `after.
    // e.g.) before="a", after="A", src_str="ababa"
    //       result: "AbAbA"
    static std::string replace(const std::string& src_str,
                               const std::string& before,
                               const std::string& after);
    // e.g.)
    //   src = "a,b,c", delim = ","
    //   result = {"a", "b", "c"}
    static std::vector<std::string> tokenize(const std::string& src,
                                             const std::string& delim);
    // Trim heading whitespace and trailing whitespace
    // e.g.)
    //   src = " a,b,c ", whitespace =" "
    //   result = "a,b,c"
    static std::string trim(const std::string& src,
                            const std::string whitespace = " \t\n");

    static bool isPureInt(const char* p);

    static char* strToLower(char *psz_buf);

    static char* convShowString(char *str, int size, char *ret = nullptr, int msize = 0);

    static bool startWith(const std::string &str, const std::string &head) {
        return str.compare(0, head.size(), head) == 0;
    }
    static bool endWith(const std::string &str, const std::string &tail) {
        return str.compare(str.size() - tail.size(), tail.size(), tail) == 0;
    }
};

class TimeHelper {
public:
    static uint64_t currentUs();
    static uint64_t currentMs();
    static uint64_t currentSec();

    static uint64_t curUs();
    static uint64_t curMs();
    static uint64_t curSec();

    static std::string getGmtTime(uint32_t expire=0);
    static std::string timeConvert(time_t ts);
    static std::string timeConvertDay(time_t ts);

    static uint64_t getUSec(const struct timeval* tm) { return tm->tv_sec*((uint64_t)1000000) + tm->tv_usec; }
};

class ThreadHelper {
public:
    static void setThreadName(const std::string& thread_name);
};

// TODO: template
class UtilHelper {
public:
    static std::string vecot2String(const std::vector<int32_t>& vec);
    static std::string vecot2String(const std::vector<std::string>& vec);

    static uint32_t murMurHash(const void *key, int32_t len);

    static std::string getFullPathDir(const std::string& szFullPath);

    static std::string getFullPathName(const std::string& szFullPath);

    static bool isInteger(const char* str);
};

class NetHelper {
public:
    static std::string addr2IpStr(uint32_t ip);

    static uint32_t ipStr2Addr(const std::string& ip);

    static uint64_t str2Addr(const std::string& ip, int16_t port = 0);

    static std::string addr2String(uint64_t ipport);

    static std::string addr2Ip(uint64_t ipport) { return addr2String( (uint32_t)(ipport & 0xFFFFFFFF)); }

    static uint16_t addr2Port(uint64_t ipport) { return (ipport >> 32) & 0xFFFF; }

    static uint32_t getLocalAddr(const char *dev_name);

    static uint32_t getLocalIp();

    static bool isLocalAddr(uint32_t ip, bool loopSkip = true);

    static uint64_t ipAndPort(uint32_t ip, uint16_t port) { return ( ((uint64_t)port << 32) | ip); }

    static std::pair<uint32_t, uint16_t> getIpPort(uint64_t ipport) {
        return std::pair<uint32_t, uint16_t>( (ipport & 0xFFFFFFFF), (ipport >> 32) & 0xFFFF );
    }

public:
    static uint64_t sLocalServerAddr;
};

class SysMgrHelper {
public:
    static int32_t popen(const std::string& cmd, std::string& output);

    static bool exist(const std::string& path);
    static int32_t mkdir(const std::string& path) { return ::mkdir(path.c_str(), 0755); }
    static bool mkdirRecursive(const std::string& path, mode_t mode=0755);
    static int32_t rmdir(const std::string& path);
    static int32_t remove(const std::string& path);
    static int32_t move(const std::string& from, const std::string& to);
    static int32_t truncate(const std::string& path, uint64_t size);
    static int32_t scan(const std::string& path, std::vector<std::string>& files_out);
    static uint64_t dirSize(const std::string& path, bool recursive = false);
    static std::string getDirPart(const std::string& full_path);
    static std::string getFileNameFromFullPath(const std::string& full_path);
    static bool isRelPath(const std::string& path);
    static void readFile(const std::string& filename, std::string& output);
    static uint64_t getTotalSpace(const std::string& path);
    static uint64_t getFreeSpace(const std::string& path);
};

class FileHelper {
public:
    static int64_t readn(int32_t fd, void *vptr, uint64_t n);
    static ssize_t writen(int32_t fd, const void *vptr, size_t n, int32_t sleeptime=0);

    static bool readFileState(const std::string& statfile, std::string& filename, uint64_t& pos);
    static void readFileState(const std::string& basename, const std::string& statfile, std::string& filename, uint64_t& pos);
    static bool writebuftofile(const std::string& filename, const std::string& buf, std::string& err);
    static uint64_t statNodealSize(const std::string& basename, const std::string& statfile);

    static std::string getDirPath(const std::string& full_path);
    static uint64_t getFileSize(const std::string& filename);
    static uint64_t getFilePos(int32_t fd);
    static uint32_t getFileTime(const std::string& filename);

    static std::string readContent(const std::string& filename);
};