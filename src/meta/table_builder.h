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

#include "public/dlog.h"
#include "meta.pb.h"
#include "common/defs.h"
#include "public/common.h"

#include <vector>
#include <string>
#include <map>
#include <set>
#include <utility>
#include <algorithm>

namespace mybase
{

static const int CONSIDER_ALL = 0; //
static const int CONSIDER_POS = 1; //主要为掩码设置
static const int CONSIDER_BASE = 2;//主要为master bucket设置
static const int CONSIDER_FORCE = 3; //强制为有效，只要同样的节点不出现在同一行

static const int NODE_OK = 0; //ok
static const int INVALID_NODE = 1; //无效
static const int TOOMANY_MASTER = 2; //某个server出现在master bucket次数过多
static const int TOOMANY_BUCKET = 3;//某个server出现在bucket次数过多
static const int SAME_NODE = 4; //同一行出现同样的节点
static const int SAME_POS = 5;//同一行出现同样的掩码

static const uint64_t INVALID_FLAG = 0;

static const int BUILD_ERROR = 0;
static const int BUILD_OK = 1;
static const int BUILD_QUICK = 2;
static const int BUILD_NO_CHANGE=3;

class TableBuilder
{
public:
    TableBuilder(uint32_t bucket_c, uint32_t copy_c) : bucketCount(bucket_c), copyCount(copy_c) { }
    virtual ~TableBuilder();
    typedef std::pair<uint64_t, uint32_t> server_id_type;
    typedef std::vector<server_id_type> hash_table_line_type;
    typedef std::map<int32_t, hash_table_line_type> hash_table_type;   // map<copy_id, vector<server_id, pos_mask_id> >
    typedef std::set<server_id_type> server_list_type;
    typedef std::map<server_id_type, int32_t> server_capable_type;

    void loadHashTable(hash_table_type& hash_table, uint64_t* p_hash_table);
    void writeHashTable(const hash_table_type& hash_table, uint64_t* p_hash_table);
    bool buildQuickTable(hash_table_type & hash_table_dest);
    void setPosMask(uint64_t m) { posMask = m; }

public:
    int32_t rebuildTableNew(const hash_table_type & hash_table_source, hash_table_type & hash_table_result);
    virtual void setAvailableServer(const std::vector<ServerInfo>& ava_ser);
    void setLogger(mybase::BaseLogger* logger) { myLog = logger; }

public:
    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    //这些打印函数是为了调试使用的
    void printTokensInNode();
    void printCountServer();
    void printAvailableServer();
    void printHashTable(hash_table_type & hash_table);

    /////////////////////////////////////////////////////////////////////////////////////////////////////////

protected:
    static void changeTokensCountInNode(std::map<server_id_type, int32_t>&count_in_node,
                                        const server_id_type & node_id,
                                        std::map<int32_t, server_list_type> &count_server,
                                        std::map<int32_t, server_list_type> &candidate_node,
                                        server_capable_type & server_capable,
                                        bool minus = true);

private:
    TableBuilder & operator =(const TableBuilder &);
    TableBuilder(const TableBuilder &);

protected:
    void initTokenCount(std::map<server_id_type, int32_t>& collector);
    bool updateNodeCount(server_id_type node_id, std::map<server_id_type, int32_t>& collector);
    void buildIndex(const std::map<server_id_type, int32_t>& collector, std::map<int32_t, server_list_type> &indexer);

    bool isNodeAvailble(const server_id_type & node_id);
    bool changeMasterNode(size_t idx, hash_table_type & hash_table_dest, bool force_flag = false);
    void sortMapByValue(std::map <server_id_type, int32_t> & tMap, std::multimap<int32_t, server_id_type> &value_tMap);
    void initCandidateNew(std::map <server_id_type, int32_t> &candidate_node, int32_t node_min_count, std::map<server_id_type, int32_t> &pcount_server);

    bool checkNodeOk(const server_id_type &node, int32_t bucket_num, int32_t position, hash_table_type& tmp_hash_table_result);
    void decrNodeCount(const server_id_type node, std::map<server_id_type, int32_t>& desc_map);
    void incrNodeCount(const server_id_type node, std::map<server_id_type, int32_t>& inc_map);
    bool chooseSlaveForInvalid(int32_t i, hash_table_type & tmp_hash_table_result);
    bool checkRebuildNeed(hash_table_type & hash_table_source);
    void getNode(server_id_type &check_node, std::set<server_id_type> & servers_temp, uint32_t mod_num);
    void initBucketSet(hash_table_type & hash_table_source);
    bool buildFirstTime(hash_table_type & tmp_hash_table_result);
    bool chooseNodeInvalid(int32_t bucket_num, int32_t position, std::map<server_id_type, int32_t> &node_flag,
    								      std::map<server_id_type, int32_t> &migrate_node_info, hash_table_type& tmp_hash_table_result,
    								      std::map<server_id_type, std::map<uint32_t, uint32_t> >& buckets_set, bool master_flag);   
    bool chooseNode(std::map<server_id_type, int32_t>& node_flag, std::map<server_id_type, int32_t>& migrate_node_info,
    						    int32_t& count_info, server_id_type &node_info, hash_table_type & tmp_hash_table_result,
    						    std::map<server_id_type, std::map<uint32_t, uint32_t> >& buckets_set, bool master_flag);

    bool eraseBucketMap(server_id_type& node_info, int32_t bucket, int32_t position,
                        std::map<server_id_type, std::map<uint32_t, uint32_t> > &buckets_set);
    bool insertBucketMap(server_id_type& node_info, int32_t bucket, int32_t position,
                         std::map<server_id_type, std::map<uint32_t, uint32_t> > &buckets_set);

protected:
    mybase::BaseLogger* myLog{nullptr};

    // count of buckets the server hold
    // 重建目标表时记录的每个节点上bucket的个数
    std::map<server_id_type, int> tokens_count_in_node_now;

    std::map<server_id_type, int32_t> tokens_count_in_node; // server id -> bucket number
    std::map<int32_t, server_list_type> count_server; // bucket number -> server list
    std::map<server_id_type, int32_t> mtokens_count_in_node; // server id -> master bucket number
    std::map<int32_t, server_list_type> mcount_server; // master bucket number -> server list

    // map<(now_count - capable_count), server>
    std::map<int32_t, server_list_type> scandidate_node; //每个bucket数目对应的候选者server list
    std::map<int32_t, server_list_type> mcandidate_node;//每个master bucket数目对应的候选者server list

    std::map<server_id_type, int32_t> m_migrate_node_info;
  	std::map<server_id_type, int32_t> s_migrate_node_info;
  	std::map<server_id_type, int32_t> m_node_flag;
  	std::map<server_id_type, int32_t> s_node_flag;
  	std::map<server_id_type, std::map<uint32_t, uint32_t> > m_buckets_set;
  	std::map<server_id_type, std::map<uint32_t, uint32_t> > s_buckets_set;

  	int32_t maxBucketCount;
  	int32_t maxMasterCount;
    server_list_type availableServer;
    server_capable_type server_capable;
    server_capable_type master_server_capable;

    uint64_t posMask{sDefaultPosMask};
    uint32_t bucketCount;
    uint32_t copyCount;
};

}
