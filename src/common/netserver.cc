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

#include "netserver.h"

#include "public/common.h"
#include "public/cclogger.h"

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

namespace mybase {

static const int32_t MaxReqBufLen = 1024*1024+4;

inline __uint64_t mkFdAndTime(int fd, int msec)
{
    return (((__uint64_t)msec)<<32) | fd;
}

inline void getFdAndTime(__uint64_t data, int& fd, int& msec)
{
    fd = data & 0xffffffff;
    msec = data>>32;
    return ;
}

bool CCloudNetConnection::read(char* errbuf)
{
    if (m_connHead.sock_index < 0) {
        snprintf(errbuf,1024,"read fd[%d]<0",m_connHead.sock_index);
        return false;
    }

    NBuffer & conBuf = m_readBuf;
    int readbuflen = conBuf.getFreeLen();
    int ret = recv(m_connHead.sock_index,conBuf.getBuff()+conBuf.getIndex(),readbuflen,MSG_DONTWAIT);
    if (ret < 0) {
        if (errno == EINTR || errno == EAGAIN) {
            return true;
        }
        else {
            snprintf(errbuf,1024,"recv<0 because[%d:%s]", errno,strerror(errno));
            return false;
        }
    }
    else if (ret == 0) {
        snprintf(errbuf,1024,"close by foreigh peer");
        return false;
    }

    conBuf.addIndex(ret);
    return true;
}

int CCloudNetConnection::recvUdp(char* recvbuf,
                                    int recvlen,
                                    unsigned int& srcIp,
                                    unsigned short& srcPort,
                                    char* errbuf)
{
    srcIp = 0;
    srcPort = 0;
    if (m_connHead.sock_index < 0) {
        snprintf(errbuf,1024,"read fd[%d]<0",m_connHead.sock_index);
        return -1;
    }

    struct   sockaddr_in   from;
    socklen_t   fromlen   =(socklen_t)sizeof(from);
    int retlen = recvfrom(m_connHead.sock_index,recvbuf,recvlen,0,(sockaddr*)(&from),&fromlen);
    if(retlen >= 0) {
        srcIp = from.sin_addr.s_addr;
        srcPort = ntohs(from.sin_port);
    }

    return retlen;
}

#define CloudAddSendPartionToEpool \
    do{\
        struct epoll_event ev;\
        ev.events =  EPOLLOUT | EPOLLERR | EPOLLHUP; \
        ev.data.u64 = mkFdAndTime(m_connHead.sock_index,m_connHead.sock_create.tv_usec);\
        int iret = epoll_ctl(wepollfd,EPOLL_CTL_ADD,m_connHead.sock_index,&ev);\
        if ( iret < 0 )\
        {\
            snprintf(errbuf,1024,"epoll_ctl epollfd[%d] add fd[%d] error[%d:%s]",wepollfd,m_connHead.sock_index,errno,strerror(errno));\
            return -1;\
        }\
    }while(0)

int CCloudNetConnection::send(const char* buf,
                                unsigned int len,
                                int wepollfd,
                                char* errbuf)
{
    int ret = 0;
    NBuffer& conBuf = m_writeBuf;
    if (conBuf.getIndex() > 0) {
        if(false == conBuf.copyBuf(buf, len)) {
            return 1;
        }
    }
    else {
        ret = ::send(m_connHead.sock_index, buf, len, MSG_DONTWAIT);
        if (ret < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                conBuf.copyBuf(buf, len);
                CloudAddSendPartionToEpool;
            }
            else {
                snprintf(errbuf,1024,"send fail:%d,%s",errno,strerror(errno));
                return -1;
            }
        }
        else {
            if (ret < (int)len) {
                int left = len - ret ;
                conBuf.copyBuf(buf+ret,left);
                CloudAddSendPartionToEpool;
            }
            else {
                return 0;
            }
        }
    }

    return 0;
}

