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

#include <string.h>
#include <stdint.h>

__attribute__((unused)) static const char* sZKMetaRootPath                  = "/root/cluster";
__attribute__((unused)) static const char* sZkMetaMasterNode                = "master";
__attribute__((unused)) static const char* sZkMetaSlavesNode                = "slaves";  // need to create
__attribute__((unused)) static const char* sMetaDataNode                    = "metadata";
__attribute__((unused)) static const char* sZkMetaDataVersionNode           = "metaversion";
__attribute__((unused)) static const char* sZkServiceGroupNode              = "servicegroup"; // need to create
__attribute__((unused)) static const char* sZkServicesMonitor               = "servicemonitor";
__attribute__((unused)) static const char* sZkServiceMonitorVersionNode     = "monitorversion";
__attribute__((unused)) static const char* sZkAgentNodes                    = "agentnodes";

// common config
__attribute__((unused)) static const char* sDevName                         = "dev_name";
__attribute__((unused)) static const char* sServicePort                     = "service_port";
__attribute__((unused)) static const char* sServerPort                      = "server_port";
__attribute__((unused)) static const char* sAdminPort                       = "admin_port";
__attribute__((unused)) static const char* sZkMasterNodePath                = "zk_master_path";
__attribute__((unused)) static const char* sZkSlaveNodePath                 = "zk_slave_path";
__attribute__((unused)) static const char* sZkMetaDataNodePath              = "zk_metadata_path";
__attribute__((unused)) static const char* sServiceGroupName                = "service_group_name";
__attribute__((unused)) static const char* sZkServicesMonitorPath           = "services_monitor_node";
__attribute__((unused)) static const char* sZkServiceMonitorEnabled         = "monitor_enabled";
__attribute__((unused)) static const char* sAppName                         = "app_name";
__attribute__((unused)) static const char* sZkAgentServerNodeList           = "_node_list";
__attribute__((unused)) static const char* sStatsPushUrl                    = "push_url";

// zookeeer config
__attribute__((unused)) static const char* sZookeeper    = "zookeeper";
__attribute__((unused)) static const char* sZkNodes      = "zk_nodes";
__attribute__((unused)) static const char* sZkRootPath   = "zk_root_path";

__attribute__((unused)) static const char* sDbPath       = "db_path";

__attribute__((unused)) static const char* sStorageCluster      = "storagecluster";
__attribute__((unused)) static const char* sMasterAddr          = "master_addr";
__attribute__((unused)) static const char* sSlaveAddr           = "slave_addr";
__attribute__((unused)) static const char* sGroupName           = "group_name";
__attribute__((unused)) static const char* sImageDumpDir        = "image_dump_dir";
__attribute__((unused)) static const char* sSchemaFile          = "schema_file";

__attribute__((unused)) static const char* sMonitorStat         = "monitorstat";
__attribute__((unused)) static const char* sStatProject         = "project";
__attribute__((unused)) static const char* sStatCity            = "city";
__attribute__((unused)) static const char* sStatGroupName       = "groupname";

__attribute__((unused)) static const char* sMysqlServer  = "mysqlserver";
__attribute__((unused)) static const char* sMysqlHost    = "mysqlhost";
__attribute__((unused)) static const char* sMysqlPort    = "mysqlport";
__attribute__((unused)) static const char* sMysqlUser    = "mysqluser";
__attribute__((unused)) static const char* sMysqlPass    = "mysqlpass";
__attribute__((unused)) static const char* sMysqlTimeout = "mysqltimeout";
__attribute__((unused)) static const char* sMysqlDbName  = "mysqldbname";

__attribute__((unused)) static const char* sOpenIdAuth      = "openidauth";
__attribute__((unused)) static const char* sClientId        = "client_id";
__attribute__((unused)) static const char* sClientSecret    = "client_secret";
__attribute__((unused)) static const char* sLoginUrl        = "login_url";
__attribute__((unused)) static const char* sDomainUrl       = "domain_url";

// service config
__attribute__((unused)) static const int32_t sDefaultServicePort 		        = 7000;
__attribute__((unused)) static const int32_t sDefaultAdminPort 		          = 7800;
__attribute__((unused)) static const int32_t sDefaultServerPort             = 5000;

__attribute__((unused)) static const int32_t  sPrefixKeyOffset 			        = 22;
__attribute__((unused)) static const uint32_t sPrefixKeyMask          	    = 0x3FFFFF;
__attribute__((unused)) static const uint32_t sMaxNamespaceCount                 = 1024;
__attribute__((unused)) static const uint32_t sMaxKeySize                   = 1024;
__attribute__((unused)) static const uint32_t sMaxKeySizeWithArea           = 1026;
__attribute__((unused)) static const uint32_t sMaxDataSize                  = 1000000;

