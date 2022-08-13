
DEV_NAME=eth12

CS_IP1=${cs_ip_array[0]}
CS_IP2=${cs_ip_array[1]}
CS_PORT1=${cs_port_array[0]}
CS_PORT2=${cs_port_array[0]}

function write_dataserver_conf()
{
    port=$1

    dst_ds_conf=$work_dir/dataserver_${port}/etc/dataserver_${port}.conf
    # mv $work_dir/dataserver_${port}/etc/dataserver.conf ${dst_ds_conf}

    sed -i "/config_server=/d" $dst_ds_conf

    # sed -i "/\[public\]/aconfig_server=$CS_IP2:${CS_PORT2}" $dst_ds_conf
    sed -i "/\[public\]/aconfig_server=$CS_IP1:${CS_PORT1}" $dst_ds_conf

    # sed -i "s/process_thread_num=16/process_thread_num=21/g" $dst_ds_conf

    sed -i "s/server_port=5191/server_port=${port}/g" $dst_ds_conf
    sed -i "s/slab_mem_size=1024/slab_mem_size=300/g" $dst_ds_conf
    sed -i "s/mdb_shm_path=\/mdb_shm_path01/mdb_shm_path=\/mdb_shm_path_${port}/g" $dst_ds_conf
    # sed -i "s/mdb_inst_shift=3/mdb_inst_shift=0/g" $dst_ds_conf
    # sed -i "s/mdb_hash_bucket_shift=24/mdb_hash_bucket_shift=20/g" $dst_ds_conf
    sed -i "s/dev_name=eth0/dev_name=${DEV_NAME}/g" $dst_ds_conf

    admin_port=`expr $port + 2000`
    sed -i "s/admin_port=8081/admin_port=${admin_port}/g" $dst_ds_conf

    sed -i "s/log_level=warn/log_level=debg/g" $dst_ds_conf
}

function write_configserver_conf()
{
    port=$1

    dst_cs_conf=$work_dir/configserver_${port}/etc/configserver_${port}.conf
    # mv $work_dir/configserver_${port}/etc/configserver.conf ${dst_cs_conf}

    sed -i "/config_server=/d" $dst_cs_conf

    # sed -i "/\[public\]/aconfig_server=$CS_IP2:${CS_PORT2}" $dst_cs_conf
    # sed -i "/\[public\]/aconfig_server=$CS_IP1:${CS_PORT1}" $dst_cs_conf

    sed -i "s/port=5198/port=${port}/g" $dst_cs_conf
    sed -i "s/log_level=warn/log_level=info/g" $dst_cs_conf
    sed -i "s/group_file=etc\/group.conf/group_file=etc\/group_${port}.conf/g" $dst_cs_conf
    sed -i "s/dev_name=eth0/dev_name=${DEV_NAME}/g" $dst_cs_conf

    admin_port=`expr $port + 2890`
    # sed -i "s/admin_port=7198/admin_port=${admin_port}/g" $dst_cs_conf
    sed -i "s/http_port=8081/http_port=${admin_port}/g" $dst_cs_conf

    # src_gp_conf=${tair_proj_dir}/etc/group.conf.default
    dst_gp_conf=$work_dir/configserver_${port}/etc/group_${port}.conf
    # cp -f $src_gp_conf $dst_gp_conf

    # sed -i "/\[group_1\]/a_accept_strategy=1" $dst_gp_conf
    sed -i "s/_data_move=0/_data_move=1/g" $dst_gp_conf
    sed -i "s/_copy_count=1/_copy_count=2/g" $dst_gp_conf
    sed -i "s/_min_data_server_count=1/_min_data_server_count=4/g" $dst_gp_conf 

    sed -i "/_server_list=/d" $dst_gp_conf
    for ds_ip_tmp in "${ds_ip_array[@]}"; do
        for ds_port_tmp in "${ds_port_array[@]}"; do
            sed -i "/# data center/a_server_list=${ds_ip_tmp}:${ds_port_tmp}" $dst_gp_conf
        done
    done
}
