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

#include "common.h"

#include "murmurhash2.h"

#include <chrono>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <fstream>
#include <sstream>

#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

static const int32_t sWriteLockStatus = -1;
static const int32_t sFreeStatus = 0;
static const std::thread::id NullThread;

CTimeInfo::CTimeInfo(std::chrono::system_clock::time_point now) {
    std::time_t raw_time = std::chrono::system_clock::to_time_t(now);
    std::tm* lt_tm = std::localtime(&raw_time);
    year = lt_tm->tm_year + 1900;
    month = lt_tm->tm_mon + 1;
    day = lt_tm->tm_mday;
    hour = lt_tm->tm_hour;
    min = lt_tm->tm_min;
    sec = lt_tm->tm_sec;

    size_t us_epoch = std::chrono::duration_cast
                      < std::chrono::microseconds >
                      ( now.time_since_epoch() ).count();
    msec = (us_epoch / 1000) % 1000;
    usec = us_epoch % 1000;
}

std::string CTimeInfo::toString() {
    thread_local char time_char[64];
    sprintf(time_char, "%04d-%02d-%02d %02d:%02d:%02d.%03d %03d",
            year, month, day, hour,
            min, sec, msec, usec);
    return time_char;
}

void GenericTimer::getTimeofDay(uint64_t& sec_out, uint32_t& us_out) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    sec_out = tv.tv_sec;
    us_out = tv.tv_usec;
}

uint64_t GenericTimer::getTimeofDay() {
    uint64_t sec_out = 0;
    uint32_t us_out = 0;
    GenericTimer::getTimeofDay(sec_out, us_out);
    return sec_out;
}

bool GenericTimer::timeout() const {
    if (!firstEventFired) {
        // First event, return `true` immediately.
        firstEventFired = true;
        return true;
    }

    auto cur = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = cur - start;
    return (duration_us < elapsed.count() * 1000000);
}

bool GenericTimer::timeoutAndReset() {
    auto cur = std::chrono::system_clock::now();

    if (!firstEventFired) {
        // First event, return `true` immediately.
        firstEventFired = true;
        return true;
    }

    std::chrono::duration<double> elapsed = cur - start;
    if (duration_us < elapsed.count() * 1000000) {
        start = cur;
        return true;
    }
    return false;
}

uint64_t GenericTimer::getElapsedUs() const {
    auto cur = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = cur - start;
    return elapsed.count() * 1000000;
}

uint64_t GenericTimer::getElapsedMs() const {
    auto cur = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = cur - start;
    return elapsed.count() * 1000;
}

bool GenericTimerTs::timeout() const {
    auto cur = std::chrono::system_clock::now();

    std::lock_guard<std::mutex> l(lock);
    if (!firstEventFired) {
        // First event, return `true` immediately.
        firstEventFired = true;
        return true;
    }

    std::chrono::duration<double> elapsed = cur - start;
    return (duration_us < elapsed.count() * 1000000);
}

bool GenericTimerTs::timeoutAndReset() {
    auto cur = std::chrono::system_clock::now();

    std::lock_guard<std::mutex> l(lock);
    if (!firstEventFired) {
        // First event, return `true` immediately.
        firstEventFired = true;
        return true;
    }

    std::chrono::duration<double> elapsed = cur - start;
    if (duration_us < elapsed.count() * 1000000) {
        start = cur;
        return true;
    }
    return false;
}

void GenericTimerTs::reset() {
    auto cur = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> l(lock);
    start = cur;
}

uint64_t GenericTimerTs::getElapsedUs() const {
    auto cur = std::chrono::system_clock::now();

    std::lock_guard<std::mutex> l(lock);
    std::chrono::duration<double> elapsed = cur - start;
    return elapsed.count() * 1000000;
}

uint64_t GenericTimerTs::getElapsedMs() const {
    auto cur = std::chrono::system_clock::now();

    std::lock_guard<std::mutex> l(lock);
    std::chrono::duration<double> elapsed = cur - start;
    return elapsed.count() * 1000;
}

size_t GenericTimerTs::getDurationSec() const {
    std::lock_guard<std::mutex> l(lock);
    return duration_us / 1000000;
}

void GenericTimerTs::resetDurationUs(uint64_t us) {
    auto cur = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> l(lock);
    duration_us = us;
    start = cur;
}

void GenericTimerTs::resetDurationMs(uint64_t ms) {
    auto cur = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> l(lock);
    duration_us = ms * 1000;
    start = cur;
}

