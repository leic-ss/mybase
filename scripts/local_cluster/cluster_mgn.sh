#!/bin/bash

script_dir=`cd $(dirname $0); pwd`
work_dir=$script_dir
proj_dir=`cd $script_dir/../..; pwd`
bin_dir=/media/sf_Shares/nubase/nubase_bin

#cs_ip_array=(192.168.57.103 192.168.56.105)
#cs_port_array=(5198)
cs_ip_array=(192.168.56.103)
cs_port_array=(5198 5199)

#ds_ip_array=(192.168.57.103 192.168.56.105)
ds_ip_array=(192.168.56.103)
ds_port_array=(5191 5192 5193 5194)

# for test two-dimensional array
ds_port_array2=( '5191 5192'
			     '5193 5194' )
for((i=1; i<${#ds_port_array2[@]}; i++))
do
	ds_port_array2_1=(${ds_port_array2[$i]})
	for ds_port in "${ds_port_array2_1[@]}"; do
		echo `expr $ds_port + 1` > /dev/null
	done
done
# end test

. ./conf.sh

[ $# != 1 ] && echo "usage: $0 <deploy | start | stop | clean>" && exit 1

if [ x$1 == x"deploy" ];then
	for ds_port in "${ds_port_array[@]}"; do
		[ -d $work_dir/dataserver_${ds_port} ] && echo "Error! $work_dir/dataserver_${ds_port} already exist!" && exit 1
	done
	for cs_port in "${cs_port_array[@]}"; do
		[ -d $work_dir/configserver_${cs_port} ] && echo "Error! $work_dir/configserver_${cs_port} already exist!" && exit 1
	done

	for ds_port in "${ds_port_array[@]}"; do
		mkdir -p $work_dir/dataserver_${ds_port}/bin
		mkdir -p $work_dir/dataserver_${ds_port}/etc
		mkdir -p $work_dir/dataserver_${ds_port}/scripts
		mkdir -p $work_dir/dataserver_${ds_port}/logs
		mkdir -p $work_dir/dataserver_${ds_port}/data

		# cp -f $tair_bin_dir/bin/tair_server $work_dir/dataserver_${ds_port}/bin
		cp -f ${bin_dir}/bin/dataserver $work_dir/dataserver_${ds_port}/bin

		# cp -rf ${mds_bin_dir}/scripts $work_dir/dataserver_${ds_port}
		cp -f $bin_dir/scripts/daemo_dataserver.sh $work_dir/dataserver_${ds_port}/scripts

		cp -f ${bin_dir}/etc/dataserver.conf $work_dir/dataserver_${ds_port}/etc/dataserver_${ds_port}.conf

		write_dataserver_conf $ds_port
		echo "DS_SERVER=ds_${ds_port}" > $work_dir/dataserver_${ds_port}/etc/bin.conf
		echo "DS_CONFIG_FILE=dataserver_${ds_port}.conf" >> $work_dir/dataserver_${ds_port}/etc/bin.conf

	done

	raft_id=1
	for cs_port in "${cs_port_array[@]}"; do
		mkdir -p $work_dir/configserver_${cs_port}/bin
		mkdir -p $work_dir/configserver_${cs_port}/etc
		mkdir -p $work_dir/configserver_${cs_port}/scripts
		mkdir -p $work_dir/configserver_${cs_port}/logs
		mkdir -p $work_dir/configserver_${cs_port}/data

		cp -f $bin_dir/bin/configserver $work_dir/configserver_${cs_port}/bin
		cp -f $bin_dir/scripts/daemo_configserver.sh $work_dir/configserver_${cs_port}/scripts

		cp -f ${bin_dir}/etc/configserver.conf $work_dir/configserver_${cs_port}/etc/configserver_${cs_port}.conf
		cp -f ${bin_dir}/etc/group.conf $work_dir/configserver_${cs_port}/etc/group_${cs_port}.conf

		write_configserver_conf $cs_port $raft_id
		echo "CS_SERVER=cs_${cs_port}" > $work_dir/configserver_${cs_port}/etc/bin.conf
		echo "CS_CONFIG_FILE=configserver_${cs_port}.conf" >> $work_dir/configserver_${cs_port}/etc/bin.conf
		raft_id=`expr $raft_id + 1`
	done

elif [ x$1 = x"start" ];then
	for ds_port in "${ds_port_array[@]}"; do
		bash $work_dir/dataserver_${ds_port}/scripts/daemo_dataserver.sh start
	done
	for cs_port in "${cs_port_array[@]}"; do
		bash $work_dir/configserver_${cs_port}/scripts/daemo_configserver.sh start
	done
elif [ x$1 = x"stop" ];then
	for ds_port in "${ds_port_array[@]}"; do
		bash $work_dir/dataserver_${ds_port}/scripts/daemo_dataserver.sh stop
	done
	for cs_port in "${cs_port_array[@]}"; do
		bash $work_dir/configserver_${cs_port}/scripts/daemo_configserver.sh stop
	done
elif [ x$1 == x"clean" ];then
	for ds_port in "${ds_port_array[@]}"; do
		bash $work_dir/dataserver_${ds_port}/scripts/daemo_dataserver.sh stop
		rm -rf $work_dir/dataserver_${ds_port}
	done
	rm -f /dev/shm/mdb_shm_path_*

	for cs_port in "${cs_port_array[@]}"; do
		bash $work_dir/configserver_${cs_port}/scripts/daemo_configserver.sh stop
		rm -rf $work_dir/configserver_${cs_port}
	done
else
	echo "usage: $0 <deploy | start | stop | clean>" && exit 1
fi

