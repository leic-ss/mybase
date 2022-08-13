Deploy a cluster is quite simple, you can deploy a local cluster for testing and cluster for production by using some scripts.

# Deploy a local cluster for testing
1. Modify scripts as needed

>`build  cmake  CMakeLists.txt  docs  etc  nubase_bin  README.md  scripts  src  tests`

* cd scripts/local_cluster
* modify following fields of cluster_mgn.sh

>`bin_dir         #absolute path of nubase_bin`

>`cs_ip_array       # config server address array`

>`ds_ip_array       # data server addr array, keep default will be ok`

* Modify following fields of conf.sh

>`DEV_NAME          # network card`

2. Deploy cluster

>`sh cluster_mgn.sh deploy`

3. Start cluster

>`sh cluster_mgn.sh start`

# Deploy a cluster for prodution
1. Assume machine A and machine B are used for cluster, the configserver port is 5198, and dataserver ports are 5191,5192,5193,5194 in each machine.

* cd scripts/cluster_test
* modify following fields of cluster_mgn.sh

>`bin_dir          # absolute path of nubase_bin`

>`cs_ip_array       # config server address array, IP of A and B`

>`ds_ip_array       # data server addr array, keep default will be ok`

* Modify following fields of conf.sh

>`DEV_NAME          # network card`

2. Deploy cluster

>`sh cluster_mgn.sh deploy`

3. Copy `dataserver_5191`, `dataserver_5192`, `dataserver_5193` and `dataserver_5194` to the machine A and B.

4. Copy `configserver_5198` to the machine A and B

5. Start all of dataserver in the machine A and B, and Start configserver in the machine A and B.
> for((id=1;id<=4;id++)) do i=\`expr 5190 + $id\`; sh dataserver_$i/scripts/daemo_dataserver.sh start; done

> sh configserver_5198/scripts/daemo_configserver.sh start

# Init cluster
Init raft group for configserver, take the local cluster test as an example, the config server and data server are deployed in machine A, and ip address of `A` is `192.168.56.101`, the raft port of master CS and slave CS are `6198` and `6199`.
>`curl -X POST -d '{"srvid": 2, "raft_addr": "192.168.56.101:6199", "srv_addr": "192.168.56.101:5199"}' 127.0.0.1:7198/api/v1/addsrv`

# Do some tests
Check the status of raft group by admin port.
> `curl 127.0.0.1:7198/api/v1/getcluster | jq .`

Check whether the cluster is ready or not, it means the cluster is ready if the serverVersion is larger than one.
> `curl 127.0.0.1:7198/api/v1/cstMonitor 2>/dev/null | head`

Check the status of cluster by admin port.
> `curl "127.0.0.1:7198/api/v1/stat" | jq .`

2. Create namespace before writing.
>`curl -X POST 127.0.0.1:7198/api/v1/namespace/create`

List all of the available namespaces.
>`curl "127.0.0.1:7198/api/v1/namespace/list" | jq .`

3. Write key-value data into namespace.
>`curl -X POST -d '{"ns": 1, "key": "k1", "value": "v1", "ttl": 100}' 127.0.0.1:7198/api/v1/put 2>/dev/null | jq .`

Check which data server is located when read and write a specified key.
>`curl 127.0.0.1:7198/api/v1/locate?key=k1 2>/dev/nul | jq .`

4. Get key-value data from namespace.
>`curl "127.0.0.1:7198/api/v1/get?ns=1&key=k1" | jq .`

5. Delete key-value data from namespace.
>`curl -X POST -d '{"ns": 1, "key": "k1"}' 127.0.0.1:7198/api/v1/del 2>/dev/null | jq .`
