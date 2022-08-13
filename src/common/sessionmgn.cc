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

#include "sessionmgn.h"
#include "public/dlog.h"

namespace mybase {

CSessionMgn::CSessionMgn(uint32_t slot_number, uint32_t timeout_precision_ms)
                        : slotNumber(slot_number), timeoutPrecision(timeout_precision_ms)
{
    slots = new std::vector<uint32_t>[slot_number];
    lastCheckMSec = ( TimeHelper::currentMs() / timeoutPrecision ) * timeoutPrecision;
}

CSessionMgn::~CSessionMgn() {
    if (slots) {
        delete[] slots;
        slots = nullptr;
    }
}

bool CSessionMgn::start()
{
    isRunning = true;
    timerThread = std::thread(&CSessionMgn::timeoutCheckThread, this);
    return true;
}

void CSessionMgn::stop()
{
    isRunning = false;
    if (timerThread.joinable()) {
        timerThread.join();
    }
}

void CSessionMgn::timeoutCheckThread()
{
    while (isRunning) {
        usleep( (timeoutPrecision >= 10 ? 10 : timeoutPrecision) * 1000 );

        checkTimeout();
    }
    return ;
}

void CSessionMgn::checkTimeout()
{
    uint64_t curmsec = TimeHelper::currentMs();
    curmsec = (curmsec / timeoutPrecision) * timeoutPrecision;

    if(curmsec < lastCheckMSec) {
        lastCheckMSec = curmsec;
        return;
    }

    uint32_t timeout_queue_num = (curmsec - lastCheckMSec) / timeoutPrecision;
    if(timeout_queue_num >= slotNumber) {
        timeout_queue_num = slotNumber;
    }

    std::unordered_map<uint32_t, std::shared_ptr<BaseSession>> session_timeout;
    do {
        std::lock_guard<std::mutex> lk(mtx);
        for(uint32_t i = 0; i < timeout_queue_num; ++i) {
            uint32_t index = (lastCheckSlot + i) % slotNumber;
            for (const auto &sessionid : slots[index]) {
                if (sessions.count(sessionid) != 0) {
                    session_timeout[sessionid] = sessions[sessionid];
                    sessions.erase(sessionid);
                }
            }
            slots[index].clear();
            // slots[index].shrink_to_fit();
        }
        sessionNum = (uint32_t)(sessions.size());

        lastCheckSlot = (lastCheckSlot + timeout_queue_num) % slotNumber;
        lastCheckMSec = curmsec;
    } while(false);

    for(auto iter = session_timeout.begin(); iter != session_timeout.end(); ++iter) {
        if (timeoutHandler) timeoutHandler(iter->second);
    }
}

void CSessionMgn::saveSession(std::shared_ptr<BaseSession> req)
{
    std::lock_guard<std::mutex> lk(mtx);

    auto iter = sessions.find(req->sequence);
    if (iter != sessions.end()) {
        return;
    }

    sessions[req->sequence] = req;
    uint32_t sleepindex = (req->timeoutMSec - lastCheckMSec) / timeoutPrecision;
    if(req->timeoutMSec <= lastCheckMSec) {
        sleepindex = 1;
    }

    if(sleepindex >= slotNumber) {
        sleepindex = slotNumber - 1;
    }

    sleepindex = (sleepindex + lastCheckSlot) % slotNumber;
    slots[sleepindex].push_back(req->sequence);

    sessionNum = sessions.size();
    return;
}

std::shared_ptr<BaseSession> CSessionMgn::eraseAndGetSession(uint32_t seq)
{
    std::lock_guard<std::mutex> lk(mtx);

    std::shared_ptr<BaseSession> retMsg = nullptr;
    auto iter = sessions.find(seq);
    if ( iter != sessions.end() ) {
        retMsg = iter->second;
        sessions.erase(iter);
    }

    sessionNum = (uint32_t)(sessions.size());
    return retMsg;
}

}
