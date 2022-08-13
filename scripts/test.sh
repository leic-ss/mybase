#!/bin/bash

# alter table Music_User_Info add column type bigint(1) not null after phone;
# ALTER TABLE Music_User_Info CHANGE COLUMN type role bigint(1) DEFAULT 0 COMMENT '注释';
# ALTER TABLE Music_Table_Info add column tag text COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '离线, 实时' after compress;

# ALTER TABLE Music_Application_Info add column rtimeout bigint(20) NOT NULL DEFAULT 0 COMMENT '读超时时间' after desc;
# ALTER TABLE Music_Application_Info add column wtimeout bigint(20) NOT NULL COMMENT '写超时时间' after desc;
# ALTER TABLE Music_Application_Info add column access bigint(3) NOT NULL COMMENT '访问权限' after desc;

mysql -h127.0.0.1 -P3306 -uroot -proot -e "drop database if exists test;"
mysql -h127.0.0.1 -P3306 -uroot -proot -e "create database if not exists test;"
mysql -h127.0.0.1 -P3306 -uroot -proot < /media/sf_Shares/dataservice/src/schema/metadata.mysql.sql test
# mysqldump -h 127.0.0.1 -P3306 --databases test > test.sql

set -x

# curl -X POST -d '{"creater": "test1@corp.netease.com"}' 127.0.0.1:8080/api/v1/dataservice/areaprealloc/del
# curl -X POST -d '{"mail": "test1@corp.netease.com"}' 127.0.0.1:8080/api/v1/dataservice/storagecluster/del
# curl -X POST -d '{"mail": "test1@corp.netease.com"}' 127.0.0.1:8080/api/v1/dataservice/businessgroup/del
# curl -X POST -d '{"creater": "test1@corp.netease.com"}' 127.0.0.1:8080/api/v1/dataservice/role/del
# curl -X POST -d '{"mail": "test1@corp.netease.com"}' 127.0.0.1:8080/api/v1/dataservice/roleuser/del
# curl -X POST -d '{"mail": "test@corp.netease.com"}' 127.0.0.1:8080/api/v1/dataservice/usermgn/del
# curl -X POST -d '{"mail": "test1@corp.netease.com"}' 127.0.0.1:8080/api/v1/dataservice/usermgn/del


curl -X POST -d '{"name": "123", "mail": "test@corp.netease.com", "phone": "12345678900"}' 127.0.0.1:8080/api/v1/dataservice/usermgn/add
curl -X POST -d '{"name": "456", "mail": "test1@corp.netease.com", "phone": "12345678900"}' 127.0.0.1:8080/api/v1/dataservice/usermgn/add
curl -X POST -d '{"name": "sdfasdfads", "mail": "test1@corp.netease.com", "phone": "111111111111"}' 127.0.0.1:8080/api/v1/dataservice/usermgn/edit
curl 127.0.0.1:8080/api/v1/dataservice/usermgn/list 2>/dev/null | jq .


curl -X POST -d '{"name": "business 1", "creator": "test1@corp.netease.com", "desc": "xxx"}' 127.0.0.1:8080/api/v1/dataservice/businessgroup/add
curl -X POST -d '{"name": "business 3", "gid": 1, "desc": "xxx"}' 127.0.0.1:8080/api/v1/dataservice/businessgroup/edit
curl 127.0.0.1:8080/api/v1/dataservice/businessgroup/list 2>/dev/null | jq .


curl -X POST -d '{"gid": 1, "name": "abc", "rule": {"xxx": 1}, "creator": "test1@corp.netease.com"}' 127.0.0.1:8080/api/v1/dataservice/role/add
curl -X POST -d '{"id": 1, "gid": 1, "name": "abcdef", "rule": {"xxx": 1}, "creator": "test1@corp.netease.com"}' 127.0.0.1:8080/api/v1/dataservice/role/edit
curl 127.0.0.1:8080/api/v1/dataservice/role/list 2>/dev/null | jq .


curl -X POST -d '{"rid": 1, "mail": "test1@corp.netease.com"}' 127.0.0.1:8080/api/v1/dataservice/roleuser/add
curl 127.0.0.1:8080/api/v1/dataservice/roleuser/list 2>/dev/null | jq .

body='{"env": "test", "name": "cluster 1", "type": 1, "status": 1,  "desc": "xxx", "creator": "test1@corp.netease.com",
		"modifier": "test1@corp.netease.com", "totalSpace": 123456,
		"info": {"master": "192.168.56.101:5299", "slave": "192.168.56.101:5298", "group": "group_1"} }'
curl -X POST -d "$body" 127.0.0.1:8080/api/v1/dataservice/storagecluster/add
body='{"id": 1, "env": "test", "name": "cluster 1", "type": 1, "status": 1,  "desc": "xxx",
		"modifier": "test1@corp.netease.com", "totalSpace": 12345678,
		"info": {"master": "192.168.56.101:5299", "slave": "192.168.56.101:5298", "group": "group_1"} }'
