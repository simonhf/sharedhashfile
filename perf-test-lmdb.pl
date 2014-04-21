use strict;

my $tar_gz = qq[2727e97de35320b0ac433ff2e811b9640bb66996.tar.gz];

if ( ! -e qq[./release/] ) { printf qq[- mkdir release\n]; system qq[mkdir ./release]; }
if ( ! -e qq[./release/$tar_gz] ) { printf qq[- get $tar_gz\n]; system qq[cd ./release/ ; wget https://gitorious.org/mdb/mdb/archive/$tar_gz]; }
if ( ! -e qq[./release/mdb-mdb/] ) { printf qq[- untar $tar_gz\n]; system qq[cd ./release/ ; tar zxfv $tar_gz]; }
if ( ! -e qq[./release/mdb-mdb/libraries/liblmdb/liblmdb.a] ) { printf qq[- compile $tar_gz\n]; system qq[cd ./release/mdb-mdb/libraries/liblmdb ; make ; make test ; cp liblmdb.a ../../../.]; }
system qq[rm -rf /dev/shm/test-*/ ; gcc -O3 -std=gnu99 -I./release/mdb-mdb/libraries/liblmdb -DTEST_LMDB -o ./release/perf-test-lmdb.t test.f.shf.c ./release/murmurhash3.o ./release/shf.o ./release/liblmdb.a -pthread && SHF_ENABLE_PERFORMANCE_TEST=1 ./release/perf-test-lmdb.t 2>&1 | tee ./release/perf-test-lmdb.txt];
