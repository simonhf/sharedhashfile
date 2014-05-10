use strict;

my $tar_gz = qq[2727e97de35320b0ac433ff2e811b9640bb66996.tar.gz];

printf qq[- make\n]; system qq[make]; # first make SharedHashFile
if ( ! -e qq[./release/$tar_gz] ) { printf qq[- get $tar_gz\n]; system qq[cd ./release/ ; wget https://gitorious.org/mdb/mdb/archive/$tar_gz]; }
if ( ! -e qq[./release/mdb-mdb/] ) { printf qq[- untar $tar_gz\n]; system qq[cd ./release/ ; tar zxfv $tar_gz]; }
if ( ! -e qq[./release/mdb-mdb/libraries/liblmdb/liblmdb.a] ) { printf qq[- compile $tar_gz\n]; system qq[cd ./release/mdb-mdb/libraries/liblmdb ; make ; make test ; cp liblmdb.a ../../../.]; }
printf qq[- perf test\n]; system qq[rm -rf /dev/shm/test-*/ ; gcc -O3 -std=gnu99 -I./release/mdb-mdb/libraries/liblmdb -DTEST_LMDB -o ./release/perf-test-lmdb.t -I./src ./src/test.f.shf.c ./release/murmurhash3.o ./release/shf.o ./release/liblmdb.a -pthread && SHF_PERFORMANCE_TEST_ENABLE=1 ./release/perf-test-lmdb.t 2>&1 | tee ./release/perf-test-lmdb.txt];
