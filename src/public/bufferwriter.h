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

#include "dlog.h"
#include "common.h"

#include <string>
#include <map>
#include <set>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

class CBufferWrite
{
public:
    CBufferWrite(int32_t fd, int32_t buflen) : m_fd(fd), m_buflen(buflen), m_buf(nullptr), m_windex(0), m_statok(true)
    {
        m_buf = (char*)malloc(buflen);
        bzero(m_buf, buflen);
    }

    void setLogger(mybase::BaseLogger* logger) { myLog = logger; }

    int32_t write(void * buf, int32_t len)
    {
        if( !m_statok ) {
            return -1;
        }
        
        if( m_windex + len > m_buflen ) {
            if( flush() < 0 ) {
                return -1;
            }

            if( m_windex + len > m_buflen ) {
                _log_err(myLog, "m_windex[%d] + len[%d] > %d", m_windex, len, m_buflen);
                return -1;
            }
        }

        memcpy(m_buf + m_windex, buf, len);
        m_windex += len;

        return len;
    }

    int32_t flush()
    {
        if( !m_statok ) {
            return -1;
        }
        
        if( m_windex > 0 ) {
            if( m_windex != FileHelper::writen(m_fd, m_buf, m_windex) ) {
                _log_err(myLog, "writen[%d] error[%d,%s]", m_fd, errno, strerror(errno));
                m_statok = false;
                return -1;
            } else {
                m_windex = 0 ;
            }
        }

        return 0;
    }

    ~CBufferWrite()
    {
        flush();
        DELETE(m_buf);
    }
    
private:
    mybase::BaseLogger* myLog{nullptr};

    int32_t m_fd;
    
    int32_t m_buflen;
    char* m_buf;

    int32_t m_windex;

    bool m_statok;
    char m_errbuf[1024];
};