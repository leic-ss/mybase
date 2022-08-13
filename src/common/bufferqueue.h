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

#include "public/buffers.h"
#include "public/threadctl.h"
#include "public/common.h"
#include "public/pipewrap.h"

#include <vector>
#include <stdexcept>
#include <iostream>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>

namespace mybase
{

struct buffer_full: public std::runtime_error{ buffer_full(const std::string& s):std::runtime_error(s){}};

class CBufferQueue
{
public:
    CBufferQueue() {_header = nullptr; _data = nullptr; }
    ~CBufferQueue() { _header = nullptr; _data = nullptr; }

    void attach(char* pBuf, uint64_t iBufSize) noexcept(false);
    void create(char* pBuf, uint64_t iBufSize) noexcept(false); // attach and init

    bool dequeue(char *buffer, uint32_t& buffersize) noexcept(false);
    bool dequeue(char *buffer1,uint32_t& buffersize1, char *buffer2, uint32_t& buffersize2) noexcept(false);

    bool peek(char *buffer, uint32_t& buffersize) noexcept(false);
    bool peek(char *buffer1, uint32_t& buffersize1, char *buffer2, uint32_t& buffersize2) noexcept(false);

    void enqueue(const char *buffer, uint32_t len) noexcept(false);
    void enqueue(const char *buffer1, uint32_t len1, const char *buffer2, uint32_t len2) noexcept(false);

    bool isEmpty() const {return _header->iBegin == _header->iEnd;}
    bool isFull(uint64_t len) const;

    uint32_t count() const;
    bool getAllData(char * buf, uint32_t& len);
        
private:
    uint64_t GetLen(char *buf) { uint64_t u; memcpy((void *)&u, buf, sizeof(uint64_t)); return u;}
    void SetLen(char *buf, uint64_t u) {memcpy(buf, (void *)&u, sizeof(uint64_t));}

private:
    const static uint64_t ReserveLen = 8;
    typedef struct Header
    {
        uint64_t iBufSize;
        uint64_t iReserveLen;
        uint64_t iBegin;
        uint64_t iEnd;
        uint32_t iCount;
    } StuHeader;

    StuHeader* _header;
    char *_data;
};

struct CEnqueUnit {
    const char* buffer1{nullptr};
    uint32_t len1{0};
    const char* buffer2{nullptr};
    uint32_t len2{0};
};

class CLockBufferQueue {
public:
    CLockBufferQueue() : m_cond(m_mutex) { m_queueTotalNum++; }

    bool init(uint64_t size, bool isCond, bool singleread=false);
    void enqueue(const char *buffer1, uint32_t len1, const char *buffer2, uint32_t len2);
    bool dequeue(char *buffer1, uint32_t & buffersize1, char *buffer2, uint32_t & buffersize2,uint64_t msec);
    void enqueue(const char *buffer1, uint32_t len1);
    bool dequeue(char *buffer1, uint32_t& buffersize1, uint64_t msec);
    int32_t enqueue(const CEnqueUnit* unitArray, int32_t num);
    // int32_t dequeue(CEnqueUnit* unitArray, int32_t num, uint64_t msec, bool is_pair=true);

    bool isEmpty();

    ~CLockBufferQueue()
    {
        if(m_tmpPtr) delete[] m_tmpPtr;
        delete[] m_ptr;
        m_queueTotalNum -= 1;
    }

    inline uint64_t getBufLen() const { return m_size; }

private:
    uint64_t m_size{0};
    mybase::CBufferQueue m_queue;
    // std::mutex m_mutex;
    CMutex m_mutex;
    bool m_isCond{false};
    CCondition<CMutex> m_cond;
    // std::condition_variable m_cond;
    bool m_singleRead{false};
    char* m_ptr{nullptr};
    char* m_tmpPtr{nullptr};
    char* m_tmpIndex{nullptr};
    char* m_tmpEnd{nullptr};

