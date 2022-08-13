#include "kv_client_api.h"

// g++ -I../src/client -I../src/common -I../src/public -I../src/packets -std=c++11 -o kvclientdemo ../src/demos/sample/kvclientapi.cc client/libkv_client_api_lib.a -lpthread -lz

int32_t main(int32_t argc, char* argv[])
{
    // sDefLogger->setLogLevel("debg");

    // kv::KvClientApi api;

    // api.setLogger(sDefLogger);
    // api.startup("10.196.96.12:5198", "10.196.96.12:5198", "JD_VDEngine_TEST_SET1");
    // // api.startup("192.168.56.101:5299", "192.168.56.101:5298", "group_1");

    // sleep(1);

 //    for (int32_t i = 0; i < 1; i++) {
	// 	std::string key = "key " + std::to_string(i);
	// 	api.put(1, key, "value " + std::to_string(i), 0, 0);
	// }

 //    for (int32_t i = 0; i < 1; i++) {
 //        std::string key = "key " + std::to_string(i);
 //        std::string data;
 //        int32_t rc = api.get(1, key, data);
 //        _log_info(sDefLogger, "get key = %s value = %s, ret = %d\n", key.c_str(), data.c_str(), rc);
 //    }

    // std::string data;
    // int32_t rc = api.get(3, "key 300392738", data);
    // _log_info(sDefLogger, "rc = %d", rc);

    // sleep(300);

    return 0;
}