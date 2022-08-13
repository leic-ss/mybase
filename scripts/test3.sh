#!/bin/bash

set -x

git log  --pretty=tformat: --numstat | awk '{ add += $1; subs += $2; loc += $1 - $2 } END { printf "added lines: %s, removed lines: %s, total lines: %s\n", add, subs, loc }'
netstat -ant|awk '/^tcp/ {++S[$NF]} END {for(a in S) print (a,S[a])}'
grep _areaCapacity_list configserver/etc/group.conf | awk -F ';' '{for(i=1;i<=NF;++i) print $i}'

[ $# -ne 1 ] && echo "usage: $0 dir" && exit 1
ds_dir=$1
log_dir=key_not_exist_log/$ds_dir

[ ! -d $log_dir ] && mkdir -p $log_dir

grep "do put" $ds_dir/logs/server.log.202010141* | grep "area\[0\]" > $log_dir/tmp1.log
grep "rocksdb get failed" $ds_dir/logs/server.log.202010141* | grep "area\[0\]" > $log_dir/tmp5.log
cat $log_dir/tmp5.log | wc -l
grep -A 1000000000 "2020-10-14 09:50" $log_dir/tmp5.log > $log_dir/tmp2.log
cat $log_dir/tmp2.log | wc -l

awk '{print substr($7, 5, length($7) - 5)}' $log_dir/tmp1.log > $log_dir/tmp3.log
awk '{print substr($10, 5, length($10) - 5)}' $log_dir/tmp2.log > $log_dir/tmp4.log


mysqldump -h 127.0.0.1 -P3306 --databases test > test.sql

scp -P1046 zhanglei09@10.196.96.12:/home/zhanglei09/test.sql .
mysql -h127.0.0.1 -P3306 -uroot -proot -e "drop database if exists test;"
mysql -h127.0.0.1 -P3306 -uroot -proot -e "create database if not exists test;"
mysql -h127.0.0.1 -P3306 -uroot -proot < test.sql test

# for((id=1;id<7;id++)) do i=`expr 5190 + id`; echo $id; done dir=dataserver_$id && ./compare2 $dir/tmp3.log $dir/tmp4.log 1 2 3 2> $dir/1.log done
# for((id=1;id<7;id++)) do i=`expr 5290 + $id`; dir=dataserver_$i && ./compare2 $dir/tmp3.log $dir/tmp4.log 1 2 3 2> $i.log; done

# for((id=1;id<=4;id++)) do i=`expr 5190 + $id`; sh dataserver_$i/scripts/daemo_dataserver.sh start; done
# for((id=1;id<=4;id++)) do i=`expr 5190 + $id`; num=`ls -lrnt dataserver_$i/data/ulog_binlog/* | wc -l`; [ $num -ge 21 ] && ls -lrnt dataserver_$i/data/ulog_binlog/* | head -n 20 | awk '{print $9}' | xargs -I {} rm -f {}; done

# for((id=1;id<=4;id++)) do i=`expr 5190 + $id`; sh dataserver_$i/scripts/daemo_dataserver.sh stop; done
# sh configserver_5198/scripts/daemo_configserver.sh stop;

# for((id=1;id<7;id++)) do i=`expr 5290 + $id`; sh dataserver_$i/scripts/daemo_dataserver.sh start; done
# for((id=1;id<7;id++)) do i=`expr 5290 + $id`; sh dataserver_$i/scripts/daemo_dataserver.sh stop; done

# for((id=1;id<7;id++)) do i=`expr 5290 + $id`; cp -rf /tmp/dataserver_$i /data$id; ln -sf /data$id/dataserver_$i dataserver_$i; done

grep _areaCapacity_list configserver/etc/group.conf | awk -F ';' '{for(i=1;i<=NF;++i) print $i}'

./imagereplay -f ./dataserver_01/data/ulog_bindump/clouddump_20210421030102_10.196.56.93_5191.bin -p 10000 -m 10.196.56.200:5298 -s 10.196.56.201:5298 -g group_1 -o tmp.log

scp -r root@192.168.56.101:/root/data1/work/cluster_test .
sed -i "s/dev_name=enp0s8/dev_name=eth12/g" dataserver_519*/etc/dataserver_519*.conf && sed -i "s/dev_name=enp0s8/dev_name=eth12/g" configserver_519*/etc/configserver_519*.conf && grep dev_name dataserver_519*/etc/dataserver_519*.conf && grep dev_name configserver_5198/etc/configserver_5198.conf

scp -r configserver_5398 rtrs@10.196.56.191:/data1
for((id=1;id<7;id++)) do i=`expr 5290 + $id`; scp -r dataserver_$i rtrs@10.196.56.191:/data$id; done
for((id=1;id<7;id++)) do i=`expr 7290 + $id`; curl 127.0.0.1:$i/api/v1/setLogLevel?level=info; done

curl "127.0.0.1:7198/api/v1/addsrv?id=2&raft_addr=192.168.56.101:6199&srv_addr=192.168.56.101:5199"
curl 127.0.0.1:7198/api/v1/getcluster
curl 127.0.0.1:7198/cst_monitor | head


for((id=1;id<=6;id++)) do i=`expr 5190 + $id`; cp -rf cluster_test.nubase.test/dataserver_$i /data$id; ln -sf /data$id/dataserver_$i dataserver_$i; done
for((id=1;id<=6;id++)) do i=`expr 5190 + $id`; sh dataserver_$i/scripts/daemo_dataserver.sh start; done

curl -X POST -d '{"srvid": 2, "raft_addr": "10.196.153.0:6198", "srv_addr": "10.196.153.0:5198", "role": "learner"}' 10.196.152.255:7080/api/v1/addsrv
curl -X POST -d '{"srvid": 2}' 127.0.0.1:7198/api/v1/rmvsrv

curl 127.0.0.1:7198/api/v1/getcluster | jq .

curl -X POST -d '{"level": "info"}' 127.0.0.1:7198/api/v1/setLogLevel
curl 127.0.0.1:7191/api/v1/getLogLevel 2>/dev/null | jq .

curl -X POST -d '{"addr": "192.168.56.103:5191"}' 127.0.0.1:7080/api/v1/storage/addsrv
curl -X POST -d '{"addr": "192.168.56.103:5192"}' 127.0.0.1:7080/api/v1/storage/addsrv

curl -X POST 127.0.0.1:7080/api/v1/table/build