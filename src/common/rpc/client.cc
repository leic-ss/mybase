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

#include "client.h"
#include "public/awaiter.h"
#include "public/common.h"

#include <cassert>
#include <condition_variable>
#include <sstream>

#include <netinet/tcp.h>

namespace rpc
{

Client::Async::Async(Client::ptr            c_,
                     ContextX::ptr          ctx_,
                     ContextX::cb           cb_,
                     uint32_t               timeout_ms,
                     CThread*               thr_,
                     mybase::BaseLogger*    l)
: thr    (thr_)
, c      (c_)
, ctx    (ctx_)
, cb     (cb_)
, tv({
  timeout_ms / 1000,
# ifdef __APPLE__
  (int)
# endif
  timeout_ms % 1000 * 1000
})
, myLog(l)
{
    if ( !thr ) thr = CThread::self();

    // assign timeout timer
    e = evtimer_new(c->nioThr->eb, [](int, short, void* clptr) {
      // convert
      auto timer = (Client::Async *)clptr;
      // timeout
      timer->timeout();
      // done
    }, this);
}

Client::Async::~Async(void) {
    // delete event
    // _log_warn(myLog, "Async delete!");
    evdel();
}

void Client::Async::timeout(void)
{
    c->timeouts++;
    // error message
    // err.elapse(tv).prepend("reach timeout: ");
    // setup timeouted
    ctx->status = ContextX::Status::TIMEOUT;
    // hold shared_ptr avoid destruct
    auto ptr(c->erase(ctx->sequence));
    // invoke
    invoke();
    // close
    // if (c->timeouts >= 50) c->close(CLOSE_BY::NATIVE_SIDE);
}

void Client::Async::invokeClose(void)
{
    ctx->status = ContextX::Status::CLOSED;
    invoke();
}

void Client::Async::invokeFailed(void)
{
    ctx->status = ContextX::Status::FAILED;

    // event delete
    evdel();

    auto self = shared_from_this();
    // dispatch
    thr->dispatch([self](CThread *) {
        // self->ctx->resMsec = TimeHelper::currentMs();

        // invoke
        if(self->cb) self->cb(self->ctx);
    });
}

void Client::Async::invoke(void)
{
    if (!e) return;

    // event delete
    evdel();
    // context error
    if (err.length()) {
        // append
        // ctx->err.appendf("tcp.ctx.err: '%s'", err.c_str());
    }
    // client error
    if (c->err.length()) {
        // comma ifneed
        // if (ctx->err.length()) ctx->err.prepend(", ");
        // prepend
        // ctx->err.prependf("tcp.err: '%s'", c->err.c_str());
    }
    // format
    if (ctx->err.length()) {
        // format
        // ctx->err.prependf("tcp: '%s', ", c->format().c_str());
    }

    // self shared ptr
    auto self = shared_from_this();
    // dispatch
    thr->dispatch([this, self] (CThread *) {
        self->cb(self->ctx);
    });
}

void Client::Async::evdel(void)
{
    if (!e) return;
    // delete
    event_free(e);
    // reset
    e = nullptr;
}

Client::Client(const std::string& host_, const int32_t& port_)
: timeouts(0)
, status  (CLOSED)
, e       (nullptr)
, ev_dns  (nullptr) {
    // do trim
    // host.trim();
    Session::setHost(host_.c_str());
    Session::setPort(port_);
}

Client::~Client(void) {
    _log_warn(myLog, "destruct client! tcp: %s", format().c_str());

    // fix issue, pure virtual function called
    if (ev_dns) evdns_base_free(ev_dns, 0);
    ev_dns = nullptr;
    // free bev
    if (bev) bufferevent_free(bev);
    bev = nullptr;
    // free e
    if (e) event_free(e);
    e = nullptr;
    // free chk evt
    // if (chkEvt) event_free(chkEvt);
    // chkEvt = nullptr;
}

Client::Async::ptr Client::erase(uint32_t seq)
{
    Client::Async::ptr ctx = nullptr;
    // find
    auto it = ctxs.find(seq);
    // find out
    if (it != ctxs.end()) {
        // assign
        ctx = it->second;
        // erase
        ctxs.erase(it);
    }
    // return
    return ctx;
}

void Client::socket_setup(void)
{
    // init fd
    fd = socket(AF_INET, SOCK_STREAM, 0);
    // error
    if (fd < 0) {
        // error
        // return (void)SysLogError("socket(AF_INET, SOCK_STREAM, 0) error: '%s'\n", FMT_STRERROR);
    }
    
    /* @see http://www.cnblogs.com/tangr206/articles/3284203.html
     *
     * @todo tcp_nodelay should not be set to client end
     */
     // flag tcp_nodelay
     int on = 1;
     // setup fd
     if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&on, sizeof(on))) {
     // error
     // SysLogError("setsockopt(%d, IPPROTO_TCP, TCP_NODELAY) error: '%s'\n", fd, FMT_STRERROR);
     }
     
    // setup fd
    if (evutil_make_socket_nonblocking(fd)) {
        // error
        // SysLogError("evutil_make_socket_nonblocking(%d) error: '%s'\n", fd, FMT_STRERROR);
    }
}

