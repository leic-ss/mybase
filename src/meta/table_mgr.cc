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

#include "table_mgr.h"
#include "common/defs.h"

#include <zlib.h>
#include <sstream>

namespace mybase
{

void TableMgr::init()
{
    flag = clientVersion = serverVersion = serverBucketCount = serverCopyCount = nullptr;
    pluginsVersion = namespaceCapacityVersion = lastLoadConfigTime = nullptr;
    migrateBlockCount = nullptr;
    hashTable = mHashTable = dHashTable = nullptr;

    hashTableDeflateDataForClient = hashTableDeflateDataForDataServer = nullptr;
    hashTableDeflateDataForClientSize = hashTableDeflateDataForDataServerSize = 0;
    fileOpened = false;
}

TableMgr::TableMgr()
{
    init();
}

TableMgr::~TableMgr()
{
    if(hashTableDeflateDataForClient) {
        free(hashTableDeflateDataForClient);
    }
    if(hashTableDeflateDataForDataServer) {
        free(hashTableDeflateDataForDataServer);
    }
    close();
}

void TableMgr::logPrintTable(uint64_t * root) const
{
    std::vector<std::string> vec;
    getHashTableStr(root, vec);

    for(auto iter = vec.begin(); iter != vec.end(); ++iter) {
        if (iter->empty()) continue;
        _log_info(myLog, "%.*s", iter->size() - 1, iter->data());
    }
}

void TableMgr::produceMd5() //初始化或者备份的时候调用
{
    char m_md5[32 + 1] = {0};
    MD5AndEncode((const uint8_t*)mMapFile.mData(), mMapFile.getSize(), (uint8_t*)m_md5);
    md5Str = m_md5;
}

void TableMgr::printTable() const
{
    printInfo();
    printf("--------------------hashtable-----------------------\n");
    printHashTable(hashTable);
    printf("--------------------Mhashtable-----------------------\n");
    printHashTable(mHashTable);
    printf("--------------------Dhashtable-----------------------\n");
    printHashTable(dHashTable);
}

void TableMgr::getTableInfo(std::string& info)
{
    if(mMapFile.mData() == nullptr) {
        info.append("please open table first!");
        return;
    }

    info.append("copy_count: ").append(std::to_string(*serverCopyCount)).append("\n");
    info.append("bucket_count: ").append(std::to_string(*serverBucketCount)).append("\n");
    info.append("flag: ").append(std::to_string(*flag)).append("\n");
    info.append("clientVersion: ").append(std::to_string(*clientVersion)).append("\n");
    info.append("serverVersion: ").append(std::to_string(*serverVersion)).append("\n");
    info.append("pluginsVersion: ").append(std::to_string(*pluginsVersion)).append("\n");
    info.append("capacityVersion: ").append(std::to_string(*namespaceCapacityVersion)).append("\n");
    info.append("lastLoadConfigTime: ").append(std::to_string(*lastLoadConfigTime)).append("\n");
    info.append("migrateBlockCount: ").append(std::to_string(*migrateBlockCount)).append("\n");

    if (hashTable) {
        info.append("\n");
        info.append("--------------------hashtable-----------------------\n");
        info.append(getHashTableStr(hashTable));
    }
    if (mHashTable) {
        info.append("\n");
        info.append("--------------------Mhashtable-----------------------\n");
        info.append(getHashTableStr(mHashTable));
    }
    if (dHashTable) {
        info.append("\n");
        info.append("--------------------Dhashtable-----------------------\n");
        info.append(getHashTableStr(dHashTable));
    }

    return ;
}

void TableMgr::printInfo() const
{
    if(mMapFile.mData() == nullptr) {
        printf("open table first");
        return;
    }
    printf("copy_count:%u\nbucket_count:%u\n", *serverCopyCount, *serverBucketCount);
    printf("flag:%u\nclientVersion:%u\nserverVersion:%u\npluginsVersion:%u\ncapacityVersion:%u\n",
            *flag, *clientVersion, *serverVersion, *pluginsVersion, *namespaceCapacityVersion);
    printf("lastLoadConfigTime:%u\nmigrateBlockCount:%d\n", *lastLoadConfigTime, *migrateBlockCount);

    return ;
}

void TableMgr::getHashTableStr(const uint64_t* root, std::vector<std::string>& vec) const
{
    if(root == nullptr) {
        return ;
    }

    for(uint32_t i = 0; i < getBucketCount(); i++) {
        char tmpbuf[1024]={0};
        int32_t index = 0;
        index += snprintf(tmpbuf+index, sizeof(tmpbuf) - index, "bucket: %u", i);

        for(uint32_t j = 0; j < getCopyCount(); ++j) {
            index += snprintf(tmpbuf + index, sizeof(tmpbuf) - index, "\t%s",
                    NetHelper::addr2String(root[j * getBucketCount() + i]).c_str());
        }

        index += snprintf(tmpbuf + index, sizeof(tmpbuf) - index, "\n");
        vec.push_back(tmpbuf);
    }

    return ;
}


std::string TableMgr::getHashTableStr(const uint64_t* root) const
{
    if(root == nullptr) {
        return std::string();
    }

    std::vector<std::string> vec;
    getHashTableStr(root, vec);

    std::string ret;
    for(auto iter = vec.begin(); iter != vec.end(); ++iter) {
        ret += *iter;
    }

    return ret;
}

void TableMgr::printHashTable(const uint64_t * root) const
{
    if(root == nullptr) {
        return;
    }

    std::string result = getHashTableStr(root);
}

bool TableMgr::translateFromText2Binary(const std::string& src_file_name, const std::string & out_file_name)
{
    FILE *fp;
    char buf[4096];
    uint64_t *p_table = nullptr;
    if((fp = fopen(src_file_name.c_str(), "r")) == nullptr) {
        return false;
    }
    uint32_t copyCount = 0;
    uint32_t serverBucketCount = 0;
    while(!feof(fp) && (fgets(buf, 4096, fp)))
    {
        char *p = nullptr;
        p = strstr(buf, "copy_count:");
        if(p != nullptr) {
            copyCount = strtoul(p + strlen("copy_count:"), NULL, 10);
            continue;
        }
        p = strstr(buf, "bucket_count:");
        if(p != nullptr) {
            serverBucketCount = strtoul(p + strlen("bucket_count:"), NULL, 10);
            if(!this->create(out_file_name, serverBucketCount, copyCount)) {
                return false;
            }
            printf(" got copy_count=%d bucket=%d\n", copyCount, serverBucketCount);
            continue;
        }

        p = strstr(buf, "-hashtable");
        if(p != NULL)
        {
            p_table = hashTable;
            continue;
        }
        p = strstr(buf, "-Mhashtable");
        if(p != NULL)
        {
            p_table = mHashTable;
            continue;
        }
        p = strstr(buf, "-Dhashtable");
        if(p != NULL)
        {
            p_table = dHashTable;
            continue;
        }

        p = strstr(buf, "flag:");
        if(p != NULL)
        {
            *flag = strtoul(p + strlen("flag:"), NULL, 10);
        }
        p = strstr(buf, "clientVersion:");
        if(p != NULL)
        {
            *clientVersion = strtoul(p + strlen("clientVersion:"), NULL, 10);
            continue;
        }
        p = strstr(buf, "serverVersion:");
        if(p != NULL)
        {
            *serverVersion = strtoul(p + strlen("serverVersion:"), NULL, 10);
            continue;
        }
        p = strstr(buf, "pluginsVersion:");
        if(p != NULL)
        {
            *pluginsVersion = strtoul(p + strlen("pluginsVersion:"), NULL, 10);
            continue;
        }
        p = strstr(buf, "capacityVersion:");
        if(p != NULL)
        {
            *namespaceCapacityVersion = strtoul(p + strlen("capacityVersion:"), NULL, 10);
            continue;
        }
        p = strstr(buf, "lastLoadConfigTime:");
        if(p != NULL)
        {
            *lastLoadConfigTime = strtoul(p + strlen("lastLoadConfigTime:"), NULL, 10);
            continue;
        }
        p = strstr(buf, "migrateBlockCount:");
        if(p != NULL)
        {
            *migrateBlockCount = strtoul(p + strlen("migrateBlockCount:"), NULL, 10);
            continue;
        }
        p = strstr(buf, "bucket:");
        if(p != NULL) {
            int bucket = 0;
            int line = 0;
            uint64_t serverid = 0;
            p += strlen("bucket:");
            bucket = strtol(p, NULL, 10);

            p = strstr(p, "line:");
            assert(p != NULL);
            p += strlen("line:");
            line = strtol(p, NULL, 10);

            p = strstr(p, "ip:");
            assert(p != NULL);
            p += strlen("ip:");
            serverid = NetHelper::str2Addr(p, 0);
            p_table[line * getBucketCount() + bucket] = serverid;
        }
    }
    sync();

    fclose(fp);
    return true;
}

char *TableMgr::mapMetaData()
{
    char *data = (char *) mMapFile.mData();
    assert(data != nullptr);

    flag = (uint32_t *) data;
    data += sizeof(uint32_t);
    clientVersion = (uint32_t *) data;
    data += sizeof(uint32_t);
    serverVersion = (uint32_t *) data;
    data += sizeof(uint32_t);
    pluginsVersion = (uint32_t *) data;
    data += sizeof(uint32_t);
    namespaceCapacityVersion = (uint32_t *) data;
    data += sizeof(uint32_t);
    lastLoadConfigTime = (uint32_t *) data;
    data += sizeof(uint32_t);
    migrateBlockCount = (int32_t *) data;
    data += sizeof(uint32_t);
    serverCopyCount = (uint32_t *) data;
    data += sizeof(uint32_t);
    serverBucketCount = (uint32_t *) data;
    data += sizeof(uint32_t);
    return data;
}

bool TableMgr::create(const std::string& file_name, uint32_t bucket_count, uint32_t copy_count)
{
    _log_info(myLog, "will create group server table");
    int32_t fok = access(file_name.c_str(), F_OK);
    if(fok == 0) {
        return false;
    }

    close();
    serverBucketCount = &bucket_count;
    serverCopyCount = &copy_count;
    if(mMapFile.openFile(file_name.c_str(), getServerTableSize()) == false) {
        serverBucketCount = serverCopyCount = nullptr;
        _log_err(myLog, "openFile failed! file_name[%s] size[%d]", file_name.c_str(), getServerTableSize());
        return false;
    }
    char *data = mapMetaData();

    *flag = sKvHtmVersion;
    *clientVersion = 1;
    *serverVersion = 1;
    *pluginsVersion = 1;
    *namespaceCapacityVersion = 1;
    (*migrateBlockCount) = -1;
    *serverCopyCount = copy_count;
    *serverBucketCount = bucket_count;

    hashTable = (uint64_t*)data;
    mHashTable = hashTable + getMigDataSkip();
    dHashTable = hashTable + getDestDataSkip();
    memset(data, 0, getHashTableByteSize());
    sync();
    fileOpened = true;
    _log_debug(myLog, "create %s ok", file_name.c_str());

    produceMd5();
    return true;
}

bool TableMgr::open(const std::string& file_name)
{
    fileName = file_name;
    
    close();
    int32_t fok = access(file_name.c_str(), F_OK);
    if(fok != 0) {
        _log_warn(myLog, "file (%s) not exist!", file_name.c_str());
        return false;
    }
    if(mMapFile.openFile(file_name.c_str()) == false) {
        return false;
    }

    char *data = mapMetaData();
    if(sKvHtmVersion != *flag) {
        _log_err(myLog, "flag error! %u != %u", sKvHtmVersion, *flag);
        close();
        return false;
    }

    _log_debug(myLog, "CConfServerTableManager:open file_name %s start, client version: %d, server version: %d, plugins version:%d",
               file_name.c_str(), *clientVersion, *serverVersion, *pluginsVersion);
    uint32_t size = getServerTableSize();

    _log_debug(myLog, "will reopen it size = %d", size);
    close();
    if(mMapFile.openFile(file_name.c_str(), size) == false) {
        return false;
    }
    data = mapMetaData();

    hashTable = (uint64_t *) data;
    mHashTable = hashTable + getMigDataSkip();
    dHashTable = hashTable + getDestDataSkip();
    fileOpened = true;

    produceMd5();
    return true;
}

void TableMgr::deflateHashTable()
{
    char *p_tmp_table = (char *) malloc(getHashTableByteSize() * 2);
    assert(p_tmp_table != nullptr);

    memcpy(p_tmp_table, (const char *) hashTable, getHashTableByteSize());
    memcpy(p_tmp_table + getHashTableByteSize(), (const char *) dHashTable, getHashTableByteSize());
    deflateHashTable(hashTableDeflateDataForClientSize, hashTableDeflateDataForClient, p_tmp_table, 1);
    deflateHashTable(hashTableDeflateDataForDataServerSize, hashTableDeflateDataForDataServer, p_tmp_table, 2);

    free(p_tmp_table);
}

void TableMgr::deflateHashTable(int32_t &hash_table_deflate_size, char *&hash_table_deflate_data,
                                          const char *hash_table, const int32_t table_count)
{
    hash_table_deflate_size = 0;
    if(hash_table_deflate_data) {
        free(hash_table_deflate_data);
        hash_table_deflate_data = nullptr;
    }

    int32_t index = getHashTableSize() * table_count;
    uint64_t source_len = index * sizeof(uint64_t);
    // uint64_t dest_len = compressBound(source_len);
    // uint8_t* dest = (uint8_t*) malloc(dest_len);

    hash_table_deflate_data = (char *) malloc(source_len);
    memcpy(hash_table_deflate_data, hash_table, source_len);
    hash_table_deflate_size = source_len;

    // int32_t ret = compress(dest, &dest_len, (uint8_t*) hash_table, source_len);
    // if(ret == Z_OK) {
    //     hash_table_deflate_data = (char *) malloc(dest_len);
    //     hash_table_deflate_size = dest_len;
    //     memcpy(hash_table_deflate_data, dest, hash_table_deflate_size);

    //     _log_info(myLog, "[%s] hashCount: %d, compress: %d => %d", fileName.c_str(), index, source_len, dest_len);
    // } else {
    //     _log_err(myLog, "[%s] compress error: %d", fileName.c_str(), ret);
    // }
    // free(dest);
    return ;
}

bool TableMgr::setServerBucketCount(uint32_t bucket_count)
{
    if(serverBucketCount == nullptr || *serverBucketCount != 0) {
        return false;
    }

    *serverBucketCount = bucket_count;
    return true;
}

bool TableMgr::setCopyCount(uint32_t copy_count)
{
    if(serverCopyCount == nullptr || *serverCopyCount != 0) {
        return false;
    }
    *serverCopyCount = copy_count;
    return true;
}

bool TableMgr::backup()
{
    produceMd5();

    std::string parentDir = SysMgrHelper::getDirPart(fileName);
    std::string dir = parentDir + "/bak";

    SysMgrHelper::mkdirRecursive(dir, 0755);

    std::string tmpfile = dir + "/" + SysMgrHelper::getFileNameFromFullPath(fileName)+".bak";

    int32_t fd = ::open(tmpfile.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if ( fd < 0 ) {
        _log_err(myLog, "fopen file[%s] for write error[%d:%s]",tmpfile.c_str(),errno,strerror(errno));
        return false;
    }
    
    int32_t writelen = 0 ;
    char * ptr = (char*)(mMapFile.mData());
    int32_t len = mMapFile.getSize();

    while(writelen < len ) {
        int32_t wlen = (len-writelen)>=4096?4096:(len-writelen);
        wlen = FileHelper::writen(fd, (void*)(ptr+writelen), wlen);
        if( wlen <= 0 ) {
            _log_err(myLog, "write file[%s] error[%d<%d][%d,%s]",tmpfile.c_str(),writelen,len,errno,strerror(errno));
            break;
        }
        writelen += wlen;
    }

    ::close(fd);

    if ( writelen != len ) {
        return false;
    }

    char backfilename[256]={0};
    time_t t = time(nullptr);
    struct tm ltm;
    localtime_r(&t, &ltm);
    if( -1 == *migrateBlockCount ) {
        snprintf(backfilename, sizeof(backfilename), "%s.%u.%04d%02d%02d%02d%02d%02d.ok",
                 tmpfile.c_str(), *serverVersion, ltm.tm_year+1900, ltm.tm_mon+1, ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
    } else {
        snprintf(backfilename, sizeof(backfilename), "%s.%u.%04d%02d%02d%02d%02d%02d",
                 tmpfile.c_str(), *serverVersion, ltm.tm_year+1900, ltm.tm_mon+1, ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
    }

    if( 0 != rename(tmpfile.c_str(),backfilename) ) {
        _log_err(myLog, "rename file[%s->%s] error[%d:%s]", tmpfile.c_str(), backfilename, errno, strerror(errno));
    } else {
        _log_info(myLog, "rename file[%s->%s] success", tmpfile.c_str(), backfilename);
    }

    return true;
}

}
