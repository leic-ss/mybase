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

#include "adminserver.h"

#include "public/dlog.h"

#include <functional>
#include <memory>
#include <unordered_map>

namespace mybase  
{

AdminServer::AdminServer() : state(UNINITIALIZED)
                           , evhtpHandler(nullptr)
{
}

/*
 1. destructor of this class
 2. destructor of member variable
 3. destructor of base class
*/
AdminServer::~AdminServer()
{
    if (evHtp && evhtpHandler) {
        evhttp_del_accept_socket(evHtp.get(), evhtpHandler);
        evhtpHandler = nullptr;
    }
}

void AdminServer::httpOk(struct evhttp_request *req,
                          int32_t code,
                          const std::string& msg)
{
    if (msg.empty()) {
        evhttp_send_reply(req, code, "OK", nullptr);
        return ;
    }

    struct evbuffer *evb = evbuffer_new();
    if (evb) {
        evbuffer_add_printf(evb, "%s", msg.c_str());/* XXX escape this */
        evhttp_send_reply(req, code, "OK", evb);
        evbuffer_free(evb);
    }
}

void AdminServer::httpError(struct evhttp_request *req,
                             int32_t code,
                             const std::string& msg)
{
    //evhttp_send_error(req, code, msg.c_str());
    struct evbuffer *evb = evbuffer_new();
    if (evb) {
        evbuffer_add_printf(evb, "%s", msg.c_str());/* XXX escape this */
        evhttp_send_reply(req, code, "ERROR", evb);
        evbuffer_free(evb);
    }
}

bool AdminServer::initialize(uint16_t port)
{
    State expected = UNINITIALIZED;
    if (!state.compare_exchange_strong(expected,
                                       INITIALIZED,
                                       std::memory_order_relaxed)) {
        log_error("AdminServer initialize failed! state[%d]!", (int32_t)state);
        return false;
    }

    auto base = event_base_new();
    if (!base) {
        log_error("Couldn't create an event_base: exiting!");
        state = UNINITIALIZED;
        return false;
    }

    evthread_use_pthreads();
    evthread_make_base_notifiable(base);
    evBase.reset(base);

    auto http = evhttp_new(base);
    if (!http) {
        log_error("couldn't create evhttp. Exiting.");
        state = UNINITIALIZED;
        return false;
    }
    evHtp.reset(http);

    /* We want to accept arbitrary requests, so we need to set a "generic"
     * cb.  We can also add callbacks for specific paths. */
    evhttp_set_gencb(http, genericHandler, this);

    if (evhtpHandler) {
        evhttp_del_accept_socket(http, evhtpHandler);
        evhtpHandler = nullptr;
    }

    evhtpHandler = evhttp_bind_socket_with_handle(http, "0.0.0.0", port);
    if (!evhtpHandler) {
        log_error("couldn't bind to port[%d] Exiting.", (int32_t)port);
        state = UNINITIALIZED;
        return false;
    }

    this->httpPort = port;
    log_info("AdminServer bind to port[%d] successfully.", (int32_t)port);

    regHandler("/test1", [](struct evhttp_request *req) {
                                const char* uri = evhttp_request_get_uri(req);
                                log_info("Matched uri: %s", uri);
                                struct evbuffer *evb = evbuffer_new();
                                evbuffer_add_printf(evb, "test response\n");
                                evhttp_send_reply(req, 200, "OK", evb);
                                evbuffer_free(evb);
                            });
    return true;
}

std::string AdminServer::parseUriPath(struct evhttp_request *req)
{
    if (!req) return std::string();
    const char* uri = evhttp_request_get_uri(req);
    std::string uri_path(uri);

    size_t find_pos = uri_path.find_first_of("?", 0);
    if (find_pos != std::string::npos) {
        uri_path = uri_path.substr(0, find_pos);
    }

    return std::move(uri_path);
}

AdminServer::HttpMap AdminServer::parseHeaders(struct evhttp_request *req)
{
    std::unordered_map<std::string, std::string> headers_map;
    if (!req) return headers_map;

    struct evkeyvalq* headers = evhttp_request_get_input_headers(req);
    for (struct evkeyval* header = headers->tqh_first;
         header;
         header = header->next.tqe_next) {
        headers_map.emplace(header->key, header->value);
    }

    return std::move(headers_map);
}

AdminServer::HttpMap AdminServer::parseCookies(struct evhttp_request* req)
{
    std::unordered_map<std::string, std::string> headers_map = parseHeaders(req);

    std::unordered_map<std::string, std::string> cookies;
    if (!req) return cookies;

    auto iter = headers_map.find("Cookie");
    if ( iter == headers_map.end() ) {
        return cookies;
    }

    std::string cookie = iter->second;
    std::vector<std::string> vec = StringHelper::tokenize(cookie, ";");
    for (auto item : vec) {
        std::vector<std::string> items = StringHelper::tokenize(item, "=");
        if (items.size() != 2) continue;

        cookies.emplace(StringHelper::trim(items[0], " "), StringHelper::trim(items[1], " "));
    }

    return cookies;
}

AdminServer::HttpMap AdminServer::parseParams(struct evhttp_request *req)
{
    std::unordered_map<std::string, std::string> params;
    if (!req) return params;
    const char* uri = evhttp_request_get_uri(req);

    struct evkeyvalq headers;
    if (0 == evhttp_parse_query(uri, &headers)) {
        struct evkeyval* header;
        for ( (header) = headers.tqh_first;
              (header) != nullptr;
              (header) = ((header)->next.tqe_next) ) {
            params.emplace(header->key, header->value);
        }
    }

    return std::move(params);
}

std::string AdminServer::readContent(struct evhttp_request* req)
{
    std::string content;
    struct evbuffer *buf = evhttp_request_get_input_buffer(req);
    while (evbuffer_get_length(buf)) {
        char cbuf[64*1024] = {0};
        int32_t len = evbuffer_remove(buf, cbuf, sizeof(cbuf));
        if (len > 0) {
            content.append(cbuf, len);
        }
    }
    return std::move(content);
}

void AdminServer::genericHandler(struct evhttp_request *req, void *arg)
{
    AdminServer* server = (AdminServer*)arg;
    server->handle(req);
}

void AdminServer::handle(struct evhttp_request *req)
{
    if (!req) return ;
    std::string uri_path = parseUriPath(req);

    log_info("uri: %s type: %d", req->uri, req->type);
    auto iter = cbs.find(uri_path);
    if (iter != cbs.end()) {
        HttpMap params = parseParams(req);
        for (auto& param : params) {
            log_info("param: <%s = %s>", param.first.c_str(), param.second.c_str());
        }

        auto callback = iter->second;
        callback(req);
    } else {
        enum evhttp_cmd_type cmd = evhttp_request_get_command(req);

        log_warn("No Matched callback for uri_path[%s] cmd[%d]", uri_path.c_str(), cmd);
        evhttp_send_error(req, 404, "No matched callback");
        // evhttp_send_reply(req, 200, "OK", nullptr);
    }

    return ;
}

bool AdminServer::regHandler(std::string path, HttpHandler cb) 
{
    cbs.emplace(path, cb);
    return true;
}

bool AdminServer::start()
{
    State expected = INITIALIZED;
    if (!state.compare_exchange_strong(expected,
                                       STARTED,
                                       std::memory_order_relaxed)) {
        log_error("AdminServer start failed! state[%d]!", (int32_t)state);
        return false;
    }

    loopThread = std::thread(&AdminServer::run, this);
    return true;
}

bool AdminServer::stop() 
{
    State expected = STARTED;
    if (!state.compare_exchange_strong(expected,
                                       STOPING,
                                       std::memory_order_relaxed)) {
        log_error("AdminServer stop failed! state[%d]!", (int32_t)state);
        return false;
    }

    if (!loopThread.joinable()) {
        log_error("AdminServer stop failed, no need!");
        state = INITIALIZED;
        return false;
    }

    breakLoop();

    //wait();

    log_info("AdminServer stop successfully, done!");
    return true;
}

void AdminServer::wait() 
{
    if (!loopThread.joinable()) {
        log_warn("AdminServer wait failed, no need!");
        return;
    }
    loopThread.join();

    state = INITIALIZED;
    log_info("AdminServer wait successfully, done!");
}

void AdminServer::run()
{
    event_base_loop(evBase.get(), EVLOOP_NO_EXIT_ON_EMPTY);
}

}
