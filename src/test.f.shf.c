/*
 * ============================================================================
 * Copyright (c) 2014 Hardy-Francis Enterprises Inc.
 * This file is part of SharedHashFile.
 *
 * SharedHashFile is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * SharedHashFile is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see www.gnu.org/licenses/.
 * ----------------------------------------------------------------------------
 * To use SharedHashFile in a closed-source product, commercial licenses are
 * available; email office [@] sharedhashfile [.] com for more information.
 * ============================================================================
 */

#define _GNU_SOURCE   /* See feature_test_macros(7) */
#include <sys/mman.h> /* for mremap() */
// #include <string.h>   /* for memcmp() */

#include "shf.private.h"
#include "shf.h"
#include "tap.h"

#ifdef TEST_LMDB

#include "lmdb.h"

#define TEST_WHAT "LMDB aka Lightning MDB"

#define TEST_INIT() \
    pid_t pid = getpid(); \
    char test_db_folder[256]   ; SHF_SNPRINTF(1, test_db_folder   , "/dev/shm/test-lmdb-%05u", pid           ); \
    char test_mkdir_folder[256]; SHF_SNPRINTF(1, test_mkdir_folder, "mkdir %s"               , test_db_folder); \
    shf_backticks(test_mkdir_folder); \
    int rc; \
    MDB_env *env; \
    MDB_dbi dbi; \
    MDB_val lmdb_key, data; \
    MDB_txn *txn; \
    MDB_cursor *cursor; \
    char sval1[32]; \
    char sval2[32]; \
    rc = mdb_env_create(&env); \
         mdb_env_set_mapsize(env, 4096 * 1000000L); \
    rc = mdb_env_open(env, test_db_folder, 0, 0664); \
    rc = mdb_txn_begin(env, NULL, 0, &txn); \
    rc = mdb_open(txn, NULL, 0, &dbi); \
    lmdb_key.mv_size = sizeof(uint32_t); \
    lmdb_key.mv_data = sval1           ; \
    data.mv_size     = sizeof(uint32_t); \
    data.mv_data     = sval2           ; \
    /* mdb_txn_abort(txn); */ \
    rc = mdb_txn_commit(txn); if (rc) { fprintf(stderr, "mdb_txn_commit: (%d) %s\n", rc, mdb_strerror(rc)); exit(1); } \
    mdb_close(env, dbi); \
    mdb_env_close(env)

#define TEST_INIT_CHILD() \
    rc = mdb_env_create     (&env                        ); if (rc) { fprintf(stderr, "mdb_env_create(): (%d) %s\n", rc, mdb_strerror(rc)); exit(1); } \
         mdb_env_set_mapsize(env, 4096 * 1000000L        ); \
    rc = mdb_env_open       (env, test_db_folder, 0, 0664); if (rc) { fprintf(stderr, "mdb_env_open(): (%d) %s\n"  , rc, mdb_strerror(rc)); exit(1); } \
    rc = mdb_txn_begin      (env, NULL, 0, &txn          ); if (rc) { fprintf(stderr, "mdb_txn_begin(): (%d) %s\n" , rc, mdb_strerror(rc)); exit(1); } \
    rc = mdb_open           (txn, NULL, 0, &dbi          ); if (rc) { fprintf(stderr, "mdb_open(): (%d) %s\n"      , rc, mdb_strerror(rc)); exit(1); }

#define TEST_PUT() \
    ((uint32_t *)sval1)[0] = key     ; \
    ((uint32_t *)sval2)[0] = key + 10; \
    if (0 == i % 1000) { \
        rc = mdb_txn_commit(txn               ); if (rc) { fprintf(stderr, "mdb_txn_commit: (%d) %s at %d\n", rc, mdb_strerror(rc), i); exit(1); } \
        rc = mdb_txn_begin (env, NULL, 0, &txn); if (rc) { fprintf(stderr, "mdb_txn_begin: (%d) %s at %d\n" , rc, mdb_strerror(rc), i); exit(1); } \
    } \
    rc = mdb_put(txn, dbi, &lmdb_key, &data, 0);  if (rc) { fprintf(stderr, "mdb_put: (%d) %s at %d\n" , rc, mdb_strerror(rc), i); exit(1); }

#define TEST_PUT_POST() \
    rc = mdb_txn_commit(txn)

#define TEST_MIX_PRE()

