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

#pragma once

#include "session.h"
#include "cthread.h"
#include "public/buffers.h"

#include <string>

#include <event2/listener.h>
#include <stdio.h>

namespace rpc {

class Server : virtual public Session
{
public:
    using ptr = std::shared_ptr<Server>;
    
private:
    Server::ptr* self_{nullptr};

public:
    Server(int32_t);
    ~Server() { }

protected:
    virtual void connect(void);

protected:
    virtual int64_t recv(uint8_t*, uint64_t);

protected:
    virtual void close(CLOSE_BY);

private:
    std::string host;
    uint16_t port;

protected:
    std::string format(void) const;
};

class RpcServer
{
public:
    RpcServer() { }
    ~RpcServer();

    bool initialize(uint16_t port);

public:
    void setLogger(mybase::BaseLogger* logger) { myLog = logger; }

public:
    static void tcpAcceptCb(evconnlistener  * listen,
                            evutil_socket_t   fd,
                            sockaddr        * addr,
                            int               socklen,
                            void            * ptr);

private:
    CThread* thr{nullptr};
    mybase::BaseLogger* myLog{nullptr};
};

}