int CCloudNetConnection::sendUdp(const char * buf,
                                    unsigned int len,
                                    unsigned int srcIp,
                                    unsigned short srcPort,
                                    char* errbuf)
{
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(srcPort);
    servaddr.sin_addr.s_addr = srcIp;
    sendto(m_connHead.sock_index, buf, len, 0, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
    return 0 ;
}

IwServer::~IwServer()
{
    if (connArray) {
        delete[] connArray;
        connArray = nullptr;
    }
    if (aliveFdArray) {
        delete[] aliveFdArray;
        aliveFdArray = nullptr;
    }
    if (unitArray) {
        delete[] unitArray;
        unitArray = nullptr;
    }
}

bool IwServer::initialize(const CCloudAddrArgu& listen_addr)
{
    connArray = new CCloudNetConnection[fdMaxNum];
    aliveFdArray = new uint32_t[fdMaxNum];
    unitArray = new mybase::CEnqueUnit[unitBatchCount];

    epollrfd = epoll_create(fdMaxNum);
    if (epollrfd < 0) {
        _log_err(myLog, "epoll_create failed! epollrfd[%d] err[%d:%s]",
                 epollrfd, errno,strerror(errno));
        return false;
    }
    setFdTag(epollrfd, FD_CLOEXEC);

    epollwfd = epoll_create(fdMaxNum);
    if (epollwfd < 0) {
        _log_err(myLog, "epoll_create failed! epollwfd[%d] error[%d:%s]",
                 epollwfd, errno,strerror(errno));
        return false;
    }
    setFdTag(epollwfd, FD_CLOEXEC);

    if (0 != pipe(sendPipe)) {
        _log_err(myLog, "pipe failed! error[%d:%s]", errno,strerror(errno));
        return false;
    }

    if (!setFdTag(sendPipe[0], O_NONBLOCK) || !setFdTag(sendPipe[1], O_NONBLOCK)) {
        return false;
    }

    epoll_event ev;
    ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
    ev.data.u64 = (((uint64_t)0)<<32) | sendPipe[0];
    int32_t ret = epoll_ctl(epollwfd, EPOLL_CTL_ADD, sendPipe[0], &ev);
    if (ret < 0) {
        _log_err(myLog, "epoll_ctl add pipe fd[%d] error:[%d:%s]", sendPipe[0], errno, strerror(errno));
        return false;
    }

    handleQueue.init(33554432, true, false);

    {
        int32_t listenfd = openListenFd(listen_addr);
        if (listenfd < 0) return false;

        CCloudNetConnection* conn = getNewConn(listenfd, listen_addr, SocketListen);
        if (!conn) return false;

        epoll_event ev;
        ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
        ev.data.u64 = (((uint64_t)conn->m_connHead.sock_create.tv_usec)<<32) | listenfd;
        int ret = epoll_ctl(epollrfd, EPOLL_CTL_ADD, listenfd, &ev);
        if (ret < 0) {
            _log_err(myLog, "CCloudNetServer::init epoll_ctl add listenfd[%d] error:[%d:%s]",
                     listenfd, errno, strerror(errno));
            return false;
        }
    }

    if ( !initServer() ) {
        _log_err(myLog, "init server failed! host[%s:%d]", listen_addr.m_bind, listen_addr.m_port);
        return false;
    }

    return true;
}

bool IwServer::start()
{
    isRunning = true;
    epollReadThd = std::thread(&IwServer::nioReadThread, this);
    epollWriteThd = std::thread(&IwServer::nioWriteThread, this);

    for (uint32_t idx = 0; idx < processorNum; idx++) {
        workerThds.push_back( std::thread(&IwServer::workerThread, this, idx) );
    }

    if ( !startServer() ) {
        _log_err(myLog, "start server failed!");
        return false;
    }

    return true;
}

void IwServer::wait()
{
    if (epollReadThd.joinable()) {
        epollReadThd.join();
    }
    if (epollWriteThd.joinable()) {
        epollWriteThd.join();
    }
    for (auto& thd : workerThds) {
        if (thd.joinable()) thd.join();
    }
    return ;
}

bool IwServer::stop()
{
    isRunning = false;
    wait();

    if ( !stopServer() ) {
        _log_err(myLog, "stop server failed!");
        return false;
    }

    _log_info(myLog, "stop servers success!");

    return true;
}

int32_t IwServer::openListenFd(const CCloudAddrArgu& addr_arg)
{
    int32_t listenfd = -1;
    struct sockaddr* addr = NULL;
    struct sockaddr_in  servaddr;
    struct sockaddr_un serv_adr;
    socklen_t socklen = 0;

    if (CModelProtocalTcp == addr_arg.m_protocal) {
        listenfd = socket(AF_INET, SOCK_STREAM, 0);
    } else if(CModelProtocalUdp == addr_arg.m_protocal) {
        listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    } else if (CModelProtocalUnix == addr_arg.m_protocal) {
        listenfd = socket(AF_UNIX, SOCK_STREAM, 0);
    } else {
        _log_err(myLog, "unknown protocol :%d", addr_arg.m_protocal);
        return -1;
    }

    if (listenfd < 0) {
        _log_err(myLog, "openserver socket error:[%d:%s]", errno, strerror(errno));
        return listenfd;
    }

    setFdTag(listenfd, FD_CLOEXEC);
    if (false == setFdTag(listenfd, O_NONBLOCK)) {
        _log_err(myLog, "setnonblock listen fd[%d]:[%d:%s]", listenfd, errno, strerror(errno));
        if(listenfd >= 0) { close(listenfd); }
        return -1;
    }

    int32_t reuse_addr_flag = 1;
    int32_t val = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse_addr_flag, sizeof(reuse_addr_flag));
    if(val < 0) {
        _log_err(myLog, "set socket SO_REUSEADDR error [errno:%d,%s]", errno, strerror(errno));
        if(listenfd >= 0) { close(listenfd); }
        return -1;
    }

    if (CModelProtocalUnix != addr_arg.m_protocal) {
        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family      = AF_INET;
        servaddr.sin_addr.s_addr = inet_addr(addr_arg.m_bind);
        servaddr.sin_port        = htons(addr_arg.m_port);
        addr = (struct sockaddr *)&servaddr;
        socklen = sizeof(struct sockaddr_in);
    } else {
        bzero(&serv_adr,sizeof(serv_adr));
        serv_adr.sun_family = AF_UNIX;
        strcpy(serv_adr.sun_path, addr_arg.m_bind);
        addr = (struct  sockaddr *)&serv_adr;
        socklen = sizeof(serv_adr.sun_family)+strlen(serv_adr.sun_path);
    }

    val = bind(listenfd, addr, socklen);
    if(val < 0) {
        _log_err(myLog, "bind[%s,%d] error[%d,%s]", addr_arg.m_bind, addr_arg.m_port, errno, strerror(errno));
        if(listenfd >= 0) { close(listenfd); }
        return -1;
    }

    _log_info(myLog, "bind[%s,%d] success!", addr_arg.m_bind, addr_arg.m_port);

    if (CModelProtocalUdp == addr_arg.m_protocal) {
        return listenfd;
    }

    if(listen(listenfd, 1024) < 0) {
        _log_err(myLog, "listen error[%d,%s]", errno, strerror(errno));
        if(listenfd >= 0) { close(listenfd); }
        return -1;
    }
    _log_info(myLog, "listen fd[%d] success!", listenfd);

    return listenfd;
}

