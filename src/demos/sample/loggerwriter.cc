#include "binlogrw.h"

#include <stdio.h>

int32_t main(int32_t argc, char* argv[])
{
	mybase::LogWriter writer;
	writer.init("/media/sf_Shares/dataservice/build/tmp.log", 10*1024);
	writer.start();

	for (uint32_t i = 0; i < 9981; i++) {
		char buf[1024] = {0};
		snprintf(buf, 1025, "%05d", i);
		writer.write(buf);
	}

	mybase::LogReader reader;
	reader.init("/media/sf_Shares/dataservice/build/tmp.log", nullptr, 1024*1024);

	char* buf;
	uint32_t len;
	while (true) {
		if (!reader.read(buf, len)) continue;

		fprintf(stderr, "buf: %d %.*s\n", len, len, buf);
		reader.releaseObj(buf);
	}

	sleep(1);
}