__attribute__((unused)) static const uint32_t sConfigMinVersion             = 10;
__attribute__((unused)) static const uint32_t sUpdateServerTableInterval    = 50;
__attribute__((unused)) static const uint32_t sUpdateMetaDataInterval       = 50;

// meta server
// __attribute__((unused)) static const char* sMetaServer                 = "metaserver";
__attribute__((unused)) static const char* sMetaService                = "metaservice";
__attribute__((unused)) static const char* sRouteService               = "routeservice";
__attribute__((unused)) static const char* sHelperService              = "helperservice";
__attribute__((unused)) static const char* sAgentService               = "agentservice";
__attribute__((unused)) static const char* sMetaSection          = "meta";
__attribute__((unused)) static const char* sStorageSection             = "storage";

__attribute__((unused)) static const char* sMdbType                    = "mdb_type";
__attribute__((unused)) static const char* sMdbShmPath                 = "mdb_shm_path";
__attribute__((unused)) static const char* sSlabMemSize                = "slab_mem_size";
__attribute__((unused)) static const char* sSlabBaseSize               = "slab_base_size";
__attribute__((unused)) static const char* sSlabFactor                 = "slab_factor";
__attribute__((unused)) static const char* sMdbHashBucketShift         = "mdb_hash_bucket_shift";
__attribute__((unused)) static const char* sMdbDefaultCapacitySize     = "mdb_default_capacity_size";

__attribute__((unused)) static const char* sProcessThreadNumber        = "process_thread_num";

__attribute__((unused)) static const char* sMetaServer                 = "meta";
__attribute__((unused)) static const char* sGroupFile                  = "group_file";
__attribute__((unused)) static const char* sRaftSection                = "raft";

__attribute__((unused)) static const char* sRaftId                     = "raft_id";
__attribute__((unused)) static const char* sRaftPort                   = "raft_port";
__attribute__((unused)) static const char* sRaftDataDir                = "raft_data_dir";
__attribute__((unused)) static const char* sRaftLogFile                = "raft_log_file";
__attribute__((unused)) static const char* sRaftLogLevel               = "raft_log_level";

__attribute__((unused)) static const char* sRaftElectionTimeoutLower   = "raft_election_timeout_lower_bound_";
__attribute__((unused)) static const char* sRaftElectionTimeoutUpper   = "raft_election_timeout_upper_bound_";

__attribute__((unused)) static const char* sPublicSection               = "public";
__attribute__((unused)) static const char* sDataDir                     = "data_dir";
__attribute__((unused)) static const char* sLogDir                      = "log_dir";
__attribute__((unused)) static const char* sLogFile                     = "log_file";
__attribute__((unused)) static const char* sLogLevel                    = "log_level";

__attribute__((unused)) static const char* sDefaultDataDir              = "data";
__attribute__((unused)) static const char* sGroupDataNeedMove           = "_data_move";
__attribute__((unused)) static const char* sGroupDataBucketNumber       = "_bucket_number";
__attribute__((unused)) static const uint32_t sDefaultBucketNumber      = 1023;
__attribute__((unused)) static const char* sStrCopyCount                = "_copy_count";
__attribute__((unused)) static const uint32_t sDefaultCopyCountNumber   = 1;
__attribute__((unused)) static const uint32_t sDataNeedMigrate          = 1;
__attribute__((unused)) static const char* sStrMinConfigversion         = "_min_config_version";
__attribute__((unused)) static const uint32_t sMinMetaVersion         = 10;
__attribute__((unused)) static const char* sStrServerDownTime           = "_server_down_time";
__attribute__((unused)) static const uint32_t sDefaultServerDownTime    = 4;
__attribute__((unused)) static const char* sStrServerList               = "_server_list";
__attribute__((unused)) static const char* sStrMinDataServerCount       = "_min_data_server_count";
__attribute__((unused)) static const char* sStrMaxDownServerCount       = "_max_down_server_count";
__attribute__((unused)) static const char* sStrBuildStrategy            = "_build_strategy";
__attribute__((unused)) static const uint32_t sDefaultBuildStrategy     = 1;
__attribute__((unused)) static const char* sStrAcceptStrategy           = "_accept_strategy";
__attribute__((unused)) static const char* sStrDataLostFlag             = "_allow_lost_flag";
__attribute__((unused)) static const char* sStrBucketPlacementFlag      = "_bucket_placement_flag";
__attribute__((unused)) static const char* sStrPosMask                  = "_pos_mask";
__attribute__((unused)) static const uint64_t sDefaultPosMask           = 0x000000000000ffffLL;
__attribute__((unused)) static const char* sStrBuildDiffRatio           = "_build_diff_ratio";
__attribute__((unused)) static const char* sDefaultBuildDiffRatio       = "0.6";
__attribute__((unused)) static const char* sStrPreLoadFlag              = "_pre_load_flag";
__attribute__((unused)) static const char* sStrTmpDownServer            = "tmp_down_server";
__attribute__((unused)) static const char* sStrAreaCapacityList         = "_areaCapacity_list";
__attribute__((unused)) static const char* sStrReportInterval           = "_report_interval";
__attribute__((unused)) static const uint32_t sDefaultReportInterval    = 5;
__attribute__((unused)) static const char* sStrGroupFile                = "group_file";
__attribute__((unused)) static const char* sUlogDir                     = "ulog_dir";
__attribute__((unused)) static const char* sUlogMigrateBaseName         = "ulog_migrate_base_name";
__attribute__((unused)) static const char* sUlogMigrateDefaultBaseName  = "kv_ulog_migrate";
__attribute__((unused)) static const char* sUlogFileSize                = "ulog_file_size";
__attribute__((unused)) static const uint32_t sUlogDefaultFileSize      = 64;
__attribute__((unused)) static const uint32_t sMbSize                   = (1 << 20);

