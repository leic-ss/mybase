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

#include "server.h"
#include "public/cclogger.h"

#include <cassert>
#include <sstream>
#include <string>

#include <cassert>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <netinet/tcp.h>

namespace rpc {

Server::Server(int32_t fd_)
{
    fd = fd_;
    // addr
    struct sockaddr_storage addr;
    // addrlen
    socklen_t addr_len(sizeof(addr));
    // ip
    char ip_[INET6_ADDRSTRLEN];
    int  port_;
    // fetch
    getpeername(fd, (struct sockaddr*)&addr, &addr_len);
    // deal with both IPv4 and IPv6
    if (addr.ss_family == AF_INET) {
        // convert
        auto s = (struct sockaddr_in *)&addr;
        // parse port
        port_ = ntohs(s->sin_port);
        // parse ip
        inet_ntop(AF_INET, &s->sin_addr, ip_, sizeof(ip_));
        // otherwise AF INET6
    } else {
        // convert
        auto s = (struct sockaddr_in6 *)&addr;
        // parse port
        port_ = ntohs(s->sin6_port);
        // parse ip
        inet_ntop(AF_INET6, &s->sin6_addr, ip_, sizeof(ip_));
    }

    // format
    // std::stringstream ss;
    // ss << ip_ << ":" << port_;
    // peer = ss.str();

    host = ip_;
    port = port_;
}

int64_t Server::recv(uint8_t* evbuf, uint64_t evlen)
{
    uint64_t len(sizeof(uint32_t));
    // check
    if (evlen < len) return 0;

    mybase::Buffer buf(evbuf, 4);
    uint32_t size = buf.readInt32();
    // length
    len += size;
    // check
    if (evlen < len) return 0;

    mybase::Buffer buf2(evbuf, len);
    buf2.readInt32();
    uint32_t seq = buf2.readInt32();
    std::string str;
    buf2.readString(str);

    _log_info(myLog, "[%s:%d] recv seq[%u] str[%.*s]", host.c_str(), port, seq, str.size(), (char*)str.data());

    auto buf3 = mybase::Buffer::alloc(100);

    buf3->writeInt32(4 + 4 + str.size());
    buf3->writeInt32(seq);
    buf3->writeString(str);

    Session::send((uint8_t*)buf3->data(), buf3->dataLen());
    return len;
}

void Server::connect(void)
{
    self_ = new std::shared_ptr<Server>(convert<Server>());
    _log_info(myLog, "server connect!");

    // real connect
    Session::connect();
    // enable read
    bufferevent_enable(bev, EV_READ);
}

void Server::close(CLOSE_BY close_by)
{
    // close
    Session::close(close_by);
    _log_info(myLog, "delete server, close_by[%d] peer[%s:%d]", close_by, host.c_str(), port);

    // delete
    delete self_;
}

std::string Server::format(void) const
{
    return std::string();
}

bool RpcServer::initialize(uint16_t port)
{
    struct sockaddr_in sin;
    // init
    sin.sin_family      = AF_INET;
    sin.sin_addr.s_addr = inet_addr("0.0.0.0");
    // assign port
    sin.sin_port = htons(port);

    auto func = [port, this, &sin](rpc::CThread * thr) {
        // bind
        evconnlistener* tcp = evconnlistener_new_bind(thr->eb,
                                         RpcServer::tcpAcceptCb,
                                         this,
                                         LEV_OPT_CLOSE_ON_FREE,
                                         2048, (sockaddr *)&sin, sizeof(sin));
        if (!tcp) {
            fprintf(stderr, "bind failed! port[%d] err[%s]\n", port, strerror(errno));
            exit(1);
        }
        // get fd
        int fd = evconnlistener_get_fd(tcp);
        // set tcp_nodelay
        int32_t on = 1;
        // set nodelay
        int32_t val = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&on, sizeof(on));

        int32_t reuse = 1;
        val = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
        (void)val;
    };

    thr = new rpc::CThread("thread 1", func, myLog);
    if (!thr) return false;

    return true;
}

void RpcServer::tcpAcceptCb(evconnlistener  * listen,
                            evutil_socket_t   fd,
                            sockaddr        * addr,
                            int               socklen,
                            void            * ptr)
{
    RpcServer* server = (RpcServer*)ptr;
    server->thr->dispatch([fd, server](CThread * thr) {
        // alloc
        Session::ptr s(std::make_shared<rpc::Server>(fd));
        // set logger
        s->setLogger(server->myLog);
        // attach
        s->attachNio(thr);
        s->attachWk(thr);
    });
}

RpcServer::~RpcServer()
{
    // stop first
    thr->stop();

    if(thr) {
        delete thr;
        thr = nullptr;
    }
}

}
