#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/table.h"
#include "rocksdb/options.h"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/statistics.h"
#include "rocksdb/cache.h"
#include "rocksdb/utilities/options_util.h"

#include "dlog.h"

int32_t main(int32_t argc, char** argv)
{
	std::string db_path = "/tmp/rocksdb_test";

	rocksdb::DB* db;
    rocksdb::DBOptions db_opts;

    std::vector<rocksdb::ColumnFamilyDescriptor> cf_descs;
    std::vector<rocksdb::ColumnFamilyHandle*> cf_handles;

	rocksdb::Status s = rocksdb::LoadLatestOptions(db_path, rocksdb::Env::Default(), &db_opts, &cf_descs, false);
    if (s.ok()) {
        log_warn("load db option success! path[%s]", db_path.c_str());
    } else if (s.IsNotFound()) {
        log_warn("db option not found, create one! path[%s]", db_path.c_str());

        db_opts.IncreaseParallelism();
        db_opts.create_if_missing = true;

        rocksdb::ColumnFamilyOptions cf_opts;
        cf_opts.OptimizeLevelStyleCompaction();
        cf_descs.emplace_back(rocksdb::ColumnFamilyDescriptor(rocksdb::kDefaultColumnFamilyName, cf_opts));
    } else {
        log_error("load db option failed! path[%s] err[%s]", db_path.c_str(), s.ToString().c_str());
        return false;
    }

    if (cf_descs.empty()) {
        log_error("initialize db failed! path[%s] err[%s]", db_path.c_str(), s.ToString().c_str());
        return false;
    }

    db_opts.statistics = rocksdb::CreateDBStatistics();

    s = rocksdb::DB::Open(db_opts, db_path, cf_descs, &cf_handles, &db);
    if (!s.ok()) {
        log_error("open data db failed! path[%s] err[%s]", db_path.c_str(), s.ToString().c_str());
        return false;
    }

   	log_info("open data db success! path[%s]", db_path.c_str());

   	// s = db->DeleteFile("000057.sst");
   	// if (!s.ok()) {
    //     log_error("delete file failed! err[%s]", s.ToString().c_str());
    //     return false;
    // }

    // log_info("delete file success!");

    char value[2000*1024] = {'a'};
    value[2000*1024 - 1] = '\0';
    for (uint32_t i = 0; i < 10000000000000; i++) {
        char key[100] = {0};
        snprintf(key, 100, "%d", i);
        if (!db->Put(rocksdb::WriteOptions(), key, value).ok()) {
        	log_error("put failed! key[%s]", key);
        }
    }

    for (auto& handle : cf_handles) {
        log_info("delete handle success!");
        delete handle;
    }

    if ( db ) {
        delete db;
        log_info("delete data db success!");
        db = nullptr;
    }
}