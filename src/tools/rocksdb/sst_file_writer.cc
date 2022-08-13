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
	rocksdb::Options options;
	rocksdb::SstFileWriter sst_file_writer(rocksdb::EnvOptions(), options);
	// Path to where we will write the SST file
	std::string file_path = "./file1.sst";

	// Open the file for writing
	rocksdb::Status s = sst_file_writer.Open(file_path);
	if (!s.ok()) {
	    log_error("Error while opening file %s, Error: %s", file_path.c_str(),
	           s.ToString().c_str());
	    return 1;
	}

	// // Insert rows into the SST file, note that inserted keys must be 
	// // strictly increasing (based on options.comparator)
	// for (...) {
	//   s = sst_file_writer.Put(key, value);
	//   if (!s.ok()) {
	//     printf("Error while adding Key: %s, Error: %s\n", key.c_str(),
	//            s.ToString().c_str());
	//     return 1;
	//   }
	// }

	// Close the file
	s = sst_file_writer.Finish();
	if (!s.ok()) {
	    log_error("Error while finishing file %s, Error: %s", file_path.c_str(),
	           s.ToString().c_str());
	    return 1;
	}
	return 0;
}