CCloudNetConnection* IwServer::getNewConn(int32_t fd,
                                          const CCloudAddrArgu& arg,
                                          CloudNetSocketType socktype,
                                          uint32_t srcip,
                                          uint16_t srcport)
{
    if (fd < 0 || fd >= (int32_t)connNum) {
        _log_err(myLog, "fd[%d] > %u", fd, connNum);
        return nullptr;
    }

    char fromaddr[64]= {0};
    inet_ntop(AF_INET, &srcip, fromaddr, sizeof(fromaddr));
    CCloudNetConnection& conn = connArray[fd];
    conn.init(bufLen);
    if (false == lockConn(fd)) {
        return nullptr;
    }

    if (conn.m_connHead.sock_index != -1) {
        _log_err(myLog, "fd[%d] != -1", conn.m_connHead.sock_index);
        conn.release();
        return nullptr;
    }
    else {
        conn.m_connHead.sock_index = fd;
        gettimeofday(&(conn.m_connHead.sock_create), nullptr);
        conn.m_connHead.src_ip = srcip;
        conn.m_connHead.src_port = ntohs(srcport);
        conn.m_readBuf.release();
        conn.m_writeBuf.release();
        conn.m_addr = arg;
        conn.m_socketType = socktype;
        snprintf(conn.m_addr.m_connip, sizeof(conn.m_addr.m_connip), "%s", fromaddr);
        conn.m_addr.m_connport = ntohs(srcport);

        if(fd > (int32_t)maxFd) maxFd = (uint32_t)fd;
    }

    unLockConn(fd);
    return &conn;
}

