#include "dlog.h"
#include "kv/image_file.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

void usage(char *exe)
{
    printf("Usage: %s -f image_file | -h\n", exe);
}

void image_entry_handler(kv::ImageEntry* entry)
{
    kv::DataEntry key;
    kv::DataEntry value;

    entry->toKeyValue(key, value);
    int32_t area = key.decodeArea();

    if (area >= 0 && (int32_t)key.area == area) {
        log_info("area[%d] key[%.*s] vsize[%d] %d : %d %d : %u (%d %d %d %d)    %u %d",
                 area, key.getSize(), key.getData(),
                 value.getSize(),
                 entry->m_metaInfo.keysize, entry->m_metaInfo.valsize,
                 entry->len, entry->endSyncTag,
                 (uint8_t)entry->keyvaluebuf[0], (uint8_t)entry->keyvaluebuf[1],
                 key.m_true_data[0], key.m_true_data[1],
                 entry->m_metaInfo.edate, entry->m_metaInfo.bucketId);
    } else if (area < 0) {
        log_info("area[%d] key[%.*s] vsize[%d] %d : %d %d : %u (%d %d %d %d)    %u %d",
                 area, key.getSize(), key.getData(),
                 value.getSize(),
                 entry->m_metaInfo.keysize,
                 entry->m_metaInfo.valsize,
                 entry->len, entry->endSyncTag,
                 (uint8_t)entry->keyvaluebuf[0], (uint8_t)entry->keyvaluebuf[1],
                 key.m_true_data[0], key.m_true_data[1],
                 entry->m_metaInfo.edate, entry->m_metaInfo.bucketId);
    }
}

int32_t main(int argc, char *argv[])
{
    std::string image_file;
    static struct option long_options[] =
    {
        {"image_file", required_argument, 0, 'f'},
        {"speed", optional_argument, 0, 's'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    uint32_t speed = 1000;

    int32_t option_index = 0;
    int32_t c = getopt_long(argc, argv, "f:s:h", long_options, &option_index);
    if (c == -1) {
        usage(argv[0]);
        exit(1);
    }

    switch (c)
    {
    case 'f':
        image_file = optarg;
        break;
    case 's':
        speed = atoi(optarg);
        break;
    case 'h':
    default:
        usage(argv[0]);
        exit(1);
    }

    auto func = bind(image_entry_handler, std::placeholders::_1);

    kv::CImageLoadDeal loader(speed);
    loader.setLogger(sDefLogger);
    loader.setHandler(func);
    bool rc = loader.init(image_file.c_str());
    if ( !rc ) {
    	log_error("load image failed! file[%s]", image_file.c_str());
    	return 0;
    }

    log_info("load image file success! file[%s]", image_file.c_str());

    loader.deal();

    return 0;
}
