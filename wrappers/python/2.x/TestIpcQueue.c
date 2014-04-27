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

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h> /* for printf() */
#include <sys/types.h> /* for getpid() */
#include <sys/wait.h> /* for waitpid() */
#include <unistd.h>
#include <linux/limits.h> /* for PATH_MAX */
#include <errno.h> /* for errno */
#include <stdlib.h> /* for exit() */
#include <sys/socket.h>
#include <sys/un.h> /* for struct sockaddr_un */
#include <sys/time.h> /* for gettimeofday() */
#include <string.h> /* for memset() */

#include <shf.private.h>
#include <shf.h>
#include "shf.queue.h"
#include "tap.h"

static pid_t
test_exec_child(
    const char  * child_path      ,
    const char  * child_file      ,
    const char  * child_argument_1,
          char  * child_argument_2)
{
    pid_t pid_child = fork(); SHF_ASSERT(pid_child >= 0, "fork(): %d: ", errno);

    if(0 == pid_child) {
        execl(child_path, child_file, child_argument_1, child_argument_2, NULL);
        /* should never come here unless error! */
        SHF_ASSERT(0, "execl(): %d: ", errno);
   }
   else {
        SHF_DEBUG("parent forked child child with pid %u\n", pid_child);
   }
   return pid_child;
} /* test_exec_child() */

static double
test_dummy(void)
{
    return 1;
} /* test_dummy() */

