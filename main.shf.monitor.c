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

#include <stdio.h>
#include <sys/types.h>
#include <signal.h> /* for kill() */

#include <shf.private.h>
#include <shf.h>

static pid_t        shf_monitor_parent_pid;
static const char * shf_monitor_path_name ;

static void
shf_monitor_delete_shf(void)
{
    char du_rm_folder[256];
    SHF_SNPRINTF(1, du_rm_folder, "du -h -d 0 %s ; rm -rf %s/", shf_monitor_path_name, shf_monitor_path_name);
    fprintf(stderr, "shf.monitor: detected pid %u gone; deleted shf; shf size before deletion: %s\n", shf_monitor_parent_pid, shf_backticks(du_rm_folder));
} /* shf_monitor_delete_shf() */

int
main(int argc, char **argv)
{
    SHF_ASSERT_INTERNAL(3 == argc, "shf.monitor: ERROR: expected 2 arguments but given %d arguments", argc);

    shf_monitor_parent_pid = atoi(argv[1]);
    shf_monitor_path_name  =      argv[2] ;

    SHF_DEBUG("shf.monitor: monitoring pid %u to delete %s\n", shf_monitor_parent_pid, shf_monitor_path_name);

    signal(SIGINT, SIG_IGN); /* ignore SIGINT, e.g. if parent process gets ctrl-c */

    while (1) {
        //debug usleep(1000000); /* wait 1/1000th of a second */
        //debug fprintf(stderr, "shf.monitor: everyday I'm monitoring %u %s\n", shf_monitor_parent_pid, shf_monitor_path_name);
        usleep(100000); /* wait 1/10th of a second */
        int value = kill(shf_monitor_parent_pid, 0);
        if (-1 == value && ESRCH == errno) {
            //debug fprintf(stderr, "shf.monitor: detected pid %u gone; deleting %s\n", shf_monitor_parent_pid, shf_monitor_path_name);
            struct stat mystat;
            value = stat(shf_monitor_path_name, &mystat);
            if      (-1 == value && ENOENT == errno        ) { /* do nothing */          } /* folder does not exist */
            else if ( 0 == value && S_ISDIR(mystat.st_mode)) { shf_monitor_delete_shf(); } /* folder exists; so delete it */
            else                                             { SHF_ASSERT(0, "ERROR: error or folder '%s' exists but is not a folder?: %u: ", shf_monitor_path_name, errno); }
            exit(0);
        }
        SHF_ASSERT(0 == value, "shf.monitor: ERROR: INTERNAL: expected value to be 0 but got %d: %u: ", value, errno);
        // todo: shf.monitor: consider monitoring which threads are still attached & not deleting while they are still attached
    }
}
