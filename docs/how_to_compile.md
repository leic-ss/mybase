Compile the nubase is quite simple, and centos and debian are available used for compile currently.

# CentOS Linux release 7.5.1804(gcc version 4.8.5)
1. Get the dependency library and install it.
```
    cd nubase

    Download https://g.hz.netease.com/nubase/deps/-/blob/master/deps-env-1.1-1.centos7.x86_64.rpm

    rpm -ivh --prefix=`pwd`/deps_prefix deps-env-1.1-1.centos7.x86_64.rpm
```

2. Compile the nubase
```
    cd nubase && mkdir build && cd build

    cmake -DCMAKE_INSTALL_PREFIX=../nubase_bin ..
    
    make
    
    make install
```

3. Possible issues
```
1) bin/protoc: error while loading shared libraries: libprotobuf.so.20: cannot open shared object file: No such file or directory

    export LD_LIBRARY_PATH=`pwd`/deps_prefix/lib

```

# Debian GNU/Linux 11 (gcc version 10.2.1)
1. Get the dependency library and install it.
```
    cd nubase

    Download https://g.hz.netease.com/nubase/deps/-/blob/master/deps-env-1.1-1.debian11.dep

    dpkg -X deps-env-1.1-1.debian11.dep `pwd`/deps_prefix
```

2. Compile the nubase
```
    cd nubase && mkdir build && cd build

    cmake -DCMAKE_INSTALL_PREFIX=../nubase_bin ..
    
    make
    
    make install
```

