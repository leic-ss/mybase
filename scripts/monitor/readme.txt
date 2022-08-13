1.  进程自动拉起脚本
	当前进程自动拉起脚本使用启动脚本，因为启动脚本中判断了进程存在会退出，不存在就会启动进程

2.  cs_mon_backup_route.sh
	监控路由表是否有变化，有变化则备份，后续考虑上报事件

3.  cs_mon_clean_file.sh
	清理集群old文件

4.  cs_mon_check_status.sh
	只在master的configserver上运行，检查集群状态



master cs:
* * * * * flock -x -w 10 /tmp/cs_bak_route.lock -c "sh /home/apps/configserver/scripts/monitor/cs_mon_backup_route.sh SD_VDEngine_SET1_server_table"
* * * * * flock -x -w 10 /tmp/cs_ck_mig.lock -c "sh /home/apps/configserver/scripts/monitor/cs_mon_check_migrate.sh SD_VDEngine_SET1_server_table"
* * * * * flock -x -w 10 /tmp/cs_ck_status.lock -c "sh /home/apps/configserver/scripts/monitor/cs_mon_check_status.sh SD_VDEngine_SET1"

*/5 * * * * flock -x -w 10 /tmp/cs_clean.lock -c "sh /home/apps/configserver/scripts/monitor/cs_mon_clean_file.sh"
*/5 * * * * flock -x -w 10 /tmp/cs_info.lock -c "sh /home/apps/configserver/scripts/monitor/cs_mon_info_monitor.sh"


slave cs:
*/5 * * * * flock -x -w 10 /tmp/cs_clean.lock -c "sh /home/apps/configserver/scripts/monitor/cs_mon_clean_file.sh"
*/5 * * * * flock -x -w 10 /tmp/cs_info.lock -c "sh /home/apps/configserver/scripts/monitor/cs_mon_info_monitor.sh"


dataserver:
*/5 * * * * flock -x -w 10 /tmp/ds1_clean.lock -c "sh /home/apps/dataserver_01/scripts/monitor/ds_mon_clean_file.sh"
*/6 * * * * flock -x -w 10 /tmp/ds2_clean.lock -c "sh /home/apps/dataserver_02/scripts/monitor/ds_mon_clean_file.sh"
*/7 * * * * flock -x -w 10 /tmp/ds3_clean.lock -c "sh /home/apps/dataserver_03/scripts/monitor/ds_mon_clean_file.sh"
*/8 * * * * flock -x -w 10 /tmp/ds4_clean.lock -c "sh /home/apps/dataserver_04/scripts/monitor/ds_mon_clean_file.sh"

*/10 * * * * flock -x -w 10 /tmp/ds1_info.lock -c "sh /home/apps/dataserver_01/scripts/monitor/ds_mon_info_monitor.sh"
*/11 * * * * flock -x -w 10 /tmp/ds2_info.lock -c "sh /home/apps/dataserver_02/scripts/monitor/ds_mon_info_monitor.sh"
*/12 * * * * flock -x -w 10 /tmp/ds3_info.lock -c "sh /home/apps/dataserver_03/scripts/monitor/ds_mon_info_monitor.sh"
*/13 * * * * flock -x -w 10 /tmp/ds4_info.lock -c "sh /home/apps/dataserver_04/scripts/monitor/ds_mon_info_monitor.sh"