#!/bin/bash

[ $# -ne 1 ] && echo "usage $0 start|stop" && exit 1

script_dir=`cd $(dirname $0); pwd`
proj_home=`cd $(dirname $script_dir); pwd`

#需要cd到目录，然后执行，否则对应的data、logs目录不在vde_home目录下
cd $proj_home
. ./etc/bin.conf

if [ x$1 == x"start" ];then
	#设置core
	ulimit -c unlimited
	#设置栈内存大小（KB）
	ulimit -s 51200

	(cd bin; [ ! -f $DS_SERVER ] && ln -sf dataserver $DS_SERVER; cd $proj_home)

	num=`ps -ef | grep -w "$DS_SERVER -f" | grep -v grep | grep -v gdb | wc -l`
	if [ $num -ne 0 ];then
		echo "$DS_SERVER already runing, exit!"
		exit 1
	fi

	bin/$DS_SERVER -f etc/$DS_CONFIG_FILE
elif [ x$1 == x"stop" ];then
	num=`ps -ef | grep -w "$DS_SERVER -f" | grep -v grep | grep -v gdb | wc -l`
	if [ $num -eq 0 ];then
		echo "$DS_SERVER not runing, exit!"
		exit 1
	fi

	pid=`ps -ef | grep -w "$DS_SERVER -f" | grep -v grep | grep -v gdb | awk '{print $2}'`
	kill -9 $pid

	while [ 1 ]
	do
		num=`ps -ef | grep -w "$DS_SERVER -f" | grep -v grep | grep -v gdb | wc -l`
		if [ $num -eq 0 ];then
			echo "$DS_SERVER stop succ, exit!"
			break
		fi

		echo "$DS_SERVER is stopping, please wait!"
		sleep 2
	done
else
	echo "invalid action[$1], usage $0 start|stop"
	exit 1;
fi
