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

#include "fdb_define.h"

namespace fdb
{

void FdbKey::set(const char* key_data, int32_t key_size, int32_t bucket_number, int32_t area)
{
    if (!key_data || key_size <= 0) return ;

    destroy();
    data_size_ = sFdbKeyBucketSize + sFdbKeyAreaSize + key_size;
    data_ = new char[data_size_];
    encodeBucketNumber(data_, bucket_number);
    encodeArea(data_ + sFdbKeyBucketSize, area);
    memcpy(data_ + sFdbKeyBucketSize + sFdbKeyAreaSize, key_data, key_size);
    alloc_ = true;
}

void FdbKey::assign(char* data, const int32_t data_size)
{
    destroy();
    data_ = data;
    data_size_ = data_size;
    alloc_ = false;
}

void FdbKey::destroy()
{
    if (!alloc_ || !data_) return ;

    delete[] data_;
    data_ = nullptr;
    data_size_ = 0;
    alloc_ = false;
}

char* FdbKey::key()
{
    int32_t meta_size = sFdbKeyBucketSize + sFdbKeyAreaSize;
    return (data_ != nullptr) ? (data_ + meta_size) : nullptr;
}

int32_t FdbKey::keySize()
{
    int32_t meta_size = sFdbKeyBucketSize + sFdbKeyAreaSize;
    return (data_size_ > meta_size) ? (data_size_ - meta_size) : 0;
}

char* FdbKey::mergedKey()
{
    int32_t meta_size = sFdbKeyBucketSize;
    return (data_ != nullptr) ? (data_ + meta_size) : nullptr;
}

int32_t FdbKey::mergedKeySize()
{
    int32_t meta_size = sFdbKeyBucketSize;
    return (data_size_ > meta_size) ? (data_size_ - meta_size) : 0;
}

int32_t FdbKey::area()
{
    int32_t bucket_size = sFdbKeyBucketSize;
    char* buf = (data_ != nullptr ? data_ + bucket_size : nullptr);
    if (buf) {
        return (static_cast<int32_t>(static_cast<uint8_t>(buf[1])) << 8) | static_cast<uint8_t>(buf[0]);
    }
    return -1;
}

void FdbKey::encodeBucketNumber(char* buf, int32_t bucket_number)
{
    for (int32_t i = 0; i < sFdbKeyBucketSize; ++i) {
        buf[sFdbKeyBucketSize - i - 1] = (bucket_number >> (i*8)) & 0xFF;
    }
}

int32_t FdbKey::decodeBucketNumber(const char* buf)
{
    int32_t bucket_number = 0;
    for (int32_t i = 0; i < sFdbKeyBucketSize; ++i) {
        bucket_number |= static_cast<int32_t>(static_cast<uint8_t>(buf[i])) << ((sFdbKeyBucketSize - i - 1) * 8);
    }
    return bucket_number;
}

void FdbKey::encodeArea(char* buf, int32_t area)
{
    buf[0] = area & 0xff;
    buf[1] = (area >> 8) & 0xff;
}

void FdbKey::buildScanKey(int32_t bucket_number, std::string& start_key, std::string& end_key)
{
    char buf[sFdbKeyBucketSize] = {0};
    encodeBucketNumber(buf, bucket_number);
    start_key.assign(buf, sFdbKeyBucketSize);
    encodeBucketNumber(buf, bucket_number+1);
    end_key.assign(buf, sFdbKeyBucketSize);
}

void FdbKey::buildScanKeyWithArea(int32_t area, std::string& start_key, std::string& end_key)
{
    char buf[sFdbKeyBucketSize + sFdbKeyAreaSize] = {0};

    encodeBucketNumber(buf, 0);
    encodeArea(buf + sFdbKeyBucketSize, area);
    start_key.assign(buf, sizeof(buf));

    encodeBucketNumber(buf, sMaxBucketNumber);
    encodeArea(buf + sFdbKeyBucketSize, area + 1);
    end_key.assign(buf, sizeof(buf));
}


void ValueMeta::encodeToBuf(uint8_t* buf)
{
    buf[10] = (uint8_t)flag;

    buf[9] = (uint8_t)version;
    buf[8] = (uint8_t)version >> 8;

    buf[7] = (uint8_t)mdate;
    buf[6] = (uint8_t)(mdate >> 8);
    buf[5] = (uint8_t)(mdate >> 16);
    buf[4] = (uint8_t)(mdate >> 24);

    buf[3] = (uint8_t)edate;
    buf[2] = (uint8_t)(edate >> 8);
    buf[1] = (uint8_t)(edate >> 16);
    buf[0] = (uint8_t)(edate >> 24);
    return ;
}

void ValueMeta::decodeFromBuf(uint8_t* buf)
{
    edate = buf[0];
    edate <<= 8;
    edate |= buf[1];
    edate <<= 8;
    edate |= buf[2];
    edate <<= 8;
    edate |= buf[3];

    mdate = buf[4];
    mdate <<= 8;
    mdate |= buf[5];
    mdate <<= 8;
    mdate |= buf[6];
    mdate <<= 8;
    mdate |= buf[7];

    version = buf[8];
    version <<= 8;
    version |= buf[9];

    flag = buf[10];
    return ;
}


void FdbItem::set(const char* value_data, const int32_t value_size)
{
    if (!value_data || value_size <= 0) return ;

    int32_t real_meta_size = ValueMeta::size();
    destroy();

    data_size_ = value_size + real_meta_size;
    data_ = new char[data_size_];

    meta_.encodeToBuf((uint8_t*)data_);
    memcpy(data_ + real_meta_size, value_data, value_size);
    alloc_ = true;
}

void FdbItem::assign(char* data, const int32_t data_size)
{
    destroy();

    data_ = data;
    data_size_ = data_size;

    int32_t real_meta_size = ValueMeta::size();
    if (data_size >= real_meta_size) {
        meta_.decodeFromBuf((uint8_t*)data_);
    }
}

void FdbItem::destroy()
{
    if (!alloc_ || !data_) return ;

    delete[] data_;
    data_ = nullptr;
    data_size_ = 0;
    alloc_ = false;
}

char* FdbItem::value()
{
    if (!data_) return nullptr;
    return data_ + ValueMeta::size();
}

int32_t FdbItem::valueSize()
{
    if ( data_size_ <= ValueMeta::size() ) return 0;
    return ( data_size_ - ValueMeta::size() );
}

}
