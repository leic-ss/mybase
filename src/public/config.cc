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

#include "config.h"

#include "common.h"

namespace mybase {

CConfig::~CConfig()
{
    for(auto& item : configMap) {
        delete item.second;
    }
}

CConfig& CConfig::getCConfig()
{
    static CConfig _config;
    return _config;
}

int32_t CConfig::parseValue(char *str, char *key, char *val)
{
    char *p = nullptr, *p1 = nullptr, *name = nullptr, *value = nullptr;

    if ( !str ) return -1;

    p = str;
    // remove prev blank
    while ((*p) == ' ' || (*p) == '\t' || (*p) == '\r' || (*p) == '\n') p++;
    p1 = p + strlen(p);

    // remove back blank
    while(p1 > p) {
        p1 --;
        if (*p1 == ' ' || *p1 == '\t' || *p1 == '\r' || *p1 == '\n') continue;
        p1 ++;
        break;
    }

    (*p1) = '\0';
    // comment line or empty line
    if (*p == '#' || *p == '\0') return -1;
    p1 = strchr(str, '=');

    if (p1 == nullptr) return -2;
    name = p;
    value = p1 + 1;
    while ((*(p1 - 1)) == ' ') p1--;
    (*p1) = '\0';

    while ((*value) == ' ') value++;
    p = strchr(value, '#');
    if (p == NULL) p = value + strlen(value);
    while ((*(p - 1)) <= ' ') p--;
    (*p) = '\0';
    if (name[0] == '\0') {
        return -2;
    }

    strcpy(key, name);
    strcpy(val, value);

    return 0;
}

// section name
char *CConfig::isSectionName(char *str) {
    if (str == nullptr || strlen(str) <= 2 || (*str) != '[') return nullptr;
        
    char *p = str + strlen(str);
    while ((*(p-1)) == ' ' || (*(p-1)) == '\t' || (*(p-1)) == '\r' || (*(p-1)) == '\n') p--;
    if (*(p-1) != ']') return nullptr;
    *(p-1) = '\0';

    p = str + 1;
    while(*p) {
        if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || (*p == '_')) {
            // do nothing
        } else {
            return nullptr;
        }
        p ++;
    }
    return (str + 1);
}

int CConfig::load(const char *filename)
{
    mtx.lock();

    FILE           *fp;
    char            key[128], value[4096], data[4096];
    int             ret, line = 0;
    
    if ((fp = fopen(filename, "rb")) == nullptr) {
        fprintf(stderr, "can not open file: %s\n", filename);
        mtx.unlock();
        return EXIT_FAILURE;
    }
    
    STR_STR_MAP *m = nullptr;
    while (fgets(data, 4096, fp)) {
        line ++;
        char *sName = isSectionName(data);
        // is section name
        if (sName != nullptr) {
            auto it = configMap.find(sName);
            if (it == configMap.end()) {
                m = new STR_STR_MAP();
                configMap.emplace(sName, m);
            } else {
                m = it->second;
            }
            continue;
        }
        ret = parseValue(data, key, value);
        if (ret == -2) {
            fprintf(stderr, "parse error, Line: %d, %s\n", line, data);
            fclose(fp);
            mtx.unlock();
            return EXIT_FAILURE;
        }
        if (ret < 0) {
            continue;
        }
        if (m == nullptr) {
            fprintf(stderr, "not in config section, Line: %d, %s\n", line, data);
            fclose(fp);
            mtx.unlock();
            return EXIT_FAILURE;
        }            

        auto it1 = m->find(key);
        if (it1 != m->end()) {
            it1->second += '\0';
            it1->second += value;
        } else {
            m->emplace(key, value);
        }
    }
    fclose(fp);

    mtx.unlock();
    return EXIT_SUCCESS;
}

const char *CConfig::getString(const char *section, const std::string& key, const char *d)
{
    mtx.lock();
    auto it = configMap.find(section);
    if (it == configMap.end()) {
        mtx.unlock();
        return d;
     }
    auto it1 = it->second->find(key);
    if (it1 == it->second->end()) {
        mtx.unlock();
        return d;
    }

    mtx.unlock();
    return it1->second.c_str();
}

std::vector<const char*> CConfig::getStringList(const char *section, const std::string& key) {
    mtx.lock();
    std::vector<const char*> ret;
    auto it = configMap.find(section);
    if (it == configMap.end()) {
        mtx.unlock();
        return ret;
    }
    auto it1 = it->second->find(key);
    if (it1 == it->second->end()) {
        mtx.unlock();
        return ret;
    }
    const char *data = it1->second.data();
    const char *p = data;
    for(int i=0; i<(int)it1->second.size(); i++) {
        if (data[i] == '\0') {
            ret.push_back(p);
            p = data+i+1;
        }
    }
    ret.push_back(p);
    mtx.unlock();
    return ret;
}

int32_t CConfig::getInt(const char *section, const std::string& key, int d)
{
    const char *str = getString(section, key);
    int32_t rv = d;

    if (StringHelper::isPureInt(str)) rv = std::stoi(str);
    return rv;
}

std::vector<int32_t> CConfig::getIntList(const char *section, const std::string& key) {
    mtx.lock();
    std::vector<int> ret;
    auto it = configMap.find(section);
    if (it == configMap.end()) {
        mtx.unlock();
        return ret;
    }
    auto it1 = it->second->find(key);
    if (it1 == it->second->end()) {
        mtx.unlock();
        return ret;
    }
    const char *data = it1->second.data();
    const char *p = data;
    for(int i=0; i<(int)it1->second.size(); i++) {
        if (data[i] == '\0') {
            ret.push_back(atoi(p));
            p = data+i+1;
        }
    }
    ret.push_back(atoi(p));

    mtx.unlock();
    return ret;
}

// 取一section下所有的key
int CConfig::getSectionKey(const char *section, std::vector<std::string> &keys)
{
    mtx.lock();
    auto it = configMap.find(section);
    if (it == configMap.end()) {
        mtx.unlock();
        return 0;
    }

    for(auto it1=it->second->begin(); it1!=it->second->end(); ++it1) {
        keys.push_back(it1->first);
    }
    mtx.unlock();
    return (int)keys.size();
}

// 得到所有section的名字
int CConfig::getSectionName(std::vector<std::string> &sections)
{
    mtx.lock();
    for(auto it=configMap.begin(); it!=configMap.end(); ++it)
    {
        sections.push_back(it->first);
    }
    mtx.unlock();
    return (int)sections.size();
}

// toString
std::string CConfig::toString()
{
    mtx.lock();
    std::string result;

	for(auto it=configMap.begin(); it!=configMap.end(); ++it) {
        result += "[" + it->first + "]\n";
	    for(auto it1=it->second->begin(); it1!=it->second->end(); ++it1) {
	        std::string s = it1->second.c_str();
            result += "    " + it1->first + " = " + s + "\n";
            if (s.size() != it1->second.size()) {
                char *data = (char*)it1->second.data();
                char *p = NULL;
                for(int i=0; i<(int)it1->second.size(); i++) {
                    if (data[i] != '\0') continue;
                    if (p) result += "    " + it1->first + " = " + p + "\n";
	                p = data+i+1;
	            }
	            if (p) result += "    " + it1->first + " = " + p + "\n";
	        }
        }
    }
    result += "\n";
    mtx.unlock();    
    return result;
}

}