#define TEST_MIX() \
    if (0 == i % 50) { \
        ((uint32_t *)sval1)[0] = key; \
        rc = mdb_txn_begin(env, NULL, 0, &txn       ); if (rc) { fprintf(stderr, "mdb_txn_begin(): (%d) %s at %d\n" , rc, mdb_strerror(rc), i); exit(1); } \
        rc = mdb_del      (txn, dbi, &lmdb_key, NULL); if (MDB_NOTFOUND == rc) { printf("ERROR: process #%u: key %u; MDB_NOTFOUND (1a)\n", process, key); exit(1); } \
        ((uint32_t *)sval1)[0] = key     ; \
        ((uint32_t *)sval2)[0] = key + 10; \
        /* need to re-init these after del or corruption occurs */ \
        lmdb_key.mv_size = sizeof(uint32_t); \
        lmdb_key.mv_data = sval1           ; \
        data.mv_size     = sizeof(uint32_t); \
        data.mv_data     = sval2           ; \
        rc = mdb_put       (txn, dbi, &lmdb_key, &data, 0); if (rc) { fprintf(stderr, "mdb_put(): (%d) %s at %d\n"       , rc, mdb_strerror(rc), i); exit(1); } \
        rc = mdb_txn_commit(txn                          ); if (rc) { fprintf(stderr, "mdb_txn_commit(): (%d) %s at %d\n", rc, mdb_strerror(rc), i); exit(1); } \
    } \
    else { \
        ((uint32_t *)sval1)[0] = key; \
        rc = mdb_txn_begin (env, NULL, MDB_RDONLY, &txn); if (rc) { fprintf(stderr, "mdb_txn_begin(): (%d) %s at %d\n" , rc, mdb_strerror(rc), i); exit(1); } \
        rc = mdb_get       (txn, dbi, &lmdb_key, &data ); if (rc) { fprintf(stderr, "mdb_get(): (%d) %s at %d\n"       , rc, mdb_strerror(rc), i); exit(1); } \
        rc = mdb_txn_commit(txn                        ); if (rc) { fprintf(stderr, "mdb_txn_commit(): (%d) %s at %d\n", rc, mdb_strerror(rc), i); exit(1); } \
        if (MDB_NOTFOUND == rc) { printf("ERROR: process #%u: key %u; MDB_NOTFOUND (1b)\n", process, key); exit(1); } \
        if (((uint32_t *)lmdb_key.mv_data)[0] != key) { printf("ERROR: process #%u: key i=%u not returned; got %u instead (1)\n", process, key, ((uint32_t *)lmdb_key.mv_data)[0]); exit(1); } \
        if (((uint32_t *)data.mv_data)[0] != key + 10) { printf("ERROR: process #%u: data i=%u not returned; got %u instead (1)\n", process, key + 10, ((uint32_t *)data.mv_data)[0]); exit(1); } \
    } \
    mix_counts[process] ++;

#define TEST_MIX_POST()

#define TEST_GET_PRE()

#define TEST_GET() \
    ((uint32_t *)sval1)[0] = key; \
    rc = mdb_txn_begin (env, NULL, MDB_RDONLY, &txn); if (rc) { fprintf(stderr, "mdb_txn_begin(): (%d) %s at %d\n" , rc, mdb_strerror(rc), i); exit(1); } \
    rc = mdb_get       (txn, dbi, &lmdb_key, &data ); if (rc) { fprintf(stderr, "mdb_get(): (%d) %s at %d\n"       , rc, mdb_strerror(rc), i); exit(1); } \
    rc = mdb_txn_commit(txn                        ); if (rc) { fprintf(stderr, "mdb_txn_commit(): (%d) %s at %d\n", rc, mdb_strerror(rc), i); exit(1); } \
    if (MDB_NOTFOUND == rc) { printf("ERROR: process #%u: key %u; MDB_NOTFOUND (2)\n", process, key); exit(1); } \
    if (((uint32_t *)lmdb_key.mv_data)[0] != key     ) { printf("ERROR: process #%u: key i=%u not returned; got %u instead (2)\n" , process, key     , ((uint32_t *)lmdb_key.mv_data)[0]); exit(1); } \
    if (((uint32_t *)data.mv_data)[0]     != key + 10) { printf("ERROR: process #%u: data i=%u not returned; got %u instead (2)\n", process, key + 10, ((uint32_t *)data.mv_data)[0]    ); exit(1); } \
    get_counts[process] ++;

#define TEST_GET_POST()

#define TEST_FINI()