int
main(int argc, char **argv) {
    SHF_UNUSE(argc);
    SHF_UNUSE(argv);

    SHF_ASSERT(argc >= 2, "ERROR: please supply an argument; c2js, c2c, or 4c");
    SHF_ASSERT((0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2js")))
    ||         (0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2c" )))
    ||         (0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("4c"  ))), "ERROR: please supply an argument; c2js, c2c, or 4c; got: '%s'", argv[1]);

         if (0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2js"))) { plan_tests( 5); }
    else if (0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2c" ))) { plan_tests(11); }
    else if (0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("4c"  ))) { plan_tests( 8); }

    pid_t pid = getpid();
    SHF_DEBUG("pid %u started\n", pid);

    if (0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2js"))) { /* just for fun, test C to C call speed; useful for comparing to V8 to C call speed */
        double test_start_time = shf_get_time_in_seconds();
        double test_iterations = 0;
        do {
            test_iterations += test_dummy();
        } while (test_iterations < 10000000);
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "   c2*: called  expected number to dummy function  // estimate %.0f keys per second", test_iterations / test_elapsed_time);
    }

    char  test_shf_name[256];
    char  test_shf_folder[] = "/dev/shm";
    SHF_SNPRINTF(1, test_shf_name, "test-%05u-ipc-queue", pid);

    SHF      * shf;
    uint32_t   uid;
    uint32_t   test_keys = 100000;
    uint32_t   test_queue_item_data_size = 4096;

    if (0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("4c"))) {
        SHF_DEBUG("'4c' mode; behaving as client\n");
        SHF_ASSERT(argc == 3, "ERROR: please supply arguments; 4c <name of shf>");

              shf_debug_verbosity_less();
              shf_init                ();
        shf = shf_attach_existing     (test_shf_folder, argv[2]); ok(NULL != shf, "    4c: shf_attach_existing() works for existing file as expected");

        uint32_t uid_queue_unused  =  shf_queue_get_name(shf, SHF_CONST_STR_AND_SIZE("queue-unused"));
        uint32_t uid_queue_a2b     =  shf_queue_get_name(shf, SHF_CONST_STR_AND_SIZE("queue-a2b"   ));
        uint32_t uid_queue_b2a     =  shf_queue_get_name(shf, SHF_CONST_STR_AND_SIZE("queue-b2a"   ));
        ok(      uid_queue_unused != SHF_UID_NONE, "    4c: shf_queue_get_name('queue-unused') returned uid as expected");
        ok(      uid_queue_a2b    != SHF_UID_NONE, "    4c: shf_queue_get_name('queue-a2b'   ) returned uid as expected");
        ok(      uid_queue_b2a    != SHF_UID_NONE, "    4c: shf_queue_get_name('queue-b2a'   ) returned uid as expected");

        {
            double   test_start_time = 0;
            uint32_t test_pull_items = 0;
            do {
                while(NULL != shf_queue_pull_tail(shf, uid_queue_a2b         )) {
                              shf_queue_push_head(shf, uid_queue_b2a, shf_uid);
                              test_pull_items ++;
                              if (test_keys == test_pull_items) { test_start_time = shf_get_time_in_seconds(); } /* start timing once test_keys have done 1st loop */
                }
            } while (test_pull_items < 500000);
            double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
            ok(1, "    4c: moved   expected number of new queue items // estimate %.0f keys per second", test_pull_items / test_elapsed_time);
        }

        {
                                            SHF_MAKE_HASH       ("lock");
            SHF_LOCK         * lock  =      shf_get_key_val_addr(shf   );
            ok(                lock != NULL                             , "    4c: got lock value address as expected");
                                            SHF_MAKE_HASH       ("line");
            volatile uint8_t * line  =      shf_get_key_val_addr(shf   );
            ok(                line != NULL                             , "    4c: got line value address as expected");

            __sync_fetch_and_add_8(line, 1); /* atomic increment */
            while (2 != *line) { SHF_CPU_PAUSE(); }

            double test_start_time = shf_get_time_in_seconds();
            double test_lock_iterations = 0;
            do {
                SHF_LOCK_WRITER(lock);
                SHF_UNLOCK_WRITER(lock);
                test_lock_iterations ++;
            } while (test_lock_iterations < 2000000);
            double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
            ok(1, "    4c: rw lock expected number of times           // estimate %.0f locks per second; with contention", test_lock_iterations / test_elapsed_time);
        }

        goto EARLY_OUT;
    } /* 4c */

          shf_debug_verbosity_less();
          shf_init                ();
          shf_set_data_need_factor(250);
    shf = shf_attach              (test_shf_folder, test_shf_name); ok(NULL != shf, "   c2*: shf_attach()          works for non-existing file as expected");

    {
        SHF_LOCK lock;
        uint8_t  line = 0; /* note: this memory does not get shared, but the memory in the hash key,value gets shared */
        if (0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2c"))) {
            memset(&lock, 0, sizeof(lock));
                      SHF_MAKE_HASH  (                            "lock"              );
               uid =  shf_put_key_val(shf, SHF_CAST(const char *, &lock), sizeof(lock));
            ok(uid != SHF_UID_NONE                                                     , "   c2*: put lock in value as expected");
                      SHF_MAKE_HASH  (                            "line"              );
               uid =  shf_put_key_val(shf, SHF_CAST(const char *, &line), sizeof(line));
            ok(uid != SHF_UID_NONE                                                     , "   c2*: put line in value as expected");
        }
    }

    uint32_t uid_queue_unused = shf_queue_new_name(shf, SHF_CONST_STR_AND_SIZE("queue-unused"));
    uint32_t uid_queue_a2b    = shf_queue_new_name(shf, SHF_CONST_STR_AND_SIZE("queue-a2b"   ));
    uint32_t uid_queue_b2a    = shf_queue_new_name(shf, SHF_CONST_STR_AND_SIZE("queue-b2a"   ));

    {
        double test_start_time = shf_get_time_in_seconds();
        for (uint32_t i = 0; i < test_keys; i++) {
            uid = shf_queue_new_item (shf, test_queue_item_data_size);
                  shf_queue_push_head(shf, uid_queue_unused, uid);
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "   c2*: created expected number of new queue items // estimate %.0f keys per second", test_keys / test_elapsed_time);
    }

    {
        double   test_start_time = shf_get_time_in_seconds();
        uint32_t test_pull_items = 0;
        while(NULL != shf_queue_pull_tail(shf, uid_queue_unused         )) {
                      shf_queue_push_head(shf, uid_queue_a2b   , shf_uid);
                      snprintf(shf_item_addr, shf_item_addr_len, "%08u", test_pull_items);
                      test_pull_items ++;
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(test_keys == test_pull_items, "   c2*: moved   expected number of new queue items // estimate %.0f keys per second", test_keys / test_elapsed_time);
    }

    pid_t child_pid = 0;
    if      (0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2js"))) { child_pid = test_exec_child(shf_backticks("which nodejs"  ), "nodejs"      , "TestIpcQueue.js", test_shf_name); }
    else if (0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2c" ))) { child_pid = test_exec_child(              "./TestIpcQueue" , "TestIpcQueue", "4c"             , test_shf_name); }
    else                                                           { SHF_ASSERT(0, "ERROR: should never get here!"); }

    {
        double   test_start_time = 0;
        uint32_t test_pull_items = 0;
        do {
            while(NULL != shf_queue_pull_tail(shf, uid_queue_b2a         )) {
                          shf_queue_push_head(shf, uid_queue_a2b, shf_uid);
                          test_pull_items ++;
                          if (test_keys == test_pull_items) { test_start_time = shf_get_time_in_seconds(); } /* start timing once test_keys have done 1st loop */
            }
            if (0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2js" ))) { /* the rw spin locks are fair but don't create unnecessary contention for javascript client */
                usleep(1000); /* 1/1000th of a second */
            }
        } while (test_pull_items < 500000);
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "   c2*: moved   expected number of new queue items // estimate %.0f keys per second", test_pull_items / test_elapsed_time);
    }

    if (0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2c"))) {
                                        SHF_MAKE_HASH       ("lock");
        SHF_LOCK         * lock  =      shf_get_key_val_addr(shf   );
        ok(                lock != NULL                             , "   c2*: got lock value address as expected");
                                        SHF_MAKE_HASH       ("line");
        volatile uint8_t * line  =      shf_get_key_val_addr(shf   );
        ok(                line != NULL                             , "   c2*: got line value address as expected");

        __sync_fetch_and_add_8(line, 1); /* atomic increment */
        while (2 != *line) { SHF_CPU_PAUSE(); }

        double test_start_time = shf_get_time_in_seconds();
        double test_lock_iterations = 0;
        do {
            SHF_LOCK_WRITER(lock);
            SHF_UNLOCK_WRITER(lock);
            test_lock_iterations ++;
        } while (test_lock_iterations < 2000000);
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "   c2*: rw lock expected number of times           // estimate %.0f locks per second; with contention", test_lock_iterations / test_elapsed_time);

        SHF_LOCK lock_without_contention;
        memset(&lock_without_contention, 0, sizeof(lock_without_contention));
        test_start_time = shf_get_time_in_seconds();
        test_lock_iterations = 0;
        do {
            SHF_LOCK_WRITER(&lock_without_contention);
            SHF_UNLOCK_WRITER(&lock_without_contention);
            test_lock_iterations ++;
        } while (test_lock_iterations < 2000000);
        test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "   c2*: rw lock expected number of times           // estimate %.0f locks per second; without contention", test_lock_iterations / test_elapsed_time);

        test_start_time = shf_get_time_in_seconds();
        test_lock_iterations = 0;
        do {
            test_lock_iterations ++;
        } while (test_lock_iterations < 2000000);
        test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "   c2*: rw lock expected number of times           // estimate %.0f locks per second; without lock", test_lock_iterations / test_elapsed_time);
    }

    shf_debug_verbosity_more();

    int status;
    waitpid(child_pid, &status, 0);

EARLY_OUT:;

    SHF_DEBUG("ending\n");
    return exit_status();
} /* main() */