CCloudNetConnection* IwServer::getExistConn(int32_t fd)
{
    if (fd < 0 || fd >= (int32_t)connNum) {
        _log_err(myLog, "fd[%d] >%u", fd, connNum);
        return nullptr;
    }

    CCloudNetConnection* conn = &connArray[fd];
    if (conn->m_connHead.sock_index != fd) {
        _log_err(myLog, "conn fd[%d] != fd[%d]", conn->m_connHead.sock_index, fd);
        conn = nullptr;
    }
    return conn;
}

void IwServer::nioReadThread()
{
    ThreadHelper::setThreadName("nio_read");
    struct epoll_event* events = new struct epoll_event[fdMaxNum];

    struct timeval curtimeval = {0};
    CCloudNetConnection* conn = nullptr;
    while(isRunning) {
        int32_t nfds = epoll_wait(epollrfd, events, fdMaxNum, 10);
        gettimeofday(&curtimeval, nullptr);
        for (int32_t n = 0 ; n < nfds; ++n) {
            int32_t fd = events[n].data.u64 & 0xffffffff;
            if (fd < 0 || fd >= (int32_t)connNum) {
                _log_err(myLog, "epoll_wait get fd[%d]<0 || >=%d, will close", fd, connNum);
                close(fd);
                continue;
            }

            conn = getExistConn(fd);
            if (nullptr == conn) {
                _log_err(myLog, "epoll_wait get fd[%d] not in connectArray, will close,shouldn't enter here,fatal eror, nfds:%d, curno:%d",
                         fd, nfds, n);
                int32_t eret = epoll_ctl(epollrfd, EPOLL_CTL_DEL, fd, nullptr);
                if (eret < 0) {
                    _log_err(myLog, "epoll_ctl m_epollrfd EPOLL_CTL_DEL fd[%d] error[%d:%s]", fd, errno, strerror(errno));
                }
                close(fd);
                continue;
            }

            switch(conn->m_socketType)
            {
                case SocketListen:
                {
                    if (CModelProtocalUdp == conn->m_addr.m_protocal) {
                        // do nothing
                    } else {
                        while(onAccpet(conn)) {

                        }
                    }
                    break;
                }
                case SocketServerConn:
                {
                    aliveFdArray[fd] = 0;
                    if (false == dealEpollIn(conn, &curtimeval)) {
                        char conninfo[1024];
                        conn->tostring(conninfo);
                        // _log_info(myLog, "dealEpollIn failed! conninfo:%s", conninfo);

                        epoll_ctl(epollrfd, EPOLL_CTL_DEL, fd, nullptr);
                        epoll_ctl(epollwfd, EPOLL_CTL_DEL, fd, nullptr);
                        releaseConn(fd);
                    }
                    break;
                }
                default:
                {
                    _log_err(myLog, "epoll_wait get fd[%d] ,socket type[%d] not support", fd, conn->m_socketType);
                    break;
                }
            }
        }

        checkFdTimeOut(curtimeval.tv_sec);
    }

    _log_info(myLog, "nio_read thread stop success!");
}