RWSimpleLock::RWSimpleLock(ELockMode lockMode) 
{
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    if (lockMode == READ_PRIORITY)
    {
        pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_READER_NP);
    }
    else if (lockMode == WRITE_PRIORITY)
    {
        pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
    }
    pthread_rwlock_init(&_rwlock, &attr);
}

void ReadWriteLock::readLock()
{
    std::unique_lock<std::mutex> lk(mtx);
    while (count < 0 || writeWaitCount > 0) {
        cond.wait(lk);
    }
    ++count;
}

void ReadWriteLock::readUnlock()
{
    std::unique_lock<std::mutex> lk(mtx);
    if (--count == 0) {
        cond.notify_one();
    }
}

void ReadWriteLock::writeLock()
{
    std::unique_lock<std::mutex> lk(mtx);
    writeWaitCount++;
    while (count != 0) {
        cond.wait(lk);
    }

    writeWaitCount--;
    count = -1;
}

void ReadWriteLock::writeUnlock()
{
    std::unique_lock<std::mutex> lk(mtx);
    count = 0;
    cond.notify_all();
}

RWLock::RWLock(bool write_first)
              : writeFirst(write_first)
              , writeThreadId()
{
}

// > 0: success
int32_t RWLock::readLock() {
    if (std::this_thread::get_id() != writeThreadId) {
        int32_t count;
        if (writeFirst) {
            do {
                while ((count = lockCount) == sWriteLockStatus || writeWaitCount > 0);//写锁定时等待
            } while (!lockCount.compare_exchange_weak(count, count + 1));
        } else {
            do {
                while ((count = lockCount) == sWriteLockStatus); //写锁定时等待
            } while (!lockCount.compare_exchange_weak(count, count + 1));
        }
    }
    return lockCount;
}

int32_t RWLock::readUnlock() {
    // ==时为独占写状态,不需要加锁
    if (std::this_thread::get_id() != writeThreadId) {
        --lockCount;
    }
    return lockCount;
}

// -1: success
int32_t RWLock::writeLock()
{
    if (std::this_thread::get_id() != writeThreadId){
        writeWaitCount++;
        for(int32_t zero=sFreeStatus; !lockCount.compare_exchange_weak(zero, sWriteLockStatus); zero=sFreeStatus);
        writeWaitCount--;
        writeThreadId=std::this_thread::get_id();
    }

    return lockCount;
}

int32_t RWLock::writeUnlock()
{
    if(std::this_thread::get_id() != writeThreadId) {
        assert(false);
    }
    assert(sWriteLockStatus==lockCount);

    writeThreadId=NullThread;
    lockCount.store(sFreeStatus);
    return lockCount;
}

// Replace all `before`s in `src_str` with `after.
// e.g.) before="a", after="A", src_str="ababa"
//       result: "AbAbA"
std::string StringHelper::replace(const std::string& src_str,
                                  const std::string& before,
                                  const std::string& after)
{
    size_t last = 0;
    size_t pos = src_str.find(before, last);
    std::string ret;
    while (pos != std::string::npos) {
        ret += src_str.substr(last, pos - last);
        ret += after;
        last = pos + before.size();
        pos = src_str.find(before, last);
    }
    if (last < src_str.size()) {
        ret += src_str.substr(last);
    }
    return ret;
}

// e.g.)
//   src = "a,b,c", delim = ","
//   result = {"a", "b", "c"}
std::vector<std::string> StringHelper::tokenize(const std::string& src,
                                                const std::string& delim)
{
    std::vector<std::string> ret;
    size_t last = 0;
    size_t pos = src.find(delim, last);
    while (pos != std::string::npos) {
        ret.push_back( src.substr(last, pos - last) );
        last = pos + delim.size();
        pos = src.find(delim, last);
    }
    if (last < src.size()) {
        ret.push_back( src.substr(last) );
    }
    return std::move(ret);
}

// Trim heading whitespace and trailing whitespace
// e.g.)
//   src = " a,b,c ", whitespace =" "
//   result = "a,b,c"
std::string StringHelper::trim(const std::string& src,
                               const std::string whitespace)
{
    // start pos
    const size_t pos = src.find_first_not_of(whitespace);
    if (pos == std::string::npos)
        return "";

    const size_t last = src.find_last_not_of(whitespace);
    const size_t len = last - pos + 1;

    return src.substr(pos, len);
}

