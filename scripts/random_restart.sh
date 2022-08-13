#!/bin/bash

MASTER_CONFIG_IP=192.168.56.101

while [ true ];do
        data_id=$(($RANDOM%4))
        ds_id=`expr $data_id + 5191`
        data_path_number=`expr $data_id + 1`

        ds_path="/data${data_path_number}/dataserver_${ds_id}"
        ds_dir="/data${data_path_number}"

        echo "[`date +'%Y-%m-%d %H:%M:%S'`] try stop, start check if migration finished!"
        while [ true ];do
                migrateBlockCount=`curl ${MASTER_CONFIG_IP}:8070/cst_monitor 2>/dev/null | head | grep migrateBlockCount | awk '{print $2}'`

                if [ $migrateBlockCount -ne -1 ];then
                        echo "[`date +'%Y-%m-%d %H:%M:%S'`] busy, migrateBlockCount: $migrateBlockCount, waiting!"
                        sleep 10
                else
                        echo "[`date +'%Y-%m-%d %H:%M:%S'`] idle, will stop ${ds_path}."
                        break
                fi
        done
        echo "[`date +'%Y-%m-%d %H:%M:%S'`] end check if migration finished! wait 5 minutes!"
        sleep 300

        sh $ds_path/scripts/daemo_dataserver.sh stop
        echo "[`date +'%Y-%m-%d %H:%M:%S'`] stop ${ds_path} success, wait for 60 seconds!"
        echo
        echo

        sleep 60
        echo "[`date +'%Y-%m-%d %H:%M:%S'`] try start, start check if migration finished!"
        while [ true ];do
                migrateBlockCount=`curl ${MASTER_CONFIG_IP}:8070/cst_monitor 2>/dev/null | head | grep migrateBlockCount | awk '{print $2}'`

                if [ $migrateBlockCount -ne -1 ];then
                        echo "[`date +'%Y-%m-%d %H:%M:%S'`] busy, migrateBlockCount: $migrateBlockCount, waiting!"
                        sleep 10
                else
                        echo "[`date +'%Y-%m-%d %H:%M:%S'`] idle, will start ${ds_path}."
                        break
                fi
        done
        echo "[`date +'%Y-%m-%d %H:%M:%S'`] end check if migration finished! wait 5 minutes"
        sleep 300

        echo "[`date +'%Y-%m-%d %H:%M:%S'`] start rm ds_path: $ds_path"
        rm -rf $ds_path
        echo "[`date +'%Y-%m-%d %H:%M:%S'`] end rm ds_path: $ds_path"
        cp -rf /home/rtrs/tmp/dataserver_${ds_id} $ds_dir

        sh $ds_path/scripts/daemo_dataserver.sh start
        echo "[`date +'%Y-%m-%d %H:%M:%S'`] start ${ds_path} success, wait for 30 seconds!"
        echo
        echo

        sleep 60
done