void IwServer::nioWriteThread()
{
    ThreadHelper::setThreadName("nio_write");
    struct epoll_event* events = new struct epoll_event[fdMaxNum];

    CCloudNetConnection* conn = nullptr;
    while (isRunning) {
        int32_t nfds = epoll_wait(epollwfd, events, fdMaxNum, 10);
        for (int32_t n = 0; n < nfds; ++n) {
            int32_t fd = events[n].data.u64 & 0xffffffff;

            if (fd < 0 || fd >= (int32_t)connNum) {
                _log_err(myLog, "epoll_wait get fd[%d]<0 || >=%d,will close", fd, connNum);
                int eret = epoll_ctl(epollwfd, EPOLL_CTL_DEL, fd, nullptr);
                if (eret < 0) {
                    _log_err(myLog, "epoll_ctl m_epollwfd EPOLL_CTL_DEL fd[%d] error[%d:%s]", fd, errno, strerror(errno));
                }

                close(fd);
                continue;
            }

            if (false == lockConn(fd)) {
                _log_err(myLog, "epoll for write[%d] ,lockConn fail", fd);
                continue;
            }

            conn = getExistConn(fd);
            if (NULL == conn) {
                unLockConn(fd);
                _log_err(myLog, "epoll for write[%d] , getExistConn fail", fd);
                delWrEpoll(fd);
                continue;
            }

            switch (conn->m_socketType) {
                case SocketServerConn:
                {
                    if (false == dealEpollOut(conn)) {
                        char conninfo[1024];
                        conn->tostring(conninfo);
                        _log_err(myLog, "dealEpollOut failed! conninfo:%s", conninfo);
                        conn->shutdown();
                    }
                    break;
                }
                default:
                {
                    char conninfo[1024];
                    conn->tostring(conninfo);
                    _log_err(myLog, "epoll_wait get fd[%d] ,socket type[%d] not support", fd, conn->m_socketType);
                    break;
                }
            }

            unLockConn(fd);
        }
    }

    _log_info(myLog, "nio_write thread stop success!");
}

void IwServer::workerThread(int32_t idx)
{
    ThreadHelper::setThreadName("worker(" + std::to_string(idx) + ")");

    CLockBufferQueue* queue = &handleQueue;
    CloudConnHead head;
    uint32_t headLen = sizeof(head);
    uint32_t rspLen = MaxReqBufLen;
    char rspBuf[MaxReqBufLen + 4];

    while(isRunning) {
        uint64_t timeout_msec = 2000;
        headLen = sizeof(head);
        rspLen = MaxReqBufLen;
        if (queue->dequeue((char*)&head, headLen, rspBuf, rspLen, timeout_msec)) {
            if (headLen == sizeof(head)) {
                rspBuf[rspLen] = 0 ;
                process(head, rspBuf, rspLen);
            }
        }
    }

    _log_info(myLog, "worker(%d) thread stop success!", idx);
}

void IwServer::sendRecvToQueue(mybase::CEnqueUnit* unitArray, int32_t unitNum) {
    int32_t haveSendNum = 0;
    if( haveSendNum >= unitNum) {
        return;
    }

    uint32_t retry_count = 1;
    do {
        try {
            mybase::CEnqueUnit* beginUnit = unitArray + haveSendNum;
            haveSendNum += handleQueue.enqueue(beginUnit, (unitNum - haveSendNum));

            if(haveSendNum >= unitNum) {
                return;
            }
        } catch(mybase::buffer_full& e) {
            _log_warn(myLog, "buffer queue is full");
        }
        
        if ( (retry_count++ % 10) == 0) break;
    } while(true);

    return ;
}

