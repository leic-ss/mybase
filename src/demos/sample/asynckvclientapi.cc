#include "kv_client_api.h"

#include "dlog.h"
#include "asynclimit.h"
#include "defs.h"

#include <getopt.h>

// g++ -I../src/client -I../src/common -I../src/public -I../src/packets -std=c++11 -o kvclientdemo ../src/demos/sample/kvclientapi.cc client/libkv_client_api_lib.a -lpthread -lz

void usage(char *exe)
{
    printf("Usage: %s -c configserver_addr -g group_name -s 50000 | -h\n", exe);
}

int32_t main(int32_t argc, char* argv[])
{
    static struct option long_options[] =
    {
        {"configserver_addr", required_argument, 0, 'c'},
        {"group_name", required_argument, 0, 'g'},
        {"speed", required_argument, 0, 's'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    std::string configserver_addr;
    std::string group_name;
    uint32_t speed;

    int32_t opt;
    int32_t option_index = 0;
    while ((opt = getopt_long(argc, argv, "c:g:s:h", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
        case 'c':
            configserver_addr = optarg;
            break;
        case 'g':
            group_name = optarg;
            break;
        case 's':
            speed = atoi(optarg);
            break;
        case 'h':
        default:
            usage(argv[0]);
            exit(1);
        }
    }

    kv::KvClientApiImpl api;

    api.setLogger(sDefLogger);
    api.startup(StringHelper::tokenize(configserver_addr, ","), group_name.c_str());

    sleep(1);

    std::string value;
    value.assign(1024, 'c');

    std::shared_ptr<mybase::CAsyncLimit> async_limit_ptr;
    async_limit_ptr.reset(new mybase::CAsyncLimit(speed));

    uint64_t num = 0;
    while ( true ) {
        char key[31];
        snprintf(key, 31, "%30lu", num);

        int32_t rc = api.asyncPut(num++, 0, key, value, 0, 0);
        if (rc != OP_RETURN_SUCCESS) {
            _log_err(sDefLogger, "asyncPut failed! key[%s] rc[%d]", key, rc);
        }

        async_limit_ptr->limitrate();
    }

    return 0;
}