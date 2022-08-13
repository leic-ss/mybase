#!/bin/bash
#功能 查看当前路由表文件是否md5有更新，如果有则备份路由表
#说明：只在主节点上运行
#input : table_name

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
old_md5=""
md5_file=$script_data_file_home/route.md5

if [ ! -e ${dir_home}/data/data/${table} ];then
	VDE_LOG "ERROR" "${dir_home}/data/data/${table} not exist, exit!"
	VDE_ALARM "${dir_home}/data/data/${table} not exist, exit!"
	exit 1
fi

if [ ! -e $md5_file ]
then
	curr_md5=`md5sum ${dir_home}/data/data/${table}|awk '{print $1}'`
	echo $curr_md5 > $md5_file

	VDE_LOG "INFO" "write md5[$curr_md5] to $md5_file, exit!"
	exit 0
else
	old_md5=`cat $md5_file`
fi

nowtime=`date "+%Y%m%d%H%M%S"`
curr_md5=`md5sum ${dir_home}/data/data/${table}|awk '{print $1}'`

if [ "${curr_md5}"x != "${old_md5}"x ]
then
	cp ${dir_home}/data/data/${table} $script_data_file_home/${table}.${local_ip}.${nowtime}
	echo $curr_md5 > $md5_file

	VDE_LOG "INFO" "route table[${dir_home}/data/data/${table}] changed, old_md5[$old_md5], curr_md5[$curr_md5]"
	#VDE_ALARM "route table[${dir_home}/data/data/${table}] changed, old_md5[$old_md5], curr_md5[$curr_md5]"
else
	VDE_LOG "INFO" "route table[${dir_home}/data/data/${table}] not change"
fi

date_2=`date -d "15 days ago" +"%Y%m%d"`
[ -d $script_data_file_home ] && rm -f $script_data_file_home/${table}.*.${date_2}*