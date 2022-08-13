#include <fstream>
#include <iostream>
#include <memory>

#include <nlohmann/json.hpp>

#include <stdint.h>
#include <string>
#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <iostream>

int32_t main()
{
	nlohmann::json cfg;
    std::ifstream cfg_in("/media/sf_Shares/dataservice/tmp.json");
    cfg_in >> cfg;

    // std::cout << cfg["data"].size();

    std::map<int32_t, std::string> maps;
    for (int32_t i = 0; i < (int32_t)cfg["data"].size(); i++) {
    	// std::cout << cfg["data"][i]["value"] << std::endl;
    	// if (cfg["data"][i]["areaid"].is_null()) continue;
    	maps.emplace((int32_t)cfg["data"][i]["value"], cfg["data"][i]["name"]);
    }

    for (auto ele : maps) {
    	std::cout << ele.first << ",  " << ele.second << std::endl;
    }
}