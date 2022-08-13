#!/bin/bash

if [ $# -ne 1 ];then
    echo "usage $0 <num>"
    exit
fi

ts=`date +'%Y%m%d%H%M%S'`

num=$1
datadir=dataserver_$num
if [ ! -d $datadir ];then
    echo "$datadir not exist!"
    exit
fi

cp -f dataserver $datadir/bin/dataserver.$ts
cd $datadir/bin
ln -sf dataserver.$ts ds_$num
cd -

ls -l $datadir/bin/

# sed -i "s/process_thread_num=16/process_thread_num=21/g" $datadir/etc/dataserver_${num}.conf
grep process_thread_num $datadir/etc/dataserver_${num}.conf