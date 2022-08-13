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

#include "common/filemapper.h"
#include "public/dlog.h"
#include "public/md5_encode.h"
#include "public/common.h"

#include <string>

namespace mybase
{

class SysMgr;
class TableMgr
{
public:
    friend class SysMgr;

    TableMgr();
    ~TableMgr();

    bool open(const std::string& file_name);
    bool create(const std::string& file_name, uint32_t bucket_count, uint32_t copy_count);
    std::string getFileName() const { return fileName; }
    void setLogger(mybase::BaseLogger* logger) { myLog = logger; mMapFile.setLogger(logger); }

    void close() { mMapFile.closeFile(); init(); }
    void sync() { mMapFile.syncFile(); }

    void printInfo() const;

    void getHashTableStr(const uint64_t* root, std::vector<std::string> & vec) const;
    std::string getHashTableStr(const uint64_t* root) const;
    void printHashTable(const uint64_t* root) const;

    void logPrintTable(uint64_t * root) const;

    void printTable() const;
    void getTableInfo(std::string& info);

    bool translateFromText2Binary(const std::string& src_file_name, const std::string& out_file_name);

    void deflateHashTable();
    void deflateHashTable(int32_t &hash_table_deflate_size, char *&hash_table_deflate_data, const char *hash_table, const int32_t table_count);

    char *getData() { return (char *)mMapFile.mData(); }
    int32_t getSize() { return mMapFile.getSize(); }

    bool setServerBucketCount(uint32_t bucket_count);
    bool setCopyCount(uint32_t copy_count);

    uint32_t getBucketCount() const { return serverBucketCount != nullptr ? *serverBucketCount : 0; }
    uint32_t getCopyCount() const { return serverCopyCount != nullptr ? *serverCopyCount : 0; }
    uint32_t getHashTableSize() const { return getBucketCount() * getCopyCount(); }
    uint32_t getHashTableByteSize() const { return getHashTableSize() * sizeof(uint64_t); }
    uint32_t getServerTableSize() const { return getHashTableByteSize() * 3 + sizeof(uint32_t) * 9; }
    uint32_t getMigDataSkip() const { return getHashTableSize(); }
    uint32_t getDestDataSkip() const { return getHashTableSize() * 2; }
    bool isFileOpened() const { return fileOpened; }

    uint64_t* getHashTable() { return hashTable; }
    uint64_t* getMHashTable() { return mHashTable; }
    uint64_t* getDHashTable() { return dHashTable; }

    bool backup(); //备份路由表文件
    void produceMd5(); //计算md5
    inline std::string getMd5() { return md5Str; }

public:
    int32_t *migrateBlockCount{nullptr};

private:
    TableMgr(const TableMgr &);
    TableMgr & operator=(const TableMgr&);
    void init();
    char *mapMetaData();

    uint32_t *flag{nullptr};
    uint32_t *clientVersion{nullptr};
    uint32_t *serverVersion{nullptr};
    uint32_t *pluginsVersion{nullptr};
    uint32_t *namespaceCapacityVersion{nullptr};
    uint32_t *lastLoadConfigTime{nullptr};
    mybase::BaseLogger* myLog{nullptr};

    uint64_t *hashTable{nullptr};
    uint64_t *mHashTable{nullptr};
    uint64_t *dHashTable{nullptr};
    uint32_t *serverCopyCount{nullptr};
    uint32_t *serverBucketCount{nullptr};

    char *hashTableDeflateDataForClient{nullptr}; // hashTable
    int32_t hashTableDeflateDataForClientSize{0};

    char *hashTableDeflateDataForDataServer{nullptr}; // hashTable + dHashTable
    int32_t hashTableDeflateDataForDataServerSize{0};
    bool fileOpened{false};

    mybase::FileMapper mMapFile;

    std::string fileName;
    std::string md5Str;
};

}
