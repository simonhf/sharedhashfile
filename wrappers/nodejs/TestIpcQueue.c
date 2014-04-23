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
exec_nodejs(char * nodejs_argument) {
    pid_t pid_child = fork(); SHF_ASSERT(pid_child >= 0, "fork(): %d: ", errno);

    if(0 == pid_child) {
        execl(shf_backticks("which nodejs"), "nodejs", "TestIpcQueue.js", nodejs_argument, NULL);
        /* should never come here unless error! */
        SHF_ASSERT(0, "execl(): %d: ", errno);
   }
   else {
        SHF_DEBUG("parent forked nodejs child with pid %u\n", pid_child);
   }
   return pid_child;
} /* exec_nodejs() */

int
main(int argc, char **argv) {
    SHF_UNUSE(argc);
    SHF_UNUSE(argv);

    plan_tests(4);

    pid_t pid = getpid();
    SHF_DEBUG("pid %u started\n", pid);

    char  test_shf_name[256];
    char  test_shf_folder[] = "/dev/shm";
    SHF_SNPRINTF(1, test_shf_name, "test-%05u-ipc-queue", pid);

    SHF      * shf;
    uint32_t   uid;
    uint32_t   test_keys = 100000;
    uint32_t   test_queue_item_data_size = 4096;

          shf_debug_verbosity_less();
          shf_init                ();
          shf_set_data_need_factor(250);
    shf = shf_attach              (test_shf_folder, test_shf_name); ok(NULL != shf, "c: shf_attach()          works for non-existing file as expected");

    uint32_t uid_queue_unused =  shf_queue_new_name(shf, SHF_CONST_STR_AND_SIZE("queue-unused"));
    uint32_t uid_queue_a2b    =  shf_queue_new_name(shf, SHF_CONST_STR_AND_SIZE("queue-a2b"   ));
    uint32_t uid_queue_b2a    =  shf_queue_new_name(shf, SHF_CONST_STR_AND_SIZE("queue-b2a"   ));

    {
        double test_start_time = shf_get_time_in_seconds();
        for (uint32_t i = 0; i < test_keys; i++) {
            uid = shf_queue_new_item (shf, test_queue_item_data_size);
                  shf_queue_push_head(shf, uid_queue_unused, uid);
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "c: created expected number of new queue items // estimate %.0f keys per second", test_keys / test_elapsed_time);
    }

    {
        double   test_start_time = shf_get_time_in_seconds();
        uint32_t test_pull_items = 0;
        while(NULL != shf_queue_pull_tail(shf, uid_queue_unused         )) {
                      shf_queue_push_head(shf, uid_queue_a2b   , shf_uid);
                      test_pull_items ++;
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(test_keys == test_pull_items, "c: moved   expected number of new queue items // estimate %.0f keys per second", test_keys / test_elapsed_time);
    }

    pid_t node_pid = exec_nodejs(test_shf_name);

    {
        double   test_start_time = 0;
        uint32_t test_pull_items = 0;
        do {
            while(NULL != shf_queue_pull_tail(shf, uid_queue_b2a         )) {
                          shf_queue_push_head(shf, uid_queue_a2b, shf_uid);
                          test_pull_items ++;
                          if (test_keys == test_pull_items) { test_start_time = shf_get_time_in_seconds(); } /* start timing once test_keys have done 1st loop */
            }
            SHF_YIELD();
        } while (test_pull_items < 500000);
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "c: moved   expected number of new queue items // estimate %.0f keys per second", test_pull_items / test_elapsed_time);
    }

    shf_debug_verbosity_more();

    int status;
    waitpid(node_pid, &status, 0);

    SHF_DEBUG("ending\n");
    return exit_status();
} /* main() */
