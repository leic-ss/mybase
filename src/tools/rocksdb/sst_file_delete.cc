#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/table.h"
#include "rocksdb/options.h"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/statistics.h"
#include "rocksdb/cache.h"
#include "rocksdb/utilities/options_util.h"

#include "dlog.h"

#include <getopt.h>
#include <stdio.h>
#include <stdint.h>

static void print_usage(char *prog_name)
{
    fprintf(stderr, "%s -d dbpath -f filename\n"
            "    -d, --db_path  db path\n"
            "    -f, --file_name  file name\n"
            "    -h, --help         display this help and exit\n"
            "    -V, --version      version and build time\n\n",
            prog_name);
}

int32_t main(int32_t argc, char* argv[])
{
	int32_t opt = 0;
    const char *opt_string = "hVd:f:";
    struct option long_opts[] =
    {
        {"dbpath", 1, nullptr, 'd'},
        {"filename", 1, nullptr, 'f'},
        {"help", 0, nullptr, 'h'},
        {"version", 0, nullptr, 'V'},
        {0, 0, 0, 0}
    };

    char* db_path = nullptr;
    char* file_name = nullptr;
    while ((opt = getopt_long(argc, argv, opt_string, long_opts, nullptr)) != -1)
    {
        switch (opt)
        {
        	case 'd':
                db_path = optarg;
                break;
            case 'f':
                file_name = optarg;
                break;
            case 'V':
                fprintf(stderr, "VERSION: %s\nBUILD_TIME: %s %s\nGIT: %s\n", PROJECT_VERSION, __DATE__, __TIME__, GIT_VERSION);
                exit(1);
            case 'h':
                print_usage(argv[0]);
                exit(0);
        }
    }

    if (!db_path || !file_name) {
    	print_usage(argv[0]);
    	return -1;
    }

    rocksdb::DB* db;
    rocksdb::DBOptions db_opts;

    std::vector<rocksdb::ColumnFamilyDescriptor> cf_descs;
    std::vector<rocksdb::ColumnFamilyHandle*> cf_handles;

	rocksdb::Status s = rocksdb::LoadLatestOptions(db_path, rocksdb::Env::Default(), &db_opts, &cf_descs, false);
    if (s.ok()) {
        log_warn("load db option success! path[%s]", db_path);
    } else if (s.IsNotFound()) {
        log_warn("db option not found, create one! path[%s]", db_path);

        db_opts.IncreaseParallelism();
        db_opts.create_if_missing = true;

        rocksdb::ColumnFamilyOptions cf_opts;
        cf_opts.OptimizeLevelStyleCompaction();
        cf_descs.emplace_back(rocksdb::ColumnFamilyDescriptor(rocksdb::kDefaultColumnFamilyName, cf_opts));
    } else {
        log_error("load db option failed! path[%s] err[%s]", db_path, s.ToString().c_str());
        return -1;
    }

    if (cf_descs.empty()) {
        log_error("initialize db failed! path[%s] err[%s]", db_path, s.ToString().c_str());
        return -1;
    }

    db_opts.statistics = rocksdb::CreateDBStatistics();

    s = rocksdb::DB::Open(db_opts, db_path, cf_descs, &cf_handles, &db);
    if (!s.ok()) {
        log_error("open data db failed! path[%s] err[%s]", db_path, s.ToString().c_str());
        return -1;
    }

   	log_info("open data db success! path[%s]", db_path);

   	s = db->DeleteFile(file_name);
   	if (!s.ok()) {
        log_error("delete file %s failed! err[%s]", file_name, s.ToString().c_str());
        return -1;
    }

    log_info("delete file %s success!", file_name);

    for (auto& handle : cf_handles) {
        log_info("delete handle success!");
        delete handle;
    }

    if ( db ) {
        delete db;
        log_info("delete data db success!");
        db = nullptr;
    }

    return 0;
}