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

Keys and values are currently binary strings, each with a maximum 32bit length.

### Allocation & Garbage Collection

Conventional malloc() does not function in shared memory, since we have to use offsets instead of conventional pointers. Hence SharedHashFile uses its own implementation of malloc() for shared memory.

To avoid memory holes then garbage collection happens from time to time upon key,value insertion. The number of key,value pairs effected during garbage collection is intentionally limited by the algorithm to a maximum of 8,192 pairs no matter how many keys have been inserted in the hash table. This means the hash table always feels very responsive.

### Hash Table Expansion

SharedHashFile is designed to expand gracefully as more key,value pairs are inserted. There are no sudden memory increases or memory doubling events. And there are no big pauses due to rehashing keys en masse.

### Locking

To reduce contention there is no single global hash table lock by design. Instead keys are sharded across 256 locks. During tests with 8 concurrent processes reading unique keys as fast as possible (with a total throughput of approx. 3 million keys accessed per second) then only about 75,000 of the 3,000,000 locks results in lock contention.

Can multiple threads in multiple processes also access SharedHashFile hash tables concurrently? Yes.

### Persistent Storage

Hash tables are stored in memory mapped files in `/dev/shm` which means the data persists even when no processes are using hash tables. However, the hash tables will not survive rebooting.

## Building

```
$ make clean ; rm -rf /dev/shm/test-*/ ; make
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
ok 5 - put expected number of              keys // 1262199 keys per second
ok 6 - got expected number of non-existing keys // 1859425 keys per second
ok 7 - got expected number of     existing keys // 1389514 keys per second
ok 8 - graceful growth cleans up after itself as expected
ok 9 - del expected number of     existing keys // 1543254 keys per second
ok 10 - del does not   clean  up after itself as expected
WHAT -MMAP REMAP SHRK PART CPU0-/s CPU1-/s CPU2-/s CPU3-/s CPU4-/s CPU5-/s CPU6-/s CPU7-/s ----TOTAL TOTAL/s
PUT: 10241 84537  998  148  335429  336856  332081  313390  327291  326020  337041  286920   2595063 2595028 -------------------------
PUT: 12866 62320  814  815  170723  183810  201937  180679  198800  184991  193124  194338   4103465 1508402 ---------------
PUT:  5588 71055  346  345  257491  258872  258922  215160  255800  244837  255948  257253   6107757 2004292 --------------------
PUT: 11156 47857  716  716  158696  163368  170530  145155  132439  162252  161291  177301   7378789 1271032 ------------
PUT: 13667 44948  850  851  109063  102743  113696  112799  108376  109406  114201  115546   8264646  885857 --------
PUT:  6079 47003  359  358  189017  149971  202287  189370  192522  192636  150331  201518   9732298 1467652 --------------
PUT:  2018 64569  136  136  239511  204904  253675  247205  253416  249577  208752  253678  11643016 1910718 -------------------
PUT:  5652 55840  361  361  170895  175807  175440  168265  177692  183717  154768  181112  13030744 1387728 -------------
PUT:  9794 46426  634  634  133082  113531  140540  139573  142818  139512  119534  138842  14098176 1067432 ----------
PUT: 11238 31471  733  733  117450   82485  113503  121348  116514  115116   78040  121808  14964440  866264 --------
PUT: 12194 31503  757  757   85971   83571   92128   88266   87460   93799   81514   90059  15667211  702771 -------
PUT:  8383 37977  508  508   66431   64611   64657   70991   75211   69827   70240   70816  16219995  552784 -----
PUT:  8482 20075  517  517   93577   95607   97218   92933   89340   95385  102849  101797  16988701  768706 -------
PUT:  5127 28570  302  302   83036   89590   82755   90553   83143   88903   89089   90110  17686015  697314 ------
PUT:  1799 34747  102  102   80756   80074   64038   82883   64040   74128   73574   75508  18281015  595000 -----
PUT:   802 15019   47   47  132521  124044  130045  124890  107458  132048  110348  132894  19275268  994253 ---------
PUT:   217 22472   20   20   76207  190108    6542  116529   87675   37834  199352   10493  20000008  724740 -------
GET:   171 19943    0    0  177575  107605  397423  127339  145236  302814  111669  388248  21757917 1757909 -----------------
GET:     0     0    0    0  500500  489297  493215  467264  478860  434835  452018  446036  25519962 3762045 -------------------------------------
GET:     0     0    0    0  470116  442537  464706  422887  465347  451462  480264  425706  29143013 3623051 ------------------------------------
GET:     0     0    0    0  475234  463942  464604  422916  479111  470205  463513  427940  32810489 3667476 ------------------------------------
GET:     0     0    0    0  486284  459073  476436  435428  468544  478425  467857  398328  36480893 3670404 ------------------------------------
GET:     0     0    0    0  390271  537541  203607  624147  462903  362255  524654  413739  40000016 3519123 -----------------------------------
MIX:     0   999    0    0  182522   40326  420068    5571   76282  182719   46642   69083  41023229 1023213 ----------
MIX:     0  2907    0    0  358663  349529  340853  337871  351921  348772  368743  332958  43812538 2789309 ---------------------------
MIX:     0  2216    0    0  346714  321900  326983  345465  364438  357768  281999  302866  46460711 2648173 --------------------------
MIX:     0  1946    0    0  377561  342591  379011  352232  370900  382622  355236  359748  49380637 2919926 -----------------------------
MIX:     0  1864    0    0  390534  390257  383082  363093  389516  392971  366974  387652  52444728 3064091 ------------------------------
MIX:     0  1496    0    0  378438  367926  379052  311062  385728  332607  366298  377307  55343166 2898438 ----------------------------
MIX:     0  1409    0    0  399831  435148  270939  396662  437549  433997  390390  412912  58520707 3177541 -------------------------------
MIX:     0   330    0    0   65711  252297       0  388008  123667   68458  323697  257459  60000024 1479317 --------------
* MIX is 2% (400000) del/put, 98% (19600000) get
make: built and tested release version
```

Notes:

* The above figures were generated on a 2nd generation i7 CPU with slower 1333Mhz DDR3 RAM.
* A line of stats is output every second during the test.
* First, 8 concurrent processes put 20 million unique keys into the hash table.
* Then, 8 concurrent processes get 20 million unique keys from the hash table.
* Then, 8 concurrent processes get 98% of 20 million unique keys while updating (del/put) the other 2%.
* In total 60 million hash table operations are performed.
* Why does put performance vary so much? This is due to kernel memory mapping overhead; 'top' shows bigger system CPU usage.

## TODO

* Tune for faster performance.
* Test performance on large Amazon EC2 instance.
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

