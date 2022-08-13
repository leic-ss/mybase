#!/bin/bash

# mysql++-3.2.5

./configure --prefix=`pwd`/mysql++ --with-mysql=/apps/mdsenv/deps
make -j3

ar rcu libmysqlpp.a mysqlpp_beemutex.o mysqlpp_cmdline.o mysqlpp_connection.o mysqlpp_cpool.o \
		mysqlpp_datetime.o mysqlpp_dbdriver.o mysqlpp_field_names.o mysqlpp_field_types.o mysqlpp_manip.o \
		mysqlpp_myset.o mysqlpp_mysql++.o mysqlpp_mystring.o mysqlpp_null.o mysqlpp_options.o mysqlpp_qparms.o \
		mysqlpp_query.o mysqlpp_result.o mysqlpp_row.o mysqlpp_scopedconnection.o mysqlpp_sql_buffer.o \
		mysqlpp_sqlstream.o mysqlpp_ssqls2.o mysqlpp_stadapter.o mysqlpp_tcp_connection.o mysqlpp_transaction.o \
		mysqlpp_type_info.o mysqlpp_uds_connection.o mysqlpp_utility.o mysqlpp_vallist.o mysqlpp_wnp_connection.o

make install
cp libmysqlpp.a `pwd`/mysql++/lib

rm -rf /apps/mdsenv/deps/include/mysql++
rm -rf /apps/mdsenv/deps/lib/libmysqlpp.so*

cp -rf `pwd`/mysql++/include/mysql++ /apps/mdsenv/deps/include
cp -rf `pwd`/mysql++/lib/libmysqlpp.a /apps/mdsenv/deps/lib
