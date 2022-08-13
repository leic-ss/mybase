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

#include "cthread.h"
#include "public/cclogger.h"

#include <stdio.h>
#include <event2/bufferevent.h>

namespace rpc
{

// client context
class ContextX : public std::enable_shared_from_this<ContextX>
{
public:
    using ptr = std::shared_ptr<ContextX>;

public:
    using cb = std::function<void (ContextX::ptr)>;

public:
    ContextX() : status(Status::OK) {}
    virtual ~ContextX(void) {}

public:
    template <class T> inline
    std::shared_ptr<T> convert(void) {
        return std::dynamic_pointer_cast<T>(shared_from_this());
    }

public:
    enum class Status : uint8_t {
        OK = 0,
        TIMEOUT,
        FAILED,
        CLOSED
    };

public:
    uint64_t reqUsec{0};
    uint64_t resUsec{0};
    uint32_t sequence{0};
    Status status{Status::OK};
    std::string err;
};

// session
class Session : public std::enable_shared_from_this<Session>
{
public:
    using ptr = std::shared_ptr<Session>;

protected:
    enum class CLOSE_BY : uint8_t {
        REMOTE_SIDE = 1,
        NATIVE_SIDE
    };

public:
    std::string err;
  
public:
    CThread* nioThr{nullptr};
    std::vector<CThread*> wkthrArr;

protected:
    int32_t fd;

protected:
    std::string     host;
    int32_t         port;

protected:
    bufferevent* bev{nullptr};

public:
    Session(mybase::BaseLogger* l=nullptr);
    virtual ~Session() {}

public:
    template <class T> inline
    std::shared_ptr<T> convert(void) {
        return std::dynamic_pointer_cast<T>(shared_from_this());
    }

public:
    virtual void attachNio(CThread *);
    virtual bool attachWk(CThread *);

    virtual void close(bool) { }

    void setLogger(mybase::BaseLogger* l) { myLog = l; }
    void setHost(const char* host_) { host = host_; }
    void setPort(const uint16_t port_) { port = port_; }

public:
    virtual void close(CLOSE_BY);

protected:
    virtual int64_t recv(uint8_t *, uint64_t) = 0;

protected:
    virtual void recv(evbuffer *);
  
protected:
    virtual bool send(const std::string&);
    virtual bool send(const uint8_t* buf, uint32_t size);

protected:
    virtual void event(short which);

protected:
    virtual void connect(void);

protected:
    virtual std::string format(void) const = 0;

protected:
    mybase::BaseLogger* myLog{nullptr};
};

}
