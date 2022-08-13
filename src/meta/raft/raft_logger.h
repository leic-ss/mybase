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

#include "libnuraft/pp_util.hxx"
#include "libnuraft/logger.hxx"

#include "cclogger.h"

using namespace nuraft;

namespace mybase
{

class RaftLogger : public nuraft::logger
{
public:
    void put_details(int level, const char* file, const char* function, size_t line, const std::string& log_line)
    {
        if (_logger.getLogLevel() < level) return ;

        static const uint32_t MAX_MSG_SIZE = 4096;
        char data1[MAX_MSG_SIZE] = {0};

        uint32_t avail_len = MAX_MSG_SIZE;
        uint32_t data_size = 0;
        {
            uint32_t cur_len = snprintf( data1, avail_len, " [%-4s] ", CCLogger::_errstr[level]);
            cur_len = (cur_len < avail_len) ? cur_len : avail_len;
            data_size += cur_len;
            avail_len = MAX_MSG_SIZE - data_size;
        }

        {
            uint32_t cur_len = snprintf( data1 + data_size, avail_len, " %s ", log_line.c_str());
            cur_len = (cur_len < avail_len) ? cur_len : avail_len;
            data_size += cur_len;
            avail_len = MAX_MSG_SIZE - data_size;
        }

        uint32_t last_slash = 0;
        for (uint32_t ii=0; file && file[ii] != 0; ++ii) {
            if (file[ii] == '/' || file[ii] == '\\') last_slash = ii;
        }

        if (file && line && avail_len) {
            uint32_t cur_len = snprintf( data1 + data_size, avail_len, "\t[%s:%lu, %s()]",
                                         file + ((last_slash)?(last_slash+1):0),
                                         line, function );
            cur_len = (cur_len < avail_len) ? cur_len : avail_len;
            data_size += cur_len;
        }

        if (avail_len == 0) {
            // remove trailing '\n'
            while (data1[data_size-1] == '\n') data_size --;
            data1[data_size-1] = '\0';
        }

        _logger.logRawMessage(data1, data_size);
    }

    void set_level(int l) { }

    int  get_level() { return _logger.getLogLevel(); }

    void setLogLevel(const char *level) { _logger.setLogLevel(level); }

    void setFileName(const char *filename, bool flag = false) { _logger.setFileName(filename, flag); }

    void setMaxFileSize( int64_t maxFileSize=0x40000000) { _logger.setMaxFileSize(maxFileSize); }

    void setMaxFileIndex( int32_t maxFileIndex= 0x0F) { _logger.setMaxFileIndex(maxFileIndex); }

private:
    int level{0};
    CCLogger _logger;
};

}
