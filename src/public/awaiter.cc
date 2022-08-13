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

#include "awaiter.h"

namespace mybase {

void AWaiter::waitUsec(size_t time_us) {
    WStatus expected = WStatus::idle;
    if (status.compare_exchange_strong(expected, WStatus::ready)) {
        // invoke() has not been invoked yet, wait for it.
        std::unique_lock<std::mutex> lk(cvLock);
        expected = WStatus::ready;
        if (status.compare_exchange_strong(expected, WStatus::waiting)) {
            if (time_us) {
                cv.wait_for(lk, std::chrono::microseconds(time_us));
            } else {
                cv.wait(lk);
            }
            status.store(WStatus::done);
        } else {
            // invoke() has grabbed `cvLock` earlier than this.
        }
    } else {
        // invoke() already has been called earlier than this.
    }
}

void AWaiter::invoke() {
    WStatus expected = WStatus::idle;
    if (status.compare_exchange_strong(expected, WStatus::done)) {
        // wait() has not been invoked yet, do nothing.
        return;
    }

    std::unique_lock<std::mutex> lk(cvLock);
    expected = WStatus::ready;
    if (status.compare_exchange_strong(expected, WStatus::done)) {
        // wait() has been called earlier than invoke(),
        // but invoke() has grabbed `cvLock` earlier than wait().
        // Do nothing.
    } else {
        // wait() is waiting for ack.
        cv.notify_all();
    }
}

}
