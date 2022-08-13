#include "common/rpc/server.h"
#include "public/dlog.h"

#include <vector>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

int32_t main(int32_t argc, char* argv[])
{
    sDefLogger->setLogLevel("debg");

    rpc::RpcServer rpc_server;
    rpc_server.setLogger(sDefLogger);
    rpc_server.initialize(atoi(argv[1]));

    sleep(30000);

    return 0;
}