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

#include "dlog.h"
#include "kv/image_file.h"
#include "kv_client_api.h"
#include "asynclimit.h"

#include "kv_packet_request_get_group.h"

#include <sstream>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

void usage(char *exe)
{
    printf("Usage: %s -S slave -G group -L local_id -f image_file -p speed -m master -s slave -g group -o log_file | -h\n", exe);
}

class LoaderHandler {
public:
    LoaderHandler() : streamer(&factory), syncNetPacketMgn(&factory, &streamer), loader() {}
    ~LoaderHandler() { }

    void setImageFile(const std::string& _image_file) { imageFile = _image_file; }
    void setSpeed(uint32_t _speed) { speed = _speed; }
    void setLocalId(uint64_t local_id) { localId = local_id; }

    void setMaster(std::string master_configserver) { masterConfigserver = master_configserver; }
    void setGroup(std::string group_name) { groupName = group_name; }

    bool retrieveServerAddr(std::string slave, std::string group)
    {
        kv::KvRequestGetGroup packet;
        packet.setGroupName(group.c_str());
        packet.setChannelId(1);

        auto tpacket = syncNetPacketMgn.sendAndRecv(NetHelper::str2Addr(slave), &packet);
        if (!tpacket || tpacket->getPCode() != kv::KV_RESP_GET_GROUP_PACKET) {
            log_error("get group packet failed, retry");
            tpacket.reset();
            return false;
        }

        kv::KvResponseGetGroup* rggp = _SC(kv::KvResponseGetGroup*, tpacket.get());
        if (rggp->config_version <= 0) {
            log_error("group doesn't exist: %s, Be sure you are Authrized! configVersion[%d]",
                      groupName.c_str(), rggp->config_version);
            return false;
        }

        bucketCount = rggp->bucket_count;
        copyCount = rggp->copy_count;

        if (bucketCount <= 0 || copyCount <= 0) {
            log_error("bucket or copy count doesn't correct, Be sure you are Authrized! "
                      "bucketCount[%d] copyCount[%d]", bucketCount, copyCount);
            return false;
        }

        uint64_t *server_list = rggp->getServerList(bucketCount, copyCount);
        if (rggp->server_list_count <= 0 || !server_list) {
            log_error("server table is empty");
            return false;
        }

        uint32_t server_list_count = rggp->server_list_count;
        log_info("server_list_count: %d", server_list_count);

        if (server_list_count != bucketCount * copyCount) {
            log_error("server table is wrong, server_list_count: %u, bucketCount: %u, copyCount: %u",
                     server_list_count, bucketCount, copyCount);
            return false;
        }

        for (uint32_t i = 0; server_list != 0 && i < server_list_count; ++i) {
            log_debug("server table: [%d] => [%s]", i, NetHelper::addr2String(server_list[i]).c_str());
            myServerList.push_back(server_list[i]);
        }
        return true;
    }

    void image_entry_handler(kv::ImageEntry* entry)
    {
        kv::DataEntry key;
        kv::DataEntry value;

        entry->toKeyValue(key, value);
        int32_t area = key.decodeArea();

        std::string _key(key.getData(), key.getSize());
        std::string _value(value.getData(), value.getSize());

        std::vector<uint64_t> server_list;
        if (!getServerId(_key, server_list)) {
            log_error("can not find serverId, return false");
            return ;
        }

        if (localId != server_list[0]) {
            return ;
        }

        limit->limitrate();

        int32_t rc = apiImpl.asyncPut(nextSeq++, area, _key, _value, entry->m_metaInfo.edate, 0);
        if (rc != 0) {
            log_error("put failed! area[%d] key[%.*s] vsize[%d] edate[%u] rc[%d]",
                      area, key.getSize(), key.getData(), value.getSize(), entry->m_metaInfo.edate, rc);
        } else {
            log_debug("put success! area[%d] key[%.*s] vsize[%d] edate[%u]",
                      area, key.getSize(), key.getData(), value.getSize(), entry->m_metaInfo.edate);
        }
        return ;
    }

