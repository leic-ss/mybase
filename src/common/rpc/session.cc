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

#include "session.h"

#include <event2/buffer.h>
#include <cassert>

namespace rpc {
  
Session::Session(mybase::BaseLogger* l)
: fd       (0)
, myLog    (l) {}

void Session::attachNio(CThread * thr_) {
    // assign
    nioThr = thr_;
    // dispatch to it's own threads to connect
    nioThr->dispatch([this](CThread *) {
        // init
        connect();
    });
}

bool Session::attachWk(CThread* thr_)
{
    for (CThread* _thr : wkthrArr) {
        if (thr_->name == _thr->name) return false;
        if (thr_ == _thr) return false;
    }

    wkthrArr.push_back(thr_);
    return true;
}

bool Session::send(const std::string& buf)
{
    assert(nioThr == CThread::self());

    if (!bev) {
        _log_err(myLog, "bev is nullptr!");
        return false;
    }

    bufferevent_write(bev, buf.data(), buf.size());
    return true;
}

bool Session::send(const uint8_t* buf, uint32_t size)
{
    assert(nioThr == CThread::self());

    if (!bev) {
        _log_err(myLog, "bev is nullptr!");
        return false;
    }

    bufferevent_write(bev, buf, size);
    return true;
}

void Session::recv(evbuffer * evbuf)
{
    if (!bev) {
        _log_err(myLog, "bev is nullptr!");
        return ;
    }

    // first
    uint8_t * buf =       evbuffer_pullup    (evbuf, -1);
    uint8_t * eof = buf + evbuffer_get_length(evbuf);
    // debug
    // recrusive parse
    while (buf < eof) {
        // do parsing
        int64_t parsed = recv(buf, eof - buf);
        // unreceived data existed
        if (parsed == 0) break;
        // parse error
        if (parsed < 0) return close(CLOSE_BY::NATIVE_SIDE);
        // move on
        buf += parsed;
        // drain parsed buffer
        evbuffer_drain(evbuf, parsed);
    }
}

void Session::event(short which)
{
    // _log_info(myLog, "event: %d", which);
    err.clear();
    if (which == (BEV_EVENT_EOF | BEV_EVENT_READING)) {
        if (errno == EAGAIN) {
          /*
           * libevent will sometimes recv again when it's not actually ready,
           * this results in a 0 return value, and errno will be set to EAGAIN
           * (try again). This does not mean there is a hard socket error, but
           * simply needs to be read again.
           *
           * but libevent will disable the read side of the bufferevent
           * anyway, so we must re-enable it.
           */
          bufferevent_enable(bev, EV_READ);
          // reset errno
          return (void)(errno = 0);
        }
    }
    // error
    if (which & BEV_EVENT_EOF) {
        // append info message
        err = "remote closed";
        // error
    } else if (which & BEV_EVENT_ERROR) {
        // // append strerror
        // err.strerror();
        // // otherwise
    } else {
        err = "reach event ";
        err.append(std::to_string(which));
    }
    // close
    close(CLOSE_BY::REMOTE_SIDE);
}

void Session::connect(void)
{
    // free before init
    if (bev) bufferevent_free(bev);
    // init bev
    bev = bufferevent_socket_new(nioThr->eb, fd, BEV_OPT_CLOSE_ON_FREE);

    // TODO: multiple threads support 2
    // bev = bufferevent_socket_new(nioThr->eb, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);

    // set callback
    bufferevent_setcb(bev,
                      // read callback
                      [](bufferevent * bev, void * ptr) {
                          // convert
                          Session * c = (Session *)ptr;
                          // read
                          c->recv(bufferevent_get_input(bev));
                          // done
                      } , /* empty write cb */ nullptr,
                      [](bufferevent * bev, short which, void * ptr) {
                          // convert
                          Session * c = (Session *)ptr;
                          // event
                          c->event(which);
                          // done
                      }, this);
}

void Session::close(CLOSE_BY close_by)
{
    // _log_info(myLog, "tcp: %s close by %s, err: %s",
    //           format().c_str(), (close_by == CLOSE_BY::NATIVE_SIDE) ? "native" : "remote", err.c_str());

    if (bev) bufferevent_free(bev);
    // set zero
    bev = nullptr;
}

}
