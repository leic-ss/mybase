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
#include "common.h"
#include "cast_helper.h"
#include "kv_packet_request_get_group.h"
#include "syncnetpacketmgn.h"

#include <sstream>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include <unordered_map>
#include <unordered_set>

void usage(char *exe)
{
    printf("Usage: %s -S slave -G group -f imagefile -a arealist -p speed -r rate -m master -s slave -g group -o log_file | -h\n", exe);
}

class KvLoaderSession : public mybase::BaseSession
{
public:
    template<typename T>
    using ptr = std::shared_ptr<T>;

public:
    KvLoaderSession(uint64_t timeout, uint32_t seq) : mybase::BaseSession(timeout, seq) {}

public:
    int32_t area;
};

class LoaderHandler {
public:
    LoaderHandler() : streamer(&factory), tcpMgn(&factory, &streamer), loader() {}
    ~LoaderHandler() { 
        if (sessionMgn) sessionMgn->stop();
        DELETE(sessionMgn);
    }

    void setImageFile(const std::string& _image_file) { imageFile = _image_file; }
    void setMaster(std::string master_configserver) { masterConfigserver = master_configserver; }
    void setGroup(std::string group_name) { groupName = group_name; }

    void setAreaList(const std::string& area_list)
    {
        log_info("image_replay_area_list: %s", area_list.c_str());
        std::vector<std::string> vec = StringHelper::tokenize(area_list, ",");
        for (auto item : vec) {
            areas.emplace(atoi(item.c_str()));
        }
    }

    void setSpeed(uint32_t _speed) { speed = _speed; }
    void setRate(uint32_t _rate) { rate = _rate; }

    bool retrieveServerAddr(std::string slave, std::string group)
    {
        kv::KvRequestGetGroup packet;
        packet.setGroupName(group.c_str());
        packet.setChannelId(1);

        auto tpacket = tcpMgn.sendAndRecv(NetHelper::str2Addr(slave), &packet);
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

        localIp = NetHelper::getLocalIp();
        log_info("localIp: %u %s", localIp, NetHelper::addr2IpStr(localIp).c_str());

        daystr = TimeHelper::timeConvertDay(TimeHelper::currentSec());

        return true;
    }

    void imageEntryHandler(kv::ImageEntry* entry)
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

        if (localIp != (server_list[0] & 0xFFFFFFFF)) {
            bucketNotMatchedCount++;
            return ;
        }

        if (areas.find(area) == areas.end()) {
            areaNotMatchedCount++;
            return ;
        }

        limit->limitrate();

        uint32_t next_seq = nextSequence();
        std::shared_ptr<mybase::BaseSession> session = std::make_shared<KvLoaderSession>(TimeHelper::currentMs() + 1000, next_seq);
        _SC(KvLoaderSession*, session.get())->area = area;
        sessionMgn->saveSession(session);

