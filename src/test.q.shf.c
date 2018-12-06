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
#include <stdio.h>        /* for printf() */
#include <sys/types.h>    /* for getpid() */
#include <sys/wait.h>     /* for waitpid() */
#include <unistd.h>
#include <linux/limits.h> /* for PATH_MAX */
#include <errno.h>        /* for errno */
#include <stdlib.h>       /* for exit() */
#include <sys/socket.h>
#include <sys/un.h>       /* for struct sockaddr_un */
#include <sys/time.h>     /* for gettimeofday() */
#include <string.h>       /* for memset() */
#include <locale.h>       /* for setlocale() */

#include <shf.private.h>
#include <shf.h>
#include "tap.h"

static double
test_dummy(void)
{
    return 1;
} /* test_dummy() */

int
main(int argc, char **argv) {
    const char * mode = NULL;

    SHF_ASSERT(NULL != setlocale(LC_NUMERIC, ""), "setlocale(): %u: ", errno);

    if (argc >= 2) {
        SHF_ASSERT((0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2js")))
        ||         (0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2c" )))
        ||         (0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("4c"  ))), "ERROR: please supply an argument; c2js, c2py, c2c, or 4c; got: '%s'", argv[1]);
    }

         if (argc > 1 && 0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2js"))) { plan_tests( 6); mode = strdup(argv[1]); }
    else if (argc > 1 && 0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2py"))) { plan_tests( 5); mode = strdup(argv[1]); }
    else if (argc > 1 && 0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("c2c" ))) { plan_tests( 9); mode = strdup(argv[1]); }
    else if (argc > 1 && 0 == memcmp(argv[1], SHF_CONST_STR_AND_SIZE("4c"  ))) { plan_tests( 7); mode = strdup(argv[1]); }
    else                                                                       { plan_tests(10); mode = "c2c"          ; } /* default if no arguments */

    pid_t pid = getpid();
    SHF_DEBUG("pid %u started; mode is '%s'\n", pid, mode);

    if (0 == memcmp(mode, SHF_CONST_STR_AND_SIZE("c2js"))) { /* just for fun, test C to C call speed; useful for comparing to V8 to C call speed */
        double test_start_time = shf_get_time_in_seconds();
        double test_iterations = 0;
        do {
            test_iterations += test_dummy();
        } while (test_iterations < 10000000);
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "   c2*: called  expected number to dummy function  // estimate %'.0f calls per second", test_iterations / test_elapsed_time);
    }

    char  test_shf_name[256];
    char  test_shf_folder[] = "/dev/shm";
    SHF_SNPRINTF(1, test_shf_name, "test-%05u-ipc-queue", pid);

    SHF      * shf;
    uint32_t   uid;
    uint32_t   test_keys = 100000;

    if (0 == memcmp(mode, SHF_CONST_STR_AND_SIZE("4c"))) {
        SHF_ASSERT(argc == 3, "ERROR: please supply arguments; 4c <name of shf>");

              shf_debug_verbosity_less();
              shf_init                ();
        shf = shf_attach_existing     (test_shf_folder, argv[2]); ok(NULL != shf, "    4c: shf_attach_existing() works for existing file as expected");
              shf_log_attach_existing (shf                     );

        shf_debug_verbosity_more(); SHF_DEBUG("'4c' mode; behaving as client\n"); shf_debug_verbosity_less();

        char     * test_q_items_addr  = shf_q_get(shf); SHF_UNUSE(test_q_items_addr); /* todo: this test doesn't actually manipulate the item itself */
        uint32_t   test_qid_free      = shf_q_get_name(shf, SHF_CONST_STR_AND_SIZE("qid-free"));
        uint32_t   test_qid_a2b       = shf_q_get_name(shf, SHF_CONST_STR_AND_SIZE("qid-a2b" ));
        uint32_t   test_qid_b2a       = shf_q_get_name(shf, SHF_CONST_STR_AND_SIZE("qid-b2a" ));
        ok(        test_qid_free     != SHF_QID_NONE, "    4c: shf_q_get_name('qid-free') returned qid as expected");
        ok(        test_qid_a2b      != SHF_QID_NONE, "    4c: shf_q_get_name('qid-a2b' ) returned qid as expected");
        ok(        test_qid_b2a      != SHF_QID_NONE, "    4c: shf_q_get_name('qid-b2a' ) returned qid as expected");

        shf_race_start(shf, SHF_CONST_STR_AND_SIZE("test-q-race-line"), 2);
        shf_debug_verbosity_more(); SHF_DEBUG("testing process b IPC queue a2b --> b2a speed\n"); shf_debug_verbosity_less();
        {
            uint32_t test_pull_items = 0;
            double   test_start_time = shf_get_time_in_seconds();
            shf_qiid = SHF_QIID_NONE;
            while(1) {
                while(SHF_QIID_NONE != shf_q_push_head_pull_tail(shf, test_qid_b2a, shf_qiid, test_qid_a2b)) {
                                       test_pull_items ++;
                }
                if (test_pull_items >= 1000000) { goto FINISH_LINE_4C; }
            }
FINISH_LINE_4C:;
            double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
            ok(1, "    4c: moved   expected number of new queue items // estimate %'.0f q items per second with contention", test_pull_items / test_elapsed_time);
        }

        {
            shf_debug_verbosity_more(); SHF_DEBUG("testing process b IPC lock speed\n"); shf_debug_verbosity_less();

                                    SHF_MAKE_HASH       ("lock");
            SHF_LOCK * lock  =      shf_get_key_val_addr(shf   );
            ok(        lock != NULL                             , "    4c: got lock value address as expected");

            shf_race_start(shf, SHF_CONST_STR_AND_SIZE("test-lock-race-line"), 2);
            double test_start_time = shf_get_time_in_seconds();
            double test_lock_iterations = 0;
            do {
                SHF_LOCK_WRITER(lock);
                SHF_UNLOCK_WRITER(lock);
                test_lock_iterations ++;
            } while (test_lock_iterations < 2000000);
            double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
            ok(1, "    4c: rw lock expected number of times           // estimate %'.0f locks per second; with contention", test_lock_iterations / test_elapsed_time);
        }

        shf_debug_verbosity_more(); SHF_DEBUG("ending child\n"); shf_debug_verbosity_less();

        shf_detach(shf);

        goto EARLY_OUT;
    } /* 4c */

          shf_debug_verbosity_less();
          shf_init                ();
          shf_set_data_need_factor(1);
    shf = shf_attach              (test_shf_folder, test_shf_name, 1 /* delete upon process exit */); ok(NULL != shf, "   c2*: shf_attach()          works for non-existing file as expected");
          shf_log_thread_new      (shf, 0 /* use default log buffer size */, STDOUT_FILENO);

    {
        shf_race_init(shf, SHF_CONST_STR_AND_SIZE("test-q-race-line"   ));
        shf_race_init(shf, SHF_CONST_STR_AND_SIZE("test-lock-race-line"));

        if (0 == memcmp(mode, SHF_CONST_STR_AND_SIZE("c2c"))) {
                      SHF_MAKE_HASH  (                     "lock");
               uid =  shf_put_key_val(shf, NULL, sizeof(SHF_LOCK));
            ok(uid != SHF_UID_NONE                                , "   c2*: put lock in value as expected");
        }
    }

    uint32_t test_qs          = 3;
    uint32_t test_q_items     = 100000;
    uint32_t test_q_item_size = 4096;
    ok(      NULL            != shf_q_new     (shf, test_qs, test_q_items, test_q_item_size, 1000), "   c2*: shf_q_new() returned as expected");
    uint32_t test_qid_free    = shf_q_new_name(shf, SHF_CONST_STR_AND_SIZE("qid-free")           );
    uint32_t test_qid_a2b     = shf_q_new_name(shf, SHF_CONST_STR_AND_SIZE("qid-a2b" )           );
    uint32_t test_qid_b2a     = shf_q_new_name(shf, SHF_CONST_STR_AND_SIZE("qid-b2a" )           );

    {
        double   test_start_time = shf_get_time_in_seconds();
        uint32_t test_pull_items = 0;
        while(SHF_QIID_NONE != shf_q_pull_tail(shf, test_qid_free          )) {
                               shf_q_push_head(shf, test_qid_a2b , shf_qiid);
                               test_pull_items ++;
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(test_q_items == test_pull_items, "   c2*: moved   expected number of new queue items // estimate %'.0f q items per second without contention", test_keys / test_elapsed_time);
    }

    pid_t child_pid = 0;
    if      (0 == memcmp(mode, SHF_CONST_STR_AND_SIZE("c2js"))) { child_pid = shf_exec_child(shf_backticks("which node || which nodejs"), shf_backticks("which node || which nodejs"), "TestIpcQueue.js", test_shf_name); } /* note: nodejs deprecated */
    else if (0 == memcmp(mode, SHF_CONST_STR_AND_SIZE("c2py"))) { child_pid = shf_exec_child(shf_backticks("which python"              ), "python"                                   , "TestIpcQueue.py", test_shf_name); } /* note: python never implemented */
    else if (0 == memcmp(mode, SHF_CONST_STR_AND_SIZE("c2c" ))) { child_pid = shf_exec_child(shf_backticks("which test.q.shf.t"        ), "test.q.shf.t"                             , "4c"             , test_shf_name); }
    else                                                        { SHF_ASSERT(0, "ERROR: should never get here!"); }

    shf_race_start(shf, SHF_CONST_STR_AND_SIZE("test-q-race-line"), 2);
    shf_debug_verbosity_more(); SHF_DEBUG("testing process a IPC queue b2a --> a2b speed\n"); shf_debug_verbosity_less();
    {
        double   test_start_time = shf_get_time_in_seconds();
        uint32_t test_pull_items = 0;
        shf_qiid = SHF_QIID_NONE;
        while (1) {
            while(SHF_QIID_NONE != shf_q_push_head_pull_tail(shf, test_qid_a2b, shf_qiid, test_qid_b2a)) {
                                   test_pull_items ++;
                if (1000000     == test_pull_items) { shf_q_push_head_pull_tail(shf, test_qid_a2b, shf_qiid, test_qid_b2a); goto FINISH_LINE_C2; }
            }
            if ((0 == memcmp(mode, SHF_CONST_STR_AND_SIZE("c2js")))
            ||  (0 == memcmp(mode, SHF_CONST_STR_AND_SIZE("c2py")))) { /* the rw spin locks are fair but don't create unnecessary contention for javascript or python client */
                usleep(1000); /* 1/1000th of a second */
            }
        }
FINISH_LINE_C2:;
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        usleep(3000); /* hack: wait 3/1000th of a second so that the oks do not conflict */
        ok(1, "   c2*: moved   expected number of new queue items // estimate %'.0f q items per second with contention", test_pull_items / test_elapsed_time);
    }

    if (0 == memcmp(mode, SHF_CONST_STR_AND_SIZE("c2c"))) {
        shf_debug_verbosity_more(); SHF_DEBUG("testing process a IPC lock speed\n"); shf_debug_verbosity_less();

                                SHF_MAKE_HASH       ("lock");
        SHF_LOCK * lock  =      shf_get_key_val_addr(shf   );
        ok(        lock != NULL                             , "   c2*: got lock value address as expected");

        shf_race_start(shf, SHF_CONST_STR_AND_SIZE("test-lock-race-line"), 2);
        double test_start_time = shf_get_time_in_seconds();
        double test_lock_iterations = 0;
        do {
            SHF_LOCK_WRITER(lock);
            SHF_UNLOCK_WRITER(lock);
            test_lock_iterations ++;
        } while (test_lock_iterations < 2000000);
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "   c2*: rw lock expected number of times           // estimate %'.0f locks per second; with contention", test_lock_iterations / test_elapsed_time);

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
        ok(1, "   c2*: rw lock expected number of times           // estimate %'.0f locks per second; without contention", test_lock_iterations / test_elapsed_time);

        test_start_time = shf_get_time_in_seconds();
        test_lock_iterations = 0;
        do {
            test_lock_iterations ++;
        } while (test_lock_iterations < 2000000);
        test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "   c2*: rw lock expected number of times           // estimate %'.0f locks per second; without lock, just loop", test_lock_iterations / test_elapsed_time);
    }

    shf_debug_verbosity_more();

    int status;
    waitpid(child_pid, &status, 0);

    ok(1, "   c2*: test still alive");

    SHF_DEBUG("ending parent\n");

    SHF_PLAIN("test: shf size before deletion: %s\n", shf_del(shf));

EARLY_OUT:;

    return exit_status();
} /* main() */
