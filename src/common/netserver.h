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

#include "public/cclogger.h"
#include "public/common.h"
#include "public/buffers.h"

#include "bufferqueue.h"

#include <mutex>
#include <atomic>
#include <thread>
#include <functional>

#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

namespace mybase {

enum CloudNetSocketType {
    SocketNotInit = 0,    //还没有初始化
    SocketListen = 1,     //监听套即可
    SocketServerConn = 2, //server 端连接过来的套接口
    SocketUnknown = 50
};

enum CModelProtocalEnum {
    CModelProtocalTcp,
    CModelProtocalUdp,
    CModelProtocalUnix,
};

struct CloudConnHead {
    int32_t sock_index;
	struct timeval sock_create;
	struct timeval request;
	struct timeval response;
	uint32_t src_ip;
	uint16_t src_port;

    CloudConnHead() {
        bzero(this, sizeof(*this));
        sock_index = -1;
    }
};

struct CCloudAddrArgu {
    CModelProtocalEnum m_protocal;
    char m_bind[64]; //监听IP和端口
    uint16_t m_port;

    char m_connip[64]; //连接过来的IP和端口
    uint16_t m_connport;

    CCloudAddrArgu() {
        bzero(this, sizeof(*this));
    }
};

class CCloudNetConnection {
public:
    CCloudNetConnection()
    {
        //atomic_set(&m_lock,1);
        m_init = false;
        m_buflen = 0;
        m_socketType = SocketNotInit;
    }
    ~CCloudNetConnection()
    {
        release();
    }

    bool init(unsigned int buflen) {
        if (m_init) {
            return true;
        }
            
        m_buflen = buflen;
        m_init = true;
        return true;
    }

    void release() {
        int fd = m_connHead.sock_index;
        m_connHead.sock_index = -1; //禁止其它线程读写
        m_readBuf.release();
        m_writeBuf.release();
        if(fd >= 0) {
            close(fd);
        }
        m_socketType = SocketNotInit;
    }

    void tostring(char * buf) {
        snprintf(buf,1024,"fd:%d,createtime:%ld,buflen:%d,socktype:%d,proto:%d,bind[%s,%d] from[%s,%d]",
            m_connHead.sock_index, m_connHead.sock_create.tv_sec,m_buflen, m_socketType, m_addr.m_protocal,
            m_addr.m_bind, m_addr.m_port, m_addr.m_connip, m_addr.m_connport);
        //snprintf(buf,1024,"fd:%d,createtime:%ld,lock:%d,buflen:%d,socktype:%d,proto:%d,bind[%s,%d] from[%s,%d]",m_connHead.sock_index, m_connHead.sock_create.tv_sec, atomic_read(&m_lock),m_buflen, m_socketType, m_addr.m_protocal, m_addr.m_bind, m_addr.m_port, m_addr.m_connip, m_addr.m_connport);
    }

    bool read(char* errbuf);
    int recvUdp(char* recvbuf, int recvlen, unsigned int& srcIp, unsigned short& srcPort, char * errbuf);
    int send(const char* buf, unsigned int len, int wepollfd, char* errbuf);
    int sendUdp(const char* buf, unsigned int len, unsigned int srcIp,unsigned short srcPort, char * errbuf);

    void shutdown() {
        ::shutdown(m_connHead.sock_index,SHUT_RDWR);
    }

    CloudConnHead m_connHead;  
    //atomic_t m_lock;
	std::mutex m_lock;
    bool m_init;
    uint32_t m_buflen;
    NBuffer m_readBuf;
    NBuffer m_writeBuf;
    CCloudAddrArgu m_addr;
    CloudNetSocketType m_socketType;      
};

class IwServer
{
public:
    virtual ~IwServer();
    virtual void setLogger(BaseLogger* logger) { myLog = logger; }
    void setUnitBatchCount(uint32_t batch_count) { unitBatchCount = batch_count; }

    bool initialize(const CCloudAddrArgu& listen_addr);
    bool start();
    void wait();
    bool stop();

    void setProcessThreadNumber(int32_t number) { processorNum = number; }
    virtual void process(const CloudConnHead& rsp_head, char* buf, uint32_t len) { }
    bool sendResponse(const CloudConnHead& head, const char* buffer, uint32_t len);
    bool sendResponse(uint32_t sock_index, uint32_t sock_create, const char* buffer, uint32_t len);

protected:
    virtual bool initServer() { return true; }
    virtual bool startServer() { return true; }
    virtual bool stopServer() { return true; }

    CCloudNetConnection* getNewConn(int32_t fd, 
                                    const CCloudAddrArgu& addr_arg,
                                    CloudNetSocketType socktype,
                                    uint32_t srcip = 0,
                                    uint16_t srcport = 0);
    CCloudNetConnection* getExistConn(int32_t fd);
    bool releaseConn(int32_t fd);

    int32_t openListenFd(const CCloudAddrArgu& addr);
    bool setFdTag(int32_t fd, int32_t tag);

    bool onAccpet(CCloudNetConnection* conn);
    bool dealEpollIn(CCloudNetConnection* conn, struct timeval* curtimeval);
    bool dealEpollOut(CCloudNetConnection* conn);

    bool lockConn(int32_t fd);
    void unLockConn(int32_t fd);

    void delWrEpoll(int32_t fd);
    void checkFdTimeOut(time_t curtime);
    void sendRecvToQueue(mybase::CEnqueUnit* unitArray, int32_t unitNum);

protected:
    void nioReadThread();
    void nioWriteThread();
    void workerThread(int32_t idx);

protected:
    std::thread epollReadThd;
    std::thread epollWriteThd;
    std::vector<std::thread> workerThds;
    uint32_t maxFd{0};
    int32_t epollrfd{0};
    int32_t epollwfd{0};
    uint32_t bufLen{1024*1024};
    uint32_t processorNum{16};
    bool isRunning{false};

    BaseLogger* myLog{nullptr};
    uint32_t connNum{102400};
    uint32_t fdMaxNum{102408};
    int32_t sendPipe[2];

    CLockBufferQueue handleQueue;
    uint32_t keepAliveTime{60}; // sec
    uint64_t lastCheckTimeout{TimeHelper::currentSec()}; // sec

    CCloudNetConnection* connArray{nullptr};
    uint32_t* aliveFdArray{nullptr};

    mybase::CEnqueUnit* unitArray{nullptr};
    int32_t unitBatchCount{10};
};

}
