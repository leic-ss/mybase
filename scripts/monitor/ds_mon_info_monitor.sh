#!/bin/bash

work_home=`cd $(dirname $0); pwd`
source $work_home/monitor_header.sh

source ${dir_home}/etc/bin.conf

#----------------------------------------------------------#

if [ x"$DS_SERVER" == x"" ] || [ x"$DS_CONFIG_FILE" == x"" ];then
    VDE_LOG "ERROR" "DS_SERVER[$DS_SERVER] DS_CONFIG_FILE[$DS_CONFIG_FILE] is invalid, exit!"
    VDE_ALARM "DS_SERVER[$DS_SERVER] DS_CONFIG_FILE[$DS_CONFIG_FILE] is invalid, exit!"
    exit 1
fi

cd  ${dir_home}
num=`ps -ef | grep -w "$DS_SERVER -f" | grep -v grep | grep -v gdb | wc -l`
if [ $num -eq 0 ];then
    VDE_LOG "ERROR" "$DS_SERVER stoped, restart it"
    #VDE_ALARM "$DS_SERVER stoped, restart it"

	sh scripts/daemo_dataserver.sh start
	sleep 2
fi

num=`ps -ef | grep -w "$DS_SERVER -f" | grep -v grep | grep -v gdb | wc -l`
if [ $num -eq 0 ];then
	VDE_LOG "ERROR" "$DS_SERVER start failed!"
    VDE_ALARM "$DS_SERVER start failed"
fi

#----------------------------------------------------------#
if [ $num -ne 0 ];then
    ds_pid=`ps -ef | grep -w "$DS_SERVER -f" | grep -v grep | grep -v gdb | awk '{print $2}'`
    ds_cpu_mem_info=`ps -e -o 'pid,comm,pcpu,rsz,vsz,stime' | grep -w $ds_pid | head -n 1 | sed 's/^[ \t]*//g'`
    if [ x"$ds_cpu_mem_info" != x"" ];then
        ds_cpu=`echo "$ds_cpu_mem_info" | awk '{print $3}'`
        ds_rmem=`echo "$ds_cpu_mem_info" | awk '{print $4}'`
        ds_vmem=`echo "$ds_cpu_mem_info" | awk '{print $5}'`

        ds_rmem=$(($ds_rmem/1024))  #MB 
        ds_vmem=$(($ds_vmem/1024))  #MB 

        VDE_LOG "INFO" "ds_cpu_mem_info: $ds_cpu,$ds_rmem,$ds_vmem"
    fi
fi

#----------------------------------------------------------#
core_file_num=`find $dir_home -name "core.*" | grep -v corebak | wc -l`
if [ $core_file_num -ne 0 ];then
    VDE_LOG "ERROR" "core_file_num: $core_file_num in $dir_home, will backup core files later"
    VDE_ALARM "core_file_num: $core_file_num in $dir_home, will backup core files later"

    nowtime=`date "+%Y%m%d%H%M%S"`
    find $dir_home -name "core.*" | grep -v corebak | xargs -I {} mv {} {}.corebak.$nowtime
fi