bool IwServer::dealEpollIn(CCloudNetConnection* conn, struct timeval* curtimeval)
{
    char errbuf[1024];
    if (false == conn->read(errbuf)) {
        _log_warn(myLog, "err: %s, peer[%s:%d]", errbuf, conn->m_addr.m_connip, conn->m_addr.m_connport);
        return false;
    }

    int32_t readNum = 0;
    int32_t minLen = 3*sizeof(int32_t);

    CloudConnHead head;
    bool headInit = false;
    int32_t packetRealLen = 0;
    int32_t unitNum = 0;

    NBuffer& conBuf = conn->m_readBuf;
    while(readNum + minLen <= conBuf.getIndex()) {
        Buffer tmpbuf((uint8_t*)conBuf.getBuff() + readNum, minLen);
        packetRealLen = tmpbuf.readInt32();
        if (packetRealLen < minLen) {
            _log_err(myLog, "dealEpollIn format not correct1[%d < %d]", packetRealLen, minLen);
            return false;
        }
        if (packetRealLen >= (int32_t)((int32_t)1024000 - 64) ) {
            _log_err(myLog, "dealEpollIn format not correct2[%d >= %d]", packetRealLen, 1024000-64);
            return false;
        }

        if ( (readNum + packetRealLen) > conBuf.getIndex() ) {
            break;
        }

        uint32_t tmp = readNum;
        readNum += packetRealLen;

        if(!headInit) {
            memcpy(&head, &(conn->m_connHead), sizeof(head));
            memcpy(&(head.request), curtimeval, sizeof(head.request));
            headInit = true;
        }

        mybase::CEnqueUnit* unit = unitArray + unitNum;
        unit->buffer1 = (const char *)&head;
        unit->len1 = sizeof(head);
        unit->buffer2 = conBuf.getBuff() + tmp;
        unit->len2 = packetRealLen;

        ++unitNum;
        if(unitNum >= unitBatchCount) {
            sendRecvToQueue(unitArray, unitNum);
            unitNum = 0;
        }
    }

    if(unitNum > 0) {
        sendRecvToQueue(unitArray, unitNum);
    }

    conBuf.addIndex(-1*readNum);
    if (readNum > 0 && conBuf.getIndex() > 0) {
        memmove(conBuf.getBuff(), conBuf.getBuff() + readNum, conBuf.getIndex());
    }

    return true;
}

void IwServer::delWrEpoll(int32_t fd)
{
    struct epoll_event ev;
    ev.events = 0; 
    ev.data.u64 = 0;
    int32_t eret = epoll_ctl(epollwfd, EPOLL_CTL_DEL, fd, &ev);
    if (eret < 0) {
        _log_err(myLog, "epoll_ctl m_epollwfd EPOLL_CTL_DEL fd[%d] error[%d:%s]", fd, errno, strerror(errno));
    }
    return ;
}

bool IwServer::dealEpollOut(CCloudNetConnection* conn)
{
    NBuffer& conBuf = conn->m_writeBuf;
    if (conBuf.getIndex() > 0) {
        int32_t ret = ::send(conn->m_connHead.sock_index, conBuf.getBuff(), conBuf.getIndex(), MSG_DONTWAIT);
        if (ret < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                return true;
            } else {
                _log_err(myLog, "dealEpollOut send<0 because[%d:%s]", errno, strerror(errno));
                return false;
            }
        } else {
            if (ret < conBuf.getIndex()) {
                conBuf.shrinkFromFront(ret);
                return true;
            } else {
                conBuf.setIndex(0);
                delWrEpoll(conn->m_connHead.sock_index);
                return true;
            }
        }
    }

    return true;
}

