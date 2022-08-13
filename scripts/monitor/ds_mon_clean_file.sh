#!/bin/sh

work_home=`cd $(dirname $0); pwd`
source $work_home/monitor_header.sh

source ${dir_home}/etc/bin.conf

#----------------------------------------------------------#

if [ x"$DS_SERVER" == x"" ] || [ x"$DS_CONFIG_FILE" == x"" ];then
    VDE_LOG "ERROR" "DS_SERVER[$DS_SERVER] DS_CONFIG_FILE[$DS_CONFIG_FILE] is invalid, exit!"
    VDE_ALARM "DS_SERVER[$DS_SERVER] DS_CONFIG_FILE[$DS_CONFIG_FILE] is invalid, exit!"
    exit 1
fi

delete_threshlod=65
hard_delete_threshlod=70
diskspace_warn_threshlod=80
delete_binlog_num_threshlod=240
delete_bindump_num_threshlod=2

used_ratio=`df | grep /dev/vda1 | awk '{print $5}' | awk -F'%' '{print $1}'`

num1=`ls $dir_home/data/ulog_binlog/* 2>/dev/null | wc -l`

while [ $num1 -gt $delete_binlog_num_threshlod ] || [ ${used_ratio} -gt ${delete_threshlod} ]
do
    last_modfile=`ls -tr $dir_home/data/ulog_binlog/* | head -n 1`
    last_modfile_detail=`ls -l ${last_modfile}`
    VDE_LOG "INFO" "num1[$num1] used_ratio=${used_ratio} rm[${last_modfile_detail}]"

    if [ ${used_ratio} -gt ${hard_delete_threshlod} ];then
        rm -f ${last_modfile}
    else
        $SRM -f ${last_modfile}
    fi
    
    sleep 2

    num1=`ls $dir_home/data/ulog_binlog/* 2>/dev/null | wc -l`
    used_ratio=`df | grep /dev/vda1 | awk '{print $5}' | awk -F'%' '{print $1}'`
done

num2=`ls $dir_home/data/ulog_bindump/* 2>/dev/null | wc -l`
if [ $num2 -gt $delete_bindump_num_threshlod ];then
    echo ""
	#VDE_LOG "WARN" "ulog_bindump, num2[$num2] larger than delete_bindump_num_threshlod[$delete_bindump_num_threshlod]"
	#VDE_ALARM "ulog_bindump, num2[$num2] larger than delete_bindump_num_threshlod[$delete_bindump_num_threshlod]"
fi

while [ $num2 -gt $delete_bindump_num_threshlod ]
do
	last_modfile_bindump=`ls -tr $dir_home/data/ulog_bindump/* | head -n 1`
	last_modfile_detail_bindump=`ls -l ${last_modfile_bindump}`

	VDE_LOG "INFO" "num2[${num2}] rm[${last_modfile_detail_bindump}]"
    $SRM -f ${last_modfile_bindump}
    sleep 2

    num2=`ls $dir_home/data/ulog_bindump/* 2>/dev/null | wc -l`
done

date_1=`date -d "15 days ago" +"%Y-%m-%d"`
[ -d $dir_home/logs ] && $SRM -f $dir_home/logs/png.log.${date_1}.*
[ -d $dir_home/logs ] && $SRM -f $dir_home/logs/png.log_dc.${date_1}.*
[ -d $dir_home/logs ] && $SRM -f $dir_home/logs/server.log.${date_1}.*
date_2=`date -d "1 days ago" +"%Y%m%d"`
[ -d $dir_home/logs ] && $SRM -f $dir_home/logs/server.log.${date_2}*


delete_serverlog_num_threshlod=10
num3=`ls $dir_home/logs/server.log.[0-9][0-9] 2>/dev/null | wc -l`
while [ $num3 -gt $delete_serverlog_num_threshlod ]
do
    last_logfile=`ls -tr $dir_home/logs/server.log.[0-9][0-9] | head -n 1`
    $SRM -f ${last_logfile}
    sleep 1

    num3=`ls $dir_home/logs/server.log.[0-9][0-9] 2>/dev/null | wc -l`
done


used_ratio=`df | grep /dev/vda1 | awk '{print $5}' | awk -F'%' '{print $1}'`
if [ ${used_ratio} -gt ${diskspace_warn_threshlod} ];then
    VDE_LOG "WARN" "/dev/sda3, num1[$num1], num2[$num2], used_ratio[$used_ratio] larger than diskspace_warn_threshlod[$diskspace_warn_threshlod]"
    VDE_ALARM "/dev/sda3, num1[$num1], num2[$num2], used_ratio[$used_ratio] larger than diskspace_warn_threshlod[$diskspace_warn_threshlod]"
fi
