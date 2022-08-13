#pragma once

#include "asynclimit.h"
#include "kv_client_api.h"
#include "dlog.h"
#include "common.h"
#include "sessionmgn.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <map>
#include <mutex>
#include <thread>

#include <stdio.h>
#include <stdint.h>

class ReqClientSession : public mybase::BaseSession
{
public:
    std::string m_key;
    std::string m_value;
    int32_t m_pCode;
    uint64_t startMs;

public:
    ReqClientSession(uint64_t timeout, uint32_t seq) : mybase::BaseSession(timeout, seq) {   }
};

class BenchMarkBase
{
public:
    BenchMarkBase(nlohmann::json& cfg);
    ~BenchMarkBase();

public:
    virtual bool initialize() = 0;

    bool start();
    void wait();

    std::string toString();

protected:
    virtual void run(int32_t idx) = 0;

protected:
    nlohmann::json config;

    std::atomic<bool> running{false};
    std::atomic<uint64_t> maxKeyRangeCount;
    std::atomic<uint64_t> maxOpsPerSec;

    uint32_t keySize;
    uint32_t valSize;
    uint32_t timeoutMsec;

    uint64_t sampleBySecInterval;

    kv::KvClientApiImpl apiImpl;

    uint32_t keyGenerator{0};
    uint32_t benchmarkType{0};
    GenericTimerTs generic_timer;

    std::shared_ptr<mybase::CAsyncLimit> asyncLimitPtr;
    std::vector<std::thread> thds;

    static thread_local char benchMarkKey[1024];
    std::vector<char> value;

    int32_t ns;
    int32_t ttl;

    std::mutex mutex;
    std::unordered_map<int32_t, uint64_t> results;
    std::unordered_map<int32_t, uint64_t> timestats;
};

class BenchMarkBySecInterval : public BenchMarkBase
{
public:
    BenchMarkBySecInterval(nlohmann::json& cfg, nlohmann::json& cluster_cfg)
                            : BenchMarkBase(cfg), clusterCfg(cluster_cfg)
    { }
    ~BenchMarkBySecInterval();

public:
    bool initialize();

protected:
    void process(uint32_t seq, int32_t pcode, int32_t result, void* data);
    void handleTimeout(std::shared_ptr<mybase::BaseSession> req);

private:
    void run(int32_t idx);

private:
    std::atomic<uint64_t> opsCount{1};
    nlohmann::json clusterCfg;

    static thread_local bool initialized;
    uint64_t startMsec;
    uint64_t lastSequence;
    mybase::CSessionMgn* sessionMgn{nullptr};
};
