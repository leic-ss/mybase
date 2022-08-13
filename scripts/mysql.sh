#/bin/bash

for debug;

mysql -h 10.196.96.12 -P3306 -uroot -e "use test; select distinct area from Kv_Data;"

mysql -h 10.196.96.12 -P3306 -uroot -e "use test; select count(distinct area) from Kv_Data;"

mysql -h 10.196.96.12 -P3306 -uroot -e "use test; select distinct aid from Music_Table_Info;"

mysql -h 10.196.96.12 -P3306 -uroot -e "use test; select count(distinct aid) from Music_Table_Info;"

# 在area分配表中有记录，但是没有在table表中记录对应的area
mysql -h 10.196.96.12 -P3306 -uroot -e "use test; select distinct aid from Music_Area_Alloced_Info where aid not in (select distinct aid from Music_Table_Info);"

# 正在线上使用的area，但是没有在table表中记录对应的area
mysql -h 10.196.96.12 -P3306 -uroot -e "use test; select distinct area from Kv_Data where area not in (select distinct aid from Music_Table_Info);"

# 正在线上使用的area，但是在area分配表中没有记录
mysql -h 10.196.96.12 -P3306 -uroot -e "use test; select distinct area from Kv_Data where area not in (select distinct aid from Music_Area_Alloced_Info);"


update Music_Area_Alloced_Info set status=1 where aid in (select distinct area from Kv_Data);