bool StringHelper::isPureInt(const char* p)
{
    if (p == nullptr || (*p) == '\0') return false;

    if ((*p) == '-') p++;
    while((*p)) {
        if ((*p) < '0' || (*p) > '9') return false;   
        p ++;
    }
    return true;
}

char* StringHelper::strToLower(char *psz_buf)
{
    if (psz_buf == nullptr) {
        return psz_buf;
    }

    char *p = psz_buf;
    while (*p) {
        if ((*p) & 0x80)
            p++;
        else if ((*p) >= 'A' && (*p) <= 'Z')
            (*p) += 32;
        p++;
    }
    return psz_buf;
}

char* StringHelper::convShowString(char *str, int size, char *ret, int msize) 
{
    int index = 0;
    if (ret == nullptr) {
       msize = size*3+5;
       ret = (char*) malloc(msize);
    }
    unsigned char *p = (unsigned char *)str;
    while (size-->0 && index<msize-4) {
       index += sprintf(ret+index, "\\%02X", *p);
       p ++;
    }
    ret[index] = '\0';
    return ret;
}

uint64_t TimeHelper::currentUs()
{
    struct timeval tm;
    gettimeofday(&tm, nullptr);
    uint64_t cur = tm.tv_sec*((uint64_t)1000000) + tm.tv_usec;
    return cur;
}

uint64_t TimeHelper::currentMs()
{
    return currentUs() / 1000;
}

uint64_t TimeHelper::currentSec()
{
    struct timeval tm;
    gettimeofday(&tm, nullptr);
    return tm.tv_sec;
}

uint64_t TimeHelper::curUs()
{
    auto now = std::chrono::duration_cast<std::chrono::microseconds>
                    (std::chrono::system_clock::now().time_since_epoch());
    return now.count();
}

uint64_t TimeHelper::curMs()
{
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>
                    (std::chrono::system_clock::now().time_since_epoch());
    return now.count();
}

uint64_t TimeHelper::curSec()
{
    auto now = std::chrono::duration_cast<std::chrono::seconds>
                    (std::chrono::system_clock::now().time_since_epoch());
    return now.count();
}

std::string TimeHelper::getGmtTime(uint32_t expire)
{
    time_t rawTime;
    char szTemp[30]={0};

    time(&rawTime);
    rawTime += expire;
    struct tm* timeInfo = gmtime(&rawTime);
    strftime(szTemp, sizeof(szTemp), "%a, %d %b %Y %H:%M:%S GMT", timeInfo);

    std::string timeStr = szTemp;
    return timeStr;
}

std::string TimeHelper::timeConvert(time_t ts)
{
    struct tm * timeinfo = localtime ( &ts );

    int32_t Year = timeinfo->tm_year+1900;
    int32_t Mon = timeinfo->tm_mon+1;
    int32_t Day = timeinfo->tm_mday;
    int32_t Hour = timeinfo->tm_hour;
    int32_t Min = timeinfo->tm_min;
    int32_t Second = timeinfo->tm_sec;

    std::string str;
    str.append(std::to_string(Year)).append("-");
    str.append(std::to_string(Mon)).append("-");
    str.append(std::to_string(Day)).append(" ");

    str.append(std::to_string(Hour)).append(":");
    str.append(std::to_string(Min)).append(":");
    str.append(std::to_string(Second));

    return str;
}

std::string TimeHelper::timeConvertDay(time_t ts)
{
    struct tm * timeinfo = localtime ( &ts );

    int32_t Year = timeinfo->tm_year+1900;
    int32_t Mon = timeinfo->tm_mon+1;
    int32_t Day = timeinfo->tm_mday;

    char str[16] = {0};
    sprintf(str, "%04d%02d%02d", Year, Mon, Day);

    return str;
}

void ThreadHelper::setThreadName(const std::string& thread_name)
{
    prctl(PR_SET_NAME, (uint64_t)thread_name.c_str());
}

std::string UtilHelper::vecot2String(const std::vector<int32_t>& vec)
{
    std::string str;
    for (auto ele : vec) {
        if (!str.empty()) {
            str.append(",");
        }
        str.append(std::to_string(ele));
    }
    return std::move(str);
}
std::string UtilHelper::vecot2String(const std::vector<std::string>& vec)
{
    std::string str;
    for (auto ele : vec) {
        if (!str.empty()) {
            str.append(",");
        }
        str.append(ele);
    }
    return std::move(str);
}

