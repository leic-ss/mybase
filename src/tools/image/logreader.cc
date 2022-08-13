#include "dlog.h"
#include "binlogrw.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

void usage(char *exe)
{
    printf("Usage: %s -f log_base_file_name | -h\n", exe);
}

int32_t main(int argc, char *argv[])
{
    std::string log_file;
    static struct option long_options[] =
    {
        {"log_file", required_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int32_t option_index = 0;
    int32_t c = getopt_long(argc, argv, "f:h", long_options, &option_index);
    if (c == -1) {
        usage(argv[0]);
        exit(1);
    }

    switch (c)
    {
    case 'f':
        log_file = optarg;
        break;
    case 'h':
    default:
        usage(argv[0]);
        exit(1);
    }

    mybase::BinLogFormat* format = nullptr;
    mybase::SyncLogReader readBinLog;

    if (!readBinLog.init(log_file, 0)) {
        log_error("init readbinlog from binlog failed! file[%s] pos[%ld]", log_file.c_str(), 0);
        return -1;
    }

    while ( readBinLog.read(format) ) {
        kv::DataEntry key;
        kv::DataEntry value;

        format->toKeyValue(key, value);

        log_info("area[%d] key[%.*s] value[%.*s] cmd[%d]",
                 key.getArea(), key.getKeyLen(), key.getKeyData(), value.getSize(), value.getData(), format->cmd);

        readBinLog.releaseObj(format);
    }
    return 0;
}
