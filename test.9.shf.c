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
#include <string.h>   /* for memcmp() */

#include "shf.private.h"
#include "shf.h"
#include "tap.h"

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
    plan_tests(21);

    uint32_t cpu_count = test_get_cpu_count();

    char test_shf_folder[] = "/dev/shm";
    pid_t pid = getpid();
    char test_shf_name[256];
    int result = snprintf(test_shf_name, sizeof(test_shf_name), "test-%05u", pid);
    SHF_ASSERT(result >= 0                                   , "%d=snprintf() too small!", result);
    SHF_ASSERT(result <= SHF_CAST(int, sizeof(test_shf_name)), "%d=snprintf() too big!"  , result);

    shf_init();

    SHF * shf = shf_attach_existing(test_shf_folder, test_shf_name); ok(NULL == shf, "c: shf_attach_existing() fails for non-existing file as expected");
          shf = shf_attach         (test_shf_folder, test_shf_name); ok(NULL != shf, "c: shf_attach()          works for non-existing file as expected");

                       SHF_MAKE_HASH       (         "key"    );
    ok(0            == shf_get_copy_via_key(shf               ), "c: shf_get_copy_via_key() could not find unput key as expected");
    ok(0            == shf_del_key         (shf               ), "c: shf_del_key()          could not find unput key as expected");
    ok(SHF_UID_NONE != shf_put_val         (shf    , "val" , 3), "c: shf_put_val()                           put key as expected");
    ok(1            == shf_get_copy_via_key(shf               ), "c: shf_get_copy_via_key() could     find   put key as expected");
    ok(3            == shf_val_len                             , "c: shf_val_len                                     as expected");
    ok(0            == memcmp              (shf_val, "val" , 3), "c: shf_val                                         as expected");
    ok(1            == shf_del_key         (shf               ), "c: shf_del_key()          could     find   put key as expected");
    ok(0            == shf_get_copy_via_key(shf               ), "c: shf_get_copy_via_key() could not find   del key as expected");
    ok(0            == shf_del_key         (shf               ), "c: shf_del_key()          could not find   del key as expected");
    ok(SHF_UID_NONE != shf_put_val         (shf    , "val2", 4), "c: shf_put_val()                         reput key as expected");
    ok(1            == shf_get_copy_via_key(shf               ), "c: shf_get_copy_via_key() could     find reput key as expected");
    ok(4            == shf_val_len                             , "c: shf_val_len                                     as expected");
    ok(0            == memcmp              (shf_val, "val2", 4), "c: shf_val                                         as expected");

    uint32_t test_keys = 250000;
    {
        shf_debug_verbosity_less();
        double test_start_time = shf_get_time_in_seconds();
        for (uint32_t i = 0; i < test_keys; i++) {
            shf_make_hash(SHF_CAST(const char *, &i), sizeof(i));
            shf_put_val(shf, SHF_CAST(const char *, &i), sizeof(i));
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "c: put expected number of              keys // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf_debug_verbosity_more();
    }

    {
        shf_debug_verbosity_less();
        double test_start_time = shf_get_time_in_seconds();
        uint32_t keys_found = 0;
        for (uint32_t i = (test_keys * 2); i < (test_keys * 3); i++) {
            shf_make_hash(SHF_CAST(const char *, &i), sizeof(i));
            keys_found += shf_get_copy_via_key(shf);
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(0 == keys_found, "c: got expected number of non-existing keys // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf_debug_verbosity_more();
    }

    {
        shf_debug_verbosity_less();
        double test_start_time = shf_get_time_in_seconds();
        uint32_t keys_found = 0;
        for (uint32_t i = 0; i < test_keys; i++) {
            shf_make_hash(SHF_CAST(const char *, &i), sizeof(i));
            keys_found += shf_get_copy_via_key(shf);
            SHF_ASSERT(sizeof(i) == shf_val_len, "INTERNAL: expected shf_val_len to be %lu but got %u\n", sizeof(i), shf_val_len);
            SHF_ASSERT(0 == memcmp(&i, shf_val, sizeof(i)), "INTERNAL: unexpected shf_val\n");
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(test_keys == keys_found, "c: got expected number of     existing keys // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf_debug_verbosity_more();
    }

    ok(0 == shf_debug_get_bytes_marked_as_deleted(shf), "c: graceful growth cleans up after itself as expected");

    {
        shf_debug_verbosity_less();
        double test_start_time = shf_get_time_in_seconds();
        uint32_t keys_found = 0;
        for (uint32_t i = 0; i < test_keys; i++) {
            shf_make_hash(SHF_CAST(const char *, &i), sizeof(i));
            keys_found += shf_del_key(shf);
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(test_keys == keys_found, "c: del expected number of     existing keys // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf_debug_verbosity_more();
    }

    ok(0 != shf_debug_get_bytes_marked_as_deleted(shf), "c: del does not    clean  up after itself as expected");

    if (getenv("SHF_ENABLE_PERFORMANCE_TEST") && atoi(getenv("SHF_ENABLE_PERFORMANCE_TEST"))) {
    }
    else {
        fprintf(stderr, "NOTE: prefix make with SHF_ENABLE_PERFORMANCE_TEST=1 ?\n");
        goto EARLY_EXIT;
    }

#define TEST_MAX_PROCESSES (16)

    uint32_t process;
    uint32_t processes = cpu_count > TEST_MAX_PROCESSES ? TEST_MAX_PROCESSES : cpu_count;
    uint32_t counts_old[TEST_MAX_PROCESSES] = { 0 };
    volatile uint32_t * put_counts = mmap(NULL, SHF_MOD_PAGE(TEST_MAX_PROCESSES*sizeof(uint32_t)), PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED | MAP_NORESERVE, -1, 0); SHF_ASSERT(MAP_FAILED != put_counts, "mmap(): %u: ", errno);
    volatile uint32_t * get_counts = mmap(NULL, SHF_MOD_PAGE(TEST_MAX_PROCESSES*sizeof(uint32_t)), PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED | MAP_NORESERVE, -1, 0); SHF_ASSERT(MAP_FAILED != get_counts, "mmap(): %u: ", errno);
    volatile uint32_t * mix_counts = mmap(NULL, SHF_MOD_PAGE(TEST_MAX_PROCESSES*sizeof(uint32_t)), PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED | MAP_NORESERVE, -1, 0); SHF_ASSERT(MAP_FAILED != mix_counts, "mmap(): %u: ", errno);
    volatile uint64_t * start_line = mmap(NULL, SHF_MOD_PAGE(                 3*sizeof(uint64_t)), PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED | MAP_NORESERVE, -1, 0); SHF_ASSERT(MAP_FAILED != mix_counts, "mmap(): %u: ", errno);
    SHF_ASSERT(sizeof(uint64_t) == sizeof(long), "INTERNAL: expected sizeof(uint64_t) == sizeof(long), but got %lu == %lu", sizeof(uint64_t), sizeof(long));
    start_line[0] = 0;
    start_line[1] = 0;
    start_line[3] = 0;
    test_keys = 50 * 1000000;
    for (process = 0; process < processes; process++) {
        pid_t fork_pid = fork();
        if (fork_pid == 0) {     /*child*/
            shf_debug_verbosity_less();
            shf = shf_attach_existing(test_shf_folder, test_shf_name);
            SHF_DEBUG("test process #%u with pid %5u\n", process, getpid());
            {
                long previous_long_value;
                SHF_UNUSE(previous_long_value);

                previous_long_value = InterlockedExchangeAdd((long volatile *) &start_line[0], 1);
                while (processes != start_line[0]) { SHF_YIELD(); }
                for (uint32_t i = 0; i < (1 + (test_keys / processes)); i++) {
                    uint32_t key = test_keys / processes * process + i;
                    put_counts[process] ++;
                    shf_make_hash(SHF_CAST(const char *, &key), sizeof(key));
                    shf_put_val(shf, SHF_CAST(const char *, &key), sizeof(key));
                }
                usleep(2000000); /* one second */
                previous_long_value = InterlockedExchangeAdd((long volatile *) &start_line[1], 1);
                while (processes != start_line[1]) { SHF_YIELD(); }
                for (uint32_t i = 0; i < (1 + (test_keys / processes)); i++) {
                    uint32_t key = test_keys / processes * process + i;
                    shf_make_hash(SHF_CAST(const char *, &key), sizeof(key));
                    if (0 == i % 50) {
                        shf_del_key(shf);
                        shf_put_val(shf, SHF_CAST(const char *, &key), sizeof(key));
                        mix_counts[process] ++;
                    }
                    else {
                        mix_counts[process] += shf_get_copy_via_key(shf);
                    }
                }
                usleep(2000000); /* one second */
                previous_long_value = InterlockedExchangeAdd((long volatile *) &start_line[2], 1);
                while (processes != start_line[2]) { SHF_YIELD(); }
                for (uint32_t i = 0; i < (1 + (test_keys / processes)); i++) {
                    uint32_t key = test_keys / processes * process + i;
                    shf_make_hash(SHF_CAST(const char *, &key), sizeof(key));
                    get_counts[process] += shf_get_copy_via_key(shf);
                }
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
            for (uint32_t win = 0; win < SHF_WINS_PER_SHF; win++) {
                tabs_mmaps   += shf->shf_mmap->wins[win].tabs_mmaps  ;
                tabs_mremaps += shf->shf_mmap->wins[win].tabs_mremaps;
                tabs_shrunk  += shf->shf_mmap->wins[win].tabs_shrunk ;
                tabs_parted  += shf->shf_mmap->wins[win].tabs_parted ;
            }
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

EARLY_EXIT:;

    return exit_status();
} /* main() */
