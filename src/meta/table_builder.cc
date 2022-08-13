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

#include "table_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

namespace mybase
{

TableBuilder::~TableBuilder()
{}

void TableBuilder::printTokensInNode()
{
    for(auto it = tokens_count_in_node.begin(); it != tokens_count_in_node.end(); it++) {
        _log_debug(myLog, "S(%s:%" PRId64 "d,%d)=%d ", NetHelper::addr2String(it->first.first).c_str(),
                   it->first.first, it->first.second, it->second);
    }

    for(auto it = tokens_count_in_node_now.begin(); it != tokens_count_in_node_now.end(); it++) {
        _log_debug(myLog, "S(%s:%" PRId64 "d,%d)=%d ", NetHelper::addr2String(it->first.first).c_str(),
                   it->first.first, it->first.second, it->second);
    }

    for(auto it = mtokens_count_in_node.begin(); it != mtokens_count_in_node.end(); it++)
    {
        _log_debug(myLog, "S(%s:%" PRId64 "d,%d)=%d ", NetHelper::addr2String(it->first.first).c_str(),
                   it->first.first, it->first.second, it->second);
    }
}

void TableBuilder::printCountServer()
{
    for(auto it = count_server.begin(); it != count_server.end(); it--) {
        _log_debug(myLog, "%d: ", it->first);
        for(auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
            _log_debug(myLog, "%" PRId64 "d, %d  ", it2->first, it2->second);
        }
    }

    _log_debug(myLog, "mcount:");
    for(auto it = mcount_server.begin(); it != mcount_server.end(); it++) {
        _log_debug(myLog, "%d: ", it->first);
        for(auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
            _log_debug(myLog, "%" PRId64 "d, %d  ", it2->first, it2->second);
        }
    }
}

void TableBuilder::printAvailableServer()
{
    _log_debug(myLog, "available server size: %u", availableServer.size());
    for(auto it = availableServer.begin(); it != availableServer.end(); it++) {
        _log_debug(myLog, "S:%s,%-3d  ", NetHelper::addr2String(it->first).c_str(), it->second);
    }
}

void TableBuilder::setAvailableServer(const std::vector<ServerInfo>& ava_server)
{
    availableServer.clear();
    for(auto ele : ava_server) {
        availableServer.insert(std::make_pair(ele.serverid(), ele.serverid() & posMask));
    }
}

void TableBuilder::printHashTable(hash_table_type& hash_table)
{
    if(hash_table.empty()) {
        return;
    }

    for(uint32_t j = 0; j < bucketCount; ++j) {
        char kk[64] = {0};
        sprintf(kk, "%u-->", j);
        std::string ss(kk);

        for(uint32_t i = 0; i < copyCount; ++i) {
            char str[1024];
            sprintf(str, "%s(%-3d)(%s)  ", NetHelper::addr2String(hash_table[i][j].first).c_str(),
                    hash_table[i][j].second, isNodeAvailble(hash_table[i][j]) ? "alive" : "dead");
            ss += str;
        }
        _log_info(myLog, "%s", ss.c_str());
    }
}

void TableBuilder::initTokenCount(std::map<server_id_type, int32_t>& collector)
{
    collector.clear();
    for(auto it = availableServer.begin(); it != availableServer.end(); it++) {
        collector[*it] = 0;
    }
}

bool TableBuilder::updateNodeCount(server_id_type node_id, std::map<server_id_type, int32_t>& collector)
{
    auto node_it = collector.find(node_id);
    if(node_it != collector.end()) {
        node_it->second++;
        return true;
    }
    return false;
}

void TableBuilder::buildIndex(const std::map<server_id_type, int32_t>& collector, std::map<int32_t, server_list_type>& indexer)
{
    indexer.clear();
    for(auto it = collector.begin(); it != collector.end(); it++) {
        indexer[it->second].insert(it->first);
    }
}

bool TableBuilder::isNodeAvailble(const server_id_type & node_id)
{
    if(node_id.first == INVALID_FLAG) {
        return false;
    }
    auto it = availableServer.find(node_id);
    return it != availableServer.end();
}

void TableBuilder::changeTokensCountInNode(std::map<server_id_type, int32_t>& count_in_node,
                                           const server_id_type &node_id,
                                           std::map<int32_t, server_list_type> &count_server_map,
                                           std::map<int32_t, server_list_type> &candidate_node_info,
                                           server_capable_type& server_capable_info,
                                           bool minus)
{
    int32_t& token_count_in_node = count_in_node[node_id];
    count_server_map[token_count_in_node].erase(node_id);
    candidate_node_info[token_count_in_node - server_capable_info[node_id]].erase(node_id);

    if( minus ) {
        token_count_in_node--;
    } else {
        token_count_in_node++;
    }

    count_server_map[token_count_in_node].insert(node_id);
    candidate_node_info[token_count_in_node - server_capable_info[node_id]].insert(node_id);
}

bool TableBuilder::changeMasterNode(size_t idx, hash_table_type & hash_table_dest, bool force_flag)
{
    int32_t chosen_line_num = -1;
    int32_t min_node_count = -1;
    for(size_t next_line = 1; next_line < copyCount; next_line++) {
        server_id_type node_id = hash_table_dest[next_line][idx];
        if ( !isNodeAvailble(node_id) ) continue;

        int32_t &mtoken_count_in_node = mtokens_count_in_node[node_id];
        if(min_node_count == -1 || min_node_count > mtoken_count_in_node) {
            chosen_line_num = next_line;
            min_node_count = mtoken_count_in_node;
        }
    }

    // no availble node
    if(min_node_count == -1) {
        return false;
    }

    server_id_type& choosen_node_id = hash_table_dest[chosen_line_num][idx];
    if ( !force_flag ) {
        if(mtokens_count_in_node[choosen_node_id] >= master_server_capable[choosen_node_id]) {
            return false;
        }
    }

    server_id_type org_node_id = hash_table_dest[0][idx];
    server_id_type new_master_node = choosen_node_id;
    hash_table_dest[0][idx] = new_master_node;
    tokens_count_in_node_now[new_master_node]++;

    changeTokensCountInNode(mtokens_count_in_node, new_master_node, mcount_server,
                            mcandidate_node, master_server_capable, false);

    if(isNodeAvailble(org_node_id)) {
        choosen_node_id = org_node_id;
        changeTokensCountInNode(mtokens_count_in_node, org_node_id, mcount_server,
                                mcandidate_node, master_server_capable, true);
    } else {
        choosen_node_id.first = INVALID_FLAG; //被选择作为master bucket之后，老的位置用0代替
    }
    return true;
}

bool TableBuilder::buildQuickTable(hash_table_type & hash_table_dest)
{
    hash_table_line_type& line = hash_table_dest[0];
    for(uint32_t idx = 0; idx < bucketCount; idx++) {
        if ( isNodeAvailble(line[idx]) ) continue;

        if( !changeMasterNode(idx, hash_table_dest, true) ) {
            _log_err(myLog, "bucket %d lost all of its duplicate so can not find out a master for it quick build failed,but can continue", idx);
        }
    }

    for(uint32_t line_number = 1; line_number < copyCount; ++line_number)
    {
        hash_table_line_type& line = hash_table_dest[line_number];
        for(uint32_t idx = 0; idx < bucketCount; idx++) {
            if( isNodeAvailble( hash_table_dest[0][idx]) ) {
                if(isNodeAvailble(line[idx]) == false) {
                    line[idx].first = 0l;
                    line[idx].second = 0;
                }
            }
        }
    }

    for(uint32_t i = 0; i < bucketCount; i++)
    {
        for(uint32_t j = 1; j < copyCount; j++)
        {
            if(hash_table_dest[j][i].first != INVALID_FLAG) continue;

            for(uint32_t n=j+1; n<copyCount; n++)
            {
                if(hash_table_dest[n][i].first != INVALID_FLAG)
                {
                    hash_table_dest[j][i] = hash_table_dest[n][i];
                    hash_table_dest[n][i].first = INVALID_FLAG;
                    hash_table_dest[n][i].second = INVALID_FLAG;
                    break;
                }
            }
        }
    }

    return true;
}

// 把server table按copy分组保存到hash_table_data
void TableBuilder::loadHashTable(hash_table_type& hash_table_data, uint64_t* p_hash_table)
{
    hash_table_data.clear();
    for(uint32_t i = 0; i < copyCount; i++) {
        hash_table_data[i].reserve(bucketCount);

        for(uint32_t j = 0; j < bucketCount; j++) {
            uint64_t ipport = *p_hash_table;
            _log_info(myLog, "load_hash_table copy_count %u bucket_count %u ipport:%s",
                      i, j, NetHelper::addr2String(ipport).c_str());
            hash_table_data[i].push_back(std::make_pair(*p_hash_table, (*p_hash_table) & posMask));
            p_hash_table++;
        }
    }
}

void TableBuilder::writeHashTable(const hash_table_type& hash_table_data, uint64_t* p_hash_table)
{
    for(auto it = hash_table_data.begin(); it != hash_table_data.end(); it++) {
        const hash_table_line_type & line = it->second;
        for(uint32_t i = 0; i < bucketCount; i++) {
            (*p_hash_table) = line[i].first;
            p_hash_table++;
        }
    }
}

void TableBuilder::sortMapByValue( std::map <server_id_type, int32_t > & tMap, std::multimap<int32_t, server_id_type> &value_tMap )
{
    value_tMap.clear();
    for (auto curr = tMap.begin(); curr != tMap.end(); curr++) {
        value_tMap.insert(std::pair<int32_t, server_id_type>(curr->second, curr->first));
    }
}

void TableBuilder::initCandidateNew(std::map<server_id_type, int32_t> &candidate_node,
                                    int32_t node_min_count,
                                    std::map<server_id_type, int32_t> &pcount_server)

{
    candidate_node.clear();
    for(auto it = pcount_server.begin(); it!=pcount_server.end(); it++) {
        candidate_node.insert(std::pair<server_id_type, int32_t> (it->first, it->second - node_min_count));
    }
}

//通过m_hashtable重建目的对照表
//return value 0 build error  1 ok 2 quick build ok  ,目前no_quick_table参数永远为true，不要用这个函数建立块表
//3表示不需要变更
int TableBuilder::rebuildTableNew(const hash_table_type & hash_table_source, hash_table_type & hash_table_result)
{
    hash_table_result = hash_table_source;
    if(!checkRebuildNeed(hash_table_result)) {
        return BUILD_NO_CHANGE;
    }

    initTokenCount(tokens_count_in_node); //according to hashtable, the count of bucket in DS
    initTokenCount(mtokens_count_in_node); //according to hashtable,the master count of bucket in DS
    initTokenCount(m_node_flag);   //the bucket is nedd to migrate or not (positive need ;0,negative not need)
    initTokenCount(s_node_flag);   //the master bucket is nedd to migrate or not
    initBucketSet(hash_table_result);

    int32_t S = availableServer.size();
    int32_t m_tokens_per_node_min = bucketCount / S;
    int32_t tokens_per_node_min = bucketCount * copyCount / S;
    maxBucketCount = bucketCount * copyCount % S;  // the count of max bucket count
    maxMasterCount = bucketCount % S; // the count of max master bucket count
 
    // compute node count: tokens_count_in_node, mtokens_count_in_node
    //tokens_count_in_node,mtokens_count_in_node
    for(auto it = hash_table_source.begin(); it != hash_table_source.end(); it++)
    {
        const hash_table_line_type & line = it->second;
        for(uint32_t i = 0; i < bucketCount; i++) {
            updateNodeCount(line[i], tokens_count_in_node);
            if(it->first == 0) {
                if ( updateNodeCount(line[i], mtokens_count_in_node) == false ) {
                    // almost every time this will happen
                }
            }
        }
    }
    initCandidateNew(s_migrate_node_info, tokens_per_node_min, tokens_count_in_node);
    initCandidateNew(m_migrate_node_info, m_tokens_per_node_min, mtokens_count_in_node);

    if(availableServer.size() < copyCount) {
        _log_err(myLog, "rebuild table fail, available size: %u, copy count: %u", availableServer.size(), copyCount);
        return BUILD_ERROR;
    }

    //step 1 calculate hash table
    //step 2 check the DS which is 0
    //stop 3 check all DS
    for(uint32_t i = 0; i < bucketCount; i++)
    {
        bool need_check_master = false;
        //if master is ont valid, skip this
        if (hash_table_result[0][i].first == INVALID_FLAG ||
            availableServer.find(hash_table_result[0][i]) == availableServer.end()) {
            continue;
        }

        for(uint32_t j = 1; j < copyCount; j++) {
            if(hash_table_result[j][i].first == INVALID_FLAG) {
                need_check_master = true;
                break;
            }
        }

        if (need_check_master) {
            _log_debug(myLog, "bucket %d need check master %d", i, m_migrate_node_info[hash_table_result[0][i]]);

            if( m_migrate_node_info[hash_table_result[0][i]] <= 0 ) {
                if(!chooseSlaveForInvalid(i, hash_table_result)) {
                    return BUILD_ERROR;
                }
            } else if(m_migrate_node_info[hash_table_result[0][i]] > 0 ) {

                //master can stay on the same DS because the maxBucketCount is positive
                if(maxMasterCount > 0 && m_node_flag[hash_table_result[0][i]] == 0) {
                    incrNodeCount(hash_table_result[0][i], m_node_flag);
                    decrNodeCount(hash_table_result[0][i], m_migrate_node_info);
                    maxMasterCount--;

                    if(!chooseSlaveForInvalid(i, hash_table_result)) {
                        return BUILD_ERROR;
                    }
                } else {
                    //master need migrate
                    //first check master is can change to slave or not
                    //second find a DS for bucket
                    if(s_migrate_node_info[hash_table_result[0][i]] <= 0 ) {
                        decrNodeCount(hash_table_result[0][i], m_migrate_node_info);
                        for(uint32_t n = 1; n < copyCount; n++) {
                            if( hash_table_result[n][i].first == INVALID_FLAG ) {
                                eraseBucketMap(hash_table_result[0][i],i,0,m_buckets_set);
                                insertBucketMap(hash_table_result[0][i],i,n,s_buckets_set);
                                hash_table_result[n][i] = hash_table_result[0][i];
                                hash_table_result[0][i].first = INVALID_FLAG;
                                break;
                            }
                        }

                        if (!chooseNodeInvalid(i,0,m_node_flag,m_migrate_node_info,hash_table_result,m_buckets_set,true)) {
                            _log_err(myLog, "choose_master_node fail, bucket: %d, position: 0", i);
                            return BUILD_ERROR;
                        }
                        if (!chooseSlaveForInvalid(i,hash_table_result)) {
                            return BUILD_ERROR;
                        }
                    } else if(s_migrate_node_info[hash_table_result[0][i]] > 0) {
                        if(maxBucketCount > 0 && s_node_flag[hash_table_result[0][i]] == 0) {
                            decrNodeCount(hash_table_result[0][i], m_migrate_node_info);
                            for(uint32_t n = 1; n < copyCount; n++) {
                                if(hash_table_result[n][i].first == INVALID_FLAG) {
                                    eraseBucketMap(hash_table_result[0][i],i,0,m_buckets_set);
                                    insertBucketMap(hash_table_result[0][i],i,n,s_buckets_set);
                                    hash_table_result[n][i] = hash_table_result[0][i];
                                    hash_table_result[0][i].first = INVALID_FLAG;
                                    break;
                                }
                            }

                            if (!chooseNodeInvalid(i,0,m_node_flag,m_migrate_node_info,hash_table_result,m_buckets_set,true)) {
                                _log_err(myLog, "choose_master_node fail, bucket: %d, position: 0", i);
                                return BUILD_ERROR;
                            }
                            if(!chooseSlaveForInvalid(i,hash_table_result)) {
                                return BUILD_ERROR;
                            }
                        } else {
                            if(!chooseSlaveForInvalid(i,hash_table_result)) {
                                return BUILD_ERROR;
                            }
                        }
                    }
                }
            }

        } // end if
    }  // end for

    //step 3
    if (hash_table_result[0][0].first == INVALID_FLAG) {
        for(uint32_t i=0; i<bucketCount; i++) {
            if(hash_table_result[0][i].first != INVALID_FLAG) {
                _log_err(myLog, "There is not INVALID_FLAG in hashtable for the first time!");
                return BUILD_ERROR;
            }
        }
        if(!buildFirstTime(hash_table_result)) {
            return BUILD_ERROR;
        } else {
            return BUILD_OK;
        }
    }

    std::multimap<int32_t, server_id_type> m_migrate_node_info_temp;
    sortMapByValue(m_migrate_node_info, m_migrate_node_info_temp);

    auto it_while = m_migrate_node_info_temp.end();
    it_while--;
    while(it_while->first > 0)
    {
        if(!chooseNode(m_node_flag,m_migrate_node_info,maxMasterCount,it_while->second,hash_table_result,m_buckets_set,true))
        {
            return BUILD_ERROR;
        }
        sortMapByValue(m_migrate_node_info,m_migrate_node_info_temp);
        it_while= m_migrate_node_info_temp.end();
        it_while--;
    }

    std::multimap<int, server_id_type> s_migrate_node_info_temp;
    sortMapByValue(s_migrate_node_info,s_migrate_node_info_temp);
    auto it_while_s= s_migrate_node_info_temp.end();
    it_while_s--;
    while(it_while_s->first > 0)
    {
        if(!chooseNode(s_node_flag,s_migrate_node_info,maxBucketCount,it_while_s->second,hash_table_result,s_buckets_set,false))
        {
            return BUILD_ERROR;
        }
        sortMapByValue(s_migrate_node_info,s_migrate_node_info_temp);
        it_while_s= s_migrate_node_info_temp.end();
        it_while_s--;
    }

    //static infomation for des table
    std::map<server_id_type, int> stat_count_in_node;
    std::map<server_id_type, int> stat_m_count_in_node;
    initTokenCount(stat_count_in_node);
    initTokenCount(stat_m_count_in_node); //according to hashtable,the master count of bucket in DS
    for(auto it = hash_table_result.begin(); it != hash_table_result.end(); it++)
    {
        const hash_table_line_type & line = it->second;
        for(uint32_t i = 0; i < bucketCount; i++)
        {
            updateNodeCount(line[i], stat_count_in_node);
            if(it->first == 0)
            {
                updateNodeCount(line[i], stat_m_count_in_node);
            }
        }
    }
    int stat_m_count=0;
    int stat_count=0;
    for(auto stat_it=stat_count_in_node.begin(); stat_it!=stat_count_in_node.end(); stat_it++)
    {
        if(stat_it->second > tokens_per_node_min )
        {
            _log_info(myLog, "Dst hashtable %s have buckets %d larger than tokens_per_node_min %d",
                      NetHelper::addr2String(stat_it->first.first).c_str(),stat_it->second,tokens_per_node_min );
            stat_count++;
        }
    }
    for(auto stat_it=stat_m_count_in_node.begin(); stat_it!=stat_m_count_in_node.end(); stat_it++)
    {
        if(stat_it->second > m_tokens_per_node_min )
        {
            _log_info(myLog, "Dst hashtable master %s have buckets %d larger than m_tokens_per_node_min %d",
                      NetHelper::addr2String(stat_it->first.first).c_str(),stat_it->second,m_tokens_per_node_min );
            stat_m_count++;
        }
    }
    const int threshold_value = 8;
    int std_value = (bucketCount * copyCount)% S;
    int std_m_value = bucketCount % S;
    if((stat_m_count - std_m_value) > threshold_value || (stat_count - std_value) > threshold_value)
    {
        _log_err(myLog, "The max count of DS is out of %d(%d,%d)",
                 threshold_value,stat_m_count - std_m_value,stat_count - std_value );
        return BUILD_ERROR;
    }

    _log_info(myLog, "new dst hashtable");
    printHashTable(hash_table_result);

    return BUILD_OK;
}

//判断是否是同一个机器，如果是返回false,否则返回true
bool TableBuilder::checkNodeOk(const server_id_type &node,int bucket_num,int position,hash_table_type & tmp_hash_table_result)
{
    uint32_t copy_index = position;
    for(uint32_t i = 0; i < copyCount; i++)
    {
        if(copy_index == i) continue;

        // if((tmp_hash_table_result[i][bucket_num].first & 0xffffffff) == (node.first & 0xffffffff)) {
        //     return false;
        // }
        if(tmp_hash_table_result[i][bucket_num].first == node.first) {
            return false;
        }
    }
    return true;
}


void TableBuilder::decrNodeCount(const server_id_type node, std::map<server_id_type, int> &desc_map)
{
    if (node.first == INVALID_FLAG) return ;

    if (desc_map.find(node) != desc_map.end()) desc_map[node]--;
    return ;
}

void TableBuilder::incrNodeCount(const server_id_type node, std::map<server_id_type, int32_t> &inc_map)
{
    if (node.first == INVALID_FLAG) return ;

    if (inc_map.find(node) != inc_map.end()) inc_map[node]++;
    return ;
}
bool TableBuilder::chooseSlaveForInvalid(int32_t i, hash_table_type & tmp_hash_table_result)
{
    for(uint32_t n = 1; n < copyCount; n++ )
    {
        if( tmp_hash_table_result[n][i].first == INVALID_FLAG)
        {
            if (!chooseNodeInvalid(i, n, s_node_flag, s_migrate_node_info, tmp_hash_table_result, s_buckets_set, false))
            {
                _log_err(myLog, "choose_slave_node fail, bucket: %d, position: %d", i, n);
                return false;
            }
        }
    }
    return true;
}

bool TableBuilder::checkRebuildNeed(hash_table_type & hash_table_source)
{
    std::set<server_id_type> compare_server;
    for(uint32_t i = 0; i < bucketCount; i++) {
        if(availableServer.find(hash_table_source[0][i]) == availableServer.end()) {
            continue;
        } else {
            compare_server.insert(hash_table_source[0][i]);
        }

        for(uint32_t j = 1; j < copyCount; j++) {
            if(hash_table_source[j][i].first != INVALID_FLAG) {
                if(availableServer.find(hash_table_source[j][i]) == availableServer.end()) {
                    _log_err(myLog, "The quick hashtable DS %s is not in available server.",
                             NetHelper::addr2String(hash_table_source[j][i].first).c_str());
                    return false;
                }
                compare_server.insert(hash_table_source[j][i]);
            } else {
                return true;
            }
        }
    }

    if(availableServer == compare_server) {
        _log_warn(myLog, "The available server is not change, do not need rebuild.");
        return false;
    }

    return true;
}

void TableBuilder::getNode(server_id_type &check_node, std::set<server_id_type> & servers_temp, uint32_t mod_num)
{
    if( mod_num == 1 ) {
        check_node = *servers_temp.begin();
        return;
    }

    uint32_t server_index = rand()%mod_num;
    auto it_temp = servers_temp.begin();
    for(uint32_t m=0; it_temp!=servers_temp.end(); it_temp++,m++)
    {
        if(server_index == m)
        {
            check_node = *it_temp;
            servers_temp.erase(it_temp);
            break;
        }
    }
}

void TableBuilder::initBucketSet(hash_table_type & hash_table_source)
{
    m_buckets_set.clear();
    s_buckets_set.clear();
    for(auto it = availableServer.begin(); it != availableServer.end(); it++) {
        std::map<uint32_t, uint32_t> temp;
        m_buckets_set.insert(std::pair<server_id_type, std::map<uint32_t, uint32_t> >(*it, temp));
        s_buckets_set.insert(std::pair<server_id_type, std::map<uint32_t, uint32_t> >(*it, temp));
    }

    for (uint32_t i = 0; i < bucketCount; i++) {
        uint32_t master_index = 0;
        if(availableServer.find(hash_table_source[master_index][i]) != availableServer.end())
        {
            _log_info(myLog, "init_bucket_set bucket_count %u copy_count[0] %s.",
                      bucketCount, NetHelper::addr2String(hash_table_source[master_index][i].first).c_str());
            m_buckets_set[hash_table_source[master_index][i]].insert(std::pair<uint32_t, uint32_t>(i, master_index));

            for(uint32_t j = 1; j < copyCount; j++) {
                _log_info(myLog, "init_bucket_set bucket_count %u copy_count[%d] %s.",
                          bucketCount, j, NetHelper::addr2String(hash_table_source[master_index][i].first).c_str());
                s_buckets_set[hash_table_source[j][i]].insert(std::pair<uint32_t, uint32_t>(i, j));
            }
        }
    }
}

bool TableBuilder::buildFirstTime(hash_table_type & tmp_hash_table_result)
{
    //keep for invalid
    for(uint32_t i = 0; i < bucketCount; i++)
    {
        if (!chooseNodeInvalid(i, 0, m_node_flag, m_migrate_node_info, tmp_hash_table_result, m_buckets_set, true))
        {
            return false;
        }
    }
    for(uint32_t i = 0; i < bucketCount; i++)
    {
        for(uint32_t j = 1; j < copyCount; j++)
        {
            if (!chooseNodeInvalid(i, j, s_node_flag, s_migrate_node_info, tmp_hash_table_result, s_buckets_set, false))
            {
                return false;
            }
        }
    }
    return true;
}

bool TableBuilder::insertBucketMap(server_id_type &node_info, int32_t bucket, int32_t position, std::map<server_id_type, std::map<uint32_t, uint32_t> > &buckets_set)
{
    buckets_set[node_info].insert(std::pair<uint32_t, uint32_t>(bucket,position));

    return true;
}

bool TableBuilder::eraseBucketMap(server_id_type & node_info, int32_t bucket, int32_t position, std::map<server_id_type, std::map< uint32_t, uint32_t> > &buckets_set)
{
    buckets_set[node_info].erase(buckets_set[node_info].find(bucket));

    return true;
}

bool TableBuilder::chooseNode( std::map<server_id_type, int32_t> &node_flag,
                                std::map<server_id_type, int32_t> &migrate_node_info,
                                int32_t &count_info,
                                server_id_type &node_info,
                                hash_table_type & tmp_hash_table_result,
                                std::map<server_id_type, std::map< uint32_t, uint32_t> > &buckets_set,
                                bool master_flag)
{
    std::multimap<int32_t, server_id_type> migrate_node_info_temp;
    sortMapByValue(migrate_node_info, migrate_node_info_temp);

    for(auto server_it = migrate_node_info_temp.begin(); server_it != migrate_node_info_temp.end(); )
    {
        if (server_it->first >= 0) break;

        std::set<server_id_type> temp_server;
        uint32_t mod_count=0;
        auto value_end = migrate_node_info_temp.upper_bound(server_it->first);
        while(server_it != value_end)
        {
            temp_server.insert(server_it->second);
            mod_count++;
            server_it++;
        }
        for(uint32_t n = mod_count; n > 0; n--)
        {
            server_id_type check_node_now = std::pair<uint64_t, uint32_t>(INVALID_FLAG,INVALID_FLAG);
            getNode(check_node_now,temp_server,n);

            for(auto index_bucket=buckets_set[node_info].begin();
                index_bucket!=buckets_set[node_info].end(); index_bucket++)
            {
                if(checkNodeOk(check_node_now,index_bucket->first,index_bucket->second,tmp_hash_table_result))
                {
                    if(master_flag)
                    {
                        decrNodeCount(node_info,s_migrate_node_info);
                        decrNodeCount(node_info,m_migrate_node_info);
                        incrNodeCount(check_node_now,s_migrate_node_info);
                        incrNodeCount(check_node_now,m_migrate_node_info);
                    }
                    else
                    {
                        decrNodeCount(node_info,s_migrate_node_info);
                        incrNodeCount(check_node_now,s_migrate_node_info);
                    }
                    tmp_hash_table_result[index_bucket->second][index_bucket->first] = check_node_now;

                    insertBucketMap(check_node_now,index_bucket->first,index_bucket->second,buckets_set);
                    eraseBucketMap(node_info,index_bucket->first,index_bucket->second,buckets_set);
                    return true;
                }
            }
        }
    }
    // choice a node for INVALID_FLAG
    //if first time do not need check again
    //not first time need second check
    std::map<server_id_type, int32_t> temp_node_flag;
    for(auto temp_flag=node_flag.begin(); temp_flag != node_flag.end(); temp_flag++ ) {
        temp_node_flag.insert(std::pair<server_id_type, int32_t> (temp_flag->first, temp_flag->second + migrate_node_info[temp_flag->first]));
    }
    sortMapByValue(temp_node_flag, migrate_node_info_temp);

    //try self
    if( node_flag[node_info] == 0 && count_info > 0)
    {
        incrNodeCount(node_info, node_flag);
        decrNodeCount(node_info, migrate_node_info);
        count_info--;
        return true;
    }

    for(auto flag_it = migrate_node_info_temp.begin(); flag_it != migrate_node_info_temp.end(); )
    {
        std::set<server_id_type> temp_server;
        uint32_t mod_count = 0;
        auto value_end = migrate_node_info_temp.upper_bound(flag_it->first);
        while(flag_it != value_end)
        {
            temp_server.insert(flag_it->second);
            mod_count++;
            flag_it++;
        }
        //选择一个超出最少的 need change
        for(uint32_t n=mod_count; n>0; n--)
        {
            server_id_type check_node_now = std::pair<uint64_t, uint32_t>(INVALID_FLAG,INVALID_FLAG);
            getNode(check_node_now, temp_server, n);
            for(auto index_bucket=buckets_set[node_info].begin();
                index_bucket!=buckets_set[node_info].end(); index_bucket++)
            {
                if(checkNodeOk(check_node_now,index_bucket->first,index_bucket->second,tmp_hash_table_result))
                {
                    if(master_flag)
                    {
                        incrNodeCount(check_node_now,m_node_flag); //inc B
                        decrNodeCount(node_info,m_migrate_node_info); //dec A
                        decrNodeCount(node_info,s_migrate_node_info);  //
                        incrNodeCount(check_node_now,s_migrate_node_info);
                    }
                    else
                    {
                        incrNodeCount(check_node_now,s_node_flag); //inc all B
                        decrNodeCount(node_info,s_migrate_node_info); //dec all A
                    }
                    count_info--;
                    tmp_hash_table_result[index_bucket->second][index_bucket->first] = check_node_now;
                    insertBucketMap(check_node_now,index_bucket->first,index_bucket->second,buckets_set);
                    eraseBucketMap(node_info,index_bucket->first,index_bucket->second,buckets_set);

                    return true;
                }
            }
        }
    }
    _log_info(myLog, "Can't find a suitable DS for node %s", NetHelper::addr2String(node_info.first).c_str());
    return true;
}

//1 find m_migrate_node_info
//2 find m_node_flag
bool TableBuilder::chooseNodeInvalid(int bucket_num, int position,
                                        std::map<server_id_type, int32_t> &node_flag,
                                        std::map<server_id_type, int32_t> &migrate_node_info,
                                        hash_table_type & tmp_hash_table_result,
                                        std::map<server_id_type, std::map<uint32_t, uint32_t> > &buckets_set,
                                        bool master_flag)
{
    std::multimap<int32_t, server_id_type> migrate_node_info_temp;
    sortMapByValue(migrate_node_info, migrate_node_info_temp);

    for(auto server_it = migrate_node_info_temp.begin(); server_it != migrate_node_info_temp.end(); )
    {
        if (server_it->first >= 0) break;

        std::set<server_id_type> temp_server;
        uint32_t mod_count = 0;
        auto value_end = migrate_node_info_temp.upper_bound(server_it->first);
        while(server_it != value_end)
        {
            temp_server.insert(server_it->second);
            mod_count++;
            server_it++;
        }
        for(uint32_t n = mod_count; n > 0; n--)
        {
            server_id_type check_node_now = std::pair<uint64_t, uint32_t>(INVALID_FLAG, INVALID_FLAG);
            getNode(check_node_now, temp_server, n);
            if(checkNodeOk(check_node_now, bucket_num, position, tmp_hash_table_result))
            {
                if(master_flag)
                {
                    incrNodeCount(check_node_now,s_migrate_node_info);
                    incrNodeCount(check_node_now,m_migrate_node_info);
                }
                else
                {
                    incrNodeCount(check_node_now,s_migrate_node_info);
                }
                insertBucketMap(check_node_now,bucket_num,position,buckets_set);

                tmp_hash_table_result[position][bucket_num] = check_node_now;
                return true;
            }
        }
    }
    // choice a node for INVALID_FLAG
    //if first time do not need check again
    //not first time need second check
    std::map<server_id_type, int32_t> temp_node_flag;
    for( auto temp_flag = node_flag.begin(); temp_flag != node_flag.end(); temp_flag++ )
    {
        temp_node_flag.insert(std::pair<server_id_type, int32_t> (temp_flag->first, temp_flag->second + migrate_node_info[temp_flag->first]));
    }
    sortMapByValue(temp_node_flag, migrate_node_info_temp);

    for(auto flag_it = migrate_node_info_temp.begin(); flag_it != migrate_node_info_temp.end(); )
    {
        std::set<server_id_type> temp_server;
        uint32_t mod_count=0;
        auto value_end = migrate_node_info_temp.upper_bound(flag_it->first);
        while(flag_it != value_end)
        {
            temp_server.insert(flag_it->second);
            mod_count++;
            flag_it++;
        }
        //选择一个超出最少的 need change
        for(uint32_t n = mod_count; n>0; n--)
        {
            server_id_type check_node_now = std::pair<uint64_t, uint32_t>(INVALID_FLAG, INVALID_FLAG);
            getNode(check_node_now, temp_server, n);
            if(checkNodeOk(check_node_now, bucket_num, position, tmp_hash_table_result))
            {
                if(master_flag)
                {
                    incrNodeCount(check_node_now,s_migrate_node_info); //inc all A
                    incrNodeCount(check_node_now,m_migrate_node_info); //inc all A
                }
                else
                {
                    incrNodeCount(check_node_now,s_migrate_node_info);
                }
                insertBucketMap(check_node_now,bucket_num,position,buckets_set);
                tmp_hash_table_result[position][bucket_num] = check_node_now;

                return true;
            }
        }
    }
    return false;
}

}
