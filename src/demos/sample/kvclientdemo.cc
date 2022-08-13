#include "kv_client_api.h"

void process(uint32_t seq, int32_t pcode, int32_t result, void* data)
{
    if (!data || result != OP_RETURN_SUCCESS) {
        _log_info(sDefLogger, "async handler! seq[%u] pcode[%d] result[%d]", seq, pcode, result);
    } else {
        kv::DataEntry* entry = _SC(kv::DataEntry*, data);
        _log_info(sDefLogger, "async handler! seq[%u] pcode[%d] result[%d] data[%.*s]",
                  seq, pcode, result, entry->getSize(), entry->getData());
    }
}

int32_t main(int32_t argc, char* argv[])
{
    sDefLogger->setLogLevel("info");
    // sDefLogger->setFileName("kv_client.log");
    
    for (uint32_t i = 0; i < 1000000000; i++) {
        kv::KvClientApi api;

        api.setClientWrapperType(kv::KvClientApiImpl::EClientWrapperType::E_CClientWrapper);
        api.setLogger(sDefLogger);
        api.startup("192.168.56.101:5198", "192.168.56.101:5199", "group_1");

        int32_t rc = api.put(3, "key 300392738", "value", 1000);
        _log_info(sDefLogger, "put! rc = %d", rc);

        std::string data;
        rc = api.get(3, "key 300392738", data);
        _log_info(sDefLogger, "get! rc[%d] data[%.*s]", rc, data.size(), data.data());

        kv::KvClientApiImpl api_impl;
        api_impl.setClientWrapperType(kv::KvClientApiImpl::EClientWrapperType::E_CClientWrapper);
        api_impl.setLogger(sDefLogger);
        api_impl.setAnsThreadNum(3);

        auto async_handler = std::bind(process, std::placeholders::_1, std::placeholders::_2,
                                       std::placeholders::_3, std::placeholders::_4);
        api_impl.setAsyncHandler(async_handler);

        api_impl.startup("192.168.56.101:5198", "192.168.56.101:5199", "group_1");

        api_impl.asyncPut(1, 3, "key 300392738", "value 2", 1000, 0);

        api_impl.asyncGet(2, 3, "key 300392738");

        // usleep(10*1000);
    }

    return 0;
}