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

#include <atomic>
#include <unordered_map>
#include <condition_variable>
#include <mutex>

namespace mybase {

class AWaiter {
private:
    enum class WStatus {
        idle    = 0x0,
        ready   = 0x1,
        waiting = 0x2,
        done    = 0x3
    };

public:
    AWaiter() : status(WStatus::idle) {}

    void reset() { status.store(WStatus::idle); }

    void wait() { waitUsec(0); }

    void waitMsec(size_t time_ms) { waitUsec(time_ms * 1000); }

    void waitUsec(size_t time_us);

    void invoke();

private:
    std::atomic<WStatus> status;
    std::mutex cvLock;
    std::condition_variable cv;
};

}
