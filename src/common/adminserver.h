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

#include "event2/event.h"
#include "event2/http.h"
#include "event2/buffer.h"
#include "event2/util.h"
#include "event2/keyvalq_struct.h"
#include "event2/http_struct.h"
#include "event2/thread.h"

#include "public/common.h"

#include <memory>
#include <thread>
#include <functional>
#include <unordered_map>
#include <atomic>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

namespace mybase
{

class AdminServer {
public:
    enum State : int32_t { UNINITIALIZED = 0, INITIALIZED = 1, STARTED = 2, STOPING = 3};
    using HttpHandler = std::function<void(struct evhttp_request *)>;
    using HttpMap = std::unordered_map<std::string, std::string>;

public:
    AdminServer();
    ~AdminServer();

public:
    bool initialize(uint16_t port = 5050);
    bool regHandler(std::string path, HttpHandler cb);

    bool start();
    bool stop();
    void wait();

private:
    void run();
    void breakLoop() { event_base_loopbreak(evBase.get()); }
    void handle(struct evhttp_request *req);

public:
    static void httpOk(struct evhttp_request *req, int32_t code=200, const std::string& msg = "");
    static void httpError(struct evhttp_request *req, int32_t code, const std::string& msg);

    static std::string parseUriPath(struct evhttp_request *req);
    static AdminServer::HttpMap parseHeaders(struct evhttp_request *req);
    static AdminServer::HttpMap parseParams(struct evhttp_request *req);
    static AdminServer::HttpMap parseCookies(struct evhttp_request* req);
    static std::string readContent(struct evhttp_request* req);

private:
    static void genericHandler(struct evhttp_request *, void *);

private:
    uint16_t httpPort;
    std::thread loopThread;
    std::unordered_map<std::string, HttpHandler> cbs;
    std::atomic<State> state;
    struct evhttp_bound_socket* evhtpHandler;

private:
    struct EventBaseDeleter { void operator()(event_base* ptr) { event_base_free(ptr); } };
    std::unique_ptr<event_base, EventBaseDeleter> evBase;
    struct EventHtpDeleter { void operator()(evhttp* ptr) { evhttp_free(ptr); } };
    std::unique_ptr<evhttp, EventHtpDeleter> evHtp;
};

}
