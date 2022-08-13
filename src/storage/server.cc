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

#include "storage.h"
#include "public/dlog.h"
#include "public/config.h"
#include "common/defs.h"
#include "public/common.h"
#include "public/daemon.h"

#include <stdio.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>

static void print_usage(char *prog_name)
{
    fprintf(stderr, "%s -f config_file\n"
            "    -f, --config_file  config file name\n"
            "    -h, --help         display this help and exit\n"
            "    -V, --version      version and build time\n\n",
            prog_name);
}

static void sign_handler(int sig)
{
    log_warn("recv sig: %d", sig);
}

int32_t main(int32_t argc, char* argv[])
{
	int32_t opt = 0;
    const char *opt_string = "hVf:";
    struct option long_opts[] =
    {
        {"config_file", 1, nullptr, 'f'},
        {"help", 0, nullptr, 'h'},
        {"version", 0, nullptr, 'V'},
        {0, 0, 0, 0}
    };

    char *config_file = nullptr;
    while ((opt = getopt_long(argc, argv, opt_string, long_opts, nullptr)) != -1)
    {
        switch (opt)
        {
            case 'f':
                config_file = optarg;
                break;
            case 'V':
                fprintf(stderr, "VERSION: %s\nBUILD_TIME: %s %s\nGIT: %s\n", PROJECT_VERSION, __DATE__, __TIME__, GIT_VERSION);
                exit(1);
            case 'h':
                print_usage(argv[0]);
                exit(0);
        }
    }

    if (!config_file) {
    	print_usage(argv[0]);
    	return -1;
    }

	if ( EXIT_SUCCESS != sDefaultConfig.load(config_file) ) {
		fprintf(stderr, "load config file[%s] failed!", config_file);
		return -1;
	}

    const char* log_file = sDefaultConfig.getString(sStorageSection, sLogFile, "logs/storage.log");
    const char* log_level = sDefaultConfig.getString(sStorageSection, sLogLevel, "info");

    sDefLogger->setFileName(log_file);
    sDefLogger->setLogLevel(log_level);

    const char *dev_name = sDefaultConfig.getString(sStorageSection, sDevName, "");
    uint32_t local_ip = NetHelper::getLocalAddr(dev_name);
    int32_t local_port = sDefaultConfig.getInt(sStorageSection, sServerPort, sDefaultServerPort);

    mybase::CCloudAddrArgu listen_addr;
    listen_addr.m_protocal = mybase::CModelProtocalTcp;
    snprintf(listen_addr.m_bind, sizeof(listen_addr.m_bind), "%s", NetHelper::addr2IpStr(local_ip).c_str());
    listen_addr.m_port = local_port;

    int32_t process_number = sDefaultConfig.getInt(sStorageSection, sProcessThreadNumber, 28);

    // daemonize();

    // for (uint32_t i=0; i<64; i++) {
    //     if ( (i == 9) || (i == SIGINT) || (i == SIGTERM) || (i == 40) ) {
    //         continue;
    //     }
    //     signal(i, SIG_IGN);
    // }
    // signal(SIGINT, sign_handler);
    // signal(SIGTERM, sign_handler);

    mybase::StorageServer server;
    server.setConfigFile(config_file);
    server.setLogger(sDefLogger);
    server.setProcessThreadNumber(process_number);
    server.initialize(listen_addr);
    server.start();

    server.wait();

    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    server.stop();

	return 0;
}