__attribute__((unused)) static const char* sDataDbPath                 = "data_db_path";
__attribute__((unused)) static const char* sSysDbPath                  = "sys_db_path";
__attribute__((unused)) static const char* sWalogPath                  = "wal_log_path";

__attribute__((unused)) static const char* sMigrateBatchSize           = "migrate_batch_size";
__attribute__((unused)) static const char* sMigrateBatchCount          = "migrate_batch_count";
__attribute__((unused)) static const char* sMaxConcMigNodesCount       = "max_conc_mig_nodes_count";

__attribute__((unused)) static const char* sRdbSection                 = "rdb";
__attribute__((unused)) static const char* sRdbRowCacheSizeMB          = "row_cache_capacity_mb";
__attribute__((unused)) static const char* sRdbRowCacheShardNum        = "row_cache_shard_num";
__attribute__((unused)) static const char* sRdbBlockCacheSizeMB        = "block_cache_size_mb";
__attribute__((unused)) static const char* sRdbBlockCacheShardNum      = "block_cache_shard_num";
__attribute__((unused)) static const char* sRdbIndexDbPath             = "index_db_path";

__attribute__((unused)) static const char* sFdbSection                 = "fdb";

__attribute__((unused)) static const char* sStrEngine                   = "storage_engine";

__attribute__((unused)) static const uint32_t sMaxFileNameLen           = 256;
__attribute__((unused)) static const uint32_t sKvHtmVersion             = 0x31766257;
__attribute__((unused)) static const uint32_t sHardCheckMigComplete     = 300;

__attribute__((unused)) static const uint32_t sMaxBucketNumber          = 65536;

__attribute__((unused)) static const uint32_t sMigrateLockLogLen        = 2*1024*1024;
__attribute__((unused)) static const uint32_t sMisecondsWaitedForWrite  = 500;
__attribute__((unused)) static const uint32_t sMaxSalveNum              = 9;

__attribute__((unused)) static const uint32_t sKvItemFlagDeleted        = 2;

__attribute__((unused)) static const int32_t sKvOperationVersion            = 1;
__attribute__((unused)) static const int32_t sKvOperationDuplicate          = 2;
__attribute__((unused)) static const int32_t sKvOperationRemote             = 4;
__attribute__((unused)) static const int32_t sKvOperationUnlock             = 8;
__attribute__((unused)) static const int32_t sKvDuplicateBusyRetryCount     = 10;

__attribute__((unused)) static const uint64_t sKvFlagServer  = 0x0000ffffffffffffLL;
__attribute__((unused)) static const uint64_t sKvFlagDonw    = 0x4000000000000000LL;

__attribute__((unused)) static const uint32_t sMaxMupdatePacketSize = 16*1024;
__attribute__((unused)) static const uint32_t sMaxMupdateThroughput = 500*1024*1024;

#define KVSLEEP(running, interval_sec) ({uint32_t count=interval_sec*5; while(count-->0 && running) usleep(200000);})

