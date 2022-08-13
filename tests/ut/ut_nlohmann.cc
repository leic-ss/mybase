#include "gtest/gtest.h"

#include <nlohmann/json.hpp>
#include "dlog.h"
#include "url.h"
#include "common.h"
// #include "json/json.h"

#include <string>

#include <stdint.h>
#include <assert.h>

// for convenience
using json = nlohmann::json;

TEST(Nlohmann, test_nlohmann_array)
{
    json obj = json::array({ 1, 2 });
    EXPECT_EQ((int32_t)obj.size(), 2);
    EXPECT_EQ(obj[0], 1);
    EXPECT_EQ(obj[1], 2);

    json obj2 = json::array();
    for (uint32_t i = 0; i < 10; i++) {
        json j;
        j.push_back("foo");
        j.push_back(1);
        j.push_back(true);
        obj2.push_back(j);
    }
    log_info("obj2: %s", obj2.dump().c_str());

    try {
    	json j3 = json::parse("{ \"happy\": true, \"pi\": 3.141}");
    	log_info("j3: %s", j3.dump().c_str());
    } catch (std::exception& e) {
    	log_info("Exception: %s", e.what());
    }

    try {
        json j3 = json::parse("{ 'happy': true, 'pi': '3.141' }");
        log_info("j3: %s", j3.dump().c_str());
    } catch (std::exception& e) {
        log_info("Exception: %s", e.what());
    }

    // log_info("url: %s", url_encode("http://www.metools.info/master/m1.html"));
    // log_info("url: %s", url_encode("openid fullname email"));
    // log_info("url: %s %s", urlEncode("http://www.metools.info/master/m1.html").c_str(), urlDecode( urlEncode("http://www.metools.info/master/m1.html") ).c_str());
    // log_info("url: %s %s", urlEncode("openid fullname email").c_str(), urlDecode( urlEncode("openid fullname email") ).c_str() );

    json j4;
    for (uint32_t i = 0; i < 3; i++) {
        json obj3 = json::array();
        for (uint32_t k = 0; k < 10; k++) {
            json j;
            j.push_back("foo");
            j.push_back(1);
            j.push_back(true);
            obj3.push_back(j);
        }

        j4["field" + i] = obj3;
    }

    json j5;
    for (uint32_t i = 0; i < 3; i++) {
        j5["field" + i] = j4;
    }

    json j6;
    for (uint32_t i = 0; i < 3; i++) {
        j6["field" + i] = j5;
    }

    // log_info("%d", j6.dump().size());

    // std::string json_str = j6.dump();
    // uint64_t start_usec = TimeHelper::currentUs();
    // for (uint32_t i = 0; i < 100000; i++) {
    //     Json::CharReaderBuilder rbuilder;
    //     Json::CharReader* reader = rbuilder.newCharReader();

    //     rbuilder["collectcomments"] = false;
    //     Json::Value root;
    //     JSONCPP_STRING errs;

    //     bool parse_ok = reader->parse(json_str.c_str(),json_str.c_str() + json_str.length(), &root, &errs);
    // }

    // uint64_t end_usec = TimeHelper::currentUs();
    // log_info("%lu", end_usec - start_usec);
}