uint32_t UtilHelper::murMurHash(const void* key, int32_t len)
{
    return mur_mur_hash2(key, len, 97);
}

std::string UtilHelper::getFullPathDir(const std::string& szFullPath)
{
    std::string::size_type pos = szFullPath.find_last_of("/\\");
    std::string szName;
    if(pos != std::string::npos) {
        szName = szFullPath.substr(0, pos);
    }else {
        szName = szFullPath;
    }
    return szName;
}

std::string UtilHelper::getFullPathName(const std::string& szFullPath)
{
    std::string::size_type pos = szFullPath.find_last_of("\\/");
    std::string szName;
    if(pos != std::string::npos) {
        szName = szFullPath.substr(pos + 1);
    } else {
        szName = szFullPath;
    }
    return szName;
}

bool UtilHelper::isInteger(const char* str)
{
    int32_t len = strlen(str);
    for(int32_t i = 0; i < len; i++) {
        if( str[i] < '0' || str[i] > '9') {
            return false;
        }
    }
    return true;
}

uint64_t NetHelper::sLocalServerAddr = 0;

std::string NetHelper::addr2IpStr(uint32_t ip)
{
    std::string str;
    unsigned char *bytes = (unsigned char *) &ip;
    str.append(std::to_string(bytes[0])).append(".").append(std::to_string(bytes[1])).append(".")
       .append(std::to_string(bytes[2])).append(".").append(std::to_string(bytes[3]));
    return std::move(str);
}

uint32_t NetHelper::ipStr2Addr(const std::string& ip_str)
{
    const char* ip = ip_str.data();
    if (ip == nullptr) return 0;

    uint32_t x = inet_addr(ip);
    if (x == (uint32_t)INADDR_NONE) {
        struct hostent *hp;
        if ((hp = gethostbyname(ip)) == nullptr) {
            return 0;
        }
        x = ((struct in_addr *)hp->h_addr)->s_addr;
    }
    return x;
}

uint64_t NetHelper::str2Addr(const std::string& ip_ptr, int16_t port)
{
    uint32_t nip = 0;
    const char* ip = ip_ptr.data();
    const char *p = strchr(ip, ':');
    if (p != nullptr && p > ip) {
        int32_t len = p - ip;
        if (len > 64) len = 64;

        char tmp[128];
        strncpy(tmp, ip, len);
        tmp[len] = '\0';
        nip = ipStr2Addr(tmp);
        port = atoi(p+1);
    } else {
        nip = ipStr2Addr(ip);
    }
    if (nip == 0) {
        return 0;
    }

    uint64_t ipport = port;
    ipport <<= 32;
    ipport |= nip;

    return ipport;
}

std::string NetHelper::addr2String(uint64_t ipport)
{
    char str[32];
    uint32_t ip = (uint32_t)(ipport & 0xffffffff);
    int port = (int)((ipport >> 32 ) & 0xffff);
    unsigned char *bytes = (unsigned char *) &ip;
    if (port > 0) {
        sprintf(str, "%d.%d.%d.%d:%d", bytes[0], bytes[1], bytes[2], bytes[3], port);
    } else {
        sprintf(str, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
    }
    return str;
}

uint32_t NetHelper::getLocalAddr(const char *dev_name)
{
    if (!dev_name) return 0;

    int32_t             fd, intrface;
    struct ifreq    buf[16];
    struct ifconf   ifc;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) <= 0) {
        return 0;
    }
  
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = (caddr_t) buf;
    if (ioctl(fd, SIOCGIFCONF, (char *) &ifc)) {
        close(fd);
        return 0;
    }
    
    intrface = ifc.ifc_len / sizeof(struct ifreq);
    while (intrface-- > 0)
    {
        if(ioctl(fd,SIOCGIFFLAGS,(char *) &buf[intrface])) {
            continue;
        }
        if(buf[intrface].ifr_flags&IFF_LOOPBACK) continue;
        if (!(buf[intrface].ifr_flags&IFF_UP)) continue;
        if (dev_name != NULL && strcmp(dev_name, buf[intrface].ifr_name)) continue;
        if (!(ioctl(fd, SIOCGIFADDR, (char *) &buf[intrface]))) {
            close(fd);
            return ((struct sockaddr_in *) (&buf[intrface].ifr_addr))->sin_addr.s_addr;
        }
    }
    close(fd);
    return 0;
}

