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

#include <stdarg.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <deque>
#include <string>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>

#ifndef _BASE_LOGGER_
#define _BASE_LOGGER_

#define CCLOG_FILE_NAME(x) strrchr( (x),'/')?strrchr( (x) ,'/')+1:(x)

#define CCLOG_LEVEL_SYSTEM  0
#define CCLOG_LEVEL_FATAL   1
#define CCLOG_LEVEL_ERROR   2
#define CCLOG_LEVEL_WARN    3
#define CCLOG_LEVEL_INFO    4
#define CCLOG_LEVEL_DEBUG   5
#define CCLOG_LEVEL_TRACE   6

// printf style log macro
#define _log_(level, l, ...)        \
    if (l && l->getLogLevel() >= level) \
        (l)->logMessage(level, CCLOG_FILE_NAME(__FILE__), __LINE__, __FUNCTION__, __VA_ARGS__)

#define _log_err(l, ...)    _log_(CCLOG_LEVEL_ERROR,   l, __VA_ARGS__)
#define _log_warn(l, ...)   _log_(CCLOG_LEVEL_WARN,    l, __VA_ARGS__)
#define _log_info(l, ...)   _log_(CCLOG_LEVEL_INFO,    l, __VA_ARGS__)
#define _log_debug(l, ...)  _log_(CCLOG_LEVEL_DEBUG,   l, __VA_ARGS__)
#define _log_trace(l, ...)  _log_(CCLOG_LEVEL_TRACE,   l, __VA_ARGS__)

#endif

#define CCLOG_LEVEL(level) CCLOG_LEVEL_##level, CCLOG_FILE_NAME(__FILE__), __LINE__, __FUNCTION__
#define CCLOG_NUM_LEVEL(level) level, CCLOG_FILE_NAME(__FILE__), __LINE__, __FUNCTION__
#define CCLOG_PRINT(level, ...) CCLOG_LOGGER.logMessage(CCLOG_LOG_LEVEL(level), __VA_ARGS__)
#define CCLOG_LOG_BASE(level, ...) (CCLOG_LOG_LEVEL_##level>CCLOG_LOGGER._level) ? (void)0 : CCLOG_PRINT(level, __VA_ARGS__) 
#define CCLOG_LOG_US(level, _fmt_, args...) \
  ((CCLOG_LOG_LEVEL_##level>CCLOG_LOGGER._level) ? (void)0 : CCLOG_LOG_BASE(level, "[%ld][%ld][%ld] " _fmt_, \
                                                            pthread_self(), nubase:CCLogger::get_cur_tv().tv_sec, \
                                                            mybase::CCLogger::get_cur_tv().tv_usec, ##args))

#define CCLOG_LOG(level, _fmt_, args...) ((CCLOG_LOG_LEVEL_##level>CCLOG_LOGGER._level) ? (void)0 : CCLOG_LOG_BASE(level, _fmt_, ##args))

namespace mybase {
using std::deque;
using std::string;

class BaseLogger {
public:
    virtual void logMessage(int32_t level, const char *file, int32_t line, const char *function, const char *fmt, ...) = 0;
    virtual void setLogLevel(const char *level) = 0;
    virtual void setFileName(const char *filename, bool flag = false) = 0;
    virtual void setMaxFileSize( int64_t maxFileSize=0x40000000) = 0;
    virtual void setMaxFileIndex( int32_t maxFileIndex= 0x0F) = 0;
    virtual int32_t getLogLevel() = 0;
    virtual const char* getLogLevelStr() = 0;

    virtual void logRawMessage(const char *data, int32_t size) {}

public:
    int32_t _level;
};

class CCLogger : public BaseLogger {
public:

  static const mode_t LOG_FILE_MODE = 0644;
    CCLogger();
    ~CCLogger();

    void rotateLog(const char *filename, const char *fmt = nullptr);

    void logMessage(int32_t level, const char *file, int32_t line, const char *function, const char *fmt, ...);

    void logRawMessage(const char *data, int32_t size);

    void setLogLevel(const char *level);
    void setFileName(const char *filename, bool flag = false);
    int32_t getLogLevel() { return _level; }
    const char* getLogLevelStr();

    void checkFile(uint32_t cur_sec);
    void setCheck(bool v) { _check = v; }
    void setCheckInterval(uint32_t check_interval) { checkInterval = check_interval; }

    void setMaxFileSize( int64_t maxFileSize=0x40000000);

    void setMaxFileIndex( int32_t maxFileIndex= 0x0F);

    static inline struct timeval get_cur_tv()
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv;
    }

    static CCLogger* instance() { return &sLogger; }

private:
    int32_t _fd;
    char *_name;
    bool _check;
    uint32_t checkInterval;
    uint32_t nextCheckTime; // sec

    size_t _maxFileIndex;
    int64_t _maxFileSize;
    bool _flag;

public:
    static CCLogger sLogger;
    static const char *const _errstr[];

private:
    std::deque<std::string> _fileList;
    pthread_mutex_t _fileSizeMutex;
    pthread_mutex_t _fileIndexMutex;
};

}