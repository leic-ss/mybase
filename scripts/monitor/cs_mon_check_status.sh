#!/bin/bash
#---------------------------------------------------------#
#监控ds的状态是否有dead 和ping的耗时
#说明：只在主节点上执行
#input group_name
#----------------------------------------------------------#
work_home=`cd $(dirname $0); pwd`
source $work_home/monitor_header.sh

source ${dir_home}/etc/bin.conf

if [ x"$CS_SERVER" == x"" ] || [ x"$CS_CONFIG_FILE" == x"" ];then
    VDE_LOG "ERROR" "CS_SERVER[$CS_SERVER] CS_CONFIG_FILE[$CS_CONFIG_FILE] is invalid, exit!"
    VDE_ALARM "CS_SERVER[$CS_SERVER] CS_CONFIG_FILE[$CS_CONFIG_FILE] is invalid, exit!"
    exit 1
fi

group_name=""
if [ $# -ne 1 ]
then
    VDE_LOG "ERROR" "usage: $0 group_name, exit!"
	exit 1
else
	group_name="$1"
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

cd $dir_home
bin/tairclient -c ${m_cs}:5198 -g ${group_name} -l "stat"  2>tairclientstat_2.txt

server_status_startrow=`cat tairclientstat_2.txt | grep -n status | awk -F: '{print $1}'`
let server_status_startrow=$server_status_startrow+1
ping_row=`cat tairclientstat_2.txt | grep -n ping | awk -F: '{print $1}'`
let server_status_endrow=$ping_row-2

cat tairclientstat_2.txt | sed -n "${server_status_startrow},${server_status_endrow}p" | while read record
do
    instance_name=`echo -e $record | sed 's/^ *//g' | cut -d ' ' -f 1`
    server_status=`echo -e $record | sed 's/^ *//g' | cut -d ' ' -f 2`
    if [ "$server_status" == "dead" ];then
        VDE_LOG "ERROR" "instance_name:${instance_name} is dead!"
        VDE_ALARM "instance_name:${instance_name} is dead!"
    fi
done

let ping_startrow=$ping_row+1
instance_startrow=`cat tairclientstat_2.txt | grep -n statistics | head -n 1 | awk -F: '{print $1}'`
let ping_endrow=$instance_startrow-2

cat tairclientstat_2.txt | sed -n "${ping_startrow},${ping_endrow}p" | while read record 
do
    instance_name=`echo -e $record | cut -d ' ' -f 1`
    ping_time=`echo -e $record | cut -d ' ' -f 2`
    ping_time=$(echo "${ping_time}*1000" | bc)
    ping_time=$(echo "${ping_time}" | awk '{print sprintf("%d", $0);}')

    if [ $ping_time -gt 300000 ];then
        VDE_LOG "ERROR" "instance_name:${instance_name} ping's delaytime is ${ping_time}!"
        VDE_ALARM "instance_name:${instance_name} ping's delaytime is ${ping_time}!"
    fi
done
