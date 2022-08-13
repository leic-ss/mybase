#include "benchmark.h"
#include "common.h"
#include "defs.h"
#include "benchmark.pb.h"

#include <thread>
#include <sstream>
#include <iostream>

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)

#include <random>

thread_local char BenchMarkBase::benchMarkKey[1024] = {0};
thread_local bool BenchMarkBySecInterval::initialized = false;

BenchMarkBase::BenchMarkBase(nlohmann::json& cfg) : config(cfg)
{
    auto& work_load_cfg = cfg["workload"];
    maxKeyRangeCount = work_load_cfg.value("max_key_range", 10000);
    maxOpsPerSec = work_load_cfg.value("max_ops_per_sec", 2000);

    keySize = work_load_cfg.value("key_size", 10);
    valSize = work_load_cfg.value("val_size", 1024);
    std::string key_generator = work_load_cfg.value("key_generator", "sequential");
    if (key_generator == "sequential") {
        keyGenerator = 1;
    } else if (key_generator == "random") {
        keyGenerator = 2;
    }

    std::string benchmark_type = work_load_cfg.value("type", "put");
    if (benchmark_type == "put") {
        benchmarkType = 1;
    } else if (benchmark_type == "get") {
        benchmarkType = 2;
    }

    timeoutMsec = work_load_cfg.value("timeout_msec", 10);

    ns = work_load_cfg.value("ns", 0);
    ttl = work_load_cfg.value("ttl", 0);

    nlohmann::json& sample = cfg["sample_interval"];
    sampleBySecInterval = sample.value("by_sec", 60);

    value.reserve(valSize);
    memset(&value[0], 'a', valSize);

    asyncLimitPtr.reset(new mybase::CAsyncLimit(maxOpsPerSec));
}

BenchMarkBase::~BenchMarkBase()
{
}

bool BenchMarkBase::start()
{
    running = true;

    generic_timer.reset();
    int32_t work_thread_count = config.value("work_thread_count", 2);

    for (int32_t idx = 0; idx < work_thread_count; idx++) {
        thds.push_back( std::thread(&BenchMarkBase::run, this, idx) );
    }

    return 0;
}

void BenchMarkBase::wait()
{
    for (auto& thd : thds) {
        if (!thd.joinable()) {
            continue;
        }

        thd.join();
    }
}

std::string BenchMarkBase::toString()
{
    std::ostringstream oss;

    oss << "count[";
    bool first = true;
    for (auto entry : results) {
        if (!first) {
            oss << ":";
        }else {
            first = false;
        }
        oss << entry.first << "," << entry.second;
    }
    oss << "]";
    oss << " time[";
    first = true;
    for (auto entry : timestats) {
        if (!first) {
            oss << ":";
        }else {
            first = false;
        }
        oss << entry.first << "," << entry.second;
    }
    oss << "]";

    return std::move(oss.str());
}

BenchMarkBySecInterval::~BenchMarkBySecInterval()
{
    if (sessionMgn) {
        sessionMgn->stop();
        DELETE(sessionMgn);
    }
}

