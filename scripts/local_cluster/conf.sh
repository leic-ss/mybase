
DEV_NAME=eth16

CS_IP1=${cs_ip_array[0]}
CS_IP2=${cs_ip_array[0]}
CS_PORT1=${cs_port_array[0]}
CS_PORT2=${cs_port_array[1]}

function write_dataserver_conf()
{
    port=$1

    dst_ds_conf=$work_dir/dataserver_${port}/etc/dataserver_${port}.conf

    sed -i "/config_server=/d" $dst_ds_conf
    sed -i "/\[public\]/aconfig_server=$CS_IP1:${CS_PORT1}" $dst_ds_conf

    sed -i "s/server_port=5191/server_port=${port}/g" $dst_ds_conf
    sed -i "s/slab_mem_size=1024/slab_mem_size=300/g" $dst_ds_conf

    sed -i "s/dev_name=eth0/dev_name=${DEV_NAME}/g" $dst_ds_conf
    # sed -i "s/storage_engine=mdb/storage_engine=rdb/g" $dst_ds_conf

    admin_port=`expr $port + 2000`
    sed -i "s/admin_port=8081/admin_port=${admin_port}/g" $dst_ds_conf
    sed -i "s/log_level=warn/log_level=info/g" $dst_ds_conf
}

function write_configserver_conf()
{
    port=$1
    my_raft_id=$2

    dst_cs_conf=$work_dir/configserver_${port}/etc/configserver_${port}.conf

    sed -i "/config_server=/d" $dst_cs_conf

    sed -i "s/server_port=5198/server_port=${port}/g" $dst_cs_conf
    sed -i "s/log_level=warn/log_level=info/g" $dst_cs_conf
    sed -i "s/group_file=etc\/group.conf/group_file=etc\/group_${port}.conf/g" $dst_cs_conf
    sed -i "s/dev_name=eth0/dev_name=${DEV_NAME}/g" $dst_cs_conf

    admin_port=`expr $port + 2000`
    sed -i "s/admin_port=7080/admin_port=${admin_port}/g" $dst_cs_conf
    sed -i "s/raft_id=1/raft_id=${my_raft_id}/g" $dst_cs_conf

    my_raft_port=`expr 6198 + $my_raft_id - 1`
    sed -i "s/raft_port=6198/raft_port=${my_raft_port}/g" $dst_cs_conf

    dst_gp_conf=$work_dir/configserver_${port}/etc/group_${port}.conf

    sed -i "/_server_list=/d" $dst_gp_conf
    for ds_ip_tmp in "${ds_ip_array[@]}"; do
        for ds_port_tmp in "${ds_port_array[@]}"; do
            sed -i "/# data center/a_server_list=${ds_ip_tmp}:${ds_port_tmp}" $dst_gp_conf
        done
    done
}