#define TEST_FINI_MASTER() \
    char test_du_folder[256]; SHF_SNPRINTF(1, test_du_folder, "du -h -d 0 %s;  rm -rf %s", test_db_folder, test_db_folder); \
    fprintf(stderr, "test: DB size before deletion: %s\n", shf_backticks(test_du_folder));

#else

#define TEST_SHF 1

#define TEST_WHAT "SharedHashFile"

#define TEST_INIT() \
    char  test_db_name[256]; \
    char  test_db_folder[] = "/dev/shm"; \
    pid_t pid              = getpid(); \
    SHF_SNPRINTF(1, test_db_name, "test-shf-%05u", pid); \
                shf_init  (); \
    SHF * shf = shf_attach(test_db_folder, test_db_name, 1 /* delete upon process exit */); \
                shf_set_data_need_factor(250)

#define TEST_INIT_CHILD() \
    shf_debug_verbosity_less(); \
    shf = shf_attach_existing(test_db_folder, test_db_name)

#define TEST_PUT() \
    shf_make_hash       (SHF_CAST(const char *, &key), sizeof(key)); \
    shf_put_key_val(shf, SHF_CAST(const char *, &key), sizeof(key))

#define TEST_PUT_POST()

#define TEST_MIX_PRE()

#define TEST_MIX() \
    shf_make_hash(SHF_CAST(const char *, &key), sizeof(key)); \
    if (0 == i % 50) { \
        shf_del_key_val(shf); \
        shf_put_key_val(shf, SHF_CAST(const char *, &key), sizeof(key)); \
        mix_counts[process] ++; \
    } \
    else { \
        mix_counts[process] += shf_get_key_val_copy(shf); \
    }

#define TEST_MIX_POST()

#define TEST_GET_PRE()

#define TEST_GET() \
    shf_make_hash(SHF_CAST(const char *, &key), sizeof(key)); \
    get_counts[process] += shf_get_key_val_copy(shf)

#define TEST_GET_POST()

#define TEST_FINI() \
    shf_detach(shf);

#define TEST_FINI_MASTER() \
    fprintf(stderr, "test: DB size before deletion: %s\n", shf_del(shf));

#endif

static uint32_t
test_get_cpu_count(void)
{
    long cpu_count     = -1;
    long cpu_count_max = -1;

#ifndef _SC_NPROCESSORS_ONLN
    SHF_ASSERT(0, "Cannot count CPUs; sysconf(_SC_NPROCESSORS_ONLN) not available\n");
#endif
    cpu_count     = sysconf(_SC_NPROCESSORS_ONLN); SHF_ASSERT(cpu_count     >= 1, "%ld=sysconf(_SC_NPROCESSORS_ONLN): %u:  ", cpu_count, errno);
    cpu_count_max = sysconf(_SC_NPROCESSORS_CONF); SHF_ASSERT(cpu_count_max >= 1, "%ld=sysconf(_SC_NPROCESSORS_CONF): %u:  ", cpu_count, errno);
    SHF_DEBUG("- %ld of %ld CPUs available\n", cpu_count, cpu_count_max);
    return cpu_count;
} /* test_get_cpu_count() */

