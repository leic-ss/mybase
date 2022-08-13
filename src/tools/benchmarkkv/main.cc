
#include <fstream>
#include <iostream>
#include <memory>

#include <nlohmann/json.hpp>

#include <stdint.h>
#include <string>
#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <iostream>

#include "benchmark.h"
#include "dlog.h"

void usage(char *exe)
{
    printf("Usage: %s -f config_file | -h\n", exe);
}

void sign_handler(int sig)
{
    _log_warn(sDefLogger, "recv sig: %d", sig);
}

int32_t main(int argc, char *argv[])
{
    for (uint32_t i=0; i<64; i++) {
        if ( (i == 9) || (i == 11) || (i == SIGINT) || (i == SIGTERM) || (i == 40) ) {
            continue;
        }
        signal(i, SIG_IGN);
    }
    signal(SIGTERM, sign_handler);

    std::string config_file;
    static struct option long_options[] =
    {
        {"config_file", required_argument, 0, 'f'},
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
        config_file = optarg;
        break;
    case 'h':
    default:
        usage(argv[0]);
        exit(1);
    }

    nlohmann::json cfg;
    std::ifstream cfg_in(config_file);
    cfg_in >> cfg;

    std::string logfile=cfg.value("logfile", "");
    std::string loglevel=cfg.value("loglevel", "");

    if(!logfile.empty()) sDefLogger->setFileName(logfile.c_str());
    if(!loglevel.empty()) sDefLogger->setLogLevel(loglevel.c_str());

    nlohmann::json cluster = cfg["cluster"];
    nlohmann::json sample = cfg["sample_interval"];

    std::shared_ptr<BenchMarkBase> benchmark_ptr;
    if (!sample["by_sec"].is_null()) {
        benchmark_ptr.reset(new BenchMarkBySecInterval(cfg, cluster));
    }

    if (!benchmark_ptr) {
        fprintf(stderr, "configure the right sample type\n");
        return -1;
    }

    if (!benchmark_ptr->initialize()) {
        fprintf(stderr, "initialize failed!\n");
        return -1;
    }

    benchmark_ptr->start();
    benchmark_ptr->wait();

    return 0;
}
