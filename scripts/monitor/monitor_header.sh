#!/bin/bash

script_path=$0
script_name=${script_path##*/}
sscript_name=${script_name%.*}

script_home=`cd $(dirname $work_home); pwd`
dir_home=`cd $(dirname $script_home); pwd`

script_data_home=$dir_home/scripts_data
script_data_file_home=$script_data_home/files/${sscript_name}
script_data_logs_home=$script_data_home/logs/${sscript_name}

[ ! -d $script_data_file_home ] && mkdir -p $script_data_file_home
[ ! -d $script_data_logs_home ] && mkdir -p $script_data_logs_home

fname=`date +'%Y-%m-%d'`
flog=$script_data_logs_home/${fname}.log

local_ip=`/sbin/ifconfig | grep "inet addr" | grep "Bcast" | head -n 1 |awk -F ":" '{print $2}'|awk '{print $1}'`
nowminute=`date "+%Y%m%d%H%M"`

SRM="$script_home/common/srm"

VDE_LOG()
{
	if [ $# -ne 2 ];then
		echo "[`date +'%Y-%m-%d %H:%M:%S'`] $nowminute ERROR $@ in function LOG_INFO is not valid" | tee -a $flog
	else
		level=$1
		info=$2

		echo "[`date +'%Y-%m-%d %H:%M:%S'`] $nowminute $level $info" | tee -a $flog
	fi
}

VDE_ALARM()
{
	info="$local_ip alarm"
	if [ $# -ne 1 ];then
		info="[VDE告警] [`date +'%Y-%m-%d %H:%M:%S'`] [$local_ip] $@ in function LOG_INFO is not valid"
	else
		info="[VDE告警] [`date +'%Y-%m-%d %H:%M:%S'`] [$local_ip] $1"
	fi

	sh $script_home/common/vde_alarm_send.sh "$info"
}

#VDE_LOG "INFO" "$script_name START RUN"

# num=`ps -ef | grep $script_name | grep -v grep | wc -l`
# if [ $num -gt 2 ];then
# 	VDE_LOG "INFO" "another script[$0] is running, exit!"
# 	exit 1
# fi