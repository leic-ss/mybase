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

#include "libnuraft/buffer.hxx"
#include "buffer_helper.h"

#include <cstring>
#include <stdexcept>

#include <assert.h>

#define chk_length(val) \
    if ( !is_valid( sizeof(val) ) ) throw std::overflow_error("not enough space")

namespace nuraft {

BufferHelper::BufferHelper(buffer& src_buf,
                                     BufferHelper::endianness endian)
    : endian_(endian)
    , buf_(src_buf)
    , pos_(0)
{}

BufferHelper::BufferHelper(ptr<buffer>& src_buf_ptr,
                                     BufferHelper::endianness endian)
    : endian_(endian)
    , buf_(*src_buf_ptr)
    , pos_(0)
{}

size_t BufferHelper::size() const {
    return buf_.size();
}

void BufferHelper::pos(size_t new_pos) {
    if (new_pos > buf_.size()) throw std::overflow_error("invalid position");
    pos_ = new_pos;
}

void* BufferHelper::data() const {
    uint8_t* ptr = (uint8_t*)buf_.data_begin();
    return ptr + pos();
}

bool BufferHelper::is_valid(size_t len) const {
    if ( pos() + len > buf_.size() ) return false;
    return true;
}

void BufferHelper::writeUInt8(uint8_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    ptr[0] = val;
    pos( pos() + sizeof(val) );
}

void BufferHelper::writeUInt16(uint16_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { put16l(val, ptr); }
    else                    { put16b(val, ptr); }
    pos( pos() + sizeof(val) );
}

void BufferHelper::writeUInt32(uint32_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { put32l(val, ptr); }
    else                    { put32b(val, ptr); }
    pos( pos() + sizeof(val) );
}

void BufferHelper::writeUInt64(uint64_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { put64l(val, ptr); }
    else                    { put64b(val, ptr); }
    pos( pos() + sizeof(val) );
}

void BufferHelper::writeInt8(int8_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    ptr[0] = val;
    pos( pos() + sizeof(val) );
}

void BufferHelper::writeInt16(int16_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { put16l(val, ptr); }
    else                    { put16b(val, ptr); }
    pos( pos() + sizeof(val) );
}

void BufferHelper::writeInt32(int32_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { put32l(val, ptr); }
    else                    { put32b(val, ptr); }
    pos( pos() + sizeof(val) );
}

void BufferHelper::writeInt64(int64_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { put64l(val, ptr); }
    else                    { put64b(val, ptr); }
    pos( pos() + sizeof(val) );
}

void BufferHelper::writeRaw(const void* raw_ptr, size_t len) {
    if ( !is_valid(len) ) {
        assert(false);
        throw std::overflow_error("not enough space");
    }
    memcpy(data(), raw_ptr, len);
    pos( pos() + len );
}

void BufferHelper::writeBuffer(const buffer& buf) {
    size_t len = buf.size() - buf.pos();
    writeRaw(buf.data(), len);
}

void BufferHelper::writeBytes(const void* raw_ptr, size_t len) {
    if ( !is_valid(len + sizeof(uint32_t)) ) {
        assert(false);
        throw std::overflow_error("not enough space");
    }
    writeUInt32(len);
    writeRaw(raw_ptr, len);
}

void BufferHelper::writeStr(const std::string& str) {
    writeBytes( str.data(), str.size() );
}

void BufferHelper::writeCStr(const char* str) {
    size_t local_pos = pos_;
    size_t buf_size = buf_.size();
    char* ptr = (char*)buf_.data_begin();

    size_t ii = 0;
    while (str[ii] != 0x0) {
        if (local_pos >= buf_size) {
            assert(false);
            throw std::overflow_error("not enough space");
        }
        ptr[local_pos] = str[ii];
        local_pos++;
        ii++;
    }
    // Put NULL character at the end.
    if (local_pos >= buf_size) {
        assert(false);
        throw std::overflow_error("not enough space");
    }
    ptr[local_pos++] = 0x0;
    pos( local_pos );
}

uint8_t BufferHelper::readUInt8() {
    uint8_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    ret = ptr[0];
    pos( pos() + sizeof(ret) );
    return ret;
}

uint16_t BufferHelper::readUInt16() {
    uint16_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { get16l(ptr, ret); }
    else                    { get16b(ptr, ret); }
    pos( pos() + sizeof(ret) );
    return ret;
}

uint32_t BufferHelper::readUInt32() {
    uint32_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { get32l(ptr, ret); }
    else                    { get32b(ptr, ret); }
    pos( pos() + sizeof(ret) );
    return ret;
}

uint64_t BufferHelper::readUInt64() {
    uint64_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { get64l(ptr, ret); }
    else                    { get64b(ptr, ret); }
    pos( pos() + sizeof(ret) );
    return ret;
}

int8_t BufferHelper::readInt8() {
    int8_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    ret = ptr[0];
    pos( pos() + sizeof(ret) );
    return ret;
}

int16_t BufferHelper::readInt16() {
    int16_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { get16l(ptr, ret); }
    else                    { get16b(ptr, ret); }
    pos( pos() + sizeof(ret) );
    return ret;
}

int32_t BufferHelper::readInt32() {
    int32_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { get32l(ptr, ret); }
    else                    { get32b(ptr, ret); }
    pos( pos() + sizeof(ret) );
    return ret;
}

int64_t BufferHelper::readInt64() {
    int64_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { get64l(ptr, ret); }
    else                    { get64b(ptr, ret); }
    pos( pos() + sizeof(ret) );
    return ret;
}

void* BufferHelper::readRaw(size_t len) {
    uint8_t* ptr = buf_.data_begin() + pos_;
    pos( pos() + len );
    return ptr;
}

void BufferHelper::readBuffer(ptr<buffer>& dst) {
    size_t len = dst->size() - dst->pos();
    void* ptr = readRaw(len);
    ::memcpy(dst->data(), ptr, len);
}

void* BufferHelper::readBytes(size_t& len) {
    len = readUInt32();
    if ( !is_valid(len) ) {
        assert(false);
        throw std::overflow_error("not enough space");
    }
    return readRaw(len);
}

std::string BufferHelper::readStr() {
    size_t len = 0;
    void* data = readBytes(len);
    if (!data) return std::string();
    return std::string((const char*)data, len);
}

const char* BufferHelper::readCStr() {
    char* ptr = (char*)buf_.data_begin() + pos_;
    size_t len = strlen(ptr);
    pos( pos() + len + 1 );
    return ptr;
}

}

