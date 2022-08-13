# overview
In order to evaluate the performance of the system; and to give an evaluation baseline for production, we conducted a performance testing, and have the result of the performance testing; it needs to be mentioned specially that our testing is not for testing's sake, but for the real-word case and the production evaluation baseline; the two kinds of testing are entirely different.

Some views that we need to mention:

Firstly, the performance testing works with ideal conditions; there are lots of factors to impact the performance in production; which is much more complicated.

Secondly, the performance testing could supply us with a baseline of performance in production.

Thirdly, the performance testing is conducted under lots of restricted conditions, such as CPU usage, network IO throughput, request timeout ratio of business, no write stall, etc.

Lastly, the performance testing works under a specified configuration; these configuration are also the baseline of performance, such as two replicas, two machines, one node or four nodes per machine, sixteen threads are used to process the request in each node, etc.  

# benchmark
## MDB
The MDB is a memory storage engine, all of the data is stored in memory, so the latency is very low, and it is very suitable for the business of high concurrency and low latency; of course, memory is expensive, and the total space is limited.

We deployed a real cluster with two replicas in production; the cluster includes eight nodes of two machines, each machine has 256G total memory, and ten gigabit capacity of network card, and fifty-six cpu cores of `E5-2660`.
As a matter of experience, in our running production environment, the cpu usage needs to be lower than 50% ~ 60%, and the timeout ratio of business request needs to be lower than 0.1%; we conducted performance testing with such conditions.

![performance_mdb](docs/imgs/performance_mdb.png)

We did the writing and reading performance test of MDB with different value length, sixteen threads are used to process the request in each node;

In condition of cpu usage is less than 50% ~ 60%，and network IO is less than 70% of maximum capacity, and timeout ratio of request is less than 0.1%, it processes more than three hundred thousand of write requests per second, or processes between five hundred thousand to eight hundred thousand of read requests per second.

## RDB
RDB is a disk storage engine. It is implemented based on rocksdb, and made some changes about how data is stored.  and also supports some new functions such as TTL, namespace and info stats. Disk storage is much cheaper compared with memory storage; it holds huge data on the disk, and is very suitable for the business of big data but not sensitive to latency; 

we deployed a real minimal cluster with two replicas in production; the cluster includes two nodes of two machines, each machine has 256G total memory, and ten gigabit capacity of network card, and fifty-six cpu cores of `E5-2660`, six disks and each disk with 880G.

As a matter of experience, in our running production environment, there should be no write stall for RDB, and the timeout ratio of business request needs to be lower than 0.1%; we conducted performance testing with such conditions.

![performance_rdb](docs/imgs/performance_rdb.png)

We did the writing and reading performance test of RDB with different value length, sixteen threads are used to process the request in each node; in terms of performance tests, write data for about nine hours continuously, the TTL of records is 14400 seconds, and no write stall happens; it uses the default setting of the Rocksdb, and uses level compaction, and keeps about 120G ~ 130G data totally with L0 to L4, it is the typical data distribution of each node in production.

* data distribution of performance test when using 512 bytes of value length
![512_bytes_of_value](docs/imgs/512_bytes_of_value.png)
* data distribution of performance test when using 1024 bytes of value length
![1024_bytes_of_value](docs/imgs/1024_bytes_of_value.png)
* data distribution of performance test when using 5120 bytes of value length
![5120_bytes_of_value](docs/imgs/5120_bytes_of_value.png)

In condition of no write stall while writing lasts for at least nine hours, and of timeout ratio of requests is less than 0.1%, it processes between one thousand to ten thousand of write requests per second, or processes between fifty thousand to sixty thousand of read requests per second.

