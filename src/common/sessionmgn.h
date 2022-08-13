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

#include "public/list.h"
#include "public/common.h"

#include <unordered_map>
#include <atomic>
#include <mutex>
#include <thread>

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

namespace mybase {

class BaseSession
{
public:
    BaseSession(uint64_t timeout, uint32_t seq) : sequence(seq), timeoutMSec(timeout) {}
    virtual ~BaseSession() {}

public:
    uint32_t sequence;
    uint64_t timeoutMSec;
};

class CSessionMgn
{
public:
    using TimeoutHandlerCallBack = std::function<void(std::shared_ptr<BaseSession>)>;

    CSessionMgn(uint32_t slot_number, uint32_t timeout_precision_ms=1);
    ~CSessionMgn();

    bool start();
    void stop();

    void setTimeoutHandler(TimeoutHandlerCallBack timeout_handler) {
        timeoutHandler = timeout_handler;
    }

    void saveSession(std::shared_ptr<BaseSession> req);
    std::shared_ptr<BaseSession> eraseAndGetSession(uint32_t seq);
    uint32_t sessionSize() { return sessionNum; }
    uint32_t nextSequence() { return sequenceNumber.fetch_add(1); }

protected:
    void addToList(std::shared_ptr<BaseSession> req, bool need_del);
    void timeoutCheckThread();
    void checkTimeout();

private:
    std::thread timerThread;
    std::function<void(std::shared_ptr<BaseSession>)> timeoutHandler;

    bool isRunning{false};
    uint32_t slotNumber{1000};
    std::vector<uint32_t>* slots{nullptr};
    uint64_t lastCheckMSec{0};
    uint32_t lastCheckSlot{0};

    std::unordered_map< uint32_t, std::shared_ptr<BaseSession> > sessions;
    std::mutex mtx;

    uint32_t timeoutPrecision{1};
    uint32_t sessionNum{0};
    std::atomic<uint32_t> sequenceNumber{0};
};

}