    bool process()
    {
        loader.setLogger(sDefLogger);
        apiImpl.setLogger(sDefLogger);

        if (!apiImpl.startup(StringHelper::tokenize(masterConfigserver, ","), groupName)) {
            log_error("startup failed!");
            return false;
        }

        log_info("startup success! master[%s] slave[%s] group[%s]", masterConfigserver.c_str(), groupName.c_str());

        auto func = std::bind(&LoaderHandler::image_entry_handler, this, std::placeholders::_1);
        loader.setHandler(func);

        bool rc = loader.init(imageFile.c_str());
        if ( !rc ) {
            log_error("load image failed! file[%s]", imageFile.c_str());
            return false;
        }

        log_info("load image file success! file[%s]", imageFile.c_str());

        limit = new mybase::CAsyncLimit(speed);

        loader.deal();
        return false;
    }

    bool getServerId(const std::string& key, std::vector<uint64_t>& servers)
    {
        uint32_t hash = UtilHelper::murMurHash(key.data(), key.size());
        servers.clear();

        if (myServerList.empty()) return false;

        hash %= bucketCount;
        for (uint32_t i = 0; i < copyCount && i < myServerList.size(); ++i) {
            uint64_t server_id = myServerList[hash + i * bucketCount];
            if (server_id != 0) {
                servers.push_back(server_id);
            }
        }

        return servers.empty() ? false : true;
    }

private:
    kv::KvPacketFactory factory;
    kv::KvPacketStreamer streamer;
    mybase::SyncNetPacketMgn syncNetPacketMgn;
    std::vector<uint64_t> myServerList;

    uint32_t bucketCount;
    uint32_t copyCount;

    std::string imageFile;
    kv::CImageLoadDeal loader;
    uint32_t speed{0};
    uint64_t localId{0};

    std::string masterConfigserver;
    std::string groupName;

    mybase::CAsyncLimit* limit;
    kv::KvClientApiImpl apiImpl;
    std::atomic<uint32_t> nextSeq{0};
};

int32_t main(int argc, char *argv[])
{
    LoaderHandler loader;

    std::string image_file;
    static struct option long_options[] =
    {
        {"Slave", required_argument, 0, 'S'},
        {"Group", required_argument, 0, 'G'},
        {"local_id", required_argument, 0, 'L'},
        {"image_file", required_argument, 0, 'f'},
        {"speed", optional_argument, 0, 'p'},
        {"master", required_argument, 0, 'm'},
        {"group", required_argument, 0, 'g'},
        {"output", optional_argument, 0, 'o'},
        {"loglevel", optional_argument, 0, 'l'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    std::string master_configserver;
    std::string slave_configserver;
    std::string group;
    std::string log_file;
    std::string log_level = "info";

    std::string SlaveConfigserver;
    std::string GroupName;

    int32_t opt;
    while ((opt = getopt_long(argc, argv, "S:G:L:f:p:m:g:o:l:h", long_options, nullptr)) != -1)
    {
        switch (opt)
        {
            case 'S':
                SlaveConfigserver = optarg;
                break;
            case 'G':
                GroupName = optarg;
                break;
            case 'L':
                loader.setLocalId(NetHelper::str2Addr(optarg));
                break;
            case 'f':
                loader.setImageFile(optarg);
                break;
            case 'p':
                loader.setSpeed(atoi(optarg));
                break;
            case 'm':
                loader.setMaster(optarg);
                break;
            case 'g':
                loader.setGroup(optarg);
                break;
            case 'o':
                log_file = optarg;
                break;
            case 'l':
                log_level = optarg;
                break;
            case 'h':
            default:
                usage(argv[0]);
                exit(1);
        }
    }

    if (!log_file.empty()) {
        sDefLogger->setFileName(log_file.c_str());
    }
    sDefLogger->setLogLevel(log_level.c_str());

    if (!loader.retrieveServerAddr(SlaveConfigserver, GroupName)) {
        log_error("retrieveServerAddr failed! slave[%s] group[%s]", SlaveConfigserver.c_str(), GroupName.c_str());
        return -1;
    }

    if (!loader.process()) {
        return -1;
    }

    return 0;
}