bool IwServer::onAccpet(CCloudNetConnection* conn)
{
    struct  sockaddr *   addr = nullptr;
    struct  sockaddr_in  inaddr;
    struct  sockaddr_un  unaddr;
    socklen_t socklen = 0;

    unsigned int defaultip = 0;
    unsigned short defalutport = 0;

    unsigned int* srcip = &defaultip;
    unsigned short* srcport = &defalutport;
    if (CModelProtocalTcp == conn->m_addr.m_protocal) {
        bzero(&inaddr,sizeof(inaddr));
        addr = (struct  sockaddr *)&inaddr;
        socklen = sizeof(struct sockaddr_in);
        srcip = &(inaddr.sin_addr.s_addr);
        srcport = &(inaddr.sin_port);
    }
    else if (CModelProtocalUnix == conn->m_addr.m_protocal) {
        bzero(&unaddr,sizeof(unaddr));
        addr = (struct  sockaddr *)&unaddr;
        socklen = sizeof(struct sockaddr_un);
    } else {
        _log_err(myLog, "unknown protocol :%d ", conn->m_addr.m_protocal);
        return false;
    }

    int32_t newfd = accept(conn->m_connHead.sock_index,addr,&socklen);
    if (newfd < 0) {
        // _log_warn(myLog, "accept[%d] error[%d:%s]",conn->m_connHead.sock_index,errno,strerror(errno));
        return false;
    }

    if (false == setFdTag(newfd, O_NONBLOCK)) {
        _log_err(myLog, "setnonblock[%d] error[%d:%s]", newfd, errno,strerror(errno));
        close(newfd);
        return false;
    }
    if(false == setFdTag(newfd, FD_CLOEXEC)) {
        _log_err(myLog, "setfdtag[%d] FD_CLOEXEC error[%d:%s]", newfd, errno, strerror(errno));
        close(newfd);
        return false;
    }

    int32_t nodelay=1;
    setsockopt(newfd, SOL_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    CCloudNetConnection* newconn = getNewConn(newfd, conn->m_addr, SocketServerConn, *srcip, *srcport);
    if (nullptr == newconn) {
        _log_err(myLog, "getNewConn[%d] error[%s]",newfd);
        close(newfd);
        return false;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
    ev.data.u64 = (((uint64_t)newconn->m_connHead.sock_create.tv_usec)<<32) | newfd;
    int iret = epoll_ctl(epollrfd, EPOLL_CTL_ADD, newfd, &ev);
    if (iret < 0) {
        releaseConn(newfd);
        _log_err(myLog, "epoll_ctl error[%d:%s] close fd[%d]",errno, strerror(errno), newfd);
        return false;
    }

    _log_info(myLog, "get connection[%s,%d] fd[%d]", newconn->m_addr.m_connip,newconn->m_addr.m_connport, newfd);
    return true;
}

bool IwServer::sendResponse(const CloudConnHead& head, const char* buffer, uint32_t len)
{
    return sendResponse(head.sock_index, head.sock_create.tv_sec, buffer, len);

    // CCloudNetConnection* conn = nullptr;
    // if (false == lockConn(head.sock_index)) {
    //     _log_err(myLog, "get msg from sendqueue ,lockConn fail!");
    //     return false;
    // }

    // conn = getExistConn(head.sock_index);
    // if (nullptr == conn) {
    //     unLockConn(head.sock_index);
    //     _log_err(myLog, "get msg from sendqueue ,getExistConn failed! fd[%d]", head.sock_index);
    //     return false;
    // }
    // if(memcmp(&(conn->m_connHead.sock_create), &(head.sock_create), sizeof(head.sock_create)) != 0) {
    //     char connbuf[1024];
    //     conn->tostring(connbuf);
    //     unLockConn(head.sock_index);
    //     _log_err(myLog, "get msg from sendqueue ,getExistConn ok,but create time not match,may have close,now conn[%s]", connbuf);
    //     return false;
    // }

    // char errbuf[1024];
    // int32_t sendret = conn->send(buffer, len, epollwfd, errbuf);
    // if (0 == sendret) {
    //     //do nothing
    // } else if (1 == sendret) {
    //     char connbuf[1024];
    //     conn->tostring(connbuf);
    //     _log_err(myLog, "get msg from sendqueue ,getExistConn ok,but buf is full,now conn[%s]", connbuf);
    // } else {
    //     char connbuf[1024];
    //     conn->tostring(connbuf);
    //     _log_err(myLog, "get msg from sendqueue ,getExistConn ok,but send fail:%s,now conn[%s] need shutdown(RD)", errbuf, connbuf);

    //     conn->shutdown();   // read thread will catch the exception
    // }

    // unLockConn(head.sock_index);
    // return true;
}

bool IwServer::sendResponse(uint32_t sock_index, uint32_t sock_create, const char* buffer, uint32_t len)
{
    CCloudNetConnection* conn = nullptr;
    if (false == lockConn(sock_index)) {
        _log_err(myLog, "get msg from sendqueue ,lockConn fail!");
        return false;
    }

    conn = getExistConn(sock_index);
    if (nullptr == conn) {
        unLockConn(sock_index);
        _log_err(myLog, "get msg from sendqueue ,getExistConn failed! fd[%d]", sock_index);
        return false;
    }
    // if(memcmp(&(conn->m_connHead.sock_create), &(sock_create), sizeof(sock_create)) != 0) {
    if ( conn->m_connHead.sock_create.tv_sec != sock_create) {
        char connbuf[1024];
        conn->tostring(connbuf);
        unLockConn(sock_index);
        _log_err(myLog, "get msg from sendqueue ,getExistConn ok,but create time not match,may have close,now conn[%s]", connbuf);
        return false;
    }

    char errbuf[1024];
    int32_t sendret = conn->send(buffer, len, epollwfd, errbuf);
    if (0 == sendret) {
        //do nothing
    } else if (1 == sendret) {
        char connbuf[1024];
        conn->tostring(connbuf);
        _log_err(myLog, "get msg from sendqueue ,getExistConn ok,but buf is full,now conn[%s]", connbuf);
    } else {
        char connbuf[1024];
        conn->tostring(connbuf);
        _log_err(myLog, "get msg from sendqueue ,getExistConn ok,but send fail:%s,now conn[%s] need shutdown(RD)", errbuf, connbuf);

        conn->shutdown();   // read thread will catch the exception
    }

    unLockConn(sock_index);
    return true;
}

bool IwServer::releaseConn(int32_t fd)
{
    if (fd >= (int32_t)connNum) {
        _log_err(myLog, "fd[%d] >%u", fd, connNum);
        return false;
    }

    CCloudNetConnection* conn = &connArray[fd];
    if (false == lockConn(fd)) {
        return false;
    }

    conn->release();
    unLockConn(fd);
    aliveFdArray[fd] = 0;
    return true;
}

bool IwServer::setFdTag(int32_t fd, int32_t tag)
{
    int32_t val = -1;
    if ((val = fcntl(fd, F_GETFL, 0)) == -1) {
        _log_err(myLog, "fcntl F_GETFL error:[%d:%s]", errno, strerror(errno));
        return false;
    }
    if (fcntl(fd, F_SETFL, val | tag) == -1) {
        _log_err(myLog, "fcntl F_SETFL error:[%d:%s]", errno, strerror(errno));
        return false;
    }

    return true;
}

bool IwServer::lockConn(int32_t fd) {
    if (fd < 0 || fd >= (int32_t)connNum) {
        _log_err(myLog, "fd[%d] >%u", fd, connNum);
        return false;
    }

    CCloudNetConnection* conn = &connArray[fd];
    conn->m_lock.lock();
    return true;
}

void IwServer::unLockConn(int fd) {
    CCloudNetConnection* conn = &connArray[fd];
    conn->m_lock.unlock();
}

void IwServer::checkFdTimeOut(time_t curtime)
{
    if(abs((int64_t)curtime - (int64_t)lastCheckTimeout) < (int64_t)keepAliveTime) {
        return;
    }

    lastCheckTimeout = curtime;
    _log_debug(myLog, "begin check timeout");

    int32_t closenum = 0;
    for (int32_t fd = 0; fd <= (int32_t)maxFd; ++fd) {
        if (aliveFdArray[fd] != 0 && SocketServerConn == connArray[fd].m_socketType) {
            epoll_ctl(epollrfd, EPOLL_CTL_DEL, fd, nullptr);
            epoll_ctl(epollwfd, EPOLL_CTL_DEL, fd, nullptr);

            CCloudNetConnection* conn = getExistConn(fd);
            if (conn) {
                char conninfo[1024];
                conn->tostring(conninfo);
                _log_info(myLog, "checkTimeout,need close,conninfo:%s", conninfo);
                releaseConn(fd);
                closenum++;
            } else {
                _log_err(myLog, "checkTimeout,need close,but can't find conninfo,so force to close fd[%d]", fd);
                close(fd);
            }
        }

        aliveFdArray[fd] = 0;
        if(SocketServerConn == connArray[fd].m_socketType) {
            aliveFdArray[fd] = 1;
        }
    }
    _log_debug(myLog, "end check timeout,maxfd :%d,closenum:%d", maxFd, closenum);
}

}
