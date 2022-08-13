#!/bin/bash

work_home=`cd $(dirname $0); pwd`
source $work_home/monitor_header.sh

source ${dir_home}/etc/bin.conf

if [ x"$CS_SERVER" == x"" ] || [ x"$CS_CONFIG_FILE" == x"" ];then
    VDE_LOG "ERROR" "CS_SERVER[$CS_SERVER] CS_CONFIG_FILE[$CS_CONFIG_FILE] is invalid, exit!"
    VDE_ALARM "CS_SERVER[$CS_SERVER] CS_CONFIG_FILE[$CS_CONFIG_FILE] is invalid, exit!"
    exit 1
fi

cd  ${dir_home}
num=`ps -ef | grep -w "$CS_SERVER -f" | grep -v grep | grep -v gdb | wc -l`
if [ $num -eq 0 ];then
    VDE_LOG "ERROR" "$CS_SERVER stoped, restart it"
    VDE_ALARM "$CS_SERVER stoped, restart it"

	sh scripts/daemo_configserver.sh start
	sleep 2
fi

num=`ps -ef | grep -w "$CS_SERVER -f" | grep -v grep | grep -v gdb | wc -l`
if [ $num -eq 0 ];then
    VDE_LOG "ERROR" "$CS_SERVER start failed!"
    VDE_ALARM "$CS_SERVER start failed"
fi

m_cs=`grep "config_server=" ${dir_home}/etc/configserver.conf |head -1|awk -F ":" '{print $1}'|awk -F "=" '{print $2}'`
if [ x"$local_ip" != x"$m_cs" ];then
    m_port=`grep "config_server=" ${dir_home}/etc/configserver.conf |head -1|awk -F ":" '{print $2}'`

    if [ x"$m_cs" != x"" ] && [ x"$m_port" != x"" ];then
        nc -z -w 1 $m_cs $m_port

        if [ $? -ne 0 ];then
            VDE_LOG "ERROR" "$m_cs:$m_port is unreachable over 1 sec"
            VDE_ALARM "$m_cs:$m_port is unreachable over 1 sec"
        fi
    fi
else
    s_cs=`grep "config_server=" ${dir_home}/etc/configserver.conf | head -2 | tail -n 1 | awk -F ":" '{print $1}'|awk -F "=" '{print $2}'`
    s_port=`grep "config_server=" ${dir_home}/etc/configserver.conf | head -2 | tail -n 1 |awk -F ":" '{print $2}'`

    if [ x"$s_cs" != x"" ] && [ x"$s_port" != x"" ];then
        nc -z -w 1 $s_cs $s_port

        if [ $? -ne 0 ];then
            VDE_LOG "ERROR" "$s_cs:$s_port is unreachable over 1 sec"
            VDE_ALARM "$s_cs:$s_port is unreachable over 1 sec"
        fi
    fi
fi

#----------------------------------------------------------#
if [ $num -ne 0 ];then
    cs_pid=`ps -ef | grep -w "$CS_SERVER -f" | grep -v grep | grep -v gdb | awk '{print $2}'`
    cs_cpu_mem_info=`ps -e -o 'pid,comm,pcpu,rsz,vsz,stime' | grep -w "$cs_pid" | head -n 1 | sed 's/^[ \t]*//g'`
    if [ x"$cs_cpu_mem_info" != x"" ];then
        cs_cpu=`echo "$cs_cpu_mem_info" | awk '{print $3}'`
        cs_rmem=`echo "$cs_cpu_mem_info" | awk '{print $4}'`
        cs_vmem=`echo "$cs_cpu_mem_info" | awk '{print $5}'`

        cs_rmem=$(($cs_rmem/1024))  #MB 
        cs_vmem=$(($cs_vmem/1024))  #MB 

        VDE_LOG "INFO" "cs_cpu_mem_info: $cs_cpu,$cs_rmem,$cs_vmem"
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