    bool getQueueDataToBuffer();
    bool dequeueNoBuffer(char *buffer1,unsigned & buffersize1,char *buffer2,unsigned & buffersize2, uint64_t msec = 0);
    bool dequeueFromBuffer(char *buffer1,unsigned & buffersize1,char *buffer2,unsigned & buffersize2);

    static std::atomic<uint64_t> m_queueTotalNum;
    static std::atomic<uint64_t> m_queueTotalMem;
};

class BufferQueue {
public:
    BufferQueue();
    ~BufferQueue() { }

public:
    bool initialize(uint64_t maxSize = 32*1024*1024);

    bool enqueue(const char* buf, uint64_t len);
    bool enqueue(const char* buf1, uint64_t len1, const char* buf2, uint64_t len2);
    bool enqueue(DataBuffer& data);
    bool enqueue(DataBuffer& hdr, DataBuffer& data);
    bool dequeue(DataBuffer& data);
    bool dequeue(DataBuffer& hdr, DataBuffer& data);

    bool dequeue(std::string& data);
    bool dequeue(std::string& hdr, std::string& data);

    uint64_t size();
    uint64_t count();
    bool isEmpty();

private:
    uint64_t getLen(char *buf);
    void setLen(char *buf, uint64_t u);
    bool tryEnqueueFull(uint64_t len);

private:
    uint64_t startPos;
    uint64_t endPos;
    uint64_t eleNumber;
    uint64_t totalBufSize;
    std::unique_ptr<char[], std::function< void(char* ptr) > > circleBuf;
};

class LockBufferQueue {
public:
    LockBufferQueue();
    ~LockBufferQueue() { }

public:
    bool initialize(uint64_t maxSize = 32*1024*1024);

    bool enqueue(const char* buf, uint64_t len);
    bool enqueue(const char* buf1, uint64_t len1, const char* buf2, uint64_t len2);
    bool enqueue(DataBuffer& data);
    bool enqueue(DataBuffer& hdr, DataBuffer& data);
    bool dequeue(DataBuffer& data, uint64_t wait_ms = 10);
    bool dequeue(DataBuffer& hdr, DataBuffer& data, uint64_t wait_ms = 10);

    uint64_t size();
    uint64_t count();
    bool isEmpty();

private:
    std::mutex cvLock;
    std::condition_variable cv;
    std::unique_ptr<BufferQueue> circleBufQueue;
};

/**
 * WARNING: only allow one reader thread and one writer thread
 *
 *   pop here
 *           \        <---  fixed size: `nodeCapacity` --->
 * tailNode [Node] -> elements (array): [elem0, elem1, ...]
 *            | next
 *            v
 *          [Node] -> elements (array): [elem0, elem1, ...]
 *            | next
 *            v
 *           ...
 *            |
 *            v
 * headNode [Node] -> elements (array): [elem0, elem1, ...]
 *            \
 *             push here
 */
template <class ElementType>
class FifoQueue
{
protected:
    using value_type        = ElementType;
    using reference         = ElementType&;
    using const_reference   = const ElementType&;

    struct Node {
        Node(uint32_t size);

        std::vector<value_type> elements;
        Node* next;
    };

    // WARNING:
    //   Should use only one popping thread and one pushing thread.
    //   Popping and pushing can be done at the same time.
    class RecycleBin {
    public:
        RecycleBin(size_t bin_size = 3);
        ~RecycleBin() { clear(); }

        void clear();
        bool empty() const { return (cursorHead == cursorTail); }
        bool full() const;

        Node* popFreeNode();
        void pushFreeNode(Node* nod);

