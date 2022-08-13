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

#ifndef KV_STAT_INFO_H
#define KV_STAT_INFO_H

#include <vector>
#include <map>

#include "common/defs.h"
#include "public/buffers.h"

#include "stat_info.h"

namespace mybase
{

class StatInfoDetail
{
public:
    enum
    {
        DATASIZE,
        USESIZE,
        ITEMCOUNT
    };

public:
    StatInfoDetail() { }
    size_t get_unit_size() const { return data_holder.size(); }
    uint64_t get_unit_value(uint32_t unit_index) const {
        return unit_index >= get_unit_size() ? 0 : data_holder[unit_index];
    }

    int64_t set_unit_value(uint32_t unit_index, int64_t v);
    int64_t add_unit_value(uint32_t unit_index, int64_t v);

    void update_stat_info_detail(const StatInfoDetail & sv);
    void insert_stat_info_detail(const StatInfoDetail & sv);

    void clear() { data_holder.clear(); }
    void format_detail(const char *prefix, std::map<std::string, std::string> &m_k_v) const;
    void format_total_detail(const char *prefix, std::map<std::string, std::string> &m_k_v) const;

    void format_detail(std::map<std::string, int64_t> &m_k_v) const;

    void encode(mybase::DataBuffer * output) const;
    void decode(mybase::DataBuffer * input);

private:
    std::vector<int64_t> data_holder;
};

class NodeStatInfo
{
public:
    NodeStatInfo() : last_update_time(0) {}
    NodeStatInfo(const NodeStatInfo & rv) : last_update_time(rv.last_update_time), data_holder(rv.data_holder) { }

    NodeStatInfo& operator =(const NodeStatInfo& rv);
    void setLastUpdateTime(uint32_t time) { last_update_time = time; }
    uint32_t getLastUpdateTime() const { return last_update_time; }

    std::map<uint32_t, StatInfoDetail> getStatData() const { return data_holder; }

    void insert_stat_detail(uint32_t area, const StatInfoDetail & detail) {
        data_holder[area].insert_stat_info_detail(detail);
    }
    void update_stat_info(uint32_t area, const StatInfoDetail & detail)
    {
        data_holder[area].update_stat_info_detail(detail);
    }
    void update_stat_info(const NodeStatInfo & rv);

    void clear() { data_holder.clear(); }

    void encode(mybase::DataBuffer * output) const;
    void decode(mybase::DataBuffer * input);

    void format_info(std::map<std::string, std::string> &m_k_v) const;
    void format_total_info(std::map<std::string, std::string> &m_k_v) const;

private:
    uint32_t last_update_time;
    std::map<uint32_t, StatInfoDetail> data_holder;
};

}

#endif
