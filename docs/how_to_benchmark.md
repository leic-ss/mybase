# Start benchmark
Benchmark is easy for nubase, it provides a simple benchmark tool.
> `build  cmake  CMakeLists.txt  docs  etc  nubase_bin  README.md  scripts  src  tests`

vim src/tools/benchmarkkv/config.json, then modify the config field as needed.

```
{
    "cluster" : {
        "configserver": "192.168.56.101:5198",                 # ip list of configserver, IP1:PORT1,IP2:PORT2,IP3:PORT3,...
        "group": "group_1"                                     # group name of cluster
    },

    "logfile": "benchmark.log",                                # benchmark log file
    "loglevel": "info",                                        # benchmark log level
    "timeout_msec": 10,                                        # timeout millisecond

    "work_thread_count": 3,                                    # how much threads to send benchmark request
    "ans_thread_count": 2,                                     # how much threads to handle response

    "workload": {
        "type": "put",                                         # "put", "get", "remove"
        "key_generator": "sequential",                         # "sequential", "random"

        "key_size": 10,                                        # key size
        "val_size": 1024,                                      # value size

        "max_key_range": 2000000,                              # max key range, 0 ~ max_key_range
        "max_ops_per_sec": 5000,                               # the count of request that be sent to nubase per second

        "ns": 1,                                        # namespace to put,get and remove
        "ttl": 1000                                            # ttl of key-value
    },

    "sample_interval": {
        "by_sec": 60                                           # sampling per sixty seconds
    }
}
```

start benchmark
>`./build/tools/benchmarkkv/benchmarkkv -f src/tools/benchmarkkv/config.json`