void Client::connect(void)
{
    // connect while closed
    if (status != CLOSED) return;
    // check thr
    assert(nioThr);
    // socket setup
    socket_setup();
    // set status
    status = CONNECTING;
    // connect
    Session::connect();
    // reset timeout
    timeouts = 0;
    // free before init
    if (ev_dns) evdns_base_free(ev_dns, 0);
    // init evdns
    ev_dns = evdns_base_new(nioThr->eb, 1);

    // if (!chkEvt) {
    //     chkEvt = event_new(nioThr->eb, -1, EV_PERSIST, [](int, short, void * ptr) {
    //         // convert
    //         Client * c = (Client *)ptr;
    //         // tick
    //         c->check();
    //         // done
    //     }, this);
    // }

    // _log_info(myLog, "connect! tcp: %s", format().c_str());
    // connect
    bufferevent_socket_connect_hostname(bev, ev_dns, AF_INET, host.c_str(), port);
}

void Client::check()
{
    uint64_t needNumber = (uint64_t)(ratio * sendNum);

    if (sendNum == 0) return ;

    if (needNumber > readNum) {
        _log_warn(myLog, "client check! sendNum[%lu] needNumber[%lu] readNum[%lu]", sendNum, needNumber, readNum);
        close();
    }

    sendNum=0;
    readNum=0;
    return ;
}

void Client::close(CLOSE_BY close_by)
{
    // _log_info(myLog, "client close! status[%d] close_by[%s] tcp: %s",
    //           status, (close_by == CLOSE_BY::NATIVE_SIDE) ? "native" : "remote", format().c_str());
    if (status == CLOSED) return ;

    // if (chkEvt && (status == CONNECTED)) {
    //     _log_info(myLog, "delete chkTimer! status[%d] close_by[%s] tcp: %s",
    //               status, (close_by == CLOSE_BY::NATIVE_SIDE) ? "native" : "remote", format().c_str());
    //     event_del(chkEvt);
    // }

    Session::close(close_by);

    // mark closed
    status = CLOSED;
    // traverse
    for (auto v : ctxs) v.second->invokeClose();
    // clear
    ctxs.clear();
    // reset err
    err.clear();

    /*
    // connect immediately
    if (close_by == CLOSE_BY::NATIVE_SIDE) return connect();
    // connect defer
    if (!e) {
        // init connect timer
        e = event_new(nioThr->eb, -1, EV_TIMEOUT, [](int, short, void * ptr) {
            // convert
            Client * c = (Client *)ptr;
            // tick
            c->connect();
            // done
        }, this);
    }

    // connect defer
    timeval tv = { 1, 0 };
    // event_add
    event_add(e, &tv);
    */
}

void Client::close(bool sync)
{
    auto self(convert<Client>());
    auto func = [this, self, sync] (CThread *) {
        _log_info(myLog, "%s close! tcp: %s", sync?"Sync":"Async", format().c_str());
        if (status == CLOSED) {
            _log_warn(myLog, "already closed! tcp: %s", format().c_str());
            return ;
        }

        // if (chkEvt && (status == CONNECTED)) {
        //     _log_info(myLog, "delete chkTimer! tcp: %s", format().c_str());
        //     event_del(chkEvt);
        // }

        Session::close(CLOSE_BY::NATIVE_SIDE);
        // mark closed
        status = CLOSED;

        // traverse
        for (auto & v : ctxs) {
            v.second->invokeClose();
        }
        // clear
        ctxs.clear();
        // reset err
        err.clear();
		// free dns
	    if (ev_dns) evdns_base_free(ev_dns, 0);
	    ev_dns = nullptr;
	    // free bev
	    if (bev) bufferevent_free(bev);
	    bev = nullptr;
	    // free e
	    if (e) event_free(e);
	    e = nullptr;
	    // free chk evt
	    // if (chkEvt) event_free(chkEvt);
	    // chkEvt = nullptr;
    };

    if (!sync) {
        nioThr->dispatch(func);
    } else {
        nioThr->sync(func);
    }
}

