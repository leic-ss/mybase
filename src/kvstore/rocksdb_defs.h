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

#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/options.h"
#include "rocksdb/types.h"
#include "common/defs.h"
#include "public/cast_helper.h"

#include <stdint.h>

namespace mybase
{

static const int32_t sRdbKeyBucketSize = 3;
static const int32_t sRdbKeyAreaSize = 2;
static const int32_t sMaxBucketNumber = (1 << 24) - 2;

class RocksdbKey
{
public:
    RocksdbKey() : data_(nullptr), data_size_(0), alloc_(false) {}
    RocksdbKey(const char* key_data, int32_t key_size, int32_t bucket_number, int32_t area)
            : data_(nullptr), data_size_(0), alloc_(false)
    {
        set(key_data, key_size, bucket_number, area);
    }

    ~RocksdbKey() { destroy(); }

    void set(const char* key_data, int32_t key_size, int32_t bucket_number, int32_t area);
    void assign(char* data, const int32_t data_size);
    void destroy();

    inline char*   data() { return data_; }
    inline int32_t size() { return data_size_; }

    char*   key();
    int32_t keySize();
    char*   mergedKey();
    int32_t mergedKeySize();
    int32_t area();

    static void encodeBucketNumber(char* buf, int32_t bucket_number);
    static int32_t decodeBucketNumber(const char* buf);

    inline int32_t getBucketNumber() { return decodeBucketNumber(data_); }

    static void encodeArea(char* buf, int32_t area);
    static int32_t decodeArea(const char* buf)
    {
        return _SC(int32_t, (_SC(uint8_t, buf[1]) << 8) | _SC(uint8_t, buf[0]));
    }

    static void buildScanKey(int32_t bucket_number, std::string& start_key, std::string& end_key);
    static void buildScanKeyWithArea(int32_t area, std::string& start_key, std::string& end_key);

private:
    char* data_;
    int32_t data_size_;
    bool alloc_;
};

struct ValueMeta {
  uint8_t  flag{0};
  uint16_t version{0};
  uint32_t mdate{0};
  uint32_t edate{0};

  void encodeToBuf(uint8_t* buf);
  void decodeFromBuf(uint8_t* buf);
  static uint32_t size() { return sizeof(uint8_t) + sizeof(uint16_t) + 2*sizeof(uint32_t); }
};


class RocksdbItem
{
public:
    RocksdbItem() : meta_(), data_(nullptr), data_size_(0), alloc_(false) {}
    ~RocksdbItem() { destroy(); }

    void set(const char* value_data, const int32_t value_size);
    void assign(char* data, const int32_t data_size);
    void destroy();

    inline char*   data() { return data_; }
    inline int32_t size() { return data_size_; }

    char*   value();
    int32_t valueSize();

    inline ValueMeta& meta() { return meta_; }
    inline uint32_t mdate() const { return meta_.mdate; }
    inline uint32_t edate() const { return meta_.edate; }
    inline uint16_t version() const { return meta_.version; }
    inline uint8_t  flag() const { return meta_.flag; }

private:
    ValueMeta meta_;
    char* data_;
    int32_t data_size_;
    bool alloc_;
};

}
