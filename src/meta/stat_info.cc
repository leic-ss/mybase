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

#include "stat_info.h"

#include <inttypes.h>

const char *format_str[] =
{
    "dataSize",
    "useSize",
    "itemCount",
};

namespace mybase
{

int64_t StatInfoDetail::set_unit_value(uint32_t unit_index, int64_t v)
{
    if(unit_index >= data_holder.size()) {
        data_holder.resize(unit_index + 1, 0);
    }
    data_holder[unit_index] = v;
    return v;
}

int64_t StatInfoDetail::add_unit_value(uint32_t unit_index, int64_t v)
{
    return set_unit_value(unit_index, v + get_unit_value(unit_index));
}

void StatInfoDetail::update_stat_info_detail(const StatInfoDetail & sv)
{
    for(uint32_t i = 0; i < sv.data_holder.size(); i++)
    {
        add_unit_value(i, sv.data_holder[i]);
    }
}

void StatInfoDetail::insert_stat_info_detail(const StatInfoDetail & sv)
{
    data_holder.resize(sv.get_unit_size(), 0);
    for(uint32_t i = 0; i < sv.data_holder.size(); i++)
    {
        set_unit_value(i, sv.data_holder[i]);
    }
}

void StatInfoDetail::format_detail(const char *prefix, std::map<std::string, std::string > &m_k_v) const
{
    char key[200];
    char value[50];
    for(uint32_t i = 0; i < data_holder.size(); i++)
    {
        // uint32_t str_index = (i > (sizeof(format_str) / sizeof(char *) - 2)) ? 0 : i + 1;
        snprintf(key, 200, "%s %s", prefix, format_str[i]);
        snprintf(value, 50, "%" PRId64 "u", data_holder[i]);
        m_k_v[key] = value;
    }
}

void StatInfoDetail::format_total_detail(const char *prefix, std::map<std::string, std::string > &m_k_v) const
{
    char key[200];
    char value[50];
    for(uint32_t i = 0; i < data_holder.size(); i++)
    {
        snprintf(key, 200, "%s %s", prefix, format_str[i]);
        snprintf(value, 50, "%" PRId64 "u", data_holder[i]);

        m_k_v[key] = value;
    }
}

void StatInfoDetail::format_detail(std::map<std::string, int64_t> &m_k_v) const
{
    for(uint32_t i = 0; i < data_holder.size(); i++)
    {
        m_k_v[format_str[i]] = data_holder[i];
    }

    return ;
}

void StatInfoDetail::encode(mybase::DataBuffer * output) const
{
    output->writeInt32(data_holder.size());
    for(uint i = 0; i < data_holder.size(); i++)
    {
        output->writeInt64(data_holder[i]);
    }
}

void StatInfoDetail::decode(mybase::DataBuffer * input)
{
    data_holder.clear();
    uint32_t size = input->readInt32();
    for(uint32_t i = 0; i < size; i++)
    {
        data_holder.push_back(input->readInt64());
    }
}

NodeStatInfo& NodeStatInfo::operator=(const NodeStatInfo& rv)
{
    if(&rv == this) return *this;
    last_update_time = rv.last_update_time;
    data_holder = rv.data_holder;
    return *this;
}
void NodeStatInfo::update_stat_info(const NodeStatInfo & rv)
{
    for(auto it = rv.data_holder.begin(); it != rv.data_holder.end(); it++)
    {
        data_holder[it->first].update_stat_info_detail(it->second);
    }
    last_update_time = rv.last_update_time;
}

void NodeStatInfo::encode(mybase::DataBuffer * output) const
{
    output->writeInt32(data_holder.size());
    for(auto it = data_holder.begin(); it != data_holder.end(); it++)
    {
        output->writeInt32(it->first);
        it->second.encode(output);
    }
}

void NodeStatInfo::decode(mybase::DataBuffer * input)
{
    data_holder.clear();
    uint32_t size = input->readInt32();
    for(uint32_t i = 0; i < size; i++)
    {
        uint32_t area = input->readInt32();
        StatInfoDetail info;
        info.decode(input);
        data_holder[area] = info;
    }
}

void NodeStatInfo::format_info(std::map<std::string, std::string> &m_k_v) const
{
    auto it = data_holder.begin();
    char str_area[20];
    for(; it != data_holder.end(); it++) {
        snprintf(str_area, 20, "%u", it->first);
        it->second.format_detail(str_area, m_k_v);
    }
}

void NodeStatInfo::format_total_info(std::map<std::string, std::string> &m_k_v) const
{
    auto it = data_holder.begin();
    char str_area[20];
    for(; it != data_holder.end(); it++) {
        snprintf(str_area, 20, "%u", it->first);
        it->second.format_total_detail(str_area, m_k_v);
    }
}

}
