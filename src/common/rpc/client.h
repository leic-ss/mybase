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

#include <map>
#include <atomic>
#include <unordered_map>

#include <stdio.h>
#include <event2/event.h>
#include <event2/dns.h>

namespace rpc
{
// client context
class ClientCtx : public ContextX
{
public:
    using ptr = std::shared_ptr<ClientCtx>;

public:
    std::shared_ptr<mybase::Buffer>        req;
    std::shared_ptr<mybase::Buffer>        rsp;
};

class Client : virtual public Session
{
public:
    using ptr = std::shared_ptr<Client>;

private:
    class Async : public std::enable_shared_from_this<Async>
    {
    public:
        using ptr = std::shared_ptr<Async>;

    public:
        std::string err;
      
    private:
        // thread where context dispatched from
        // we should dispatch to this thread while invoking
        CThread*                thr{nullptr};

    private:
        // client
        Client::ptr       c;
        // clisnt context
        ContextX::ptr     ctx;
        // client context callback
        ContextX::cb      cb;
        // timeout
        timeval           tv;
        // evtimer
        struct event*     e{nullptr};

        mybase::BaseLogger*       myLog{nullptr};

    private:
        friend Client;

    public:
        Async(Client::ptr, ContextX::ptr, ContextX::cb, uint32_t, CThread* thr_=nullptr, mybase::BaseLogger* l=nullptr);
        ~Async(void);

    public:
        void invoke(void);
        void invokeClose(void);
        void invokeFailed(void);

    public:
        template <class T> inline
        std::shared_ptr<T> convert(void) {
            return std::dynamic_pointer_cast<T>(ctx);
        }

    private:
        void timeout(void);
      
    private:
        void evdel(void);
    };

protected:
    // connection status
    enum Status {
        CLOSED,
        CONNECTED,
        CONNECTING,
    };

public:
    uint32_t        timeouts;

protected:
    // TODO: thread safe
    std::unordered_map<uint32_t, Async::ptr> ctxs;

protected:
    Status status;

private:
    struct event*   e{nullptr};
    // struct event*   chkEvt{nullptr};

protected:
    uint64_t        sendNum{0};
    uint64_t        readNum{0};
    uint32_t        chkInterval{10};

    double          ratio{0.7};
    std::atomic<uint32_t> nextSequence{0};

private:
    evdns_base*                                             ev_dns{nullptr};

public:
    Client(const std::string&, const int &);

public:
    virtual ~Client(void);

public:
    void request(ContextX::ptr ctx, ContextX::cb, uint32_t timeout_ms);
    void request(ContextX::ptr, uint32_t timeout_ms=0);

public:
    bool send(ContextX::ptr);

public:
    Client::Async::ptr erase(uint32_t);

public:
    inline int isConnected(void) { return status == CONNECTED; }

    void close(bool sync=true);

protected:
    virtual int64_t recv(uint8_t *, uint64_t);

protected:
    void event(short which);

protected:
    void connect(void);
    // void invoke();

protected:
    void check();
    void heartbeat();

protected:
    void close(CLOSE_BY);

private:
    void socket_setup(void);

protected:
    std::string format(void) const;

public:
    friend std::ostream & operator << (std::ostream &, const Client &);
};

}
