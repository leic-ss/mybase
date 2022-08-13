#!/bin/bash

cd sqlite-autoconf-3330000
./configure --prefix=`pwd`/sqlite_bin
make -j3
make install

[ ! -d /apps/mdsenv/deps ] && mkdir /apps/mdsenv/deps

mkdir -p /apps/mdsenv/deps/include/sqlite
cp sqlite_bin/include/* /apps/mdsenv/deps/include/sqlite
cp sqlite_bin/lib/libsqlite3.a /apps/mdsenv/deps/lib