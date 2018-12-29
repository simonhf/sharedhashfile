# SharedHashFile: Share Hash Tables With Stable Key Hints Stored In Memory Mapped Files Between Arbitrary Processes

SharedHashFile is a lightweight, embeddable NoSQL key value store / hash table with stable key hints, a zero-copy IPC queue, & a multiplexed IPC logging library written in C for Linux.  There is no server process.  Data is read and written directly from/to shared memory or SSD; no sockets are used between SharedHashFile and the application program. APIs for C & C++.

![Nailed It](http://simonhf.github.io/sharedhashfile/images/10m-tps-nailed-it.jpeg)

[![Build Status](https://travis-ci.org/simonhf/sharedhashfile.svg?branch=master)](https://travis-ci.org/simonhf/sharedhashfile)

## Project Goals

* Faster speed.
* Simpler & smaller footprint.
* Lower memory usage.
* Concurrent, in memory access.

## Technology

### Data Storage

Data is kept in shared memory by default, making all the data accessible to any processes. Up to 4 billion keys can be stored in a single SharedHashFile hash table which is limited in size only by available RAM.

For example, let's say you have a box with 128GB RAM of which 96GB is used by SharedHashFile hash tables. The box has 24 cores and there are 24 processes (e.g. nginx forked slave processes or whatever) concurrently accessing the 96GB of hash tables. Each process shares exactly the same 96GB of shared memory.

### Direct Memory Access

Because a key,value pair is held in shared memory across n processes, that shared memory can change and/or move at any time. Therefore, getting a value always results in receiving a thread local copy of the value.

### Data Encoding

Keys and values are currently binary strings, with a combined maximum length of 2^32 bytes.

### Allocation & Garbage Collection

Conventional malloc() does not function in shared memory, since we have to use offsets instead of conventional pointers. Hence SharedHashFile uses its own implementation of malloc() for shared memory.

To avoid memory holes then garbage collection happens from time to time upon key,value insertion. The number of key,value pairs effected during garbage collection is intentionally limited by the algorithm to a maximum of 8,192 pairs no matter how many keys have been inserted in the hash table. This means the hash table always feels very responsive.

### Hash Table Expansion

SharedHashFile is designed to expand gracefully as more key,value pairs are inserted. There are no sudden memory increases or memory doubling events. And there are no big pauses due to rehashing keys en masse.

### Locking

To reduce contention there is no single global hash table lock by design. Instead keys are sharded across 256 locks to reduce lock contention.

### Optional Fixed Length Keys and Values

For use cases with high levels of writing then performance can suffer due to too many system mmap() calls due to recycling / shrinking memory mapped areas when removing memory holes due to deleted keys.

To improve performance for write heavy use cases, keys and values can be fixed in size across the entire hash table, which means deleted keys can be easily re-used without creating memory holes, and no expensive system mmap() calls are necessary.

Using fixed length keys and values also reduces the amount of RAM used because the key and value sizes are no longer stored, e.g. 100 million keys and values would save 100 million * 8 bytes = 800 million bytes.

### Persistent Storage

Hash tables are stored in memory mapped files in `/dev/shm` which means the data persists even when no processes are using hash tables. However, the hash tables will not survive rebooting.

### Unique Identifers AKA Stable Key Hints

Unlike other hash tables, every key stored in SharedHashFile gets assigned its own UID, e.g. ```shf_make_hash("key", 3); uint32_t uid =  shf_put_key_val(shf, "val", 3)```. To get the same key in the future, choose between accessing the key via its key, or via its UID, e.g. ```shf_make_hash("key", 3); shf_get_key_val_copy(shf)``` or ```shf_get_uid_val_copy(shf, uid)```.

What are UIDs useful for? UIDs don't take up any extra resources and can be thought of as resource 'free'. Accessing a key by its UID is faster than accessing the key via its key. Because a UID is only 32bits in size then it can be easily stored as a reference to a key in your program, or embedded in the values of of key,value pairs, or even embedded within other keys.

Example usage: If uid1 points to key ```"user-id-<xyz>"```, and uid2 points to key ```"facebook.com"```, then another 'mash up' key might be ```"<uid1><uid2>"```. Want to find out if ```"user-id-<xyz>"``` has ```"facebook.com"``` in their personal URL whitelist? Just see if key ```"<uid1><uid2>"``` exists.

What does the 'stable' in 'stable key hint' mean? It means that the UID stays the same even if the key and/or value bytes move around in memory.

### Zero-Copy IPC Queues

How does it work? Create X fixed-sized queue elements, and Y queues to push & pull those queue elements to/from.

Example: Imagine two processes ```Process A``` & ```Process B```. ```Process A``` creates 100,000 queue elements and 3 queues; ```queue-free```, ```queue-a2b```, and ```queue-b2a```. Intitally, all queue elements are pushed onto ```queue-free```. ```Process A``` then spawns ```Process B``` which attaches to the SharedHashFile in order to pull from ```queue-a2b```. To perform zero-copy IPC then ```Process A``` can pull queue elements from ```queue-free```, manipulate the fixed size, shared memory queue elements, and push the queue elements into ```queue-a2b```. ```Process B``` does the opposite; pulls queue elements from ```queue-a2b```, manipulates the fixed size, shared memory queue queue elements, and pushes the queue elements into ```queue-b2a```. ```Process A``` can also pull queue items from ```queue-b2a``` in order to digest the results from ```Process B```.

So how many queue elements per second can be moved back and forth by ```Processes A``` & ```Process B```? On a Lenovo W530 laptop then about 90 million per second if both ```Process A``` & ```Process B``` are written in C.

Note: When a queue element is moved from one queue to another then it is not copied, only a reference is updated.

### Multiplexed IPC Logging

How does it work? ```Process A``` calls shf_log_thread_new() which creates a shared memory log buffer and a log output thread which periodically monitors for new log lines. ```Process B``` calls shf_log_attach_existing() to start logging to the same shared log. Log using C macros SHF_PLAIN() and SHF_DEBUG(). If shf_log_thread_new() has not been called then output goes automatically to stdout, else the logging is multiplexed by the log output thread.

Example output:

```
sharedhashfile$ cat debug/test.q.shf.t.tout
1..10
=0.000000 23056 pid 23056 started; mode is 'c2c'
=0.000013 23056 - SHF_SNPRINTF() // 'test-23056-ipc-queue'
ok 1 -    c2*: shf_attach()          works for non-existing file as expected
=0.000002 23060 shf.monitor: monitoring pid 23056 to delete /dev/shm/test-23056-ipc-queue.shf
1..7
=0.000001 23064 pid 23064 started; mode is '4c'
=0.000010 23064 - SHF_SNPRINTF() // 'test-23064-ipc-queue'
ok 1 -     4c: shf_attach_existing() works for existing file as expected
#0.003948 1 --> auto mapped to thread id 23061
#0.003948 1 shf_log_thread(shf=?){}
ok 2 -    c2*: put lock in value as expected
ok 3 -    c2*: shf_q_new() returned as expected
ok 4 -    c2*: moved   expected number of new queue items // estimate 51,044,225 q items per second without contention
#0.131327 2 --> auto mapped to thread id 23064
#0.131327 2 '4c' mode; behaving as client
ok 2 -     4c: shf_q_get_name('qid-free') returned qid as expected
ok 3 -     4c: shf_q_get_name('qid-a2b' ) returned qid as expected
ok 4 -     4c: shf_q_get_name('qid-b2a' ) returned qid as expected
#0.158467 2 shf_race_start() // 2 horses started after 0.000001 seconds
#0.158474 2 testing process b IPC queue a2b --> b2a speed
#0.158467 3 --> auto mapped to thread id 23056
#0.158467 3 shf_race_start() // 2 horses started after 0.027820 seconds
#0.158484 3 testing process a IPC queue b2a --> a2b speed
ok 5 -     4c: moved   expected number of new queue items // estimate 53,106,512 q items per second with contention
#0.179207 2 testing process b IPC lock speed
ok 6 -     4c: got lock value address as expected
ok 5 -    c2*: moved   expected number of new queue items // estimate 52,951,698 q items per second with contention
#0.180467 3 testing process a IPC lock speed
ok 6 -    c2*: got lock value address as expected
#0.180475 3 shf_race_start() // 2 horses started after 0.000000 seconds
#0.180475 2 shf_race_start() // 2 horses started after 0.001165 seconds
ok 7 -    c2*: rw lock expected number of times           // estimate 4,422,109 locks per second; with contention
ok 7 -     4c: rw lock expected number of times           // estimate 4,411,319 locks per second; with contention
#0.633875 2 ending child
ok 8 -    c2*: rw lock expected number of times           // estimate 51,144,435 locks per second; without contention
ok 9 -    c2*: rw lock expected number of times           // estimate 381,821,029 locks per second; without lock, just loop
ok 10 -    c2*: test still alive
#0.677166 3 ending parent
#0.677177 3 shf_del(shf=?)
#0.677180 3 - SHF_SNPRINTF() // 'du -h -d 0 /dev/shm/test-23056-ipc-queue.shf ; rm -rf /dev/shm/test-23056-ipc-queue.shf/'
#0.677181 3 shf_detach(shf=?)
#0.677183 3 shf_log_thread_del(shf=?) // waiting for log thread to end
=0.686385 3 shf_backticks('du -h -d 0 /dev/shm/test-23056-ipc-queue.shf ; rm -rf /dev/shm/test-23056-ipc-queue.shf/')
=0.724220 3 - read 39 bytes from the pipe
test: shf size before deletion: 394M /dev/shm/test-23056-ipc-queue.shf
```

Notes:

* The above example shows the output from 4 different thread ids:
  * 29912 is the thread id of the test program main thread; aka '3'.
  * 29916 is the thread id of the shf.monitor program main thread; never participates in multiplexed logging.
  * 29917 is the thread id of the test program log output thread; aka '1'.
  * 29920 is the thread id of the test program main thread; recursively spawned from 29912; aka '2'.
* Lines beginning with '=':
  * Were output to stdout; i.e. lines logged before or after the log output thread exists.
  * Show the time stamp relative to the start of the process logging.
  * Show the 5 digit thread id.
* Lines beginning with '#':
  * Were output to stdout using the log output thread.
  * Show the time stamp relative to the start of the log output thread.
  * Show the thread id mapped to a unique short integer; 1 usually means the log output thread itself.
* Other lines -- e.g. test 'ok' lines -- will automatically become multiplexed log lines if the log output thread exists.

## Building

Build the release code using ```make```, and the debug code using ```make debug```. Tests are run automatically.

```
root@16vcpu:/# make clean ; make
rm -rf release debug
make: variable: PROD_SRCS=murmurhash3.c shf.c tap.c
make: variable: PROD_OBJS=release/murmurhash3.o release/shf.o release/tap.o
make: variable: TEST_SRCS=test.1.tap.c test.9.shf.c
make: variable: TEST_OBJS=release/test.1.tap.o release/test.9.shf.o
make: variable: TEST_EXES=release/test.1.tap.t release/test.9.shf.t
make: compling: release/test.1.tap.o
make: compling: release/murmurhash3.o
make: compling: release/shf.o
make: compling: release/tap.o
make: linking: release/test.1.tap.t
make: running: release/test.1.tap.t
1..1
ok 1 - All passed
make: compling: release/test.9.shf.o
make: linking: release/test.9.shf.t
make: running: release/test.9.shf.t
1..10
ok 1 - shf_attach_existing() fails for non-existing file as expected
ok 2 - shf_attach()          works for non-existing file as expected
ok 3 - shf_get_copy_via_key() could not find unput key as expected
ok 4 - shf_get_copy_via_key() could     find   put key as expected
ok 5 - put expected number of              keys // 2293581 keys per second
ok 6 - got expected number of non-existing keys // 3812667 keys per second
ok 7 - got expected number of     existing keys // 3021523 keys per second
ok 8 - graceful growth cleans up after itself as expected
ok 9 - del expected number of     existing keys // 3109056 keys per second
ok 10 - del does not   clean  up after itself as expected
running tests on: via command: 'cat /proc/cpuinfo | egrep 'model name' | head -n 1'
running tests on: `model name   : Intel(R) Xeon(R) CPU E5-2670 0 @ 2.60GHz`
-OP MMAP REMAP SHRK PART TOTAL ------PERCENT OPERATIONS PER PROCESS PER SECOND -OPS
--- -k/s --k/s --/s --/s M-OPS 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 -M/s
PUT  3.4  16.9 1298  626   0.0 10 10  8  5  3  2  8  9  9  6  2  8  4  7  8  4  0.0
PUT 51.4 363.7 1410 1200   5.4  7  6  7  6  7  5  6  7  6  6  6  7  7  6  5  6  5.4 -------
PUT 65.2 336.9 2036 2036  10.1  7  5  7  6  7  6  7  7  6  6  6  7  7  6  5  6  4.6 ------
PUT 56.8 331.7 1888 1888  14.9  6  5  6  7  6  7  7  7  6  6  6  7  7  6  5  6  4.9 ------
PUT 73.8 286.8 2200 2200  18.8  7  5  5  7  7  7  7  7  6  7  6  6  6  5  5  7  3.9 -----
PUT 21.2 412.9  726  726  25.1  6  5  5  7  7  8  7  7  6  6  6  6  6  5  5  7  6.3 --------
PUT 77.9 312.0 2554 2557  29.3  6  6  6  6  6  7  6  6  6  6  6  7  6  6  6  7  4.2 -----
PUT 96.7 272.2 3044 3041  32.5  7  6  6  6  6  7  6  6  6  7  6  7  6  6  6  7  3.1 ----
PUT 63.3 303.3 1804 1804  36.6  7  6  6  5  5  7  6  7  5  6  5  7  7  6  6  7  4.2 -----
PUT 11.5 380.9  349  349  43.2  7  6  6  5  5  7  6  6  6  5  6  7  7  6  6  6  6.6 --------
PUT 26.3 444.5  895  898  49.1  7  6  6  5  5  7  6  7  6  6  6  7  7  6  6  7  5.9 -------
PUT 55.0 283.9 1862 1860  53.8  5  6  6  6  6  6  5  7  6  6  7  7  7  6  6  7  4.7 ------
PUT 75.0 312.1 2480 2480  57.4  7  7  6  5  6  5  7  7  6  6  7  7  6  7  5  6  3.6 ----
PUT 88.5 191.2 2859 2858  60.6  7  7  6  6  7  6  7  5  7  5  7  7  6  7  6  5  3.2 ----
PUT 90.6 244.9 2853 2854  63.5  7  7  5  7  7  5  7  5  7  5  7  7  6  7  7  5  2.9 ---
PUT 82.6 258.8 2455 2455  66.4  7  7  5  7  7  6  7  5  6  5  7  7  5  7  6  5  3.0 ---
PUT 69.7 185.9 1970 1970  70.3  7  7  6  5  7  6  7  6  5  5  7  7  6  7  7  5  3.9 -----
PUT 28.8 409.6  761  760  75.6  6  7  6  6  6  5  6  6  7  5  7  7  5  7  7  6  5.2 ------
PUT  7.2 490.9  230  230  82.0  6  8  5  6  6  5  5  6  5  5  7  8  6  8  7  6  6.4 --------
PUT 11.1 414.9  391  391  88.4  6  7  7  6  6  5  5  6  6  5  6  7  5  7  7  6  6.4 --------
PUT 22.3 323.4  810  810  94.5  7  8  8  7  6  6  5  7  6  5  7  2  6  8  7  7  6.1 --------
PUT 26.1 302.4 1124 1124  99.3  2  5  8  9  7  8 10  1  9  9  6  0  8  4  9  6  4.8 ------
PUT  1.6   6.8  239  239 100.0  0  0  0  0  0  0  0  0 14 58  0  0  5  0 23  0  0.7
MIX  0.0   0.0    0    0 100.0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.0
MIX 21.8 103.9    0    0 105.3  5  6  6  7  7  7  7  5  8  6  6  3  7  6  7  7  5.3 -------
MIX  0.5   8.2    0    0 114.0  6  6  5  6  7  5  6  7  7  6  7  6  7  7  6  6  8.7 -----------
MIX  0.0   8.5    0    0 122.8  6  6  5  6  7  5  6  7  7  6  7  6  7  7  6  6  8.7 -----------
MIX  0.0   9.6    0    0 131.7  6  6  6  6  7  6  6  7  6  6  7  6  7  7  6  6  8.9 -----------
MIX  0.0  11.1    0    0 140.8  6  6  7  6  7  6  5  7  6  6  7  5  7  7  5  6  9.1 ------------
MIX  0.0  12.0    0    0 149.6  6  6  7  7  6  6  6  6  7  6  6  6  6  6  6  7  8.8 -----------
MIX  0.0  12.7    0    0 158.3  7  6  6  7  6  6  7  6  7  6  6  6  6  6  7  7  8.7 -----------
MIX  0.0  14.0    0    0 167.1  7  6  6  7  6  6  7  6  7  6  6  6  6  6  7  7  8.8 -----------
MIX  0.0  14.7    0    0 176.2  7  6  6  5  6  6  7  6  7  6  6  5  6  6  7  7  9.1 ------------
MIX  0.0  16.3    0    0 185.1  6  7  7  7  7  7  6  6  6  5  6  7  6  5  6  6  8.9 -----------
MIX  0.0  17.0    0    0 194.3  6  7  7  7  7  6  6  6  6  5  6  7  6  5  6  6  9.2 ------------
MIX  0.0   7.1    0    0 200.0  9  9  7  2  3 10  9  5  0 11  4 14  3  7  6  1  5.7 -------
GET  0.0   0.0    0    0 200.0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.0
GET  0.0   2.1    0    0 200.3  3  6  7  6  7  6  6  6  7  7  7  3  7  7  7  7  0.3
GET  0.0   2.4    0    0 208.5  6  6  6  6  7  6  6  6  7  6  7  6  6  7  6  7  8.2 ----------
GET  0.0   0.0    0    0 217.4  6  7  7  6  7  5  6  6  7  6  7  6  6  7  6  6  8.9 -----------
GET  0.0   0.0    0    0 226.3  6  7  8  6  7  5  6  6  7  5  7  5  6  7  6  5  9.0 -----------
GET  0.0   0.0    0    0 235.3  6  7  7  6  8  5  6  6  8  5  7  5  5  7  6  6  9.0 -----------
GET  0.0   0.0    0    0 244.2  6  7  6  8  7  5  6  6  7  6  7  6  6  5  6  6  8.9 -----------
GET  0.0   0.0    0    0 253.5  5  8  5  8  6  6  7  8  7  5  8  5  5  5  7  5  9.3 ------------
GET  0.0   0.0    0    0 263.1  4  8  5  9  5  5  8  9  5  5  8  5  5  5  8  5  9.6 ------------
GET  0.0   0.0    0    0 272.9  4  8  5  7  5  7  8  9  5  5  8  5  5  5  8  5  9.8 -------------
GET  0.0   0.0    0    0 282.3  5  7  5  6  8  7  7  8  5  5  7  5  5  6  7  5  9.4 ------------
GET  0.0   0.0    0    0 291.0  6  2  6  6  9 10  8  4  6  6  2  6  6  6  8  6  8.7 -----------
GET  0.0   0.0    0    0 298.7 12  0 10  3  0  9  0  0  4 12  0 12 13  9  0 15  7.7 ----------
GET  0.0   0.0    0    0 300.0 32  0  0  0  0  0  0  0  0 23  0 27 18  0  0  0  1.3 -
* MIX is 2% (2000000) del/put, 98% (12100654) get
make: built and tested release version
```

Notes:

* The above figures were generated on a Rackspace cloud server with 16 vCPUs & 60GB RAM.
* A line of stats is output every second during the test.
* During the 'PUT' phase, 16 concurrent processes put 100 million unique keys into the hash table.
* Then during the 'MIX' phase, 16 concurrent processes get 98% of 100 million unique keys while updating (del/put) the other 2%.
* Then during the 'GET' phase, 16 concurrent processes get 100 million unique keys from the hash table; in this case up to 9.8 million get operations per second across the 16 concurrent processes.
* In total 300 million hash table operations are performed.
* Why does put performance vary so much? This is due to kernel memory mapping overhead; 'top' shows bigger system CPU usage.

## Performance

Here's an example on an 8 core Lenovo W530 laptop showing a hash table with 100 million keys, and then doing 2% delete/insert and 98% read at a rate of over 10 million operations per second:

```
$ make clean ; make release
$ PATH=release:$PATH SHF_PERFORMANCE_TEST_ENABLE=1 test.f.shf.t
...
perf testing: SharedHashFile
running tests on: via command: 'cat /proc/cpuinfo | egrep 'model name' | head -n 1'
running tests on: `model name   : Intel(R) Core(TM) i7-3720QM CPU @ 2.60GHz`
-OP MMAP REMAP SHRK PART TOTAL ------PERCENT OPERATIONS PER PROCESS PER SECOND -OPS
--- -k/s --k/s --/s --/s M-OPS 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 -M/s
PUT  0.2   0.0    0    0   0.0 32 30  0  0  0  0  0 38  0  0  0  0  0  0  0  0  0.0
PUT 30.0  89.4 1767 1767   4.5 13 13 13 12 13 12 12 13  0  0  0  0  0  0  0  0  4.5 -----
PUT 30.6  70.8 1925 1925   8.6 13 12 13 13 13 12 13 12  0  0  0  0  0  0  0  0  4.1 -----
PUT 17.1 103.3 1090 1090  13.7 12 12 13 13 13 12 13 13  0  0  0  0  0  0  0  0  5.1 ------
PUT 37.2  47.6 2334 2334  16.4 13 12 13 13 13 12 12 13  0  0  0  0  0  0  0  0  2.6 ---
PUT 15.7  88.1  944  944  21.4 13 12 13 12 12 12 13 12  0  0  0  0  0  0  0  0  5.0 ------
PUT 15.6 105.9 1035 1035  26.1 13 12 13 12 13 12 13 13  0  0  0  0  0  0  0  0  4.7 ------
PUT 34.3  63.6 2180 2181  29.3 13 12 13 12 12 13 12 13  0  0  0  0  0  0  0  0  3.1 ----
PUT 39.7  48.8 2478 2478  31.9 13 12 13 12 13 13 13 12  0  0  0  0  0  0  0  0  2.7 ---
PUT 32.1  47.3 1950 1949  35.0 12 12 12 12 13 13 12 12  0  0  0  0  0  0  0  0  3.0 ----
PUT  9.2 108.6  542  542  40.6 13 13 13 12 13 12 13 13  0  0  0  0  0  0  0  0  5.7 -------
PUT  8.4 132.2  552  552  46.4 13 12 13 12 12 12 13 12  0  0  0  0  0  0  0  0  5.8 -------
PUT 18.1  44.8 1184 1184  51.0 12 12 13 12 12 13 12 13  0  0  0  0  0  0  0  0  4.6 ------
PUT 25.3  98.8 1622 1622  54.4 13 12 13 12 13 12 13 13  0  0  0  0  0  0  0  0  3.4 ----
PUT 27.0  52.5 1730 1730  56.9 12 13 12 13 13 12 12 13  0  0  0  0  0  0  0  0  2.5 ---
PUT 35.4  67.9 2260 2260  59.4 13 13 13 13 13 13 12 12  0  0  0  0  0  0  0  0  2.5 ---
PUT 38.1  52.3 2382 2383  61.9 13 12 12 13 13 13 13 12  0  0  0  0  0  0  0  0  2.5 ---
PUT 37.2  18.8 2306 2306  64.4 13 13 12 13 13 13 12 12  0  0  0  0  0  0  0  0  2.5 ---
PUT 33.7  25.1 2059 2059  67.1 13 12 12 13 12 13 13 12  0  0  0  0  0  0  0  0  2.8 ---
PUT 23.8  75.4 1427 1426  70.1 13 12 12 13 13 13 13 12  0  0  0  0  0  0  0  0  3.0 ---
PUT 12.2 191.3  705  706  73.9 12 13 13 13 13 13 12 12  0  0  0  0  0  0  0  0  3.8 -----
PUT  4.5  15.7  270  269  80.8 12 12 13 13 13 13 12 12  0  0  0  0  0  0  0  0  6.9 ---------
PUT  5.2 129.8  347  347  87.0 13 12 13 12 13 13 12 12  0  0  0  0  0  0  0  0  6.2 --------
PUT  8.4 133.8  557  557  92.4 13 13 12 13 12 13 12 13  0  0  0  0  0  0  0  0  5.4 -------
PUT 14.3   6.2  933  933  97.3 13 12 12 13 12 13 13 12  0  0  0  0  0  0  0  0  4.9 ------
PUT 11.5  16.5  777  777 100.0 11 15 10 13 11 12 13 15  0  0  0  0  0  0  0  0  2.7 ---
MIX  0.0   0.0    0    0 100.0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.0
MIX  2.3   5.6    0    0 101.2 12 14 11 12 12 12 13 13  0  0  0  0  0  0  0  0  1.2 -
MIX  0.3   3.9    0    0 110.4 12 13 13 13 12 13 12 13  0  0  0  0  0  0  0  0  9.2 ------------
MIX  0.0   4.1    0    0 119.7 13 13 13 12 12 13 13 13  0  0  0  0  0  0  0  0  9.3 ------------
MIX  0.0   5.3    0    0 129.4 13 13 12 13 12 13 12 13  0  0  0  0  0  0  0  0  9.8 -------------
MIX  0.0   5.6    0    0 139.0 13 13 12 12 12 13 13 12  0  0  0  0  0  0  0  0  9.6 ------------
MIX  0.0   6.3    0    0 148.5 13 13 13 12 12 12 13 12  0  0  0  0  0  0  0  0  9.5 ------------
MIX  0.0   7.0    0    0 158.4 13 13 13 12 12 13 12 13  0  0  0  0  0  0  0  0  9.9 -------------
MIX  0.0   7.0    0    0 167.7 12 13 13 12 12 13 12 13  0  0  0  0  0  0  0  0  9.3 ------------
MIX  0.0   7.9    0    0 176.7 13 13 13 13 12 13 12 13  0  0  0  0  0  0  0  0  9.1 ------------
MIX  0.0   8.7    0    0 186.5 13 12 13 13 12 13 12 13  0  0  0  0  0  0  0  0  9.8 -------------
MIX  0.0   8.7    0    0 196.0 13 13 13 12 12 12 12 13  0  0  0  0  0  0  0  0  9.5 ------------
MIX  0.0   3.5    0    0 200.0 10  8 12 15 17 11 15 11  0  0  0  0  0  0  0  0  4.0 -----
GET  0.0   0.0    0    0 200.0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.0
GET  0.0   0.6    0    0 206.5 13 13 13 12 10 13 13 13  0  0  0  0  0  0  0  0  6.5 --------
GET  0.0   0.0    0    0 217.9 13 12 13 13 12 12 12 13  0  0  0  0  0  0  0  0 11.4 ---------------
GET  0.0   0.0    0    0 229.3 13 12 13 13 13 11 12 13  0  0  0  0  0  0  0  0 11.4 ---------------
GET  0.0   0.0    0    0 240.6 13 13 13 13 13 12 12 13  0  0  0  0  0  0  0  0 11.3 ---------------
GET  0.0   0.0    0    0 251.9 13 13 13 12 13 13 10 13  0  0  0  0  0  0  0  0 11.3 ---------------
GET  0.0   0.0    0    0 263.2 12 12 13 12 13 13 12 13  0  0  0  0  0  0  0  0 11.3 ---------------
GET  0.0   0.0    0    0 274.4 13 13 13 13 12 13 13 11  0  0  0  0  0  0  0  0 11.2 --------------
GET  0.0   0.0    0    0 285.9 12 13 13 13 12 12 13 13  0  0  0  0  0  0  0  0 11.5 ---------------
GET  0.0   0.0    0    0 297.3 12 13 13 13 12 12 13 12  0  0  0  0  0  0  0  0 11.4 ---------------
GET  0.0   0.0    0    0 300.0  6  9  0 11 22 21 24  6  0  0  0  0  0  0  0  0  2.7 ---
* MIX is 2% (2000000) del/put, 98% (12100654) get
DB size: 3.9G   /dev/shm/test-shf-19973.shf
```

## Performance comparison with LMDB aka Lightning MDB

Here's the same test as above but using LMDB instead of SharedHashFile:

Notes:

* Figure out why performance has halved even though the code hasn't changed :-(
* To make the test a more apples to apples hash table comparison:
* mdb_get() & mdb_put() are used (instead of faster cursor functions) to test LMDB as a hash table.
* The LMDB DB file is stored in /dev/shm so that disk performance does not effect the results.
* PUT & MIX disclaimer: LMDB is designed to be fast at reading, not writing.
* PUT is done using LMDBs transactions (read: global lock?)
* Pro: Without transactions then PUT only manages about 0.1M per second but all threads get to join in.
* Con: With transactions then typically only one thread gets to PUT.
* Con: Transaction locking does not appear to be fair and so although all processes are trying to write a transaction, one thread tends to always win.
* Con: The PUT write starts off at a healthy 1.1M per second but slowly goes down to 0.2M per second.
* MIX used only 50% CPU. Presumably due to a heavy weight global lock?
* GET works very fast at 5.xM per second, but about half the speed of SharedHashFile.
* LMDB only used 2.6G versus 3.9G for SharedHashFile.

```
$ perl perf-test-lmdb.pl
...
perf testing: LMDB aka Lightning MDB
running tests on: via command: 'cat /proc/cpuinfo | egrep 'model name' | head -n 1'
running tests on: `model name   : Intel(R) Core(TM) i7-3720QM CPU @ 2.60GHz`
-OP MMAP REMAP SHRK PART TOTAL ------PERCENT OPERATIONS PER PROCESS PER SECOND -OPS
--- -k/s --k/s --/s --/s M-OPS 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 -M/s
PUT  0.0   0.0    0    0   0.0  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.0
PUT  0.0   0.0    0    0   1.1  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  1.1 -
PUT  0.0   0.0    0    0   1.9  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.8 -
PUT  0.0   0.0    0    0   2.6  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.7
PUT  0.0   0.0    0    0   3.3  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.7
PUT  0.0   0.0    0    0   3.9  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.6
PUT  0.0   0.0    0    0   4.4  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.6
PUT  0.0   0.0    0    0   5.0  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.5
PUT  0.0   0.0    0    0   5.4  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.5
PUT  0.0   0.0    0    0   5.9  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.5
PUT  0.0   0.0    0    0   6.3  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.4
PUT  0.0   0.0    0    0   6.8  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.4
PUT  0.0   0.0    0    0   7.2  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.4
PUT  0.0   0.0    0    0   7.6  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.4
PUT  0.0   0.0    0    0   7.9  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.4
PUT  0.0   0.0    0    0   8.3  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.3
PUT  0.0   0.0    0    0   8.6  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.3
...
PUT  0.0   0.0    0    0  72.1  0  0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0.3
PUT  0.0   0.0    0    0  72.3  5  0 93  2  0  0  0  0  0  0  0  0  0  0  0  0  0.2
...
PUT  0.0   0.0    0    0 100.0100  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.2
MIX  0.0   0.0    0    0 100.0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.0
MIX  0.0   0.0    0    0 100.3 13 12 12 13 13 12 12 13  0  0  0  0  0  0  0  0  0.3
MIX  0.0   0.0    0    0 101.3 12 12 12 13 12 12 13 13  0  0  0  0  0  0  0  0  1.0 -
MIX  0.0   0.0    0    0 102.3 13 12 13 13 12 13 12 12  0  0  0  0  0  0  0  0  1.0 -
...
MIX  0.0   0.0    0    0 198.3 13 13 12 13 13 12 12 12  0  0  0  0  0  0  0  0  1.0 -
MIX  0.0   0.0    0    0 199.4 12 12 12 12 13 13 13 13  0  0  0  0  0  0  0  0  1.1 -
MIX  0.0   0.0    0    0 200.0 13 27 10  3 17  8 11 11  0  0  0  0  0  0  0  0  0.6
GET  0.0   0.0    0    0 200.0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.0
GET  0.0   0.0    0    0 203.0 13 13 12 13 11 13 12 13  0  0  0  0  0  0  0  0  3.0 ---
GET  0.0   0.0    0    0 208.1 13 12 13 13 12 13 11 13  0  0  0  0  0  0  0  0  5.2 ------
GET  0.0   0.0    0    0 213.4 12 11 14 12 13 13 13 11  0  0  0  0  0  0  0  0  5.2 ------
GET  0.0   0.0    0    0 218.6 13 11 14 12 13 12 13 12  0  0  0  0  0  0  0  0  5.2 ------
GET  0.0   0.0    0    0 223.8 13 11 13 13 13 13 12 12  0  0  0  0  0  0  0  0  5.3 -------
GET  0.0   0.0    0    0 229.2 12 12 12 13 14 14 12 12  0  0  0  0  0  0  0  0  5.4 -------
GET  0.0   0.0    0    0 234.3 12 13 12 12 11 13 13 13  0  0  0  0  0  0  0  0  5.1 ------
GET  0.0   0.0    0    0 239.6 11 12 14 12 12 13 14 12  0  0  0  0  0  0  0  0  5.3 -------
GET  0.0   0.0    0    0 244.9 12 12 14 10 13 13 14 13  0  0  0  0  0  0  0  0  5.3 -------
GET  0.0   0.0    0    0 250.1 11 11 13 13 13 13 14 13  0  0  0  0  0  0  0  0  5.3 -------
GET  0.0   0.0    0    0 255.3 13 12 12 12 13 12 13 12  0  0  0  0  0  0  0  0  5.2 ------
GET  0.0   0.0    0    0 260.5 13 12 11 13 12 14 13 11  0  0  0  0  0  0  0  0  5.2 ------
GET  0.0   0.0    0    0 265.8 13 12 12 11 13 13 14 11  0  0  0  0  0  0  0  0  5.2 ------
GET  0.0   0.0    0    0 270.8 14 12 12 12 13 13 13 12  0  0  0  0  0  0  0  0  5.1 ------
GET  0.0   0.0    0    0 275.9 12 14 13 13 13 12 14 10  0  0  0  0  0  0  0  0  5.1 ------
GET  0.0   0.0    0    0 281.3 12 14 13 12 12 13 14 10  0  0  0  0  0  0  0  0  5.4 -------
GET  0.0   0.0    0    0 286.4 14 13 12 13 12 13 13 12  0  0  0  0  0  0  0  0  5.2 ------
GET  0.0   0.0    0    0 291.7 13 12 11 11 13 13 13 13  0  0  0  0  0  0  0  0  5.3 -------
GET  0.0   0.0    0    0 296.7 12 13 12 13 13 14 11 13  0  0  0  0  0  0  0  0  5.0 ------
GET  0.0   0.0    0    0 300.0 12 22 11 19 10  0  0 25  0  0  0  0  0  0  0  0  3.2 ----
GET  0.0   0.0    0    0 300.0  0  0  0  0  0  0  0100  0  0  0  0  0  0  0  0  0.0
* MIX is 2% (2000000) del/put, 98% (12100654) get
DB size: 2.6G   /dev/shm/test-lmdb-20848
```

## TODO

* Add high performance IPC queue notification mechanism and tests based upon eventfd.
* If using private queue batching for performance, add element crash recovery mechanism.
* Add performance test for multiplexed logging and compare to e.g. log4cxx.
* Allow values bigger than 4KB to be their own mmap(); so IPC queue & log addrs never change.
* Auto dump remaining shared memory log atexit.
* Extend shf_log() to seemlessly log to a file instead of stdout.
* Convert shf.log to work with shf_log() instead of slower fopen().
* Add API documentation via doxygen for log operations.
* Add API documentation via doxygen for key value operations.
* Add API documentation via literate programming.
* Add more tests & enforce 100% code coverage.
* Support key,value data types other than binary strings with 32bit length.
* Support in-memory persistence past reboot.
* Support walking of all key,value pairs in the hash table.
* Support stack key types, e.g. push, pop, shift, unshift.
* Add networking layer for distributed hash table.
* Add command line utility tools.
* Ensure client can crash at any time without corrupting hash table.
* Port to Linux-like OSs which do not support mremap().
* Port to Windows.
* Test performance on flash drives.
* Email feedback [@] sharedhashfile [.] com with more wishes!

