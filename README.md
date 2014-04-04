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
ok 5 - put expected number of              keys // 1083081 keys per second
ok 6 - got expected number of non-existing keys // 1342162 keys per second
ok 7 - got expected number of     existing keys // 1170652 keys per second
ok 8 - graceful growth cleans up after itself as expected
ok 9 - del expected number of     existing keys // 1182870 keys per second
ok 10 - del does not   clean  up after itself as expected
-OP -MMAP -REMAP SHRK PART -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -TOTAL -CPU
--- ----- ------ ---- ---- -0/s -1/s -2/s -3/s -4/s -5/s -6/s -7/s -8/s -9/s 10/s 11/s 12/s 13/s 14/s 15/s ---OPS -*/s
PUT  1846  16894  789  626   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0.0M 0.0M
PUT 18300 132617  838  119 136k 130k 134k 143k 136k 134k 105k 136k 139k 132k 115k 128k  97k 116k 141k  94k   2.0M 2.0M ----------
PUT 10763 102071  352  352  95k 105k 101k  86k  96k  96k  71k  92k  74k  92k 101k  70k  90k  85k 107k  74k   3.4M 1.4M -------
PUT 21310  57019  661  661  55k  43k  60k  44k  61k  60k  43k  59k  40k  43k  56k  40k  62k  43k  49k  58k   4.2M 0.8M ----
PUT  7555 128500  241  241 122k  98k 127k 124k 115k 119k 104k 133k 127k 115k 103k  94k 129k  97k  97k 129k   6.0M 1.8M ---------
PUT 21787  84705  722  723  71k  70k  74k  71k  64k  78k  61k  70k  65k  76k  60k  71k  60k  79k  68k  77k   7.1M 1.1M -----
PUT 30163  77620  924  923  68k  67k  64k  62k  57k  68k  64k  57k  61k  63k  75k  57k  65k  64k  69k  56k   8.1M 1.0M -----
PUT  8327 130231  222  222 121k  98k  90k  91k 114k 125k  98k 123k 115k 124k 109k 120k  84k 108k 117k 117k   9.8M 1.7M ---------
PUT  7191 135760  251  251 124k 134k 127k  97k 130k 129k 124k 134k 114k 134k  97k 135k 137k 124k 111k 131k  11.7M 1.9M ----------
PUT 21802 104197  730  731  97k  99k  98k  89k  91k  94k  93k  95k  76k  99k  89k  99k 101k  90k  81k 104k  13.2M 1.5M -------
PUT 28166  77277  927  926  61k  73k  67k  55k  61k  63k  70k  60k  55k  69k  66k  69k  62k  57k  62k  68k  14.2M 1.0M -----
PUT 34669  94778 1069 1070  67k  68k  63k  62k  72k  69k  61k  61k  63k  72k  75k  70k  63k  65k  69k  61k  15.3M 1.0M -----
PUT 27234  64662  801  800  67k  70k  73k  69k  71k  72k  72k  70k  68k  78k  75k  71k  71k  67k  67k  66k  16.4M 1.1M -----
PUT 11614 131750  306  306  82k 121k 125k 105k 116k 124k 120k 123k 119k 116k 105k 121k 111k  85k 117k 118k  18.1M 1.8M ---------
PUT  2625 140452   86   86 134k 136k 137k 115k 142k 145k 140k 144k 128k 125k 139k 142k 124k 113k 135k 138k  20.2M 2.1M ----------
PUT  6010 137401  224  225 114k 126k 121k 132k 132k 128k 134k 129k 122k 120k 117k 125k 108k 132k 131k 126k  22.2M 2.0M ----------
PUT 13889 133803  469  468 105k 122k 113k 112k 118k 112k 112k 111k 113k 116k 113k 106k 119k 118k 119k 113k  24.0M 1.8M ---------
PUT 20021 114029  670  670  92k  97k  91k  93k  99k  91k 100k  95k  91k  94k  97k  93k  93k  95k 100k  92k  25.5M 1.5M -------
PUT 25807 120622  851  851  81k  91k  90k  77k  90k  70k  90k  93k  73k  88k  82k  87k  92k  79k  91k  82k  26.8M 1.3M ------
PUT 31029  74051 1027 1028  79k  60k  73k  71k  77k  78k  80k  74k  62k  77k  73k  72k  65k  78k  77k  68k  27.9M 1.1M ------
PUT 34390  74113 1084 1083  72k  67k  67k  74k  69k  69k  75k  72k  74k  67k  70k  59k  69k  69k  73k  70k  29.0M 1.1M -----
PUT 36075 101364 1125 1125  64k  68k  68k  70k  79k  69k  71k  77k  72k  71k  74k  71k  70k  70k  69k  75k  30.2M 1.1M -----
PUT 33892 121341 1034 1035  68k  75k  76k  69k  68k  72k  64k  71k  75k  66k  65k  68k  67k  71k  68k  71k  31.3M 1.1M -----
PUT 29914  51972  868  869  80k  90k  86k  86k  82k  78k  84k  83k  93k  87k  84k  89k  80k  81k  73k  86k  32.6M 1.3M ------
PUT 20353 136464  560  558 103k 107k 106k 108k  98k  83k  97k  96k  90k 104k  94k  98k  98k  98k  94k  97k  34.1M 1.5M --------
PUT  8043 126830  208  208  95k 115k 122k 101k  96k 117k 112k 129k 123k  85k 117k 128k 124k 108k 108k 123k  35.9M 1.8M ---------
PUT  2874 143734   79   79  99k 143k 146k 128k  99k 116k 137k 143k 134k 103k 145k 140k 152k 137k 128k 142k  37.9M 2.1M ----------
PUT  2892 126252  105  105  89k 139k 127k 130k  95k 130k 136k 140k 133k 105k 128k 136k 139k 142k 113k 138k  39.9M 2.0M ----------
PUT  4094 158361  146  146 130k 102k 127k 136k 130k 132k 104k 142k 134k 135k 120k 130k 135k 113k 134k 109k  41.9M 2.0M ----------
PUT  6744 106544  224  224 130k 105k 108k 126k 125k 113k 100k 126k 127k 132k 129k 124k 137k 108k 128k 115k  43.8M 1.9M ---------
PUT  9221 143005  323  324  89k 120k 107k 103k 108k 100k 110k 107k  99k 127k 106k 116k 127k 107k 113k 120k  45.5M 1.7M ---------
PUT 10902 120776  363  362  76k 116k 115k 110k  90k  87k 120k 107k  77k 121k  85k  87k 116k 116k  97k  82k  47.1M 1.6M --------
PUT 13621  74524  449  451  86k  92k  97k  88k  97k  87k  98k 101k  87k 104k  90k  95k 100k 103k 104k  93k  48.6M 1.5M -------
PUT 17875 136876  633  632  85k  84k  85k  82k  77k  96k  95k  98k  98k  87k  92k  94k  86k  95k  92k  84k  50.0M 1.4M -------
PUT 24675  73902  841  840  85k  95k 104k  93k  82k  99k  95k 107k 101k  87k 101k 100k  91k 100k 104k  98k  51.5M 1.5M -------
PUT 25114 115778  812  812  89k  81k  88k  83k  56k  84k  90k  86k  89k  57k  89k  65k  86k  93k  94k  71k  52.8M 1.3M ------
PUT 29537 142791  962  963  81k  83k  85k  74k  83k  80k  79k  80k  84k  80k  85k  81k  80k  85k  87k  75k  54.1M 1.3M ------
PUT 23038  55359  763  764  65k  61k  63k  57k  53k  61k  54k  51k  51k  60k  58k  56k  55k  62k  57k  56k  55.0M 0.9M ----
PUT 25776  49581  837  835  64k  54k  69k  68k  48k  65k  52k  64k  58k  60k  54k  65k  63k  55k  56k  65k  55.9M 0.9M ----
PUT 31511  76298 1026 1027  72k  73k  71k  63k  55k  72k  56k  58k  62k  59k  73k  66k  72k  73k  68k  66k  56.9M 1.0M -----
PUT 29616  61831  931  932  66k  62k  48k  48k  57k  63k  66k  68k  59k  55k  63k  59k  57k  63k  59k  60k  57.9M 0.9M ----
PUT 34575  61439 1114 1112  67k  68k  68k  63k  69k  67k  65k  69k  59k  53k  71k  66k  71k  69k  64k  64k  58.9M 1.0M -----
PUT 35957 111151 1097 1097  70k  71k  75k  70k  68k  68k  69k  73k  71k  71k  78k  69k  72k  70k  70k  67k  60.0M 1.1M -----
PUT 32595 141856 1010 1011  67k  61k  71k  60k  66k  63k  65k  66k  65k  65k  69k  67k  73k  65k  64k  68k  61.1M 1.0M -----
PUT 34170 116986 1023 1024  71k  68k  78k  67k  72k  70k  66k  72k  74k  70k  75k  71k  77k  67k  63k  68k  62.2M 1.1M -----
PUT 28780  62006  861  859  72k  69k  73k  70k  69k  66k  65k  69k  70k  64k  64k  62k  65k  62k  65k  68k  63.2M 1.1M -----
PUT 28008  42337  821  821  70k  76k  76k  73k  78k  74k  80k  74k  76k  80k  75k  67k  77k  72k  71k  71k  64.4M 1.2M ------
PUT 22320  37454  635  635  67k  74k  69k  71k  66k  72k  69k  70k  73k  71k  73k  68k  70k  67k  63k  70k  65.5M 1.1M -----
PUT 17518  75644  468  468  68k  82k  83k  76k  63k  76k  83k  89k  72k  81k  83k  75k  83k  84k  65k  70k  66.7M 1.2M ------
PUT 14961 169012  409  410  97k  91k  84k  90k  87k  85k  87k  94k  87k  93k  99k  86k 101k  98k  64k  94k  68.1M 1.4M -------
-OP -MMAP -REMAP SHRK PART -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -TOTAL -CPU
--- ----- ------ ---- ---- -0/s -1/s -2/s -3/s -4/s -5/s -6/s -7/s -8/s -9/s 10/s 11/s 12/s 13/s 14/s 15/s ---OPS -*/s
PUT 10041 177611  276  276 105k 108k 103k  80k 109k  83k  79k  96k  79k 111k 110k  73k 112k 110k  74k 104k  69.6M 1.5M -------
PUT  6968  88035  167  166 127k 112k 135k 106k 131k 128k 119k 118k 100k 124k 138k 120k 134k 135k 114k 121k  71.6M 1.9M ----------
PUT  3237  49976   84   84 129k 101k 130k 124k 123k 127k 127k 112k 126k 120k 126k 114k 128k 131k 119k 126k  73.5M 1.9M ----------
PUT  1948 188564   64   64 122k 105k 132k 124k 135k 127k 126k 124k 137k 112k 135k 126k 129k 130k 131k 133k  75.5M 2.0M ----------
PUT  2057 218239   72   72 137k 127k 127k 129k 133k 122k 131k 119k 124k 116k 116k 111k 127k 132k 129k 119k  77.4M 2.0M ----------
PUT  2591  80258   86   87 131k 141k 138k 131k 137k 138k 136k 137k 134k 123k 134k 135k 126k 135k 141k 131k  79.5M 2.1M -----------
PUT  3099  80498  113  112 126k 133k 117k 129k 130k 142k 134k 138k 122k 112k 125k 131k 134k 136k 134k 128k  81.6M 2.0M ----------
PUT  4214 206009  147  147 111k 132k 111k 112k 107k 133k 131k 121k 118k 117k 133k 119k 131k 122k 121k 122k  83.5M 1.9M ---------
PUT  4735 164426  161  161 111k  60k 123k 100k  91k 134k 110k 127k  59k 125k 123k 100k 133k 117k 124k 125k  85.2M 1.7M ---------
PUT  7011  74710  254  255 134k  85k 122k 116k 116k 153k 122k 142k 105k 139k 140k 114k 135k 109k 146k 128k  87.2M 2.0M ----------
PUT  7897  75331  268  267 122k 104k 109k 102k 119k 132k 109k 111k 108k 127k 122k  96k 115k  95k 114k 111k  88.9M 1.8M ---------
PUT  8713 170176  307  307 119k 115k  85k  86k 122k 112k  87k 101k 112k 124k 117k 109k  87k 101k 121k 115k  90.6M 1.7M --------
PUT 11494 192046  396  396 130k 119k  94k 120k 116k 104k  75k  82k 131k 134k 120k  87k 104k 115k 123k 122k  92.4M 1.7M ---------
PUT 12283  90544  433  433 140k 130k  78k 134k 144k  68k 123k  20k 137k 140k  80k 132k  49k 135k 139k 142k  94.1M 1.8M ---------
PUT 13742  47132  431  431 118k  99k  69k 220k 131k  93k 179k 204k 206k  15k  71k 177k 108k  71k  27k  36k  95.9M 1.8M ---------
GET  7139  15754    8    8  68k  73k 252k  22k  67k 275k  21k 271k   1k 206k 274k  21k 261k 116k 165k 164k  98.1M 2.2M -----------
GET  1399   1382    0    0 165k 156k 151k 151k 163k 149k 151k 163k 152k 166k 148k 164k 171k 154k 151k 166k 100.6M 2.5M ------------
GET   117     31    0    0 151k 155k 144k 159k 170k 139k 161k 162k 158k 162k 164k 172k 173k 144k 166k 138k 103.0M 2.5M ------------
GET     6      4    0    0 161k 144k 170k 166k 131k 148k 151k 154k 159k 150k 157k 162k 175k 135k 159k 171k 105.5M 2.4M ------------
GET     2      0    0    0 162k 145k 165k 167k 148k 150k 149k 152k 163k 161k 158k 152k 168k 143k 155k 158k 107.9M 2.4M ------------
GET     0      0    0    0 172k 158k 110k 173k 174k 157k 156k 171k 164k 162k 174k 161k 106k 158k 155k 160k 110.4M 2.5M ------------
GET     0      0    0    0 167k 158k 116k 171k 171k 160k 156k 165k 164k 175k 172k 156k 122k 158k 161k 159k 112.9M 2.5M -------------
GET     0      0    0    0 143k 164k 158k 157k 179k 163k 161k 133k 142k 185k 147k 140k 176k 166k 166k 150k 115.4M 2.5M ------------
GET     0      0    0    0 142k 178k 161k 164k 160k 147k 150k 152k 149k 147k 152k 145k 181k 159k 159k 155k 117.8M 2.5M ------------
GET     0      0    0    0 165k 166k 174k 148k 139k 153k 155k 154k 160k 138k 155k 139k 160k 173k 161k 138k 120.2M 2.4M ------------
GET     0      0    0    0 155k 149k 170k 165k 156k 149k 157k 155k 168k 159k 165k 155k 150k 137k 146k 164k 122.7M 2.4M ------------
GET     0      0    0    0 129k 153k 175k 163k 169k 158k 175k 154k 167k 170k 165k 150k 106k 157k 143k 168k 125.1M 2.4M ------------
GET     0      0    0    0 162k 162k 168k 166k 170k 163k 173k 161k 163k 173k 170k 163k 112k 165k 166k 108k 127.6M 2.5M -------------
GET     0      0    0    0 160k 148k 155k 139k 142k 160k 143k 175k 163k 172k 162k 160k 177k 143k 150k 156k 130.1M 2.5M ------------
GET     0      0    0    0 159k 169k 172k 139k 149k 179k 132k 174k 152k 177k 158k 147k 146k 132k 168k 158k 132.5M 2.5M ------------
GET     0      0    0    0 144k 171k 171k 142k 164k 177k 136k 155k 154k 180k 179k 146k 139k 136k 173k 140k 135.0M 2.5M ------------
GET     0      0    0    0 170k 174k 148k 173k 135k 155k 151k 150k 173k 176k 174k 167k 167k 133k 173k 137k 137.5M 2.5M -------------
GET     0      0    0    0 161k 173k 162k 162k 157k 149k 159k 161k 172k 158k 159k 160k 167k 128k 172k 127k 140.0M 2.5M ------------
GET     0      0    0    0 160k 170k 157k 166k 175k 164k 145k 154k 181k 144k 154k 157k 174k 132k 169k 131k 142.5M 2.5M -------------
GET     0      0    0    0 162k 165k 171k 170k 161k 161k 137k 169k 166k 136k 135k 158k 141k 155k 169k 160k 144.9M 2.5M ------------
GET     0      0    0    0 166k 165k 165k 167k 164k 167k 139k 163k 164k 138k 133k 165k 132k 168k 162k 172k 147.4M 2.5M ------------
GET     0      0    0    0 154k 161k 168k 171k 140k 161k 162k 175k 154k 162k 141k 156k 141k 156k 165k 141k 149.9M 2.5M ------------
GET     0      0    0    0 146k 166k 166k 160k 165k 155k 153k 152k 150k 150k 163k 149k 158k 160k 148k 150k 152.3M 2.4M ------------
GET     0      0    0    0 154k 153k 173k 148k 175k 155k 157k 132k 152k 129k 179k 150k 175k 167k 134k 155k 154.7M 2.4M ------------
GET     0      0    0    0 143k 144k 174k 181k 171k 156k 144k 174k 143k 133k 182k 131k 144k 168k 132k 174k 157.2M 2.4M ------------
GET     0      0    0    0 167k 157k 165k 166k 157k 155k 147k 153k 161k 155k 167k 136k 175k 163k 140k 161k 159.6M 2.5M ------------
GET     0      0    0    0 163k 157k 153k 163k 165k 149k 168k 143k 158k 184k 156k 149k 156k 154k 162k 160k 162.1M 2.5M -------------
GET     0      0    0    0 168k 134k 170k 167k 134k 170k 166k 132k 171k 189k 149k 133k 149k 148k 149k 188k 164.6M 2.5M ------------
GET     0      0    0    0 165k 135k 166k 169k 139k 165k 165k 141k 169k 187k 148k 134k 148k 148k 148k 189k 167.1M 2.5M ------------
GET     0      0    0    0 164k 128k 158k 160k 157k 146k 149k 157k 177k 176k 148k 126k 156k 141k 164k 176k 169.5M 2.4M ------------
GET     0      0    0    0 176k 151k 168k 169k 167k 143k 150k 157k 173k 141k 150k 151k 160k 161k 151k 142k 172.0M 2.5M ------------
GET     0      0    0    0 169k 161k 140k 166k 171k 166k 160k 159k 142k 158k 145k 160k 165k 168k 144k 173k 174.5M 2.5M -------------
GET     0      0    0    0 162k 169k 161k 158k 172k 165k 162k 166k 163k 165k 153k 141k 137k 166k 133k 148k 176.9M 2.5M ------------
GET     0      0    0    0 152k 169k 164k 162k 162k 151k 159k 151k 149k 165k 169k 154k 166k 143k 162k 170k 179.4M 2.5M -------------
GET     0      0    0    0 160k 166k 157k 170k 164k 170k 153k 168k 144k 147k 173k 163k 168k 134k 143k 163k 181.9M 2.5M -------------
-OP -MMAP -REMAP SHRK PART -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -TOTAL -CPU
--- ----- ------ ---- ---- -0/s -1/s -2/s -3/s -4/s -5/s -6/s -7/s -8/s -9/s 10/s 11/s 12/s 13/s 14/s 15/s ---OPS -*/s
GET     0      0    0    0 159k 179k 168k 163k 161k 176k 175k 153k 172k 175k 158k 157k 134k 139k 142k 145k 184.4M 2.5M -------------
GET     0      0    0    0 176k 168k 147k 178k 172k 153k 175k 101k 149k 129k 163k 162k 169k 175k 166k 131k 186.9M 2.5M ------------
GET     0     58    0    0 214k 207k  16k 214k 206k  33k 212k  92k 193k 105k  13k 216k 170k 216k 213k 211k 189.4M 2.5M ------------
GET     0    676    0    0  64k  85k 240k   9k  63k 199k 265k 248k 135k 136k 237k 259k  47k 263k  98k 123k 191.8M 2.4M ------------
MIX     0   1853    0    0 147k 130k 225k 214k 145k 231k   5k 220k  80k 234k 233k 118k 239k  27k 112k  93k 194.2M 2.4M ------------
MIX     0   2104    0    0 166k 158k 155k 165k 165k 161k 158k 161k 153k 160k 165k 104k 153k 135k 158k 162k 196.6M 2.4M ------------
MIX     0   2044    0    0 154k 155k 155k 148k 155k 165k 171k 151k 133k 133k 161k 167k 157k 139k 126k 142k 199.0M 2.4M ------------
MIX     0   2461    0    0 167k 154k 160k 164k 147k 152k 162k 141k 152k 140k 137k 147k 146k 147k 158k 163k 201.4M 2.4M ------------
MIX     0   2439    0    0 156k 134k 168k 175k 167k 147k 143k 155k 159k 166k 142k 148k 153k 152k 165k 131k 203.8M 2.4M ------------
MIX     0   2315    0    0 153k 159k 160k 144k 161k 143k 157k 161k 145k 144k 147k 147k 171k 169k 141k 156k 206.2M 2.4M ------------
MIX     0   2582    0    0 145k 163k 155k 146k 157k 157k 159k 144k 149k 154k 153k 157k 147k 162k 162k 165k 208.6M 2.4M ------------
MIX     0   2496    0    0 159k 147k 139k 166k 161k 161k 159k 161k 124k 154k 165k 144k 155k 141k 160k 156k 211.0M 2.4M ------------
MIX     0   2585    0    0 174k 157k 154k 144k 168k 161k 156k 168k 145k 138k 137k 150k 133k 145k 143k 143k 213.4M 2.4M ------------
MIX     0   2924    0    0 150k 164k 144k 153k 140k 164k 141k 150k 149k 155k 148k 164k 150k 165k 145k 155k 215.8M 2.4M ------------
MIX     0   2620    0    0 157k 152k 156k 160k 154k 159k 154k 155k 152k 150k 154k 137k 154k 160k 134k 160k 218.2M 2.4M ------------
MIX     0   2716    0    0 161k 153k 163k 157k 157k 148k 166k 138k 151k 126k 152k 140k 161k 154k 163k 161k 220.6M 2.4M ------------
MIX     0   2931    0    0 151k 158k 164k 167k 143k 156k 104k 171k 162k 174k 160k 140k 162k 110k 157k 157k 222.9M 2.4M ------------
MIX     0   2733    0    0 145k 157k 153k 164k 150k 156k 162k 133k 155k 149k 152k 134k 149k 144k 153k 163k 225.3M 2.4M ------------
MIX     0   3591    0    0 159k 182k 136k 142k 157k 173k 137k 148k 134k 161k 159k 156k 160k 131k 161k 134k 227.7M 2.4M ------------
MIX     0   2975    0    0 167k 165k 140k 142k 175k 164k 142k 137k 138k 163k 166k 140k 165k 130k 179k 129k 230.1M 2.4M ------------
MIX     0   3121    0    0 164k 176k 139k 150k 141k 141k 141k 140k 128k 178k 146k 168k 175k 152k 145k 166k 232.5M 2.4M ------------
MIX     0   3230    0    0 171k 164k 134k 163k 172k  90k 136k 164k  89k 174k 166k 166k 176k 163k 164k 172k 234.9M 2.4M ------------
MIX     0   3318    0    0 162k 158k 146k 164k 152k 154k 149k 152k 151k 132k 152k 152k 135k 145k 151k 171k 237.3M 2.4M ------------
MIX     0   3598    0    0 174k 154k 172k 154k 153k 173k 154k 160k 153k 122k 154k 146k 121k 147k 152k 172k 239.7M 2.4M ------------
MIX     0   4114    0    0 131k 168k 169k 162k 165k 156k 163k 166k 152k 163k 170k 135k 161k 159k 131k 133k 242.1M 2.4M ------------
MIX     0   3653    0    0 148k 153k 168k 154k 165k 142k 153k 138k 155k 154k 147k 149k 153k 165k 158k 137k 244.5M 2.4M ------------
MIX     0   3693    0    0 152k 146k 169k 156k 168k 144k 148k 127k 159k 158k 152k 139k 153k 145k 171k 173k 246.9M 2.4M ------------
MIX     0   3776    0    0 139k 160k 166k 154k 173k 154k 161k 138k 163k 144k 157k 132k 161k 166k 152k 146k 249.3M 2.4M ------------
MIX     0   4028    0    0 159k 150k 137k 136k 150k 154k 128k 149k 168k 168k 153k 143k 170k 163k 154k 150k 251.7M 2.4M ------------
MIX     0   4256    0    0 157k 171k 157k 121k 171k 153k 120k 159k 159k 167k 159k 145k 168k 160k 137k 140k 254.1M 2.4M ------------
MIX     0   4066    0    0 178k 172k 174k 131k 174k 142k 129k 174k 165k 180k 163k 149k 147k 157k 129k 128k 256.6M 2.4M ------------
MIX     0   3961    0    0 190k 182k 188k 137k 181k 140k 137k 180k  97k 175k 180k 142k 104k 142k 148k 148k 259.0M 2.4M ------------
MIX     0   3938    0    0 150k 156k 164k 140k 162k 131k 128k 148k 158k 156k 158k 151k 176k 149k 154k 177k 261.4M 2.4M ------------
MIX     0   4361    0    0 140k 137k 165k 160k 164k 123k 126k 157k 162k 166k 165k 166k 160k 160k 169k 164k 263.8M 2.4M ------------
MIX     0   4278    0    0 167k 161k 161k 155k 167k 127k 129k 160k 141k 162k 164k 148k 164k 160k 161k 158k 266.3M 2.4M ------------
MIX     0   5034    0    0 158k 147k 148k 161k 166k 131k 133k 160k 160k 157k 167k 161k 163k 156k 161k 155k 268.7M 2.4M ------------
MIX     0   4530    0    0 171k 133k 178k 129k 134k 133k 136k 137k 178k 169k 167k 177k 134k 173k 165k 132k 271.1M 2.4M ------------
MIX     0   4796    0    0 182k 136k 185k 163k 166k 161k 160k  70k  73k 178k 179k 186k 135k 180k 173k 132k 273.5M 2.4M ------------
MIX     0   4633    0    0 165k 125k 172k 164k 167k 169k 174k 150k 151k 167k 170k 170k 124k 163k 125k 123k 275.9M 2.4M ------------
MIX     0   4839    0    0 174k 129k 156k 153k 142k 175k 173k 135k 167k 177k 171k 140k 129k 140k 142k 144k 278.3M 2.4M ------------
MIX     0   4531    0    0 195k 143k  52k 189k 151k 192k 184k 137k 150k 186k  73k 140k 142k 148k 192k 152k 280.7M 2.4M ------------
MIX     0   3586    0    0 148k 165k   0k 219k 191k 203k 202k 149k 191k  15k   0k 191k 164k 182k 200k 191k 283.1M 2.4M ------------
MIX     0   2387    0    0   0k 207k   0k 121k  10k   1k 275k   0k 315k   0k   0k 295k 167k 305k 237k 317k 285.3M 2.2M -----------
MIX     0    381    0    0   0k   0k   0k   0k   0k   0k 273k   0k 217k   0k   0k 218k   0k 121k   0k  32k 286.1M 0.8M ----
* MIX is 2% (2000000) del/put, 98% (12100654) get
make: built and tested release version
```

Notes:

* The above figures were generated on a Rackspace cloud server with 16 vCPUs & 60GB RAM.
* A line of stats is output every second during the test.
* During the 'PUT' phase, 16 concurrent processes put 100 million unique keys into the hash table.
* Then during the 'GET' phase, 16 concurrent processes get 100 million unique keys from the hash table.
* Then during the 'MIX' phase, 16 concurrent processes get 98% of 100 million unique keys while updating (del/put) the other 2%.
* In total 300 million hash table operations are performed.
* Why does put performance vary so much? This is due to kernel memory mapping overhead; 'top' shows bigger system CPU usage.

## Performance

Here's an example on an 8 core Lenovo W530 laptop showing a hash table with 20 million keys, and then doing 2% delete/insert and 98% read at a rate of over 5 million operations per second:

```
$ cat /proc/cpuinfo | egrep "model name" | head -n 1
model name  : Intel(R) Core(TM) i7-3720QM CPU @ 2.60GHz

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
ok 5 - put expected number of              keys // 1858503 keys per second
ok 6 - got expected number of non-existing keys // 2526264 keys per second
ok 7 - got expected number of     existing keys // 2136307 keys per second
ok 8 - graceful growth cleans up after itself as expected
ok 9 - del expected number of     existing keys // 2180191 keys per second
ok 10 - del does not   clean  up after itself as expected
-OP -MMAP -REMAP SHRK PART -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -CPU -TOTAL -CPU
--- ----- ------ ---- ---- -0/s -1/s -2/s -3/s -4/s -5/s -6/s -7/s -8/s -9/s 10/s 11/s 12/s 13/s 14/s 15/s ---OPS -*/s
PUT  1584  16894  665  626   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0.0M 0.0M 
PUT 18919 130814 1550  707 476k 486k 436k 458k 474k 462k 464k 478k   0k   0k   0k   0k   0k   0k   0k   0k   3.7M 3.7M -------------------
PUT 15915 119144 1000 1001 385k 369k 384k 388k 390k 360k 373k 399k   0k   0k   0k   0k   0k   0k   0k   0k   6.6M 3.0M ---------------
PUT 24161  94668 1491 1490 251k 239k 262k 271k 265k 256k 257k 265k   0k   0k   0k   0k   0k   0k   0k   0k   8.7M 2.0M ----------
PUT  7580 129936  489  489 453k 466k 478k 482k 477k 457k 475k 481k   0k   0k   0k   0k   0k   0k   0k   0k  12.3M 3.7M -------------------
PUT 24610  88575 1584 1585 264k 269k 280k 278k 267k 235k 263k 268k   0k   0k   0k   0k   0k   0k   0k   0k  14.4M 2.1M ----------
PUT 27393  85735 1681 1681 242k 240k 238k 251k 239k 246k 234k 241k   0k   0k   0k   0k   0k   0k   0k   0k  16.3M 1.9M ---------
PUT  6782 106069  398  397 366k 369k 359k 309k 326k 422k 371k 307k   0k   0k   0k   0k   0k   0k   0k   0k  19.1M 2.8M --------------
GET   102  12073    0    0 103k 120k 113k 201k 164k  89k  97k 227k   0k   0k   0k   0k   0k   0k   0k   0k  20.2M 1.1M -----
GET     0      0    0    0 679k 712k 705k 651k 684k 704k 684k 661k   0k   0k   0k   0k   0k   0k   0k   0k  25.5M 5.4M ----------------------------
GET     0      0    0    0 689k 698k 691k 635k 697k 688k 672k 685k   0k   0k   0k   0k   0k   0k   0k   0k  30.9M 5.3M ---------------------------
GET     0      0    0    0 680k 705k 673k 698k 701k 699k 679k 687k   0k   0k   0k   0k   0k   0k   0k   0k  36.2M 5.4M ----------------------------
GET     0      0    0    0 288k 204k 257k 254k 192k 259k 307k 178k   0k   0k   0k   0k   0k   0k   0k   0k  38.1M 1.9M ---------
MIX     0   3947    0    0 405k 541k 465k 460k 548k 481k 394k 562k   0k   0k   0k   0k   0k   0k   0k   0k  41.9M 3.8M -------------------
MIX     0   4207    0    0 656k 644k 681k 677k 694k 690k 673k 629k   0k   0k   0k   0k   0k   0k   0k   0k  47.1M 5.2M ---------------------------
MIX     0   3078    0    0 643k 698k 682k 684k 695k 664k 670k 641k   0k   0k   0k   0k   0k   0k   0k   0k  52.4M 5.3M ---------------------------
MIX     0   2056    0    0 683k 556k 611k 619k 501k 604k 702k 608k   0k   0k   0k   0k   0k   0k   0k   0k  57.2M 4.8M -------------------------
MIX     0      1    0    0  52k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k   0k  57.2M 0.1M 
* MIX is 2% (400000) del/put, 98% (19600000) get
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