void Client::request(ContextX::ptr ctx, ContextX::cb cb, uint32_t timeout_ms)
{
    auto self(convert<Client>());

    // convert
    auto async(std::make_shared<Async>(self, ctx, cb, timeout_ms, nioThr, myLog));

    // dispatch to own thread
    nioThr->dispatch([this, ctx, async, timeout_ms](CThread *) {
        // insert
        ctxs.emplace(ctx->sequence, async);
        // timeout timer
        if(timeout_ms > 0) {
            evtimer_add(async->e, &async->tv);
        }
        // connect first
		if (status == CLOSED) connect();

        // do request
        if ( !send(ctx) ) {
            erase(ctx->sequence);
            async->invokeFailed();
        }
    });
}

void Client::request(ContextX::ptr ctx, uint32_t timeout_ms)
{
    auto self(convert<Client>());

    if (ctx->reqUsec == 0) {
        ctx->reqUsec = TimeHelper::currentMs();
    }

    mybase::AWaiter awaiter;
    nioThr->dispatch([self, this, &awaiter, ctx, timeout_ms](CThread *) {
        // request
        request(ctx, [self, this, ctx, &awaiter] (ContextX::ptr) {
            awaiter.invoke();
            // done
        }, timeout_ms);
    });

    awaiter.waitMsec(0);
    return ;
}

bool Client::send(ContextX::ptr ctx)
{
    sendNum++;

    auto buf = ctx->convert<ClientCtx>()->req;
    return Session::send((uint8_t*)buf->data(), buf->dataLen());
}

int64_t Client::recv(uint8_t * evbuf, uint64_t evlen)
{
    // _log_info(myLog, "%.*s", evlen, (char*)evbuf);

    uint32_t len(sizeof(uint32_t));
    // check
    if (evlen < len) return 0;

    mybase::Buffer buf(evbuf, 4);
    uint32_t size = buf.readInt32();
    // length
    len = size;
    // check
    if (evlen < len) return 0;

    mybase::Buffer buf2(evbuf, len);
    buf2.pos(sizeof(uint32_t));
    uint32_t seq = buf2.readInt32();

    auto ptr(erase(seq));
    if (!ptr) return len;

    ClientCtx::ptr ctx = ptr->convert<ClientCtx>();
    ctx->sequence = seq;
    ctx->rsp = mybase::Buffer::alloc(buf2.curDataLen());
    ctx->rsp->writeBytes(buf2.curData(), buf2.curDataLen());

    ptr->invoke();

    // error
    if (err.empty()) {
        readNum++;
        return len;
    }

    // read length
    return -1;
}

void Client::event(short which)
{
    if (which & BEV_EVENT_CONNECTED) {
        // enable read
        bufferevent_enable(bev, EV_READ);
        // set status flag to CONNECTED
        status = CONNECTED;

        //SysLogInfo("tcp: '%s' connected\n", format().c_str());
        _log_info(myLog, "tcp: %s", format().c_str());

        // if (chkEvt) {
        //     // connect defer
        //     timeval tv = { chkInterval, 0 };
        //     // event_add
        //     event_add(chkEvt, &tv);

        //     sendNum = 0;
        //     readNum = 0;

        //     _log_info(myLog, "add chkTimer! tcp: %s chkInterval[%d]", format().c_str(), chkInterval);
        // }

        // record log
        return ;
    }
    // event
    Session::event(which);
}

std::string Client::format(void) const
{
    std::string str;
    // ip + port
    str.append(host).append(" ").append(std::to_string(port)).append(":").append(std::to_string(fd)).append(" ").append(std::to_string(timeouts));
    // judge status
    switch (status)
    {
        case Client::CLOSED:
            str.append(", CLOSED");
            break;
        case Client::CONNECTED:
            str.append(", CONNECTED");
            break;
        case Client::CONNECTING:
            str.append(", CONNECTING");
            break;
        default:
            break;
    }
    // done
    return str.append(", ").append(nioThr->format());
}

std::ostream & operator << (std::ostream & o, const Client & c)
{
    auto str(c.format());
    // find
    auto colon(str.find(':'));
    // format
    return o << str.substr(0, colon) << str.substr(colon);
}

}
