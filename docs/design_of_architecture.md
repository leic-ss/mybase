# overview
Assume that Nubase works with 3 replicas, it contains **Configserver** and **Dataserver** and **Client**, The **Configserver** is used to manage the meta data of cluster, The **Dataserver** is used to store the data actually, The **Client** is the SDK of cluster, it provides API to read and write the data.

# Architecture
![architecture](docs/imgs/architecture.png)

As showed above, The **Configserver** contains a Raft group, it can ensure the strong consistency of meta data, when the leader of raft group has changes, The **Dataserver** could detect it and then update the leader of Configserver group.

The **Dataserver** takes charge of actual data storage, It uses asynchronous replication between multiple replicas of the **Dataserver**, Asynchronous replication is good for performance, but it may data loss in some extreme situations, and some other issues also need pay attention to, such as disorder replication and repeated replication.

A good system need to meet the requirements of business with the proper way, And asynchronous replication is the best choice for the business scenario that allows data loss in extreme situation.

The **Client** contains SDK of cluster, users could read and write the system through it, And it support C++/Java/Python api currently, **Client** will pull the route table and cache it first, then it could contact the **DataServer** to read and write actually.

# Routing table
The routing table is the crucial meta data of the configserver, it is used to locate the Dataserver for read and write in the Client, and It is also used for data migration when the cluster have some state changes, the system contains three kinds of routing table, namely called quick table, migration table and destination table.

Normally, These three kinds of routing table will be identical exactly when the cluster has no changes. The details of these three kinds of routing table will be illustrated in the following sections.

## Quick Table
The quick table stores the latest state of cluster, assume that the datasrever of A/B/C is alive, the quick table will be built and shown as bellow for example.

![quick table](docs/imgs/quick_table.png)

## Migration Table
The migration table stores the migration state of cluster, It shows the progress of data migration by comparing with quick table and destination table, such as which bucket is in data migrating and which one have finished the migration.

![migrate table](docs/imgs/mig_table.png)

## Destination Table
The destination table stores the final state of cluster when the cluster state changes, for example, the final state may shown as below when the new node D join into cluster.

![destination table](docs/imgs/dest_table.png)

# Cluster State Change
Normally, the cluster state is stable, but it may make some changes in certain situations, such as a node is down or a new node joins the cluster; in these cases, cluster state changes and data migration will be triggered and finished.

## Node is down
As shown below, assume that one node is down, it will show how the cluster changes the state and migrates the data.

![status_change](docs/imgs/status_change.png)

* Firstly, as shown in figure 1, the cluster contains A/B/C and other data server nodes, and all of the data server nodes work fine in the cluster, they send the heartbeat message to the config server. 

* Secondly, as shown in figure 2, one of the nodes is down, which is data server C shown above, and it will not send the heartbeat message to the config server; after a while, the config server will check and find that the data server C is down, then the config server will rebuild the routing table and increment the version of it.

* Thirdly, as shown in figure 3, the config server will check the version of the routing table while other alive data servers send the heartbeat message to it;  if the version does not match, it will send the latest routing table to other data servers.

* Lastly, as shown in figure 4, the alive data servers will receive the latest routing table with the response message, and it will compare the quick table and destination table, then it finds which bucket needs to be migrated, and where to be migrated, and it will send the migration finished message to the config server once one bucket finishes the migration;  then the config server updates the migration table, it will update the quick table once all of the buckets have been migrated, finally the quick table, migration table, and destination will be the same exactly.

## How Routing table changes
Next, will introduce how the routing table changes while one node down or joins.

### Node is down
![fault_tolerant](docs/imgs/fault_tolerant.png)

* Firstly, as shown in figure 1, the cluster is stable, and all of the routing tables are the same exactly. 

* Secondly, as shown in figure 2, the data server D joins the cluster, then the config server rebuilds the destination table, which shows the final status of the cluster after data server D joins.

* Thirdly, as shown in figure 3, the migration table will be updated by the config server once one bucket has finished migrating.

* Fourthly, as shown in figure 4, the migration table will be the same as the destination table once all of the buckets have finished migrating.

* Lastly, as shown in figure 5, the config server will update the quick table once all of the buckets have finished migrating, then all of the routing tables will be the same exactly.

### new node joins
![node_expansion](docs/imgs/node_expansion.png)

* Firstly, as shown in figure 1, the cluster is stable, and all of the routing tables are the same exactly. 

* Secondly, as shown in figure 2, if the data server D is down, the config server rebuilds the quick table and migration table; which shows the latest status of the cluster, and the quick table is used for the client to recover the read and write operations;  besides, the config server also rebuilds the destination table, which shows the final status of the cluster after data server D is down.

* Thirdly, as shown in figure 3, the migration table will be updated by the config server once one bucket has finished migrating.

* Fourthly, as shown in figure 4, the migration table will be the same as the destination table once all of the buckets have finished migrating.

* Lastly, as shown in figure 5, the config server will update the quick table once all of the buckets have finished migrating, then all of the routing tables will be the same exactly.

# Load Balance

The key point of load balance is the routing table building. In order to show how the load balance works, assume the cluster includes A, B, and C all three nodes; it will show how the routing table is built as below.

![buckets](docs/imgs/buckets.png)

* Firstly, assume that the bucket number of the cluster is 1024, and the cluster works with three replicas, so every node should hold 341 buckets as the master at least, and also hold 1023 buckets for the total number at least.

* Secondly, there will be some buckets left when building the routing table. The config server will add these buckets to the node averagely;  finally, node A may hold 341 buckets, node B may hold 341 buckets, and node C may hold 342 buckets.

* Lastly, each node will hold the list of the bucket, for example, node A holds bucket [0, 340], node B holds bucket [341, 681], and node C holds bucket [682, 1023]. Of course, the routing table may be different in a real-world case.

# Storage Engines
To make it common to use, it supports different storage engines, not also memory, but also disk storage engines; it contains Memcachedb, Rocksdb and Forestdb so far, and more kinds of storage engines will be added in the next. 

![storage_engine](docs/imgs/storage_engine.png)

As shown above, dataserver contains some modules, and the storage engine is an abstract layer; it defines some interfaces which are used by other modules, to support different storage engines; it only needs to implement such interfaces by using specific storage engines.

Currently, Memcachedb and Rocksdb work fine in production, and Forestdb is optional; the reason for introducing Forestdb is that Forestdb has a better reading performance than Rocksdb; ​compared with Rocksdb, Forestdb has a more balanced reading and writing performance.
