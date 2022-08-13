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

#include "kv_client.h"
#include "common/defs.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include <stdio.h>

namespace mybase
{

using namespace google::protobuf;
using namespace google::protobuf::compiler;
using namespace google::protobuf::io;
using namespace google::protobuf::internal;

static const uint32_t sCmdMaxLine = 4096;

KvClient::KvClient()
{
    cmd_map["help"] = &KvClient::do_cmd_help;
    cmd_map["quit"] = &KvClient::do_cmd_quit;
    cmd_map["exit"] = &KvClient::do_cmd_quit;
    cmd_map["put"] = &KvClient::do_cmd_put;
    cmd_map["get"] = &KvClient::do_cmd_get;
    cmd_map["getpb"] = &KvClient::do_cmd_getpb;
    cmd_map["locate"] = &KvClient::do_cmd_locate;
    cmd_map["remove"] = &KvClient::do_cmd_remove;
    cmd_map["delall"] = &KvClient::do_cmd_clear_ns;
    cmd_map["stat"] = &KvClient::do_cmd_stat;
}

KvClient::~KvClient()
{ }

void KvClient::printUsage(char *prog_name)
{
    fprintf(stderr,
            "%s -s server:port\n OR\n%s -c configserver:port -g groupname\n\n"
            "    -s, --server           data server,default port:%d\n"
            "    -c, --configserver     default port: %d\n"
            "    -g, --groupname        group name\n"
            "    -l, --cmd_line         exec cmd\n"
            "    -q, --cmd_file         script file\n"
            "    -h, --help             print this message\n"
            "    -v, --verbose          print debug info\n"
            "    -V, --version          print version\n\n", prog_name,
            prog_name, sDefaultServerPort, sDefaultServerPort);
}

bool KvClient::parseCmdLine(int argc, char * const argv[])
{
    const char *optstring = "s:c:hVvl:q:";
    struct option longopts[] = { { "server", 1, nullptr, 's' }, 
                                 { "configserver", 1, nullptr, 'c' },
                                 { "cmd_line", 1, nullptr, 'l' },
                                 { "cmd_file", 1, nullptr, 'q' },
                                 { "help", 0, nullptr, 'h' },
                                 { "verbose", 0, nullptr, 'v' },
                                 { "version", 0, nullptr, 'V' },
                                 { 0, 0, 0, 0 } };

    int32_t opt;
    while ((opt = getopt_long(argc, argv, optstring, longopts, nullptr)) != -1) {
        switch (opt)
        {
            case 'c':
            {
                if (!server_addr.empty() && is_config_server == false) {
                    return false;
                }

                server_addr = optarg;
                is_config_server = true;
            }
            break;
            case 's':
            {
                if (!server_addr.empty() && is_config_server == true) {
                    return false;
                }
                server_addr = optarg;
                is_data_server = true;
            }
            break;
            case 'l':
                cmd_line = optarg;
                break;
            case 'q':
                cmd_file_name = optarg;
                break;
            case 'v':
                sDefLogger->setLogLevel("debg");
                break;
            case 'V':
                fprintf(stderr, "BUILD_TIME: %s %s\n", __DATE__, __TIME__);
                return false;
            case 'h':
                printUsage(argv[0]);
                return false;
        }
    }

    if (server_addr.empty()) {
        printUsage(argv[0]);
        return false;
    }

    return true;
}

cmd_call KvClient::parseCmd(const std::string& key_str, VSTRING &param)
{
    char* key = (char*)key_str.data();

    cmd_call cmdCall = nullptr;
    char *token = nullptr;

    while (*key == ' ') {
        key++;
    }

    token = key + strlen(key);
    while (*(token - 1) == ' ' || *(token - 1) == '\n' || *(token - 1) == '\r') {
        token--;
    }

    *token = '\0';
    if (key[0] == '\0') {
        return nullptr;
    }

    token = strchr(key, ' ');
    if (token != nullptr) {
        *token = '\0';
    }

    auto it = cmd_map.find(StringHelper::strToLower(key));
    if (it == cmd_map.end()) {
        return nullptr;
    } else {
        cmdCall = it->second;
    }

    if (token != nullptr) {
        token++;
        key = token;
    } else {
        key = nullptr;
    }

    param.clear();
    while ((token = strsep(&key, " ")) != nullptr) {
        if (token[0] == '\0') {
            continue;
        }
        param.push_back(token);
    }
    return cmdCall;
}

void KvClient::cancel()
{
    is_cancel = true;
}

bool KvClient::start()
{
    bool done = true;
    clientApi.setTimeout(500);
    // clientApi.setLogger(sDefLogger);

    if (is_config_server) {
        done = clientApi.startup(StringHelper::tokenize(server_addr, ","));
    } else {
        if (is_data_server) {
            // done = clientApi.directup(server_addr);
        } else {
            done = false;
        }
    }

    if (done == false) {
        fprintf(stderr, "%s cann't connect %s.\n",
                server_addr.c_str(), is_config_server ? "configserver" : "dataserver");
        // _log_err(sDefLogger, "%s cann't connect %s.",
        //          server_addr.c_str(), is_config_server ? "configserver" : "dataserver");
        return false;
    }

    VSTRING param;
    if ( !cmd_line.empty() ) {
        auto cmd_list = StringHelper::tokenize(cmd_line, ";");
        for (uint32_t i = 0; i < cmd_list.size(); i++) {
            cmd_call this_cmd_call = parseCmd(cmd_list[i], param);
            if (this_cmd_call == nullptr) {
                continue;
            }
            (this->*this_cmd_call)(param);
        }
    } else if (!cmd_file_name.empty()) {
        FILE *fp = fopen(cmd_file_name.c_str(), "rb");
        if (fp != nullptr) {
            char buffer[sCmdMaxLine] = {0};
            while (fgets(buffer, sCmdMaxLine, fp)) {
                cmd_call this_cmd_call = parseCmd(buffer, param);
                if (this_cmd_call == nullptr) {
                    fprintf(stderr, "unknown command.\n\n");
                    continue;
                }
                (this->*this_cmd_call)(param);
            }
            fclose(fp);
        } else {
            fprintf(stderr, "open failure: %s\n\n", cmd_file_name.c_str());
        }
    } else {
        while (done) {
            if (fprintf(stderr, "KV> ") < 0) {
                break;
            }

            char buffer[sCmdMaxLine] = {0};
            if (fgets(buffer, sCmdMaxLine, stdin) == nullptr) {
                break;
            }

            cmd_call this_cmd_call = parseCmd(buffer, param);
            if (this_cmd_call == nullptr) {
                fprintf(stderr, "unknown command.\n\n");
                continue;
            }

            if (this_cmd_call == &KvClient::do_cmd_quit) {
                break;
            }

            (this->*this_cmd_call)(param);
            is_cancel = false;
        }
    }

    // stop
    clientApi.close();

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int64_t KvClient::ping(uint64_t server_id, bool printinfo)
{
    if (server_id == 0ul) {
        return 0L;
    }

    int ping_count = 10;
    int64_t total = 0;
    int64_t timecost = 0 ;
    for (int i = 0; i < ping_count; ++i) {
        // timecost = clientApi.ping(server_id);
        total += timecost;
        if( printinfo ) {
            fprintf(stderr, "%20s %ld usec\n", NetHelper::addr2String(server_id).c_str(), timecost);
        }
    }
    return total / ping_count;
}

void KvClient::do_cmd_quit(VSTRING &param)
{
    return;
}

void KvClient::printHelp(const char *cmd)
{
    if (cmd == nullptr || strcmp(cmd, "put") == 0) {
        fprintf(stderr, "------------------------------------------------\n"
                "SYNOPSIS   : put key data [area] [expired]\n"
                "DESCRIPTION: area   - namespace , default: 0\n"
                "             expired- in seconds, default: 0,never expired\n");
    }

    if (cmd == nullptr || strcmp(cmd, "locate") == 0) {
        fprintf(stderr, "------------------------------------------------\n"
                "SYNOPSIS   : locate key\n"
                "DESCRIPTION: locate which server this key is located\n");
    }

    if (cmd == nullptr || strcmp(cmd, "get") == 0) {
        fprintf(stderr, "------------------------------------------------\n"
                "SYNOPSIS   : get key [area]\n");
    }

    if (cmd == nullptr || strcmp(cmd, "getpb") == 0) {
        fprintf(stderr, "------------------------------------------------\n"
                "SYNOPSIS   : getpb key [area]\n");
    }

    if (cmd == nullptr || strcmp(cmd, "remove") == 0) {
        fprintf(stderr, "------------------------------------------------\n"
                "SYNOPSIS   : remove key [area]\n");
    }

    if (cmd == nullptr || strcmp(cmd, "delall") == 0) {
        fprintf(stderr, "------------------------------------------------\n"
                "SYNOPSIS   : stat\n"
                "DESCRIPTION: get stat info\n");
    }
    if (cmd == nullptr || strcmp(cmd, "stat") == 0) {
        fprintf(stderr, "------------------------------------------------\n"
                "SYNOPSIS   : delall area\n"
                "DESCRIPTION: delete all data of [area]\n");
    }

    fprintf(stderr, "\n");
}

void KvClient::do_cmd_help(VSTRING &param)
{
    if (param.size() == 0U) {
        printHelp(nullptr);
    } else {
        printHelp(param[0].c_str());
    }

    return;
}

void KvClient::do_cmd_put(VSTRING &param)
{
    if (param.size() < 2U || param.size() > 4U) {
        printHelp("put");
        return;
    }

    int ns = default_area;
    int expired = 0;

    if (param.size() > 2U) {
        ns = atoi(param[2].c_str());
    }
    if (param.size() > 3U) {
        expired = atoi(param[3].c_str());
    }

    int ret = clientApi.put(ns, param[0], param[1], expired, 0);
    // fprintf(stderr, "put: %s\n", clientApi.get_error_msg(ret));
    fprintf(stderr, "put: %d\n", ret);
    return ;
}

void KvClient::do_cmd_locate(VSTRING &param)
{
    if (param.size() < 1U) {
        printHelp("locate");
        return;
    }

    // locate
    std::vector<uint64_t> server_list;
    bool ret = clientApi.getServerId(param[0], server_list);
    if (!ret) {
        fprintf(stderr, "failed to locate key: %s\n", param[0].c_str());
        return;
    }

    for (auto it = server_list.begin(); it != server_list.end(); ++it) {
        fprintf(stderr, "server: %s\n", NetHelper::addr2String(*it).c_str());
    }

    return;
}

void KvClient::do_cmd_get(VSTRING &param)
{
    if (param.size() < 1U || param.size() > 2U) {
        printHelp("get");
        return;
    }
    int area = default_area;
    if (param.size() >= 2U) {
        area = atoi(param[1].c_str());
    }

    // get
    std::string data;
    GetOption opt;
    int ret = clientApi.get(area, param[0], data, &opt);
    if (ret != OP_RETURN_SUCCESS) {
        // fprintf(stderr, "get failed: %s.\n", client_helper.get_error_msg(ret));
        fprintf(stderr, "get failed: %d\n", ret);
    } else if (!data.empty()) {
        char *p = StringHelper::convShowString((char*)data.data(), data.size());

        fprintf(stderr, "TIME: %u ~ %u\n", opt.mdate, opt.edate);
        fprintf(stderr, "KEY: %s, LEN: %d\n raw data: %.*s, %s\n", param[0].c_str(),
                (int32_t)data.size(), (int32_t)data.size(), data.data(), p);
        free(p);
    }

    return;
}

void KvClient::do_cmd_getpb(VSTRING &param)
{
    if (param.size() < 1U || param.size() > 2U) {
        printHelp("getpb");
        return;
    }
    int area = default_area;
    if (param.size() >= 2U) {
        area = atoi(param[1].c_str());
    }

    // get
    std::string data;
    GetOption opt;
    int ret = clientApi.get(area, param[0], data, &opt);
    if (ret != OP_RETURN_SUCCESS) {
        // fprintf(stderr, "get failed: %s.\n", client_helper.get_error_msg(ret));
        fprintf(stderr, "get failed: %d\n", ret);
    } else if (!data.empty()) {
        DescriptorPool pool;
        FileDescriptorProto file;
        file.set_name("empty_message.proto");
        file.add_message_type()->set_name("EmptyMessage");

        pool.BuildFile(file);

        const Descriptor* descriptor = pool.FindMessageTypeByName("EmptyMessage");

        DynamicMessageFactory dynamic_factory(&pool);
        std::shared_ptr<Message> msg(dynamic_factory.GetPrototype(descriptor)->New());

        msg->ParsePartialFromArray(data.data(), data.size());
        msg->IsInitialized();

        fprintf(stderr, "TIME: %u ~ %u\n", opt.mdate, opt.edate);
        fprintf(stderr, "KEY: %s, LEN: %d\npb data:\n%s\n", param[0].c_str(),
                (int32_t)data.size(), msg->DebugString().c_str());
    }

    return;
}

// remove
void KvClient::do_cmd_remove(VSTRING &param)
{
    if (param.size() < 2U) {
        printHelp("remove");
        return;
    }

    int area = default_area;
    if (param.size() >= 2U) {
        area = atoi(param[1].c_str());
    }

    int ret = clientApi.remove(area, param[0]);
    // fprintf(stderr, "remove: %s.\n", client_helper.get_error_msg(ret));
    fprintf(stderr, "remove: %d.\n", ret);

    return;
}

void KvClient::do_cmd_stat(VSTRING &param)
{
    // if (is_data_server) {
    //     fprintf(stderr, "direct connect to ds, can not use stat\n");
    //     return;
    // }
    
    // std::map<std::string, std::string> out_info;
    // std::string group = group_name;
    // clientApi.queryFromConfigserver(KvPacketRequestQueryInfo::Q_AREA_CAPACITY, group, out_info);
    // fprintf(stderr, "%20s %20s\n", "area", "quota");
    // for (auto it = out_info.begin(); it != out_info.end(); it++) {
    //     fprintf(stderr, "%20s %20s\n", it->first.c_str(), it->second.c_str());
    // }
    // fprintf(stderr, "\n");

    // fprintf(stderr, "%20s %20s\n", "server", "status");
    // clientApi.queryFromConfigserver(KvPacketRequestQueryInfo::Q_DATA_SEVER_INFO, group, out_info);
    // for (auto it = out_info.begin(); it != out_info.end(); it++) {
    //     fprintf(stderr, "%20s %20s\n", it->first.c_str(), it->second.c_str());
    // }
    // fprintf(stderr, "\n");

    // fprintf(stderr, "%20s %20s\n", "server", "ping");
    // std::set<uint64_t> servers;
    // clientApi.getServers(servers);
    // for (auto it = servers.begin(); it != servers.end(); ++it) {
    //     int64_t ping_time = ping(*it, false);
    //     fprintf(stderr, "%20s %20lf\n", NetHelper::addr2String(*it).c_str(), ping_time / 1000.);
    // }

    // fprintf(stderr, "\nserver statistics\n");
    // // for (it = out_info.begin(); it != out_info.end(); it++)
    // // {
    // //     if ("dead" == it->second)
    // //         continue;

    // //     map<string, string> out_info2;
    // //     map<string, string>::iterator it2;

    // //     uint64_t server_id = tbsys::CNetUtil::strToAddr(it->first.c_str(), 0);
    // //     client_helper.query_from_configserver(request_query_info::Q_STAT_INFO,
    // //                                           group, out_info2, server_id);
    // //     for (it2 = out_info2.begin(); it2 != out_info2.end(); it2++)
    // //     {
    // //         fprintf(stderr, "%s : %s %s\n", it->first.c_str(),
    // //                 it2->first.c_str(), it2->second.c_str());
    // //     }
    // // }

    // fprintf(stderr, "\narea statistics\n");
    // std::map<std::string, std::string> out_info2;
    // clientApi.queryFromConfigserver(KvPacketRequestQueryInfo::Q_STAT_INFO, group, out_info2, 0);
    // for (auto it2 = out_info2.begin(); it2 != out_info2.end(); it2++)
    // {
    //     fprintf(stderr, "%s %s\n", it2->first.c_str(), it2->second.c_str());
    // }

    return ;
}

void KvClient::do_cmd_clear_ns(VSTRING &param)
{
    if (param.size() != 1U) {
        printHelp("delall");
        return;
    }

    int area = atoi(param[0].c_str());
    if (area < -4) {
        fprintf(stderr, "invalid area %d\n", area);
        return;
    }

    // remove
    clientApi.clearNamespace(area);
    return;
}

} // namespace mybase

mybase::KvClient gKvClient;
void sign_handler(int32_t sig)
{
    switch (sig)
    {
        case SIGTERM:
        case SIGINT:
            gKvClient.cancel();
    }
}

int32_t main(int argc, char *argv[])
{   
    signal(SIGPIPE, SIG_IGN );
    signal(SIGHUP, SIG_IGN );
    signal(SIGINT, sign_handler);
    signal(SIGTERM, sign_handler);

    // sDefLogger->setFileName("kv_client.log", true);
    sDefLogger->setLogLevel("info");

    if (gKvClient.parseCmdLine(argc, argv) == false) {
        return EXIT_FAILURE;
    }

    if (gKvClient.start()) {
        return EXIT_SUCCESS;
    } else {
        return EXIT_FAILURE;
    }
}
