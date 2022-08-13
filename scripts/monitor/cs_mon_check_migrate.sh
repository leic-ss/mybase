#!/bin/bash
#功能 用来监控迁移是否卡死，60min未迁移完成则认为卡死，并且每次均会上报
#说明：只在主节点上执行

work_home=`cd $(dirname $0); pwd`
source $work_home/monitor_header.sh

source ${dir_home}/etc/bin.conf

if [ x"$CS_SERVER" == x"" ] || [ x"$CS_CONFIG_FILE" == x"" ];then
    VDE_LOG "ERROR" "CS_SERVER[$CS_SERVER] CS_CONFIG_FILE[$CS_CONFIG_FILE] is invalid, exit!"
    VDE_ALARM "CS_SERVER[$CS_SERVER] CS_CONFIG_FILE[$CS_CONFIG_FILE] is invalid, exit!"
    exit 1
fi

if [ $# -ne 1 ]
then
	VDE_LOG "ERROR" "usage: $0 route_table_name, exit!"
	exit 1
fi

if [ ! -e ${dir_home}/etc/$CS_CONFIG_FILE ];then
    VDE_LOG "ERROR" "${dir_home}/etc/configserver.conf not exist!"
    VDE_ALARM "${dir_home}/etc/configserver.conf not exist!"
    exit 1
fi

m_cs=`grep "config_server=" ${dir_home}/etc/configserver.conf |head -1|awk -F ":" '{print $1}'|awk -F "=" '{print $2}'`
if [ x"$local_ip" != x"$m_cs" ];then
	VDE_LOG "ERROR" "local_ip[$local_ip] is not master_cs_ip[$m_cs]"
	VDE_ALARM "local_ip[$local_ip] is not master_cs_ip[$m_cs]"
	exit 1
fi

#----------------------------------------------------------#

table="$1"
migrate_file=$script_data_file_home/migrate.num

cd $dir_home
[ ! -e data/data/$table ] && echo "[`date +'%Y-%m-%d %H:%M:%S'`] INFO data/data/$table not exist, exit!" | tee -a $flog && eixt 1
mBlockCount=`tools/cst_monitor data/data/$table | grep migrateBlockCount | head -n 1 | awk -F ':' '{print $2}'`

if [ $mBlockCount -ne -1 ];then
	echo "[`date +'%Y-%m-%d %H:%M:%S'`] $mBlockCount" >> $migrate_file

	VDE_LOG "ERROR" "mBlockCount is $mBlockCount"
	VDE_ALARM "Migrating, mBlockCount is $mBlockCount"
else
	rm -f $migrate_file
	touch $migrate_file
fi

nums=`cat $migrate_file | wc -l`
if [ $nums -ge 60 ];then
	VDE_LOG "ERROR" "mirate last more than 60min"
	VDE_ALARM "mirate last more than 60min"
fi