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

#include "threadctl.h"

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

#include <stdint.h>

// the constructor just launches some amount of workers
CThreadPool::CThreadPool(uint64_t threads)
{
    for( uint64_t i = 0; i < threads; ++i ) {
        workers.emplace_back(
            [this]
            {
                while ( true )
                {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait( lock, [this] { return this->stop || !this->tasks.empty(); } );

                        if(this->stop && this->tasks.empty()) {
                            return;
                        }
                        task = std::move( this->tasks.front() );

                        this->tasks.pop();
                    }

                    task();
                }
            }
        );
    }
}

// add new work item to the pool
bool CThreadPool::execute(std::function<void()> func)
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop) return false;

        tasks.emplace( func );
    }
    condition.notify_one();
    return true;
}

// the destructor joins all threads
CThreadPool::~CThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }

    condition.notify_all();
    for(std::thread &worker: workers) {
        if (worker.joinable()) worker.join();
    }
}
