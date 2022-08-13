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

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <atomic>
#include <time.h>
#include <sys/time.h>

#include <stdint.h>

inline void onIdle(int32_t msec)
{
    struct timeval tval;
    if(msec < 1) {
        msec = 1;
    }
    tval.tv_sec = msec/1000;
    tval.tv_usec = (msec%1000) * 1000;
    select(0, (fd_set*)0, (fd_set*)0, (fd_set*)0, &tval);
}

class CMutex
{
public:
    CMutex(pthread_mutexattr_t* attrs = 0){        
        pthread_mutex_init(&lock_, attrs);
    }
    ~CMutex(){
        pthread_mutex_destroy(&lock_);
    }
    int32_t acquire(){
        return pthread_mutex_lock(&lock_);
    }
    int32_t acquire(const struct timespec* _time){
        return pthread_mutex_timedlock(&lock_,_time);
    }
    int32_t release(){
        return pthread_mutex_unlock(&lock_);
    }
    pthread_mutex_t& lock(){
        return lock_;
    }
protected:
    pthread_mutex_t lock_;
};

template <class MYLOCK>
class CGuard
{
public:
    CGuard(MYLOCK& lock) : m_lock(&lock) {        
        acquire();
    }
    ~CGuard() {
        release();
    }
    int32_t acquire () {
        return m_owner = m_lock->acquire();
    }
    int32_t release() {
        if(m_owner == -1) {
            return -1;
        } else{
            m_owner = -1;
            return m_lock->release();
        }
    }
    bool locked() const{
        return m_owner != -1;
    }

private:
    CGuard();
    CGuard (const CGuard<MYLOCK> &);
    void operator= (const CGuard<MYLOCK> &);

private:
    MYLOCK* m_lock;
    int32_t m_owner;
};

template <class MUTEX>
class CCondition
{
public:
    CCondition (MUTEX &m) : mutex_ (m)    {
        pthread_condattr_t attr;
        pthread_condattr_init(&attr);
        if (pthread_cond_init (&this->cond_, &attr) != 0) {
            fprintf(stderr, "call pthread_cond_init failed\n");
        }
        pthread_condattr_destroy(&attr);
    }

    ~CCondition (void){
        if (this->destroy() == -1) {
            fprintf(stderr, "remove CCondition failed\n");
        }
    }
    int32_t wait(void){
        return pthread_cond_wait (&this->cond_, &this->mutex_.lock());
    }
    int32_t wait(const struct timespec* _time){
        return pthread_cond_timedwait (&this->cond_, &this->mutex_.lock(), _time);
    }
    int32_t signal (void){
        return pthread_cond_signal (&this->cond_);
    }
    int32_t broadcast (void){
        return pthread_cond_broadcast (&this->cond_);
    }
    int32_t destroy (void){
        int32_t result = 0;
        while ((result = pthread_cond_destroy (&this->cond_)) == -1 && errno == EBUSY) {
            pthread_cond_broadcast (&this->cond_);
            onIdle(50);
        }
        return result;
    }
    MUTEX &mutex(void){
        return this->mutex_;
    }

protected:
    pthread_cond_t cond_;
    MUTEX &mutex_;
private:
    void operator= (const CCondition<MUTEX> &);
    CCondition (const CCondition<MUTEX> &);
};

class CThreadPool {
public:
    CThreadPool(uint64_t);

    bool execute(std::function<void()>);
    ~CThreadPool();

private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;

    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop{false};
};

template<class T>
class CQueueThread {
public:
    CQueueThread() { }
    CQueueThread(uint32_t thread_count, std::function<void(const T& data)> _handler);
    ~CQueueThread();

    bool setThreadCount(uint32_t thread_count);
    bool setHandler(std::function<void(const T& data)> _handler);
    bool enqueue(const T& data);

    bool start();
    void wait();
    void stop();

private:
    std::vector< std::thread > workers;
    uint32_t threadCount{1};

    uint32_t maxQueuSize{1000000};
    // queue
    std::queue<T> _queue;           
    std::atomic<bool> running{false};

protected:
    std::mutex queue_mutex;
    std::condition_variable condition;

    std::function<void(const T& _data)> handler;
};

template<class T>
bool CQueueThread<T>::setThreadCount(uint32_t thread_count) {
    if (running) return false;
    threadCount = thread_count;
    return true;
}

template<class T>
bool CQueueThread<T>::setHandler(std::function<void(const T& data)> _handler)
{
    if (running) return false;
    handler = _handler;
    return true;
}

template<class T>
CQueueThread<T>::CQueueThread(uint32_t thread_count, std::function<void(const T& data)> _handler)
{
    threadCount = thread_count;
    handler = _handler;
}

template<class T>
bool CQueueThread<T>::start()
{
    running = true;
    for( uint64_t i = 0; i < threadCount; ++i ) {
        workers.emplace_back(
            [this]
            {
                while ( true )
                {
                    T data;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait( lock, [this] { return !this->running || !this->_queue.empty(); } );

                        if(!this->running && this->_queue.empty()) {
                            return;
                        }
                        data = std::move( this->_queue.front() );

                        this->_queue.pop();
                    }

                    if (handler) handler(data);
                }
            }
        );
    }

    return true;
}

template<class T>
void CQueueThread<T>::wait()
{
    for(std::thread &worker: workers) {
        if (worker.joinable()) worker.join();
    }
}

template<class T>
void CQueueThread<T>::stop()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        running = false;
    }

    condition.notify_all();
    for(std::thread &worker: workers) {
        if (worker.joinable()) worker.join();
    }
}

template<class T>
bool CQueueThread<T>::enqueue(const T& data)
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(!running) return false;

        _queue.emplace( data );
    }
    condition.notify_one();
    return true;
}

template<class T>
CQueueThread<T>::~CQueueThread()
{
    stop();
}
