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

#include "cthread.h"
#include "public/awaiter.h"

#include <unistd.h>
#include <sys/syscall.h>
#include <tuple>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <pthread.h>

namespace rpc {

static Attr<CThread>            thread_self;
static std::mutex               thread_mutex;
static std::condition_variable  thread_cond;

CThread::CThread(const std::string& name_, event_base* eb_, mybase::BaseLogger* l)
: eb    (eb_)
, thr   (nullptr)
, name  (name_)
, myLog (l) {
    // assert
    assert(eb);
    // init
    init();
}

CThread::CThread(const std::string& name_, callback start, mybase::BaseLogger* l)
: eb   (event_base_new())
, name (name_)
, myLog (l)
{
    std::unique_lock<std::mutex> lock(thread_mutex);
    // loop
    thr = new std::thread([this, start](void) {
        // TODO: multiple threads support 1
        // evthread_use_pthreads();
        // evthread_make_base_notifiable(pEventBase);

        // lock
        thread_mutex.lock();
        // start
        if (start != nullptr) start(this);
        // init
        init();
        // unlock
        thread_mutex.unlock();
        // notify
        thread_cond.notify_one();
        // start
        _log_info(myLog, "start thread %s", name.c_str());
        // loop
        event_base_loop(eb, 0);
        _log_info(myLog, "stop thread %s", name.c_str());
    });
    // wait
    thread_cond.wait(lock);
}
  
void CThread::init(void)
{
#if defined(__linux__)
    // get id
    tid = syscall(SYS_gettid);
    // set name if not main
    // main thread name will be used to killall
    // do not change
    if (name.compare("main")) {
          // setup name at most 16 char
          pthread_setname_np(pthread_self(), name.c_str());
    }
    // osx
#elif defined(__APPLE__) && defined(__MACH__)
    do {
      // get with pthreadid
      uint64_t tid;
      // fetch
      pthread_threadid_np(nullptr, &tid);
      // assign
      tid = (pid_t)tid;
      // done
    } while (0);
#endif
    
    // set self
    thread_self = this;
    // set status
    status = IDLE;
    // socket pair
    evutil_socket_t fds[2];
    // init socketpair
    if (evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, fds)) abort();
    // make none blocking
    evutil_make_socket_nonblocking(fds[0]);
    evutil_make_socket_nonblocking(fds[1]);
    // assign
    fd_r = fds[0];
    fd_w = fds[1];
    // init event
    e = event_new(eb, fd_r, EV_READ | EV_PERSIST, [](int fd, short, void * ptr) {
        // convert
        CThread * thr = (CThread *)ptr;
        // do wake
        thr->wake();
      // done
    }, this);
    // event add
    event_add(e, nullptr);
}

CThread::~CThread(void)
{
    // stop();

    // delete e
    if (e) event_free(e);
    // close
    if (fd_r) close(fd_r);
    if (fd_w) close(fd_w);
    // base loop
    if (thr) {
        // free base
        event_base_free(eb);
        // free thread
        delete thr;
    }

    for (auto& ele : timers) {
        Timer* t = ele.second;
        delete t;
    }
    timers.clear();
}

CThread * CThread::self(void) {
    // get self
    return thread_self.get();
}

void CThread::stop(void) {
    // has thr
    if (!thr) return;
    // status
    auto status(this->status = EXIT);
    // send
    ::send(fd_w, &status, 1, 0);
    // join
    thr->join();
}

void CThread::wake(void) {
    // read
    uint8_t buf[4096];
    // do recv
    ssize_t len = recv(fd_r, buf, sizeof(buf), 0);
    // judge length
    if (len <= 0) return;
    // assign idle
    if (status == IDLE) status = WAKE;
    // callbacks
    std::list<callback> cbs;
    // loop
    do {
        // lock
        spin.lock();
        // empty
        if (callbacks.empty()) {
            if (status == WAKE) status = IDLE;
            spin.unlock();
            break;
        }

        // get front
        cbs.swap(callbacks);
        // unlock
        spin.unlock();
        // traverse
        for (auto & cb : cbs) if (cb) (cb)(this);
        // clear
        cbs.clear();
      // check
    } while (true);
    // set up idle flat
    if (status == WAKE) status = IDLE;
    // ifexit
    bool ifexit = false;
    // traverse buf
    for (ssize_t i = 0; i < len; i++) {
        // catch exit signal
        if (buf[i] == EXIT) ifexit = true;
    }
    // check
    if (!ifexit) return;
    // loop break

    _log_info(myLog, "thread %d stop begin!", tid);
    event_base_loopbreak(eb);
}

void CThread::sync(const callback & cb)
{
    // must not be the same thread
    if (this == self()) return cb(this);
    // sync by lock
    mybase::AWaiter awaiter;
    // dispatch
    dispatch([&](CThread * thr) {
        // invoke cb
        cb(thr);

        awaiter.invoke();
    });
    // wait
    awaiter.wait();
}

bool CThread::dispatch(const callback & cb, Schedule schedule)
{
    // invoke with the same thread
    if (schedule == Schedule::STACK && this == self()) {
        // invoke
        cb(this);
        // done
        return true;
    }

    spin.lock();
    // push back
    callbacks.emplace_back(cb);
    // unlock
    spin.unlock();
    // idle status
    Status status(this->status);
    // judge status
    switch (status) {
    case EXIT:
        return false;
    case IDLE:
        return send(fd_w, &status, 1, 0) == 1;
    case WAKE:
        return true;
    default:
        break;
    }
    // err
    return false;
}

void CThread::registerTimer(uint64_t id, std::function<void()> timer_cb, uint64_t timeout_usec, bool persist)
{
    auto func = [this, id, timer_cb, timeout_usec, persist] (CThread * thr) {
        auto iter = timers.find(id);
        if (iter != timers.end()) {
            Timer* t = iter->second;
            delete t;

            timers.erase(iter);
        }

        uint16_t flag = 0;
        if (persist) {
            flag = EV_PERSIST;
        } else {
            flag = EV_TIMEOUT;
        }

        struct Timer* timer = new Timer();
        timer->id = id;
        timer->cb = timer_cb;
        struct event* timer_evt = event_new(eb, -1, flag, [](int, short, void * ptr) {
            Timer* t = (Timer*)(ptr);
            if (t->cb) t->cb();
        }, timer);
        timer->e = timer_evt;

        // connect defer
        timeval tv = { 0, timeout_usec };
        // event_add
        event_add(timer_evt, &tv);

        _log_info(myLog, "add hbTimer! id: %lu timeout_usec[%lu]", id, timeout_usec);

        timers.emplace(id, timer);
    };

    this->dispatch(func);
}

void CThread::unregisterTimer(uint64_t id)
{
    auto func = [this, id] (CThread * thr) {
        auto iter = timers.find(id);
        if (iter != timers.end()) {
            Timer* t = iter->second;
            delete t;

            timers.erase(iter);
        }
    };

    this->dispatch(func);
}

std::string CThread::format(void)
{
    std::string str;
    // lock
    spin.lock();
    // cbs length
    size_t task(callbacks.size());
    // unlock
    spin.unlock();
    // foramt
    return str.append(name).append(": ").append(std::to_string(tid)).append(" ").append(std::to_string(task));
}

std::ostream & operator << (std::ostream & o, CThread & t) {
    // done
    return o << t.format();
}
  
}
