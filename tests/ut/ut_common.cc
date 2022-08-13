#include "gtest/gtest.h"

#include "cast_helper.h"
#include "common.h"
#include "dlog.h"
#include "bitmap.h"
#include "asynclimit.h"
#include "raft_logger.h"

#include <chrono>
#include <thread>
#include <iostream>

class BaseTestData
{
public:
    BaseTestData() {}
    virtual ~BaseTestData() {}

public:
    uint32_t sequence;
};

class TestData : public BaseTestData
{
public:
    std::string key;
    std::string value;

public:
    TestData() : BaseTestData() {   }
};

void thread_func(int32_t n){
    std::cout << "n=" << n << std::endl;
}

TEST(common, test_sysmgr_popen)
{
	std::string output;
	//int32_t rc = SysMgrHelper::popen("curl 127.0.0.1:8080/api/v1/dataservice/role/list", output);
    //log_info("rc[%d] output[%s]", rc, output.c_str());

	for (uint32_t i = 0; i < 1; i++) {
		std::shared_ptr<BaseTestData> session = std::make_shared<TestData>();
    	_SC(TestData*, session.get())->value.assign(1024*1024, 'c');
    	std::shared_ptr<BaseTestData> session2 = session;

    	usleep(100*1000);
	}
}

TEST(common, test_thread)
{
    std::thread t(thread_func, 100);
    std::thread t2 = std::move(t);

    usleep(100*1000);
    std::cout << "t is joinable? " << t.joinable() << std::endl;
    std::cout << "t2 is joinable? " << t2.joinable() << std::endl;
    t2.join();

    std::cout << TimeHelper::timeConvert(1619232795) << std::endl;
}

TEST(common, test_bitmap)
{
    CBitMap bitmap;
    uint32_t number = 65535;
    bitmap.resize(number, true);

    EXPECT_EQ(bitmap.size(), number);

    nubase::CAsyncLimit limit(1000);
    for (uint32_t i = 0; i < number; i++) {
        // limit.limitrate();
        EXPECT_TRUE(bitmap.test(i));
    }
}

TEST(common, test_asynclimit)
{
    // nubase::CAsyncLimit limit(1000);
    
    for (uint32_t i = 0; i < 1000; i++) {
        // limit.limitrate();

        if (i % 1000 == 0) {
            log_info("i = %u", i);
        }
    }
}

TEST(common, test_benchmark)
{
    uint32_t number = 10000000;

    uint64_t startusec = TimeHelper::currentUs();
    for (uint32_t i = 0; i < number; i++) {
        char* buf = (char*)malloc(1024 + (i % 1024) );

        if (i % 10000) free(buf);
    }

    log_info("time = %u", TimeHelper::currentUs() - startusec);
}

TEST(common, test_compare)
{
    uint64_t num1 = 0xFFFFFFFFFFFFFFFF - 10;
    uint64_t num2 = 0xFFFFFFFFFFFFFFFF;

    EXPECT_TRUE(num1 < num2);
    EXPECT_TRUE((int64_t)num1 < (int64_t)num2);
}

TEST(common, test_common)
{
    uint32_t ts = 1623119804;
    std::string str = TimeHelper::timeConvertDay(ts);
    fprintf(stderr, "%s\n", str.c_str());

    fprintf(stderr, "%d %d\n", ',', '-');
    fprintf(stderr, "%c %c\n", 44, 45);

    // nubase::RaftLogger* logger = new nubase::RaftLogger;
    // logger->setLogLevel("debg");

    // _log_info(logger, "");
    // _log_debug(logger, "test");
    // _log_info(logger, "test1 %d", 1);

    nubase::CCLogger* logger1 = new nubase::CCLogger();
    nubase::CCLogger* logger2 = new nubase::CCLogger();

    _log_info(logger1, "aaa");
    _log_info(logger2, "bbb");

    std::shared_ptr<TestData> ptr1 = std::make_shared<TestData>();
    std::shared_ptr<TestData> ptr2 = ptr1;

    ptr1.reset();
    _log_info(logger1, "ptr1 is %s, ptr2 is %s", ptr1 ? "not null" : "null", ptr2 ? "not null" : "null");

    nubase::RaftLogger logger;
}