#define LIKELY(x)       (__builtin_expect(!!(x), 1))
#define UNLIKELY(x)     (__builtin_expect(!!(x), 0))

#define UINT_VER_GEQ(newver,oldver)  ( (int32_t)(newver) - (int32_t)(oldver) >= 0 ) 
#define UINT_VER_LEQ(newver,oldver)  ( (int32_t)(newver) - (int32_t)(oldver) <= 0 ) 


enum ResultCode : int32_t {
    OP_RETURN_SUCCESS 				          = 0,
    OP_RETURN_FAILED           		          = -1,

    KV_ROCKSDB_kCorruption                    = -3002,
    KV_ROCKSDB_kNotSupported                  = -3003,
    KV_ROCKSDB_kInvalidArgument               = -3004,
    KV_ROCKSDB_kIOError                       = -3005,
    KV_ROCKSDB_kMergeInProgress               = -3006,
    KV_ROCKSDB_kIncomplete                    = -3007,
    KV_ROCKSDB_kShutdownInProgress            = -3008,
    KV_ROCKSDB_kTimedOut                      = -3009,
    KV_ROCKSDB_kAborted                       = -3010,
    KV_ROCKSDB_kBusy                          = -3011,
    KV_ROCKSDB_kExpired                       = -3012,
    KV_ROCKSDB_kTryAgain                      = -3013,
    KV_ROCKSDB_kCompactionTooLarge            = -3014,
    KV_ROCKSDB_kColumnFamilyDropped           = -3015,
    KV_ROCKSDB_kMaxCode                       = -3016,

    // kv
    KV_RETURN_LOCK_EXIST                      = -3975,
    KV_RETURN_INVALID_ARGUMENT                = -3982,
    KV_RETURN_MIGRATE_BUSY                    = -3984,
    KV_RETURN_WRITE_NOT_ON_MASTER             = -3986,

    KV_RETURN_MIGRATE_OVERLOAD                = -5991,
    KV_RETURN_MIGRATE_DONE                    = -6000,

    KV_RETURN_SERVER_CAN_NOT_WORK             = -3987,
    KV_RETURN_DATA_EXPIRED                    = -3988,
    KV_RETURN_TIMEOUT                         = -3989,
    KV_RETURN_SEND_FAILED 		 	          = -3990,
    KV_RETURN_ITEMSIZE_ERROR                  = -3991,

    KV_RETURN_VERSION_ERROR                   = -3997,
    KV_RETURN_DATA_NOT_EXIST                  = -3998,

    KV_RETURN_FAILED                          = -3999,
    KV_RETURN_PROXY                           = -4000,
    KV_RETURN_NOT_SUPPORTED_AREA              = -4500,
    KV_RETURN_ITEM_NOT_ENOUTH                 = -4501,

    KV_RETURN_DUPLICATE_ROUTE_CHANGE_ERROR    = -5007,

    KV_FORESTDB_kMinCode                      = -7000,
};

enum MessageType : int32_t {
    KV_REQ_MESSAGE_PUT                           = 1,
    KV_RES_MESSAGE_RETURN                        = 2,

    KV_REQ_MESSAGE_GET                           = 3,
    KV_RES_MESSAGE_GET                           = 4,

    KV_REQ_MESSAGE_GET_META                      = 5,
    KV_RES_MESSAGE_GET_META                      = 6,

    KV_REQ_HEARTBEAT_MESSAGE                     = 7,
    KV_RES_HEARTBEAT_MESSAGE                     = 8,

    KV_REQ_MESSAGE_DUMP                          = 9,
    KV_RES_MESSAGE_DUMP_RETURN                   = 10,

    KV_REQ_BUCKET_HEARTBEAT                      = 11,

    KV_REQ_MESSAGE_FINISH_MIGRATE                = 12,
};

enum KvServerFlag : int32_t {
   KV_SERVERFLAG_CLIENT = 0,
   KV_SERVERFLAG_DUPLICATE,
   KV_SERVERFLAG_MIGRATE,
   KV_SERVERFLAG_PROXY,
};

enum KvServerOperation : int32_t {
   KV_ITEM_FLAG_ADDCOUNT = 1,
   KV_ITEM_FLAG_DELETED = 2,
   KV_ITEM_FLAG_ITEM = 4,
   KV_ITEM_FLAG_LOCKED=8,
   KV_ITEM_FLAG_SET,
};

class AreaStat {
public:
   uint64_t data_size_value{0};
   uint64_t use_size_value{0};
   uint64_t item_count_value{0};
};
