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

#include "libnuraft/pp_util.hxx"
#include "libnuraft/ptr.hxx"

#include <cstdint>

namespace nuraft {

#define put16l(val, ptr) {          \
    ptr[0] = (val >>  0) & 0xff;    \
    ptr[1] = (val >>  8) & 0xff;    }

#define put16b(val, ptr) {          \
    ptr[1] = (val >>  0) & 0xff;    \
    ptr[0] = (val >>  8) & 0xff;    }

#define put32l(val, ptr) {          \
    ptr[0] = (val >>  0) & 0xff;    \
    ptr[1] = (val >>  8) & 0xff;    \
    ptr[2] = (val >> 16) & 0xff;    \
    ptr[3] = (val >> 24) & 0xff;    }

#define put32b(val, ptr) {          \
    ptr[3] = (val >>  0) & 0xff;    \
    ptr[2] = (val >>  8) & 0xff;    \
    ptr[1] = (val >> 16) & 0xff;    \
    ptr[0] = (val >> 24) & 0xff;    }

#define put64l(val, ptr) {          \
    ptr[0] = (val >>  0) & 0xff;    \
    ptr[1] = (val >>  8) & 0xff;    \
    ptr[2] = (val >> 16) & 0xff;    \
    ptr[3] = (val >> 24) & 0xff;    \
    ptr[4] = (val >> 32) & 0xff;    \
    ptr[5] = (val >> 40) & 0xff;    \
    ptr[6] = (val >> 48) & 0xff;    \
    ptr[7] = (val >> 56) & 0xff;    }

#define put64b(val, ptr) {          \
    ptr[7] = (val >>  0) & 0xff;    \
    ptr[6] = (val >>  8) & 0xff;    \
    ptr[5] = (val >> 16) & 0xff;    \
    ptr[4] = (val >> 24) & 0xff;    \
    ptr[3] = (val >> 32) & 0xff;    \
    ptr[2] = (val >> 40) & 0xff;    \
    ptr[1] = (val >> 48) & 0xff;    \
    ptr[0] = (val >> 56) & 0xff;    }


#define get16l(ptr, val) {              \
    uint16_t tmp = ptr[1]; tmp <<= 8;   \
    tmp |= ptr[0];                      \
    val = tmp;                          }

#define get16b(ptr, val) {              \
    uint16_t tmp = ptr[0]; tmp <<= 8;   \
    tmp |= ptr[1];                      \
    val = tmp;                          }

#define get32l(ptr, val) {              \
    uint32_t tmp = ptr[3]; tmp <<= 8;   \
    tmp |= ptr[2]; tmp <<= 8;           \
    tmp |= ptr[1]; tmp <<= 8;           \
    tmp |= ptr[0];                      \
    val = tmp;                          }

#define get32b(ptr, val) {              \
    uint32_t tmp = ptr[0]; tmp <<= 8;   \
    tmp |= ptr[1]; tmp <<= 8;           \
    tmp |= ptr[2]; tmp <<= 8;           \
    tmp |= ptr[3];                      \
    val = tmp;                          }

#define get64l(ptr, val) {              \
    uint64_t tmp = ptr[7]; tmp <<= 8;   \
    tmp |= ptr[6]; tmp <<= 8;           \
    tmp |= ptr[5]; tmp <<= 8;           \
    tmp |= ptr[4]; tmp <<= 8;           \
    tmp |= ptr[3]; tmp <<= 8;           \
    tmp |= ptr[2]; tmp <<= 8;           \
    tmp |= ptr[1]; tmp <<= 8;           \
    tmp |= ptr[0];                      \
    val = tmp;                          }

#define get64b(ptr, val) {              \
    uint64_t tmp = ptr[0]; tmp <<= 8;   \
    tmp |= ptr[1]; tmp <<= 8;           \
    tmp |= ptr[2]; tmp <<= 8;           \
    tmp |= ptr[3]; tmp <<= 8;           \
    tmp |= ptr[4]; tmp <<= 8;           \
    tmp |= ptr[5]; tmp <<= 8;           \
    tmp |= ptr[6]; tmp <<= 8;           \
    tmp |= ptr[7];                      \
    val = tmp;                          }

class buffer;
class BufferHelper {
public:
    enum endianness {
        LITTLE = 0x0,
        BIG = 0x1,
    };

    BufferHelper(buffer& src_buf, endianness endian = LITTLE);

    BufferHelper(ptr<buffer>& src_buf_ptr, endianness endian = LITTLE);

    __nocopy__(BufferHelper);

public:
    inline size_t pos() const { return pos_; }
    size_t size() const;

    void pos(size_t new_pos);
    void* data() const;

    void writeUInt8(uint8_t val);
    void writeUInt16(uint16_t val);
    void writeUInt32(uint32_t val);
    void writeUInt64(uint64_t val);

    void writeInt8(int8_t val);
    void writeInt16(int16_t val);
    void writeInt32(int32_t val);
    void writeInt64(int64_t val);

    void writeRaw(const void* raw_ptr, size_t len);
    void writeBuffer(const buffer& buf);
    void writeBytes(const void* raw_ptr, size_t len);
    void writeStr(const std::string& str);
    void writeCStr(const char* str);

    uint8_t readUInt8();
    uint16_t readUInt16();
    uint32_t readUInt32();
    uint64_t readUInt64();

    int8_t readInt8();
    int16_t readInt16();
    int32_t readInt32();
    int64_t readInt64();

    void* readRaw(size_t len);
    void readBuffer(ptr<buffer>& dst);
    void* readBytes(size_t& len);

    std::string readStr();
    const char* readCStr();

private:
    bool is_valid(size_t len) const;

    // Endianness.
    endianness endian_;

    // Reference to buffer to read or write.
    buffer& buf_;

    // Current position.
    size_t pos_;
};

}

