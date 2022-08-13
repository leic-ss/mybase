/************************************************************************
Copyright 2020 ~ 2021
Author: zhanglei

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
 
    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**************************************************************************/

#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

#include <string.h>
#include <stdint.h>

struct StrHash
{
    uint64_t operator()(const std::string& str) const
    {
        return std::hash<std::string>{}(str);
    }
};

typedef std::unordered_map<std::string, std::string> STR_STR_MAP;
typedef STR_STR_MAP::iterator STR_STR_MAP_ITER;
typedef std::unordered_map<std::string, STR_STR_MAP*> STR_MAP;
typedef STR_MAP::iterator STR_MAP_ITER;

namespace mybase {

class CConfig {
public:
    CConfig() {}
    ~CConfig();

    int32_t load(const char *filename);
    const char *getString(const char *section, const std::string& key, const char *d = NULL);
    std::vector<const char*> getStringList(const char *section, const std::string& key);
    int32_t getInt(char const *section, const std::string& key, int32_t d = 0);
    std::vector<int32_t> getIntList(const char *section, const std::string& key);
    int32_t getSectionKey(const char *section, std::vector<std::string> &keys);
    int32_t getSectionName(std::vector<std::string> &sections);
    std::string toString();
    static CConfig& getCConfig();

private:
    std::mutex mtx;
    // two level map
    STR_MAP configMap;

private:
    // parse string
    int32_t parseValue(char *str, char *key, char *val);
    char *isSectionName(char *str);
};

}

static mybase::CConfig& sDefaultConfig = mybase::CConfig::getCConfig();