        int32_t rc = apiImpl.asyncPut(next_seq, area, _key, _value, entry->m_metaInfo.edate, 0);
        if (rc != 0) {
            log_error("put failed! area[%d] key[%.*s] vsize[%d] edate[%u] rc[%d]",
                      area, key.getSize(), key.getData(), value.getSize(), entry->m_metaInfo.edate, rc);
            sessionMgn->eraseAndGetSession(next_seq);
            errCount++;
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

        auto async_handler = std::bind(&LoaderHandler::handler, this, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
        apiImpl.setAsyncHandler(async_handler);

        if ( !sessionMgn ) sessionMgn = new mybase::CSessionMgn(1000*1000, 1);
        if ( !sessionMgn ) return false;

        auto timeout_handler = std::bind(&LoaderHandler::handleTimeout, this, std::placeholders::_1);
        sessionMgn->setTimeoutHandler(timeout_handler);
        sessionMgn->start();

        if (!apiImpl.startup(StringHelper::tokenize(masterConfigserver, ","), groupName)) {
            log_error("startup failed!");
            return false;
        }

        log_info("startup success! servers[%s] group[%s]", masterConfigserver.c_str(), groupName.c_str());

        auto func = std::bind(&LoaderHandler::imageEntryHandler, this, std::placeholders::_1);
        loader.setHandler(func);
        auto done_func = std::bind(&LoaderHandler::doneHandler, this);
        loader.setDoneHandler(done_func);

        bool rc = loader.init(imageFile.c_str());
        if ( !rc ) {
            log_error("load image failed! file[%s]", imageFile.c_str());
            return false;
        }

        log_info("load image file success! file[%s]", imageFile.c_str());

        if (!api.startup(StringHelper::tokenize(masterConfigserver, ","), groupName)) {
            log_error("startup failed!");
            return false;
        }

        {
            log_info("image_replay_speed: %d", speed);
            limit = new mybase::CAsyncLimit(speed);
        }

        {
            log_info("image_loader_speed: %d", rate);
            loader.setRate(rate);
        }

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
    void handleTimeout(std::shared_ptr<mybase::BaseSession> req)
    {
        log_info("timeout handler! seq: %u", req->sequence);

        timeOutCount++;
        int32_t area = _SC(KvLoaderSession*, req.get())->area;
        {
            std::lock_guard<std::mutex> l(mtx);
            auto itr = areaTimeoutMap.find(area);
            if (itr == areaTimeoutMap.end()) {
                areaTimeoutMap.emplace(area, 1);
            } else {
                areaTimeoutMap[area]++;
            }
        }
    }

    void handler(uint32_t seq, int32_t pcode, int32_t result, void* data)
    {
        std::shared_ptr<mybase::BaseSession> session = sessionMgn->eraseAndGetSession(seq);
        if (!session) {
            return ;
        }

        succCount++;
        int32_t area = _SC(KvLoaderSession*, session.get())->area;
        {
            std::lock_guard<std::mutex> l(mtx);
            auto itr = areaCountMap.find(area);
            if (itr == areaCountMap.end()) {
                areaCountMap.emplace(area, 1);
            } else {
                areaCountMap[area]++;
            }
        }
    }

    void doneHandler()
    {
        log_info("totalCount: %u", loader.getRecordNum());
        log_info("succCount: %u", succCount.load());
        log_info("errCount: %u", errCount.load());
        log_info("timeOutCount: %u", timeOutCount.load());
        log_info("bucketNotMatchedCount: %u", bucketNotMatchedCount.load());
        log_info("areaNotMatchedCount: %u", areaNotMatchedCount.load());

        for(auto item : areaCountMap) {
            log_info("area count: %d %lu", item.first, item.second);
        }
        for(auto item : areaTimeoutMap) {
            log_info("area timeout: %d %lu", item.first, item.second);
        }
    }
    uint32_t nextSequence() { return nextSeq.fetch_add(1); }

private:
    kv::KvPacketFactory factory;
    kv::KvPacketStreamer streamer;
    mybase::SyncNetPacketMgn tcpMgn;
    std::vector<uint64_t> myServerList;

    uint32_t bucketCount;
    uint32_t copyCount;

    std::string imageFile;
    kv::CImageLoadDeal loader;
    uint64_t localIp{0};
    uint32_t speed{1000};
    uint32_t rate{20000};

    mybase::CSessionMgn* sessionMgn{nullptr};
    std::atomic<uint32_t> nextSeq{1};

    std::string daystr;
    std::string masterConfigserver;
    std::string slaveConfigserver;
    std::string groupName;

    mybase::CAsyncLimit* limit;
    kv::KvClientApiImpl apiImpl;
    kv::KvClientApi api;

    std::unordered_set<int32_t> areas;

    std::atomic<uint64_t> succCount{0};
    std::atomic<uint64_t> errCount{0};
    std::atomic<uint64_t> timeOutCount{0};
    std::atomic<uint64_t> bucketNotMatchedCount{0};
    std::atomic<uint64_t> areaNotMatchedCount{0};

    std::mutex mtx;
    std::unordered_map<int32_t, uint64_t> areaCountMap;
    std::unordered_map<int32_t, uint64_t> areaTimeoutMap;
};

int32_t main(int argc, char *argv[])
{
    LoaderHandler loader;

    std::string image_file;
    static struct option long_options[] =
    {
        {"Slave", required_argument, 0, 'S'},
        {"Group", required_argument, 0, 'G'},
        {"imagefile", required_argument, 0, 'f'},
        {"arealist", required_argument, 0, 'a'},
        {"speed", required_argument, 0, 'p'},
        {"rate", required_argument, 0, 'r'},
        {"master", required_argument, 0, 'm'},
        {"slave", required_argument, 0, 's'},
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
    while ((opt = getopt_long(argc, argv, "S:G:f:a:p:r:m:s:g:o:l:h", long_options, nullptr)) != -1)
    {
        switch (opt)
        {
            case 'S':
                SlaveConfigserver = optarg;
                break;
            case 'G':
                GroupName = optarg;
                break;
            case 'f':
                loader.setImageFile(optarg);
                break;
            case 'a':
                loader.setAreaList(optarg);
                break;
            case 'p':
                loader.setSpeed(atoi(optarg));
                break;
            case 'r':
                loader.setRate(atoi(optarg));
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
