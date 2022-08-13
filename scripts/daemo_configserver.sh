#!/bin/bash

[ $# -ne 1 ] && echo "usage $0 start|stop" && exit 1

script_dir=`cd $(dirname $0); pwd`
proj_home=`cd $(dirname $script_dir); pwd`

#需要cd到目录，然后执行，否则对应的data、logs目录不在vde_home目录下
cd $proj_home
. ./etc/bin.conf

if [ x"$1" == x"start" ];then
	#设置core
	ulimit -c unlimited
	#设置栈内存大小（KB）
	ulimit -s 20480

	(cd bin; [ ! -f $CS_SERVER ] && ln -sf configserver $CS_SERVER; cd $proj_home)

	num=`ps -ef | grep -w "$CS_SERVER -f" | grep -v grep | grep -v gdb | wc -l`
	if [ $num -ne 0 ];then
		echo "$CS_SERVER already runing, exit!"
		exit 1
	fi

	bin/$CS_SERVER -f etc/$CS_CONFIG_FILE
elif [ x"$1" == x"stop" ];then
	num=`ps -ef | grep -w "$CS_SERVER -f" | grep -v grep | grep -v gdb | wc -l`
	if [ $num -eq 0 ];then
		echo "$CS_SERVER not runing, exit!"
		exit 1
	fi

	pid=`ps -ef | grep -w "$CS_SERVER -f" | grep -v grep | grep -v gdb | awk '{print $2}'`
	kill -9 $pid

	while [ 1 ]
	do
		num=`ps -ef | grep -w "$CS_SERVER -f" | grep -v grep | grep -v gdb | wc -l`
		if [ $num -eq 0 ];then
			echo "$CS_SERVER stop succ, exit!"
			break
		fi

		echo "$CS_SERVER is stopping, please wait!"
		sleep 2
	done
else
	echo "invalid action[$1], usage $0 start|stop"
	exit 1;
fi
