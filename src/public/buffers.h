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

#include "dlog.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>
#include <memory>

#define DEFAULT_MAX_NBUFFER_SIZE (1024*1024+256)   //1M + 256
#define DEFAULT_MAX_DATABUFFER_SIZE (2048)

namespace mybase {

/**
 * not thread safe, user meed to ensure thread safe
 */
class NBuffer
{
public:
    NBuffer(int32_t maxBufLen=DEFAULT_MAX_NBUFFER_SIZE);
    ~NBuffer();

    void release();
    bool copyBuf(const char * buf ,int32_t len);

    void expand(int32_t expandLen=4096);
    bool setNewSize(int32_t len);

    bool setMaxBuffLen(uint32_t max_buff_len);

    char* getBuff() const { return m_buf; }
    int32_t getLen() const { return m_len; }

    int32_t getFreeLen();

    int32_t getIndex() const { return m_index; }
    void setIndex(int32_t index) { m_index = index; }

    void addIndex(int32_t len);
    void shrinkFromFront(int32_t len);
    bool checkCanOp(int32_t len);

protected:
    bool reallocBuf(int32_t len);

private:
    char* m_buf;
    int32_t m_len;

    int32_t m_index;
    int32_t m_maxBufLen;
};

// TODO: optimize
class DataBuffer
{
public:
    DataBuffer();
    ~DataBuffer();

    DataBuffer(uint8_t* wrapbuf, int32_t len);

    void destroy();
    char* getData();
    char* getCurData();
    uint32_t getDataLen();
    char* getFree();
    int32_t getFreeLen();
    void drainData(int32_t len);
    void pourData(int32_t len);
    void stripData(int32_t len);

    bool expect(int32_t len);

    bool shrink();
    bool pos(uint32_t p);
    void clear();
    void writeInt8(uint8_t n);
    void writeInt16(uint16_t n);
    void writeInt32(uint32_t n);
    void writeInt64(uint64_t n);
    void writeBytes(const void* src, int32_t len);

    void fillInt8(uint8_t *dst, uint8_t n);
    void fillInt16(uint8_t *dst, uint16_t n);
    void fillInt32(uint8_t *dst, uint32_t n);  
    void fillInt64(uint8_t *dst, uint64_t n);

    void writeString(const char *str);
    void writeString(const std::string& str);

    void writeVector(const std::vector<int32_t>& v);
    void writeVector(const std::vector<uint32_t>& v);
    void writeVector(const std::vector<int64_t>& v);
    void writeVector(const std::vector<uint64_t>& v);

    uint8_t readInt8();
    uint16_t readInt16();
    uint32_t readInt32();
    uint64_t readInt64();

    bool readBytes(void *dst, int32_t len);
    bool readString(char *&str, int32_t len);
    bool readString(std::string& str);
    bool readVector(std::vector<int32_t>& v);
    bool readVector(std::vector<uint32_t>& v);
    bool readVector(std::vector<int64_t>& v);
    bool readVector(std::vector<uint64_t>& v);
    void ensureFree(int32_t len);

    int32_t findBytes(const char *findstr, int32_t len);

private:
    inline void expand(int32_t need);

private:
    uint8_t* _pstart;
    uint8_t* _pend;
    uint8_t* _pfree;
    uint8_t* _pdata;

    uint8_t* _wrapBuf;
    uint32_t _wrapLen;
};

class CBufferMgn
{
public:
    CBufferMgn() { bzero(m_md5, sizeof(m_md5)); }
    CBufferMgn(const CBufferMgn& lft ) { deepCopy(lft); }
    ~CBufferMgn() { release(); }

    CBufferMgn& operator=(const CBufferMgn& lft);

    void append(char ch);
    void append(const char * str, int32_t len = 0 );

    const char* c_str() const { return m_buf; }
    int32_t getLen() const { return m_pos; }
    const uint8_t* getMd5() const { return m_md5; }

    bool writeToFile(const char* filename);
    bool readFromFile(const char* filename);

protected:
    void deepCopy(const CBufferMgn & lft);
    void release();

private:
    mybase::BaseLogger* myLog{nullptr};
    char* m_buf{nullptr};
    uint8_t m_md5[36];
    int32_t m_pos{0};
    int32_t m_size{0};
};

class Buffer
{
public:
    using ptr = std::shared_ptr<Buffer>;

public:
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(uint8_t* wrapbuf, int32_t len);
    ~Buffer();

private:
    Buffer() {}

public:
    static std::shared_ptr<Buffer> alloc(const int32_t size = 32);

public:
    char* data();
    int32_t dataLen();

    char* curData();
    int32_t curDataLen();

    char* freeData();
    int32_t freeLen();

    bool pos(uint32_t p);
    bool pourData(uint32_t len);

    void writeInt8(uint8_t n);
    void writeInt16(uint16_t n);
    void writeInt32(uint32_t n);
    void writeInt64(uint64_t n);

    void writeBytes(const void* src, int32_t len);
    void writeString(const std::string& str);

    void fillInt8(uint8_t *dst, uint8_t n);
    void fillInt16(uint8_t *dst, uint16_t n);
    void fillInt32(uint8_t *dst, uint32_t n);
    void fillInt64(uint8_t *dst, uint64_t n);

    uint8_t readInt8();
    uint16_t readInt16();
    uint32_t readInt32();
    uint64_t readInt64();

    bool readBytes(void *dst, int32_t len);
    bool readString(std::string& str);

private:
    void expand(int32_t need);

private:
    uint8_t* _pstart{nullptr};
    uint8_t* _pend{nullptr};
    uint8_t* _pfree{nullptr};
    uint8_t* _pdata{nullptr};

    uint8_t* _wrapBuf{nullptr};
    uint32_t _wrapLen{0};
};

}
