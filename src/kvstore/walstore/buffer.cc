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

#include "buffer.h"

#include <cstring>
#include <iostream>
#include <stdint.h>
#include <assert.h>

namespace walstore {

Buffer::Buffer(uint8_t* wrapbuf, int32_t len)
{
    _wrapBuf = wrapbuf;
    _wrapLen = len;

    _pstart = _wrapBuf;
    _pend = _wrapBuf + _wrapLen;
    _pfree = _wrapBuf + _wrapLen;
    _pdata = _wrapBuf;
}

Buffer::~Buffer()
{
    if (!_pstart) return ;

    // be alloced by data buffer, need freeData
    if( _wrapBuf != _pstart) {
        ::free(_pstart);
    }
    _pend = _pfree = _pdata = _pstart = nullptr;
}

std::shared_ptr<Buffer> Buffer::alloc(const int32_t size)
{
    std::shared_ptr<Buffer> ptr(new Buffer());
    ptr->expand(size);

    return ptr;
}

char* Buffer::data()
{
    return (char*)_pstart;
}

int32_t Buffer::dataLen()
{
    return static_cast<int32_t>(_pfree - _pstart);
}

char* Buffer::curData()
{
    return (char*)_pdata;
}

int32_t Buffer::curDataLen()
{
    return static_cast<int32_t>(_pfree - _pdata);
}

char* Buffer::freeData()
{
    return (char*)_pfree;
}

int32_t Buffer::freeLen()
{
    return static_cast<int32_t>(_pend - _pfree);
}

bool Buffer::pos(uint32_t p)
{
    uint32_t dLen = _pfree - _pstart;
    if (p >= dLen) {
        return false;
    }

    _pdata = _pstart + p;
    return true;
}

bool Buffer::pourData(uint32_t len)
{
    if ( (_pend - _pfree) < len) return false;

    _pfree += len;
    return true;
}

void Buffer::writeInt16(uint16_t n)
{
    expand(2);
    _pfree[1] = (uint8_t)n;
    n = static_cast<uint16_t>(n >> 8);
    _pfree[0] = (uint8_t)n;
    _pfree += 2;
}

void Buffer::writeInt32(uint32_t n)
{
    expand(4);
    _pfree[3] = (uint8_t)n;
    n >>= 8;
    _pfree[2] = (uint8_t)n;
    n >>= 8;
    _pfree[1] = (uint8_t)n;
    n >>= 8;
    _pfree[0] = (uint8_t)n;
    _pfree += 4;
}

void Buffer::writeInt64(uint64_t n)
{
    expand(8);
    _pfree[7] = (uint8_t)n;
    n >>= 8;
    _pfree[6] = (uint8_t)n;
    n >>= 8;
    _pfree[5] = (uint8_t)n;
    n >>= 8;
    _pfree[4] = (uint8_t)n;
    n >>= 8;
    _pfree[3] = (uint8_t)n;
    n >>= 8;
    _pfree[2] = (uint8_t)n;
    n >>= 8;
    _pfree[1] = (uint8_t)n;
    n >>= 8;
    _pfree[0] = (uint8_t)n;
    _pfree += 8;
}

void Buffer::writeBytes(const void *src, int32_t len)
{
    expand(len);
    memcpy(_pfree, src, len);
    _pfree += len;
}

void Buffer::writeString(const std::string& str)
{
    int32_t len = str.size();
    expand(static_cast<int32_t>(len+sizeof(uint32_t)));

    writeInt32(len);
    writeBytes(str.data(), len);
}

uint8_t Buffer::readInt8()
{
    assert(_pdata + 1 <= _pfree);
    return (*_pdata++);
}

uint16_t Buffer::readInt16()
{
    assert(_pdata + 2 <= _pfree);
    uint16_t n = _pdata[0];
    n = static_cast<uint16_t>(n << 8);
    n = static_cast<uint16_t>(n | _pdata[1]);
    _pdata += 2;
    return n;
}

uint32_t Buffer::readInt32()
{
    assert(_pdata + 4 <= _pfree);
    uint32_t n = _pdata[0];
    n <<= 8;
    n |= _pdata[1];
    n <<= 8;
    n |= _pdata[2];
    n <<= 8;
    n |= _pdata[3];
    _pdata += 4;
    assert(_pfree>=_pdata);
    return n;
}

uint64_t Buffer::readInt64()
{
    assert(_pdata + 8 <= _pfree);
    uint64_t n = _pdata[0];
    n <<= 8;
    n |= _pdata[1];
    n <<= 8;
    n |= _pdata[2];
    n <<= 8;
    n |= _pdata[3];
    n <<= 8;
    n |= _pdata[4];
    n <<= 8;
    n |= _pdata[5];
    n <<= 8;
    n |= _pdata[6];
    n <<= 8;
    n |= _pdata[7];
    _pdata += 8;
    assert(_pfree>=_pdata);
    return n;
}

bool Buffer::readBytes(void *dst, int32_t len)
{
    if (_pdata + len > _pfree) {
        return false;
    }
    memcpy(dst, _pdata, len);
    _pdata += len;
    assert(_pfree>=_pdata);
    return true;
}

bool Buffer::readString(std::string& str)
{
    if (_pdata + sizeof(int32_t) > _pfree) {
        return false;
    }
    int32_t slen = readInt32();
    if (_pfree - _pdata < slen) {
        slen = static_cast<int32_t>(_pfree - _pdata);
    }

    if (slen > 0) {
        str.append((char*)_pdata, (uint64_t)slen);
    }
    _pdata += slen;
    assert(_pfree >= _pdata);
    return true;
}

void Buffer::expand(int32_t need)
{
    if (_pstart == nullptr) {
        int32_t len = 256;
        while (len < need) {
            len <<= 1;
        }
        _pfree = _pdata = _pstart = (uint8_t*)malloc(len);
        _pend = _pstart + len;
    } else if (_pend - _pfree < need) {
        int32_t flen = static_cast<int32_t>(_pend - _pfree);
        int32_t dlen = static_cast<int32_t>(_pfree - _pstart);

        if (flen < need || flen * 4 < dlen) {
            int32_t bufsize = static_cast<int32_t>((_pend - _pstart) * 2);
            while (bufsize - dlen < need) {
                bufsize <<= 1;
            }

            uint8_t* newbuf = (uint8_t *)malloc(bufsize);
            if (newbuf == nullptr) {
                // do nothing
            }
            assert(newbuf != nullptr);
            if (dlen > 0) {
                memcpy(newbuf, _pstart, dlen);
            }
            ::free(_pstart);

            uint32_t read_len = (_pdata - _pstart);
            _pstart = newbuf;
            _pdata = _pstart + read_len;

            _pfree = _pstart + dlen;
            _pend = _pstart + bufsize;
        } else {
            uint32_t read_len = (_pdata - _pstart);

            memmove(_pstart, _pdata, dlen);
            _pfree = _pstart + dlen;
            _pdata = _pstart + read_len;
        }
    }
}

}