bool BenchMarkBySecInterval::initialize()
{
    std::string cs_addr = clusterCfg["configserver"].get<std::string>();
    std::string group_name = clusterCfg["group"].get<std::string>();

    apiImpl.setLogger(sDefLogger);

    uint32_t ans_thread_count = config.value("ans_thread_count", 2);

    auto handler = std::bind(&BenchMarkBySecInterval::process, this, std::placeholders::_1,
                             std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    apiImpl.setAsyncHandler(handler);
    apiImpl.setAnsThreadNum(ans_thread_count);
    apiImpl.setClientWrapperType(kv::KvClientApiImpl::EClientWrapperType::E_CClientWrapper);

    bool done = apiImpl.startup(StringHelper::tokenize(cs_addr, ","), group_name);
    if ( !done ) {
        fprintf(stderr, "can not connect to %s %s", cs_addr.c_str(), group_name.c_str());
        return false;
    }

    sessionMgn = new mybase::CSessionMgn(1000*1000, 1);
    if (!sessionMgn) return false;

    auto timeout_handler = std::bind(&BenchMarkBySecInterval::handleTimeout, this, std::placeholders::_1);
    sessionMgn->setTimeoutHandler(timeout_handler);
    sessionMgn->start();

    return true;
}

void BenchMarkBySecInterval::process(uint32_t seq, int32_t pcode, int32_t result, void* data)
{
    std::shared_ptr<mybase::BaseSession> session = sessionMgn->eraseAndGetSession(seq);
    if (!session) {
        return ;
    }

    uint64_t time_cost = TimeHelper::currentMs() - _SC(ReqClientSession*, session.get())->startMs;

    int32_t slot = 0;
    if (time_cost >= 0 && time_cost < 10) {
        slot = 0;
    } else if ( time_cost >= 10 && time_cost < 20) {
        slot = 1;
    } else if ( time_cost >= 20 && time_cost < 50) {
        slot = 2;
    } else if ( time_cost >= 50 && time_cost < 100) {
        slot = 3;
    } else {
        slot = 4;
    }

    {
        std::lock_guard<std::mutex> l(mutex);
        auto itr = results.find(result);
        if (itr == results.end()) {
            results.emplace(result, 1);
        } else {
            results[result]++;
        }

        if (timestats.find(slot) == timestats.end()) {
            timestats.emplace(slot, 1);
        } else {
            timestats[slot]++;
        }
    }
}

void BenchMarkBySecInterval::handleTimeout(std::shared_ptr<mybase::BaseSession> req)
{
    std::lock_guard<std::mutex> l(mutex);
    auto itr = results.find(-3989);
    if (itr == results.end()) {
        results.emplace(-3989, 1);
    } else {
        results[-3989]++;
    }

    if (timestats.find(5) == timestats.end()) {
        timestats.emplace(5, 1);
    } else {
        timestats[5]++;
    }

    return ;
}

void BenchMarkBySecInterval::run(int32_t idx)
{
    ThreadHelper::setThreadName("worker" + std::to_string(idx));

    sleep(2);
    std::string data;
    data.assign(&value[0], valSize);

    // wait to start
    uint64_t start_msec = TimeHelper::currentMs();

    std::random_device r;
    std::default_random_engine e(r());
    std::uniform_int_distribution<uint64_t> uid(0, maxKeyRangeCount);

    auto& work_load_cfg = config["workload"];
    std::string key_generator = work_load_cfg.value("key_generator", "sequential");
    std::string benchmark_type = work_load_cfg.value("type", "put");

    while (running) {
        uint64_t sequence = opsCount.fetch_add(1);

        if (keyGenerator == 1) {
            uint64_t key_number = sequence % maxKeyRangeCount;
            std::ostringstream oss;
            oss << "%0" << keySize << "lu";
            snprintf(benchMarkKey, 1024, oss.str().c_str(), key_number);
        } else if (keyGenerator == 2) {
            std::ostringstream oss;
            oss << "%0" << (keySize >> 2) << "lu" << "%0" << (keySize - (keySize >> 2) ) << "lu";
            snprintf(benchMarkKey, keySize+1, oss.str().c_str(), uid(e), uid(e));
        }

        uint64_t cur_msec = TimeHelper::currentMs();

        std::shared_ptr<mybase::BaseSession> session = std::make_shared<ReqClientSession>(TimeHelper::currentMs() + timeoutMsec, sequence);
        _SC(ReqClientSession*, session.get())->startMs = cur_msec;
        sessionMgn->saveSession(session);

        int32_t rc = -1;
        if (benchmarkType == 1) {
            rc = apiImpl.asyncPut(sequence, ns, benchMarkKey, data, ttl, 0);
        } else if (benchmarkType == 2) {
            rc = apiImpl.asyncGet(sequence, ns, benchMarkKey);
        } else if (benchmarkType == 3) {
            rc = apiImpl.asyncRemove(sequence, ns, benchMarkKey);
        }

        if (rc != 0) {
            session = sessionMgn->eraseAndGetSession(sequence);

            std::lock_guard<std::mutex> l(mutex);
            auto itr = results.find(rc);
            if (itr == results.end()) {
                results.emplace(rc, 1);
            } else {
                results[rc]++;
            }
        }

        std::string result_info;
        {
            do {
                std::lock_guard<std::mutex> l(mutex);

                if ((cur_msec <= start_msec) || ( (cur_msec - start_msec) < sampleBySecInterval * 1000)) break;

                result_info = toString();
                results.clear();
                timestats.clear();

                uint64_t send_count = sequence - lastSequence;
                _log_info(sDefLogger, "%lu gen[%s] type[%s] ksize[%d] vsize[%d] bySec[%d] send[%lu] %s",
                          cur_msec, key_generator.c_str(), benchmark_type.c_str(),
                          keySize, valSize, sampleBySecInterval, send_count, result_info.c_str());
                lastSequence = sequence;
                start_msec = TimeHelper::currentMs();
            } while(false);
        }

        asyncLimitPtr->limitrate();
    }
}