uint32_t NetHelper::getLocalIp()
{
    uint32_t ip;
    int32_t fd, intrface;
    struct ifreq buf[32];
    struct ifconf ifc;
    ip = -1;
    if ((fd = socket (AF_INET, SOCK_DGRAM, 0)) >= 0) {
        ifc.ifc_len = sizeof buf;
        ifc.ifc_buf = (caddr_t) buf;
        if (!ioctl (fd, SIOCGIFCONF, (char *) &ifc)) {
            intrface = ifc.ifc_len / sizeof (struct ifreq); 
            while (intrface-- > 0) {
                if (!(ioctl (fd, SIOCGIFADDR, (char *) &buf[intrface]))) {
                    ip = inet_addr( inet_ntoa( ((struct sockaddr_in*)(&buf[intrface].ifr_addr))->sin_addr) );
                    break;
                }
            }
        }
        close(fd);
    }
    return ip;
}

bool NetHelper::isLocalAddr(uint32_t ip, bool loopSkip)
{
    int32_t         fd, intrface;
    struct ifreq    buf[16];
    struct ifconf   ifc;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) <= 0) {
        return false;
    }

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = (caddr_t) buf;
    if (ioctl(fd, SIOCGIFCONF, (char *) &ifc)) {
        close(fd);
        return false;
    }

    intrface = ifc.ifc_len / sizeof(struct ifreq);
    while (intrface-- > 0)
    {
        if(ioctl(fd,SIOCGIFFLAGS,(char *) &buf[intrface])) {
            continue;
        }
        if(loopSkip && buf[intrface].ifr_flags&IFF_LOOPBACK) continue;
        if (!(buf[intrface].ifr_flags&IFF_UP)) continue;
        if (ioctl(fd, SIOCGIFADDR, (char *) &buf[intrface])) {
            continue;
        }
        if (((struct sockaddr_in *) (&buf[intrface].ifr_addr))->sin_addr.s_addr == ip) {
            close(fd);
            return true;
        }
    }
    close(fd);
    return false;
}

int32_t SysMgrHelper::popen(const std::string& cmd, std::string& output)
{
    char result_buf[1024] = {0};
    int32_t rc = 0;

    FILE* fp = ::popen(cmd.c_str(), "r");
    if(nullptr == fp) {
        return -1;
    }

    while(fgets(result_buf, sizeof(result_buf), fp) != nullptr) {
        if('\n' == result_buf[strlen(result_buf)-1]) {
            result_buf[strlen(result_buf)-1] = '\0';
        }
        output.append(result_buf);
    }

    rc = pclose(fp);
    if( rc != 0 ) {
        return rc;
    } else {
        printf("cmd[%s] child status[%d] rc[%d]", cmd.c_str(), rc, WEXITSTATUS(rc));
    }

    return 0;
}

bool SysMgrHelper::mkdirRecursive(const std::string& path, mode_t mode)
{
    std::string::size_type pos = 0;
    for(; pos != std::string::npos; ){
        pos = path.find("/", pos + 1);
        std::string s;
        if(pos == std::string::npos){
            s = path.substr(0, path.size());
            return -1 != ::mkdir(s.c_str(), mode);
        }else{
            s = path.substr(0, pos);
            ::mkdir(s.c_str(), mode);
        }
    }
    return true;
}

bool SysMgrHelper::exist(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return true;
}

int32_t SysMgrHelper::rmdir(const std::string& path) {
    if (!exist(path)) return 0;

    std::string cmd = "rm -rf " + path;
    return ::system(cmd.c_str());
}

int32_t SysMgrHelper::remove(const std::string& path) {
    if (!exist(path)) return 0;
    return ::remove(path.c_str());
}

int32_t SysMgrHelper::move(const std::string& from, const std::string& to) {
    if (!exist(from)) return 0;

    std::string cmd = "mv " + from + " " + to;
    return ::system(cmd.c_str());
}

int32_t SysMgrHelper::truncate(const std::string& path, uint64_t size) {
    if (!exist(path)) return 0;
    return ::truncate(path.c_str(), size);
}

