#include "common/wal.h"

#include <thread>
#include <vector>

#include <unistd.h>

int32_t main(int32_t argc, char* argv[])
{
	mybase::wal::Wal wal_log;
	if (!wal_log.initial("./test")) {
		fprintf(stderr, "wal load failed\n");
		return -1;
	}

	auto write_func = [&wal_log] () {
		for (uint32_t i = 0; i < 10000000; i++) {
			auto log = mybase::Buffer::alloc(100);
			log->writeInt32(i);

			wal_log.appendLog(log);
		}
	};

	std::vector<std::thread> vec;
	for (int32_t i = 0; i < 10; i++) {
		vec.emplace_back(std::thread(write_func));
	}

	auto read_func = [&wal_log] () {
		uint64_t start_seq = wal_log.startIndex();

		while (true) {
			uint64_t last_seq = wal_log.lastIndex();

			if (last_seq < start_seq) {
				usleep(10);
				continue;
			}

			uint64_t end_seq = start_seq + 10;
			auto vec = wal_log.logEntries(start_seq, end_seq);

			// for (uint64_t i = start_seq; i <= last_seq; i++) {
			// 	auto log = wal_log.entryAt(i);

			// 	uint32_t num = log->readInt32();
			// 	num = log->readInt32();
			// 	fprintf(stderr, "seq: %lu num: %u\n", i, num);
			// }

			for (auto log : vec) {
				uint32_t num = log->readInt32();
				num = log->readInt32();
				fprintf(stderr, "seq: %lu num: %u\n", start_seq, num);
			}

			start_seq = start_seq + vec.size() + 1;

			if(start_seq >= last_seq) usleep(10);
		}
	};

	std::thread rthd = std::thread(read_func);

	for (auto& thd : vec) {
		thd.join();
	}

	rthd.join();

	return 0;
}