    private:
        // Max number of reusable nodes.
        size_t binSize;
        // Freed nodes that will be reused.
        std::vector<Node*> reusableNodes;
        // Circular cursor to the slot to be pushed next.
        std::atomic<uint64_t> cursorHead;
        // Circular cursor to the node to be popped.
        std::atomic<uint64_t> cursorTail;
    };

public:
    FifoQueue();
    ~FifoQueue() { clear(); }

public:
    int32_t init( uint32_t node_capacity = 100, uint32_t capacity_ = uint32_t(-1) );

public:
    size_t getCapacity() const { return queueCapacity; }
    bool full() const { return size() == queueCapacity; }
    bool empty() const { return headIdx == tailIdx; }
    uint32_t size() const { return headIdx - tailIdx; }

    value_type front();
    reference frontRef();
    const_reference frontRef() const;

    int32_t pop();
    int32_t push(const value_type& value);
    void clear();

private:
    // Pop/push reusable free `Node`.
    RecycleBin recycleBin;
    // Current head index.
    std::atomic<uint64_t> headIdx;
    // Current `headNode`s start index.
    std::atomic<uint64_t> headNodeStartIdx;
    // Head node.
    Node* headNode;
    // Current tail index.
    std::atomic<uint64_t> tailIdx;
    // Current `tailNode`s start index.
    std::atomic<uint64_t> tailNodeStartIdx;
    // Tail node.
    Node* tailNode;
    // Capacity of the entire queue.
    uint32_t queueCapacity;
    // Capacity of each `Node`.
    uint32_t nodeCapacity;
};

template <class ElementType>
FifoQueue<ElementType>::Node::Node(uint32_t size)
                                  : elements(size)
                                  , next(nullptr)
{ }

template <class ElementType>
FifoQueue<ElementType>::RecycleBin::RecycleBin(size_t bin_size)
                                              : binSize(bin_size)
                                              , reusableNodes(bin_size)
                                              , cursorHead(0)
                                              , cursorTail(0)
{ }

template <class ElementType>
void FifoQueue<ElementType>::RecycleBin::clear() {
    while (!empty()) {
        Node* tem = popFreeNode();
        delete tem;
    }
    reusableNodes.clear();
}

template <class ElementType>
typename FifoQueue<ElementType>::Node* FifoQueue<ElementType>::RecycleBin::popFreeNode() {
    assert( !empty() );

    Node* getNode = reusableNodes[cursorTail];
    cursorTail = (cursorTail + 1) % binSize;
    getNode->next = nullptr;
    return getNode;
}

template <class ElementType>
bool FifoQueue<ElementType>::RecycleBin::full() const {
    uint32_t next_slot = (cursorHead + 1) % binSize;
    if (next_slot == cursorTail) return true;
    return false;
}

template <class ElementType>
void FifoQueue<ElementType>::RecycleBin::pushFreeNode(Node* nod) {
    assert( !full() );

    reusableNodes[cursorHead] = nod;
    cursorHead = (cursorHead + 1) % binSize;
}

template <class ElementType>
FifoQueue<ElementType>::FifoQueue() : recycleBin(3)
                                    , headIdx(0)
                                    , headNodeStartIdx(0)
                                    , headNode(nullptr)
                                    , tailIdx(0)
                                    , tailNodeStartIdx(0)
                                    , tailNode(nullptr)
                                    , queueCapacity(0)
                                    , nodeCapacity(0)
{}

template <class ElementType>
int32_t FifoQueue<ElementType>::init(uint32_t node_capacity, uint32_t capacity_)
{
    if (0 == capacity_) return -1;
    if (headNode || tailNode) return -1;

    queueCapacity = capacity_;
    nodeCapacity = nodeCapacity > capacity_
                   ? capacity_
                   : node_capacity;
    Node* tem = new Node(nodeCapacity);
    if (!tem) return -1;

    headNode = tailNode = tem;
    return 0;
}

// template <class ElementType>
// typename FifoQueue<ElementType>::value_type FifoQueue<ElementType>::front()
// {
//     uint32_t bias = tailIdx - tailNodeStartIdx;
//     return tailNode->elements[bias];
// }

template <class ElementType>
typename FifoQueue<ElementType>::value_type FifoQueue<ElementType>::front()
{
    uint32_t bias = tailIdx - tailNodeStartIdx;
    return tailNode->elements[bias];
}

template <class ElementType>
typename FifoQueue<ElementType>::reference FifoQueue<ElementType>::frontRef()
{
    uint32_t bias = tailIdx - tailNodeStartIdx;
    return tailNode->elements[bias];
}

template <class ElementType>
typename FifoQueue<ElementType>::const_reference FifoQueue<ElementType>::frontRef() const
{
    uint32_t bias = tailIdx - tailNodeStartIdx;
    return tailNode->elements[bias];
}

template <class ElementType>
int32_t FifoQueue<ElementType>::pop()
{
    if ( empty() ) return -1;

    uint32_t offset = tailIdx - tailNodeStartIdx;
    if( (offset + 1) == nodeCapacity ) {
        tailNodeStartIdx = tailIdx + 1;
        Node* thisnod = tailNode;
        tailNode = thisnod->next;
        if ( recycleBin.full() ) {
            delete thisnod;
        } else {
            recycleBin.pushFreeNode(thisnod);
        }
    }

    ++tailIdx;
    return 0;
}

template <class ElementType>
int32_t FifoQueue<ElementType>::push(const value_type& value)
{
    if ( full() ) return -1;

    uint32_t offset = headIdx - headNodeStartIdx;
    headNode->elements[offset] = value;

    if( (offset + 1) == nodeCapacity ) {
        Node* newNode = nullptr;
        if ( recycleBin.empty() ) {
            newNode = new Node(nodeCapacity);
            assert( newNode );
        } else {
            newNode = recycleBin.popFreeNode();
        }
        assert( newNode->next == nullptr );
        headNode->next = newNode;
        headNode = newNode;
        headNodeStartIdx = headIdx + 1;
    }

    ++headIdx;
    return 0;
}

template <class ElementType>
void FifoQueue<ElementType>::clear()
{
    if (!queueCapacity) return;

    Node* tem = tailNode;
    while (tem) {
        Node* next_node = tem->next;
        delete tem;
        tem = next_node;
    }

    recycleBin.clear();

    headIdx = tailIdx = 0;
    headNodeStartIdx = tailNodeStartIdx = 0;
    headNode = tailNode = nullptr;
    queueCapacity = 0;
}

template <class ElementType>
class IpcFifoQueue
{
public:
    using value_type        = ElementType;
    using reference         = ElementType&;
    using const_reference   = const ElementType&;

public:
    enum Status : int32_t {
        IDLE = '0',
        BUSY = '1'
    };

public:
    IpcFifoQueue() : pipeWrapper() { }
    ~IpcFifoQueue() { queue.clear(); }

public:
    bool init( uint32_t node_capacity = 100, uint32_t capacity_ = uint32_t(-1) ) { return queue.init(node_capacity, capacity_) == 0; }

