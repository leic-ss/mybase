#include "bufferqueue.h"
#include "dlog.h"

#include <getopt.h>
#include <stdint.h>

void usage(char *exe)
{
    printf("Usage: %s -t type -n number | -h\n", exe);
}

void ipc_queue_sample(uint64_t max_count)
{
	mybase::IpcFifoQueue<uint32_t> queue;
	if (!queue.init()) {
		_log_err(sDefLogger, "queue init failed!");
		return ;
	}
	auto producer_func = [&] (void) {
		for (uint32_t i = 0; i < max_count; i++) {
			if (!queue.push(i)) {
				_log_err(sDefLogger, "queue push failed! i[%u]", i);
				continue;
			}
			_log_info(sDefLogger, "queue push success! i[%u]", i);

			GenericTimer::sleepUs( ( (i * 100) % 100 )*1000);		
		}
	};

	auto consumer_func = [&] (void) {
		uint32_t num;
		while( true ) {
			if (!queue.front(num, 1000000)) {
				_log_warn(sDefLogger, "consumer wait!");
				continue;
			}

			_log_info(sDefLogger, "consum num: %u", num);

			queue.pop();
		}
	};

	std::thread producer( producer_func );

	GenericTimer::sleepUs(1000);
	std::thread consumer( consumer_func );

	if(producer.joinable()) {
		producer.join();
	}
  
	if (consumer.joinable()) {
		consumer.join();
	}
}

void lock_queue_sample(uint64_t max_count)
{
	mybase::LockFifoQueue<uint32_t> queue;
	if (!queue.init()) {
		_log_err(sDefLogger, "queue init failed!");
		return ;
	}
	auto producer_func = [&] (void) {
		for (uint32_t i = 0; i < max_count; i++) {
			if (!queue.push(i)) {
				_log_err(sDefLogger, "queue push failed! i[%u]", i);
				continue;
			}
			_log_info(sDefLogger, "queue push success! i[%u]", i);

			GenericTimer::sleepUs( ( (i * 10) % 10 ));		
		}
	};

	auto consumer_func = [&] (void) {
		uint32_t num;
		while( true ) {
			if (!queue.pop(num, 1)) {
				_log_warn(sDefLogger, "consumer wait!");
				continue;
			}

			_log_info(sDefLogger, "consum num: %u", num);
		}
	};

	std::thread producer( producer_func );

	GenericTimer::sleepUs(1000);
	std::thread consumer1( consumer_func );
	std::thread consumer2( consumer_func );

	if(producer.joinable()) {
		producer.join();
	}
  
	if (consumer1.joinable()) {
		consumer1.join();
	}
	if (consumer2.joinable()) {
		consumer2.join();
	}
}

int32_t main(int32_t argc, char* argv[])
{
	static struct option long_options[] =
    {
        {"type", required_argument, 0, 't'},
        {"number", optional_argument, 0, 'n'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    std::string type;
    uint64_t maxCount = 100;

    int32_t opt;
    int32_t option_index = 0;
    while ((opt = getopt_long(argc, argv, "t:n:h", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
        case 't':
            type = optarg;
            break;
        case 'n':
        	maxCount = atol(optarg);
        	break;
        case 'h':
        default:
            usage(argv[0]);
            exit(1);
        }
    }

    if (type == "ipc") {
    	ipc_queue_sample(maxCount);
    } else if (type == "lock") {
    	lock_queue_sample(maxCount);
    } else {
    	usage(argv[0]);
    }

    return 0;
}
