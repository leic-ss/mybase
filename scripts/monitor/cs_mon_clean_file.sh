#!/bin/bash
#---------------------------------------------------------#
#删除 cs /data/home/tdengine/logs/config.log.${2014-03-01}.*  30前的数据
#删除 备份路由表/data/home/tdengine/data/data/bak/*server_table.bak.*.20140301* 30天之前的数据
#删除 配置文件 /data/home/tdengine/etc/group.conf.20131201*  30天之前的数据
#----------------------------------------------------------#

work_home=`cd $(dirname $0); pwd`
source $work_home/monitor_header.sh

source ${dir_home}/etc/bin.conf

if [ x"$CS_SERVER" == x"" ] || [ x"$CS_CONFIG_FILE" == x"" ];then
    VDE_LOG "ERROR" "CS_SERVER[$CS_SERVER] CS_CONFIG_FILE[$CS_CONFIG_FILE] is invalid, exit!"
    VDE_ALARM "CS_SERVER[$CS_SERVER] CS_CONFIG_FILE[$CS_CONFIG_FILE] is invalid, exit!"
    exit 1
fi

#----------------------------------------------------------#

date_1=`date -d "15 days ago" +"%Y-%m-%d"`
date_2=`date -d "15 days ago" +"%Y%m%d"`
date_3=`date -d "15 days ago" +"%Y%m%d"`

[ -d $dir_home/logs ] && $SRM -f $dir_home/logs/config.log.${date_1}.*
[ -d $dir_home/data/data/bak ] && $SRM -f $dir_home/data/data/bak/*server_table.bak.*.${date_2}*
[ -d $dir_home/etc ] && $SRM -f $dir_home/etc/group.conf.${date_3}*

#find $dir_home -name 'tde_api.log'|xargs rm