    bool push(const value_type& value);
    bool front(value_type& value, uint64_t wait_us);
    bool pop() { return queue.pop(); }

protected:
    FifoQueue<ElementType> queue;
    Pipe pipeWrapper;
    SpinLock spin;
    std::atomic<Status> status{IDLE};
};

template <class ElementType>
bool IpcFifoQueue<ElementType>::push(const value_type& value) {
    spin.lock();
    int32_t ret = queue.push(value);
    spin.unlock();

    if ( ret != 0 ) return false;

    Status expect = IDLE;
    if (status.compare_exchange_strong(expect, BUSY)) {
        return pipeWrapper.write(&status, 1) == 1;
    }

    return true;
}

template <class ElementType>
bool IpcFifoQueue<ElementType>::front(value_type& value, uint64_t wait_us)
{
    do {
        // lock
        spin.lock();
        // empty
        if (queue.empty()) {
            if (status == BUSY) status = IDLE;
            spin.unlock();
            break;
        }
        spin.unlock();

        value = queue.frontRef();
        return true;
    } while (false);

    struct timeval tv = {0};
    tv.tv_sec = wait_us/1000000;
    tv.tv_usec = wait_us % 1000000;

    bool rc = pipeWrapper.wait_for_read(&tv);
    if (!rc) {
        return false;
    }

    char buff[1024] = {0};
    pipeWrapper.read(buff, 1024);

    if (!queue.empty()) {
        value = queue.frontRef();
        return true;
    }

    return false;
}

template <class ElementType>
class LockFifoQueue
{
public:
    using value_type        = ElementType;
    using reference         = ElementType&;
    using const_reference   = const ElementType&;

public:
    LockFifoQueue() : m_cond(m_mutex) { }
    ~LockFifoQueue() { queue.clear(); }

public:
    bool init( uint32_t node_capacity = 100, uint32_t capacity_ = uint32_t(-1) ) { return queue.init(node_capacity, capacity_) == 0; }