int32_t SysMgrHelper::scan(const std::string& path,
                           std::vector<std::string>& files_out)
{
    DIR *dir_info = nullptr;
    struct dirent *dir_entry = nullptr;

    dir_info = opendir(path.c_str());
    while ( dir_info && (dir_entry = readdir(dir_info)) ) {
        files_out.push_back(dir_entry->d_name);
    }
    closedir(dir_info);
    return 0;
}
uint64_t SysMgrHelper::dirSize(const std::string& path,
                               bool recursive)
{
    uint64_t ret = 0;
    DIR *dir_info = nullptr;
    struct dirent *dir_entry = nullptr;

    dir_info = opendir(path.c_str());
    while ( dir_info && (dir_entry = readdir(dir_info)) ) {
        std::string d_name = dir_entry->d_name;
        if (d_name == "." || d_name == "..") continue;

        std::string full_path = path + "/" + d_name;

        if (dir_entry->d_type == DT_REG) {
            struct stat st;
            if (stat(full_path.c_str(), &st) == 0) {
                ret += st.st_size;
            }

        } else if (recursive && dir_entry->d_type == DT_DIR) {
            ret += dirSize(full_path, recursive);
        }
    }
    closedir(dir_info);
    return ret;
}

std::string SysMgrHelper::getDirPart(const std::string& full_path) {
    size_t pos = full_path.rfind("/");
    if (pos == std::string::npos) return ".";
    return full_path.substr(0, pos);
}

std::string SysMgrHelper::getFileNameFromFullPath(const std::string& full_path)
{
    std::string::size_type pos = full_path.find_last_of("\\/");
    std::string sz_name;
    if(pos != std::string::npos){
        sz_name = full_path.substr(pos + 1);
    }else {
        sz_name = full_path;
    }
    return sz_name;
}

bool SysMgrHelper::isRelPath(const std::string& path) {
    // If start with `./` or `../`: relative path.
    if (path.substr(0, 2) == "./" ||
        path.substr(0, 3) == "../") return true;

    // If filename only: relative path.
    if (path.find("/") == std::string::npos) return true;

    // Otherwise: absolute path.
    return false;
}

void SysMgrHelper::readFile(const std::string& filename, std::string& output) {
    std::ifstream fs;

    fs.open(filename);
    if (!fs.good()) return;

    std::stringstream ss;
    ss << fs.rdbuf();
    fs.close();

    output = ss.str();
}

uint64_t SysMgrHelper::getTotalSpace(const std::string& path) {
    struct statvfs res;
    int32_t rc = statvfs(path.c_str(), &res);
    if (rc != 0) return 0;
    return (uint64_t)res.f_bsize * res.f_blocks;
}

uint64_t SysMgrHelper::getFreeSpace(const std::string& path) {
    struct statvfs res;
    int32_t rc = statvfs(path.c_str(), &res);
    if (rc != 0) return 0;
    return (uint64_t)res.f_bsize * res.f_bavail;
}

ssize_t FileHelper::readn(int32_t fd, void *vptr, size_t n)
{
    size_t  nleft;
    ssize_t nread;
    char    *ptr;

    ptr = (char*)vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                nread = 0;      /* and call read() again */
            } else {
                return (-1);
            }
        } else if (nread == 0) {
            break;              /* EOF */
        }

        nleft -= nread;
        ptr   += nread;
    }

    return(n - nleft);      /* return >= 0 */
}

ssize_t FileHelper::writen(int32_t fd, const void *vptr, size_t n, int32_t sleeptime) /* write "n" bytes to a descriptor. */
{
    size_t  nleft;
    ssize_t nread;

    char* ptr = (char*)vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nread = write(fd, ptr, nleft)) <= 0) {
            if (errno == EINTR||errno==EAGAIN) {
                nread = 0;      /* and call read() again */
                if ( sleeptime ) {
                    usleep(sleeptime);
                }
            } else {
                return(-1);
            }
        } 

        nleft -= nread;
        ptr   += nread;
    }
    return(n - nleft);      /* return >= 0 */
}

