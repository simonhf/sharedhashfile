# SharedHashFile: Share Hash Tables Stored In Memory Mapped Files Between Arbitrary Processes & Threads

SharedHashFile is a lightweight NoSQL hash table library written in C for Linux, operating fully in memory.  There is no server process.  Data is read and written directly from/to shared memory; no sockets are used between SharedHashFile and the application program.

## Project Goals

* Faster speed.
* Simpler & smaller footprint.
* Lower memory usage.
* Concurrent, in memory access.

## Technology

### Data Storage

Data is kept in shared memory by default, making all the data accessible to separate processes and/or threads. Up to 4 billion keys can be stored in a single SharedHashFile hash table which is limited in size only by available RAM.

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

Can multiple threads in multiple processes also access SharedHashFile hash tables concurrently? Yes.

### Persistent Storage

Hash tables are stored in memory mapped files in `/dev/shm` which means the data persists even when no processes are using hash tables. However, the hash tables will not survive rebooting.

## Building

```
root@16vcpu:/# make clean ; rm -rf /dev/shm/test-*/ ; make
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

Here's an example on an 8 core Lenovo W530 laptop showing a hash table with 50 million keys, and then doing 2% delete/insert and 98% read at a rate of over 10 million operations per second:

```
$ make clean ; rm -rf /dev/shm/test-*/ ; SHF_ENABLE_PERFORMANCE_TEST=1 make
rm -rf release debug
make: variable: DEPS_H=murmurhash3.h shf.defines.h shf.h shf.lock.h shf.private.h tap.h
make: variable: DEPS_HPP=SharedHashFile.hpp
make: variable: PROD_SRCS_C=murmurhash3.c shf.c tap.c
make: variable: PROD_OBJS_C=release/murmurhash3.o release/shf.o release/tap.o
make: variable: PROD_SRCS_CPP=SharedHashFile.cpp
make: variable: PROD_OBJS_CPP=release/SharedHashFile.o
make: variable: TEST_SRCS_C=test.1.tap.c test.9.shf.c
make: variable: TEST_OBJS_C=
make: variable: TEST_SRCS_CPP=test.a.shf.cpp
make: variable: TEST_OBJS_CPP=
make: variable: TEST_EXES=release/test.1.tap.t release/test.9.shf.t release/test.a.shf.t
make: compiling: release/test.1.tap.o
make: compiling: release/murmurhash3.o
make: compiling: release/shf.o
make: compiling: release/tap.o
make: compiling: release/SharedHashFile.o
make: linking: release/test.1.tap.t
make: running: release/test.1.tap.t
1..1
ok 1 - All passed
make: compiling: release/test.9.shf.o
make: linking: release/test.9.shf.t
make: running: release/test.9.shf.t
1..21
ok 1 - shf_attach_existing() fails for non-existing file as expected
ok 2 - shf_attach()          works for non-existing file as expected
ok 3 - shf_get_copy_via_key() could not find unput key as expected
ok 4 - shf_del_key()          could not find unput key as expected
ok 5 - shf_put_val()                           put key as expected
ok 6 - shf_get_copy_via_key() could     find   put key as expected
ok 7 - shf_val_len                                     as expected
ok 8 - shf_val                                         as expected
ok 9 - shf_del_key()          could     find   put key as expected
ok 10 - shf_get_copy_via_key() could not find   del key as expected
ok 11 - shf_del_key()          could not find   del key as expected
ok 12 - shf_put_val()                         reput key as expected
ok 13 - shf_get_copy_via_key() could     find reput key as expected
ok 14 - shf_val_len                                     as expected
ok 15 - shf_val                                         as expected
ok 16 - put expected number of              keys // estimate 4050542 keys per second
ok 17 - got expected number of non-existing keys // estimate 5102659 keys per second
ok 18 - got expected number of     existing keys // estimate 4814089 keys per second
ok 19 - graceful growth cleans up after itself as expected
ok 20 - del expected number of     existing keys // estimate 5583620 keys per second
ok 21 - del does not   clean  up after itself as expected
running tests on: via command: 'cat /proc/cpuinfo | egrep 'model name' | head -n 1'
running tests on: `model name   : Intel(R) Core(TM) i7-3720QM CPU @ 2.60GHz`
-OP MMAP REMAP SHRK PART TOTAL ------PERCENT OPERATIONS PER PROCESS PER SECOND -OPS
--- -k/s --k/s --/s --/s M-OPS 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 -M/s
PUT  0.4   0.9   49    0   0.0 36 22 22  0  0  0  0 27  0  0  0  0  0  0  0  0  0.0
PUT 29.0 160.2 1888 1680   4.2 13 13 12 12 12 13 12 13  0  0  0  0  0  0  0  0  4.2 -----
PUT 27.4 157.7 1734 1734   8.1 13 12 13 12 13 12 13 12  0  0  0  0  0  0  0  0  3.9 -----
PUT 16.4 175.8 1027 1028  13.1 13 13 12 12 13 13 12 12  0  0  0  0  0  0  0  0  5.0 ------
PUT 32.6 106.5 2069 2068  15.7 12 13 13 12 13 12 12 13  0  0  0  0  0  0  0  0  2.5 ---
PUT 22.0  96.4 1327 1327  17.9 13 12 13 13 12 13 13 12  0  0  0  0  0  0  0  0  2.2 --
PUT  4.5 157.9  282  282  22.4 13 12 13 12 12 13 12 12  0  0  0  0  0  0  0  0  4.5 -----
PUT  8.1  84.7  527  527  25.1 13 13 12 13 13 12 12 13  0  0  0  0  0  0  0  0  2.7 ---
PUT 27.6 145.6 1791 1792  28.4 12 12 12 12 13 12 12 13  0  0  0  0  0  0  0  0  3.3 ----
PUT 36.6  90.9 2303 2302  30.9 13 13 13 12 12 13 12 12  0  0  0  0  0  0  0  0  2.5 ---
PUT 34.9 129.1 2144 2144  33.4 12 13 13 12 12 13 12 13  0  0  0  0  0  0  0  0  2.5 ---
PUT 21.0 152.4 1244 1244  37.2 13 13 12 12 12 12 13 12  0  0  0  0  0  0  0  0  3.8 -----
PUT  4.7 215.8  301  301  43.7 13 13 13 12 12 12 13 13  0  0  0  0  0  0  0  0  6.6 --------
PUT 11.4 179.3  753  753  48.6 13 13 13 12 12 13 12 13  0  0  0  0  0  0  0  0  4.8 ------
PUT  5.1  24.9  362  362  50.0  8 11 11 18 14 11 15 12  0  0  0  0  0  0  0  0  1.4 -
MIX  0.0   0.0    0    0  50.0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.0
MIX  1.6   7.5    0    0  57.0 12 13 12 13 13 12 13 12  0  0  0  0  0  0  0  0  7.0 ---------
MIX  0.0   5.8    0    0  67.9 13 13 12 13 12 12 13 13  0  0  0  0  0  0  0  0 10.9 --------------
MIX  0.0   7.4    0    0  79.0 13 13 12 13 12 13 13 13  0  0  0  0  0  0  0  0 11.0 --------------
MIX  0.0   9.3    0    0  89.8 12 13 12 13 12 12 13 13  0  0  0  0  0  0  0  0 10.8 --------------
MIX  0.0   9.4    0    0 100.0 13 12 13 12 13 13 12 12  0  0  0  0  0  0  0  0 10.2 -------------
GET  0.0   0.0    0    0 100.0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0.0
GET  0.0   0.4    0    0 100.5 12 13 12 13 13 13 10 13  0  0  0  0  0  0  0  0  0.5
GET  0.0   0.0    0    0 112.8 12 13 13 13 13 12 13 12  0  0  0  0  0  0  0  0 12.3 ----------------
GET  0.0   0.0    0    0 125.0 13 13 12 13 13 12 12 12  0  0  0  0  0  0  0  0 12.2 ----------------
GET  0.0   0.0    0    0 137.2 13 13 12 13 13 13 12 12  0  0  0  0  0  0  0  0 12.2 ----------------
GET  0.0   0.0    0    0 149.3 12 13 13 12 12 13 12 13  0  0  0  0  0  0  0  0 12.1 ----------------
GET  0.0   0.0    0    0 150.0  8  4 19  0  0  6 28 35  0  0  0  0  0  0  0  0  0.7
* MIX is 2% (1000000) del/put, 98% (6050327) get
make: compiling: release/test.a.shf.o
make: linking: release/test.a.shf.t
make: running: release/test.a.shf.t
1..22
ok 1 - new SharedHashFile returned object as expected
ok 2 - ->AttachExisting() fails for non-existing file as expected
ok 3 - ->Attach()         works for non-existing file as expected
ok 4 - ->GetCopyViaKey() could not find unput key as expected
ok 5 - ->DelKey()        could not find unput key as expected
ok 6 - ->PutVal()                         put key as expected
ok 7 - ->GetCopyViaKey() could     find   put key as expected
ok 8 - shf_val_len                                as expected
ok 9 - shf_val                                    as expected
ok 10 - ->DelKey()        could     find   put key as expected
ok 11 - ->GetCopyViaKey() could not find   del key as expected
ok 12 - ->DelKey()        could not find   del key as expected
ok 13 - ->PutVal()                       reput key as expected
ok 14 - ->GetCopyViaKey() could     find reput key as expected
ok 15 - shf_val_len                                as expected
ok 16 - shf_val                                    as expected
ok 17 - put expected number of              keys // estimate 4021107 keys per second
ok 18 - got expected number of non-existing keys // estimate 5255414 keys per second
ok 19 - got expected number of     existing keys // estimate 5274607 keys per second
ok 20 - graceful growth cleans up after itself as expected
ok 21 - del expected number of     existing keys // estimate 5312581 keys per second
ok 22 - del does not   clean  up after itself as expected
make: built and tested release version
```

## TODO

* Tune for faster performance.
* Test performance on flash drives.
* Support key,value data types other than binary strings with 32bit length.
* Support in-memory persistence past reboot.
* Support walking of all key,value pairs in the hash table.
* Support stack key types, e.g. push, pop, shift, unshift.
* Support access of previously added keys via UID instead of key.
* Support detaching from a hash table.
* Add LRU mechanism for using SharedHashFile to e.g. accelerate network based hash tables such as redis, memcached, and/or Aerospike.
* Add logging.
* Add networking layer for distributed hash table.
* Add more tests & enforce 100% code coverage.
* Add command line utility tools.
* Port to Linux-like OSs which do not support mremap().
* Port to Windows.
* Email feedback [@] sharedhashfile [.] com with more wishes!

