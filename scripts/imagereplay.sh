#!/bin/bash

num=`ps -ef | grep "/home/rtrs/imagereplay3 -S" | grep -v grep | wc -l`
if [ $num -ne 0 ]; then
        echo "[`date +'%Y-%m-%d %H:%M:%S'`] num is $num, already running! exit"
        exit 1
fi

echo "[`date +'%Y-%m-%d %H:%M:%S'`] no imagereplay3 running! work"

dump_path=/home/rtrs/dataserver_01/data/ulog_bindump
filename=`ls $dump_path -l | tail -n 1 | awk '{print $9}'`
( /home/rtrs/imagereplay3 -S 10.196.96.45:5198 -G JD_VDEngine_SET1 -a 311 -f $dump_path/$filename -m 10.196.152.246:5198 -s 10.196.152.247:5198 -g JD_NUBASE_SET1 -o dump1.log & )

dump_path=/home/rtrs/dataserver_02/data/ulog_bindump
filename=`ls $dump_path -l | tail -n 1 | awk '{print $9}'`
( /home/rtrs/imagereplay3 -S 10.196.96.45:5198 -G JD_VDEngine_SET1 -a 311 -f $dump_path/$filename -m 10.196.152.246:5198 -s 10.196.152.247:5198 -g JD_NUBASE_SET1 -o dump2.log & )

dump_path=/home/rtrs/dataserver_03/data/ulog_bindump
filename=`ls $dump_path -l | tail -n 1 | awk '{print $9}'`
( /home/rtrs/imagereplay3 -S 10.196.96.45:5198 -G JD_VDEngine_SET1 -a 311 -f $dump_path/$filename -m 10.196.152.246:5198 -s 10.196.152.247:5198 -g JD_NUBASE_SET1 -o dump3.log & )

dump_path=/home/rtrs/dataserver_04/data/ulog_bindump
filename=`ls $dump_path -l | tail -n 1 | awk '{print $9}'`
( /home/rtrs/imagereplay3 -S 10.196.96.45:5198 -G JD_VDEngine_SET1 -a 311 -f $dump_path/$filename -m 10.196.152.246:5198 -s 10.196.152.247:5198 -g JD_NUBASE_SET1 -o dump4.log & )