curl -X POST -d "$body" 127.0.0.1:8080/api/v1/dataservice/storagecluster/edit
curl 127.0.0.1:8080/api/v1/dataservice/storagecluster/list 2>/dev/null | jq .


curl -X POST -d '{"user": "test1@corp.netease.com", "scid": 1, "areaid": 123}' 127.0.0.1:8080/api/v1/dataservice/areaprealloc/add
curl 127.0.0.1:8080/api/v1/dataservice/areaprealloc/list 2>/dev/null | jq .


curl -X POST -d '{"name": "server group 1", "hashSlots": 10, "autoRehash": 1, "mode": 2, "desc": "xxx"}' 127.0.0.1:8080/api/v1/dataservice/servicegroup/add
curl -X POST -d '{"id": 1, "name": "server group 2", "hashSlots": 10, "autoRehash": 1, "mode": 2, "desc": "xxx"}' 127.0.0.1:8080/api/v1/dataservice/servicegroup/edit
curl 127.0.0.1:8080/api/v1/dataservice/servicegroup/list 2>/dev/null | jq .


curl -X POST -d '{"gid": 1, "node": "192.168.56.101:6000"}' 127.0.0.1:8080/api/v1/dataservice/servicegroupnode/add
# curl -X POST -d '{"gid": 1, "node": "127.0.0.1:8091"}' 127.0.0.1:8080/api/v1/dataservice/servicegroupnode/add


curl -X POST -d '{"gid": 1, "sid": 2, "role": 0, "wratio": 100, "rratio": 100}' 127.0.0.1:8080/api/v1/dataservice/servicegroupstorage/add
curl -X POST -d '{"id": 1, "gid": 1, "sid": 1, "role": 1, "wratio": 100, "rratio": 100}' 127.0.0.1:8080/api/v1/dataservice/servicegroupstorage/edit
curl 127.0.0.1:8080/api/v1/dataservice/servicegroupstorage/list 2>/dev/null | jq .


curl -X POST -d '{"env": "test", "name": "abc", "type": 1, "keyFormat": "kv format 1", "mode": 2, "kvDesc": "xxx", "mode": 2,
				"compress": 1, "rwDesc": "xxx", "tag": 1, "creator": "test1@corp.netease.com", "modifier": "test1@corp.netease.com", "versioncare": 0, "readversion": 0,
				"writeversion": 0, "versionswitch": 0, "serviceGroupId": 1, "areaId": 100, "ttl": 1000,
				"wtimeout": 10, "rtimeout": 10, "serialize": 1, "schemaImport": "xxx", "schemaConfig": "xxx", "proto": "xxx", "pbClassName": "xxx"}' \
				127.0.0.1:8080/api/v1/dataservice/tableinfo/add
curl -X POST -d '{"tableid": 1, "env": "test", "name": "abc adfsdfasdfad", "type": 1, "keyFormat": "kv format 1", "mode": 2, "kvDesc": "xxx", "mode": 2,
				"compress": 1, "rwDesc": "xxx", "tag": 1, "modifier": "test1@corp.netease.com"}' \
				127.0.0.1:8080/api/v1/dataservice/tablebasic/edit
curl 127.0.0.1:8080/api/v1/dataservice/tableinfo/list 2>/dev/null | jq .


curl -X POST -d '{"name": "xxx 1", "desc": "xxxxxx 2", "creator": "test1@corp.netease.com", "modifier": "test1@corp.netease.com"}' \
				127.0.0.1:8080/api/v1/dataservice/applicationinfo/add
curl -X POST -d '{"appid": 1, "name": "xxx 1", "desc": "xxxxxx 3", "modifier": "test1@corp.netease.com"}' \
				127.0.0.1:8080/api/v1/dataservice/applicationinfo/edit
curl 127.0.0.1:8080/api/v1/dataservice/applicationinfo/list 2>/dev/null | jq .


curl -X POST -d '{"appid": 1, "tableid": 1}' 127.0.0.1:8080/api/v1/dataservice/applicationtable/add
curl -X POST -d '{"appid": 1, "tableid": 2}' 127.0.0.1:8080/api/v1/dataservice/applicationtable/add
curl -X POST -d '{"appid": 1, "tableid": 3}' 127.0.0.1:8080/api/v1/dataservice/applicationtable/add
curl 127.0.0.1:8080/api/v1/dataservice/applicationtable/list 2>/dev/null | jq .
curl -X POST -d '{"appid": 1, "tableid": 3}' 127.0.0.1:8080/api/v1/dataservice/applicationtable/del
curl 127.0.0.1:8080/api/v1/dataservice/applicationtable/list 2>/dev/null | jq .

# mysql -h127.0.0.1 -P3306 -uroot -proot -e "drop database test;"
