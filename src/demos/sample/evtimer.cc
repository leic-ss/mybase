#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include <thread>

#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/util.h>

#include "dlog.h"

int32_t main(int32_t argc, char* argv[])
{
	struct timeval tv = {0};
	struct event_base* base = event_base_new();
	auto timeout_cb = [&](int, short, void * ptr) {
		log_info("timeout_cb!");
	};
	struct event* evt = event_new(base, -1, EV_PERSIST, timeout_cb, nullptr);

	evutil_timerclear(&tv);
	tv.tv_sec = 1;
	event_add(evt, &tv);

	auto backgroud_func = [&]() {
		sleep(5);
		log_info("backgroud_func start!");

		event_del(evt);

		sleep(2);
		event_add(evt, &tv);

		sleep(2);
		event_del(evt);

		event_free(evt);

		log_info("backgroud_func end!");
	};
	std::thread t(backgroud_func);

	event_base_dispatch(base);

	if (t.joinable()) t.join();

	return 0;
}
