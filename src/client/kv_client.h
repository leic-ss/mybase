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

#include "kv_client_api.h"
#include "public/dlog.h"

#include <string>
#include <vector>
#include <map>

#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>

namespace mybase {

using VSTRING = std::vector<std::string>;
class KvClient;
using cmd_call = void(KvClient::*)(VSTRING &param);
using str_cmdcall_map = std::map<std::string, cmd_call>;

class KvClient {
public:
    KvClient();
    ~KvClient();
    bool parseCmdLine(int32_t argc, char* const argv[]);

    bool start();
    void cancel();

private:
    void printUsage(char *prog_name);

    cmd_call parseCmd(const std::string& key, VSTRING &param);
    void printHelp(const char *cmd);

    int64_t ping(uint64_t server_id, bool printinfo=false);

    void do_cmd_quit(VSTRING &param);
    void do_cmd_help(VSTRING &param);
    void do_cmd_put(VSTRING &param);
    void do_cmd_locate(VSTRING &param);
    void do_cmd_get(VSTRING &param);
    void do_cmd_getpb(VSTRING &param);
    void do_cmd_remove(VSTRING &param);
    void do_cmd_stat(VSTRING &param);
    void do_cmd_clear_ns(VSTRING &param);

private:
    mybase::BaseLogger* myLog{nullptr};
    str_cmdcall_map cmd_map;
    KvClientApi clientApi;

    std::string server_addr;
    std::string group_name;
    std::string cmd_line;
    std::string cmd_file_name;

    bool is_config_server{false};
    bool is_data_server{false};
    bool is_cancel{false};

    int32_t key_format;
    int32_t default_area{0};
};

}