int main(void)
{
    plan_tests(1);

    if (getenv("SHF_PERFORMANCE_TEST_ENABLE") && atoi(getenv("SHF_PERFORMANCE_TEST_ENABLE"))) {
    }
    else {
        fprintf(stderr, "NOTE: prefix make with SHF_PERFORMANCE_TEST_ENABLE=1 ?\n");
        goto EARLY_EXIT;
    }

    uint32_t cpu_count_desired = getenv("SHF_PERFORMANCE_TEST_CPUS") ? atoi(getenv("SHF_PERFORMANCE_TEST_CPUS")) : 0;
    uint32_t test_keys_desired = getenv("SHF_PERFORMANCE_TEST_KEYS") ? atoi(getenv("SHF_PERFORMANCE_TEST_KEYS")) : 0;

    TEST_INIT();

#define TEST_MAX_PROCESSES (16)

#ifdef TEST_LMDB
             uint32_t   test_keys_default = 100 * 1000000; /* assume enough RAM is available for LMDB */
#else
             uint64_t   vfs_available_md  = shf_get_vfs_available(shf) / 1024 / 1024;
             uint32_t   test_keys_10m     = vfs_available_md / 436 * 10; /* 10M keys is about 436MB */
             uint32_t   test_keys_default = test_keys_10m > 100 ? 100 * 1000000 : test_keys_10m * 1000000; SHF_ASSERT(test_keys_default > 0, "ERROR: only %luMB available on /dev/shm but 10M keys takes at least 436MB for SharedHashFile", vfs_available_md);
#endif
             uint32_t   test_keys         = test_keys_desired ? test_keys_desired : test_keys_default;
             uint32_t   cpu_count         = cpu_count_desired ? cpu_count_desired : test_get_cpu_count();
             uint32_t   process;
             uint32_t   processes = cpu_count > TEST_MAX_PROCESSES ? TEST_MAX_PROCESSES : cpu_count;
             uint32_t   counts_old[TEST_MAX_PROCESSES] = { 0 };
    volatile uint32_t * put_counts = mmap(NULL, SHF_MOD_PAGE(TEST_MAX_PROCESSES*sizeof(uint32_t)), PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED | MAP_NORESERVE, -1, 0); SHF_ASSERT(MAP_FAILED != put_counts, "mmap(): %u: ", errno);
    volatile uint32_t * get_counts = mmap(NULL, SHF_MOD_PAGE(TEST_MAX_PROCESSES*sizeof(uint32_t)), PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED | MAP_NORESERVE, -1, 0); SHF_ASSERT(MAP_FAILED != get_counts, "mmap(): %u: ", errno);
    volatile uint32_t * mix_counts = mmap(NULL, SHF_MOD_PAGE(TEST_MAX_PROCESSES*sizeof(uint32_t)), PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED | MAP_NORESERVE, -1, 0); SHF_ASSERT(MAP_FAILED != mix_counts, "mmap(): %u: ", errno);
    volatile uint64_t * start_line = mmap(NULL, SHF_MOD_PAGE(                 3*sizeof(uint64_t)), PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED | MAP_NORESERVE, -1, 0); SHF_ASSERT(MAP_FAILED != mix_counts, "mmap(): %u: ", errno);
    SHF_ASSERT(sizeof(uint64_t) == sizeof(long), "INTERNAL: expected sizeof(uint64_t) == sizeof(long), but got %lu == %lu", sizeof(uint64_t), sizeof(long));
    start_line[0] = 0;
    start_line[1] = 0;
    start_line[2] = 0;
    for (process = 0; process < processes; process++) {
        pid_t fork_pid = fork();
        if (fork_pid == 0) {     /*child*/
            SHF_DEBUG("test process #%u with pid %5u\n", process, getpid());
            {
                long previous_long_value;
                SHF_UNUSE(previous_long_value);

                previous_long_value = InterlockedExchangeAdd((long volatile *) &start_line[0], 1);
                while (processes != start_line[0]) { SHF_YIELD(); }
                TEST_INIT_CHILD();
                for (uint32_t i = 0; i < (1 + (test_keys / processes)); i++) {
                    uint32_t key = test_keys / processes * process + i;
                    put_counts[process] ++;
                    TEST_PUT();
                }
                TEST_PUT_POST();
                TEST_MIX_PRE();
                usleep(2000000); /* 2 seconds */
                previous_long_value = InterlockedExchangeAdd((long volatile *) &start_line[1], 1);
                while (processes != start_line[1]) { SHF_YIELD(); }
                for (uint32_t i = 0; i < (1 + (test_keys / processes)); i++) {
                    uint32_t key = test_keys / processes * process + i;
                    TEST_MIX();
                }
                TEST_MIX_POST();
                TEST_GET_PRE();
                usleep(2000000); /* 2 seconds */
                previous_long_value = InterlockedExchangeAdd((long volatile *) &start_line[2], 1);
                while (processes != start_line[2]) { SHF_YIELD(); }
                for (uint32_t i = 0; i < (1 + (test_keys / processes)); i++) {
                    uint32_t key = test_keys / processes * process + i;
                    TEST_GET();
                }
                TEST_GET_POST();
                TEST_FINI();
                exit(0);
            }
            break;
        }
        else if (fork_pid > 0) { /*parent*/
            /* loop again */
        }
    }

    /* parent monitors & reports on forked children */
    uint32_t seconds            = 0;
    uint32_t key_total;
    uint32_t key_total_old      = 0;
    uint64_t tabs_mmaps_old     = 0;
    uint64_t tabs_mremaps_old   = 0;
    uint64_t tabs_shrunk_old    = 0;
    uint64_t tabs_parted_old    = 0;
    uint32_t message            = 0;
    const char * message_text = "PUT";
#ifdef SHF_DEBUG_VERSION
    uint64_t lock_conflicts_old = 0;
#endif
    char graph_100[] = "----------------------------------------------------------------------------------------------------";
    fprintf(stderr, "perf testing: " TEST_WHAT "\n");
    fprintf(stderr, "running tests on: via command: '%s'\n",               "cat /proc/cpuinfo | egrep 'model name' | head -n 1" );
    fprintf(stderr, "running tests on: `%s`\n"             , shf_backticks("cat /proc/cpuinfo | egrep 'model name' | head -n 1"));
    do {
        if (0 == seconds % 50) {
#ifdef SHF_DEBUG_VERSION
            fprintf(stderr, "-LOCKC ");
#endif
            fprintf(stderr, "-OP MMAP REMAP SHRK PART TOTAL ------PERCENT OPERATIONS PER PROCESS PER SECOND -OPS\n");
#ifdef SHF_DEBUG_VERSION
            fprintf(stderr, "------ ");
#endif
            fprintf(stderr, "--- -k/s --k/s --/s --/s M-OPS 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 -M/s\n");
        }
        seconds ++;
        // todo: add % system CPU time to per second summary line; why does put require so much system?
#ifdef SHF_DEBUG_VERSION
        {
            uint64_t lock_conflicts = 0;
            for (uint32_t win = 0; win < SHF_WINS_PER_SHF; win++) {
                lock_conflicts += shf->shf_mmap->wins[win].lock.conflicts;
            }
            fprintf(stderr, "%6lu ", lock_conflicts - lock_conflicts_old);
            lock_conflicts_old = lock_conflicts;
        }
#endif
        fprintf(stderr, "%s", message_text);
        {
            uint64_t tabs_mmaps   = 0;
            uint64_t tabs_mremaps = 0;
            uint64_t tabs_shrunk  = 0;
            uint64_t tabs_parted  = 0;
#ifdef TEST_SHF
            for (uint32_t win = 0; win < SHF_WINS_PER_SHF; win++) {
                tabs_mmaps   += shf->shf_mmap->wins[win].tabs_mmaps  ;
                tabs_mremaps += shf->shf_mmap->wins[win].tabs_mremaps;
                tabs_shrunk  += shf->shf_mmap->wins[win].tabs_shrunk ;
                tabs_parted  += shf->shf_mmap->wins[win].tabs_parted ;
            }
#endif
            fprintf(stderr, "%5.1f %5.1f %4lu %4lu",
                (tabs_mmaps   - tabs_mmaps_old  ) / 1000.0,
                (tabs_mremaps - tabs_mremaps_old) / 1000.0,
                (tabs_shrunk  - tabs_shrunk_old )         ,
                (tabs_parted  - tabs_parted_old )         );
            tabs_mmaps_old   = tabs_mmaps  ;
            tabs_mremaps_old = tabs_mremaps;
            tabs_shrunk_old  = tabs_shrunk ;
            tabs_parted_old  = tabs_parted ;
        }
        {
            key_total = 0;
            uint32_t key_total_this_second = 0;
            for (process = 0; process < TEST_MAX_PROCESSES; process++) {
                key_total             += put_counts[process] + get_counts[process] + mix_counts[process];
                key_total_this_second += put_counts[process] + get_counts[process] + mix_counts[process] - counts_old[process];
            }
            fprintf(stderr, " %5.1f", key_total / 1000.0 / 1000.0);
            for (process = 0; process < TEST_MAX_PROCESSES; process++) {
                fprintf(stderr, "%3.0f", (put_counts[process] + get_counts[process] + mix_counts[process] - counts_old[process]) * 100.0 / (0 == key_total_this_second ? 1 : key_total_this_second));
                counts_old[process] = put_counts[process] + get_counts[process] + mix_counts[process];
            }
            uint32_t key_total_per_second = key_total - key_total_old;
            fprintf(stderr, "%5.1f %s\n", key_total_per_second / 1000.0 / 1000.0, &graph_100[100 - (key_total_per_second / 750000)]);
            if      (0 == message && key_total >= (1 * test_keys)) { message ++; message_text = "MIX"; }
            else if (1 == message && key_total >= (2 * test_keys)) { message ++; message_text = "GET"; }
            key_total_old = key_total;
        }
        usleep(1000000); /* one second */
    } while (key_total < (3 * test_keys));
    fprintf(stderr, "* MIX is 2%% (%u) del/put, 98%% (%u) get\n", test_keys * 2 / 100, test_keys * 98 / 100);

    TEST_FINI_MASTER();

EARLY_EXIT:;

    ok(1, "test still alive");
    return exit_status();
} /* main() */
