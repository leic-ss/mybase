ls -l dataserver_529*/logs/server.log.202010141*

grep "rocksdb get failed" dataserver_5291/logs/server.log > tmp1.log
grep "rocksdb get failed" tmp1.log > tmp1.log

grep "do put" dataserver_5291/logs/server.log > tmp1.log
grep "do put" dataserver_5291/logs/server.log.202010141* > tmp1.log

grep "rocksdb get failed" dataserver_5291/logs/server.log.202010141* > tmp2.log
grep "rocksdb get failed" dataserver_5291/logs/server.log >> tmp2.log

awk '{print substr($7, 5, length($7) - 5)}' tmp1.log > tmp3.log
awk '{print substr($10, 5, length($10) - 5)}' tmp2.log > tmp4.log




grep -A 1000000000 "2020-10-13 23:20" dataserver_5291/logs/server.log > tmp1.log

grep "rocksdb get failed" tmp1.log > tmp2.log

while read line
do
	key=`echo $line | awk '{print substr($10, 5, length($10) - 5)}'`
	echo $key
	grep "$key" tmp1.log | grep "do put"
done < tmp2.log

for line in `cat tmp1.log`
do
	key=`echo $line | awk '{print substr($10, 5, length($10) - 5)}'`
	echo $key
	grep "$key" dataserver_5291/logs/server.log
done

ps -ef | grep ds_ | grep -v grep | awk '{print $2}' | xargs -I {} kill -s 42 {}