bool FileHelper::writebuftofile(const std::string& filename, const std::string& buf, std::string& err)
{
    char statfiletmp[256]={0};
    snprintf(statfiletmp, sizeof(statfiletmp), "%s.bak", filename.c_str());

    char errbuf[1024] = {0};
    int32_t fd = open(statfiletmp,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if ( fd < 0 ) {
        snprintf(errbuf, 1024, "open file[%s] for write error[%d:%s]", statfiletmp, errno, strerror(errno));
        err = errbuf;
        if(SysMgrHelper::exist(statfiletmp)) ::remove(statfiletmp);
        return false;
    }

    int wlen = buf.size();
    if ( wlen != writen(fd, buf.c_str(), wlen)) {
        snprintf(errbuf,1024,"writefilestate file[%s] error [%d,%s]", statfiletmp, errno, strerror(errno));
        err = errbuf;

        close(fd);
        if(SysMgrHelper::exist(statfiletmp)) ::remove(statfiletmp);
        return false;
    }
    close(fd);

    if( 0 != rename(statfiletmp, filename.c_str())) {
        snprintf(errbuf, 1024, "rename file[%s->%s] error[%d:%s]", statfiletmp, filename.c_str(), errno, strerror(errno));
        err = errbuf;

        if(SysMgrHelper::exist(statfiletmp)) ::remove(statfiletmp);
        return false;
    } else {
        return true;
    }

    return true;
}

uint64_t FileHelper::statNodealSize(const std::string& basename, const std::string& statfile)
{
    std::string curfilename;
    uint64_t curfilepos = 0 ;
    readFileState(basename, statfile, curfilename, curfilepos);

    std::string dir = UtilHelper::getFullPathDir(basename);
    uint64_t retlen = 0;

    struct dirent** namelist = nullptr;
    int num = scandir(dir.c_str(), &namelist, 0, alphasort);
    for(int i = 0;  i < num; ++i) {
        std::string filename = dir + "/" + namelist[i]->d_name;
        if ( strncmp(filename.c_str(), basename.c_str(), basename.size()) != 0 ) {
            continue;
        }
        
        if ( filename < curfilename) {
            continue;
        } else if ( filename > curfilename ) {
            retlen += getFileSize(filename);
        } else if ( filename == curfilename ) {
            uint64_t filesize = getFileSize(filename);
            if ( filesize > curfilepos ) {
                retlen += (filesize - curfilepos);
            }
        }
    }

    for(int i = 0; i < num; ++i) {
        free(namelist[i]);
    }
    
    free(namelist);
    return retlen;
}

bool FileHelper::readFileState(const std::string& statfile, std::string &filename, uint64_t& pos)
{
    pos = 0;
    int32_t rfd = open(statfile.c_str(), O_RDONLY);
    if ( rfd < 0 ) {
        return true;
    }

    char buf[1024]={0};
    readn(rfd, buf, sizeof(buf));
    close(rfd);

    char* spaceptr = strchr(buf, ' ');
    if ( !spaceptr ) {
        return true;
    }

    std::string lastname(buf, spaceptr - buf);
    ++spaceptr;

    filename = lastname;
    pos = atol(spaceptr);

    return true;
}

static std::string rollProduceFilename(const std::string& log_base_path, uint32_t logindex)
{
    char file[256] = {0};
    snprintf(file, sizeof(file), "%s.%06u", log_base_path.c_str(), logindex);
    return file;
}

void FileHelper::readFileState(const std::string& basename, const std::string& statfile, std::string& filename, uint64_t& pos)
{
    filename = "";
    pos = 0;
    readFileState(statfile, filename, pos);

    if ( 0 == filename.size() || strncmp(basename.c_str(), filename.c_str(), basename.size()) != 0 ) {
       filename = rollProduceFilename(basename, 1);
       pos = 0;
    }
}

uint64_t FileHelper::getFileSize(const std::string& filename)
{
    struct stat statbuf;
    if ( 0 == stat(filename.c_str(), &statbuf)) {
        return statbuf.st_size;
    } else {
        return 0;
    }
}

std::string FileHelper::getDirPath(const std::string& full_path)
{
    std::string::size_type pos = full_path.find_last_of("/");
    std::string sz_name;
    if(pos != std::string::npos){
        sz_name = full_path.substr(0, pos);
    } else {
        sz_name = full_path;
    }
    return sz_name;  
}

uint64_t FileHelper::getFilePos(int32_t fd)
{
    uint64_t ret;

retry:
    ret = lseek(fd, 0, SEEK_CUR);
    if ( (uint64_t)-1 == ret ) {
        if ( errno == EINTR) {
            goto retry;
        }
        return ret;
    }

    return ret;
}

uint32_t FileHelper::getFileTime(const std::string& filename)
{
    struct stat statbuf;
    if( 0 == stat(filename.c_str(), &statbuf) ) {
        return  statbuf.st_mtime;
    }

    return 0;
}

std::string FileHelper::readContent(const std::string& filename)
{
    std::ifstream in(filename);
    std::ostringstream content;
    content << in.rdbuf();
    std::string str = content.str();

    return std::move(str);
}