/************************************************************************
Modifications Copyright 2020 ~ 2021
Author: zhanglei
Email: shanshenshi@126.com

Original Copyright:
See URL: https://github.com/datatechnology/cornerstone
See URL: https://github.com/eBay/NuRaft

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

#ifndef _WAL_BUFFER_HXX_
#define _WAL_BUFFER_HXX_

#include <string>
#include <memory>

namespace walstore {

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
#endif //_BUFFER_HXX_