    bool push(const value_type& value);
    bool pop(value_type& value, uint64_t wait_us);

protected:
    FifoQueue<ElementType> queue;

    CMutex m_mutex;
    CCondition<CMutex> m_cond;
};

template <class ElementType>
bool LockFifoQueue<ElementType>::push(const value_type& value) {
    if ( queue.push(value) != 0 ) return false;

    m_cond.signal();
    return true;
}

template <class ElementType>
bool LockFifoQueue<ElementType>::pop(value_type& value, uint64_t wait_sec) {
    struct timeval tv;
    gettimeofday(&tv, 0);
    struct timespec _time;
    _time.tv_sec = tv.tv_sec + wait_sec;
    _time.tv_nsec = tv.tv_usec * 1000;

    CGuard<CMutex> guard(m_mutex);

    if (queue.empty()) {
        m_cond.wait(&_time);
    }

    if (queue.empty()) return false;

    value = queue.front();
    queue.pop();

    return true;
}

template <class MSGTYPE>
class CTMsgQueueCond
{
public:
    CTMsgQueueCond() {}
    ~CTMsgQueueCond()
    {
        std::unique_lock<std::mutex> lk(cvLock);
        while(m_queue.size() > 0)
        {
            MSGTYPE* _pvalue = m_queue.front();
            if(_pvalue != nullptr) {
                delete _pvalue;
                m_queue.pop_front();
            } else {
                break;
            }
        }
    }
    bool postmsg(MSGTYPE* msg)
    {
        std::unique_lock<std::mutex> lk(cvLock);
        if(m_queue.size() > 100000) {
            return false;
        }
        m_queue.push_back(msg);
        cv.notify_all();
        return true;
    }
    MSGTYPE* getmsg()
    {
        std::unique_lock<std::mutex> lk(cvLock);
        if(m_queue.size() <= 0) {
            cv.wait(lk);
            if(m_queue.size() <= 0) {
                return nullptr;
            }
        }

        MSGTYPE* _pvalue = m_queue.front();
        if(_pvalue != nullptr) {
            m_queue.pop_front();
            return _pvalue;
        } else {
            return nullptr;
        }

    }
    MSGTYPE* getmsg_usec(const uint64_t time_us)
    {
        std::unique_lock<std::mutex> lk(cvLock);
        if(m_queue.size() <= 0) {
            cv.wait_for(lk, std::chrono::microseconds(time_us));
            if(m_queue.size() <= 0) {
                return nullptr;
            }
        }

        MSGTYPE* _pvalue = m_queue.front();
        if(_pvalue != nullptr) {
            m_queue.pop_front();
            return _pvalue;
        } else {
            return nullptr;
        }
    }

    MSGTYPE* getmsg_msec(uint64_t msec)
    {
        return getmsg_usec(msec * 1000);
    }

    uint32_t size()
    {
        std::unique_lock<std::mutex> lk(cvLock);
        return m_queue.size();
    }

private:
    std::deque<MSGTYPE*> m_queue;

    std::mutex cvLock;
    std::condition_variable cv;
};

}
