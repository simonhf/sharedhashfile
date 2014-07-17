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

// $ rm /tmp/dummy-log.txt ; make debug && (perl -e '$|++;foreach(0..3){printf qq[perl1: $_\n];sleep 2;}' | ./debug/shf.log /tmp/dummy-log.txt perl1 & perl -e '$|++;foreach(0..9){printf qq[perl2: $_\n];sleep 1;}' | ./debug/shf.log /tmp/dummy-log.txt perl2 ; cat /tmp/dummy-log.txt)
//
// shf.log: started logging to file '/tmp/dummy-log.txt' on behalf of 'perl1'
// shf.log: started logging to file '/tmp/dummy-log.txt' on behalf of 'perl2'
// perl1: 0
// perl2: 0
// perl2: 1
// perl1: 1
// perl2: 2
// perl2: 3
// perl1: 2
// perl2: 4
// perl2: 5
// perl2: 6
// perl1: 3
// perl2: 7
// shf.log: finished logging to file '/tmp/dummy-log.txt' on behalf of 'perl1'
// perl2: 8
// perl2: 9
// shf.log: finished logging to file '/tmp/dummy-log.txt' on behalf of 'perl2'

#include <stdio.h>
#include <sys/types.h>
#include <sys/select.h>
#include <signal.h>

#include <shf.private.h>
#include <shf.h>

static const char * shf_log_peer;
static const char * shf_log_path_name;

static void
append_line(const char * line) {
    /* todo: replace this hacky & slow fopen() with an ultra fast shared memory solution, synchronized via SHF :-) */
    FILE *file = fopen(shf_log_path_name, "a"); SHF_ASSERT(NULL != file, "shf.log: ERROR: fopen('%s', 'a') failed", shf_log_path_name);
    fprintf(file, "%s", line);
    fclose(file);
}

static int
is_ready(int fd) {
    fd_set fdset;
    struct timeval timeout;
    //int ret;
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    timeout.tv_sec = 0;
    timeout.tv_usec = 1;
    return select(fd+1, &fdset, NULL, NULL, &timeout) == 1 ? 1 : 0;
}

int
main(int argc, char **argv)
{
    SHF_ASSERT_INTERNAL(3 == argc, "shf.log: ERROR: expected 2 arguments but given %d arguments", argc);

    shf_log_path_name = argv[1];
    shf_log_peer      = argv[2];

    char start_message[256];
    SHF_SNPRINTF(0, start_message, "shf.log: started logging to file '%s' on behalf of '%s'\n", shf_log_path_name, shf_log_peer);
    append_line(start_message);

    signal(SIGINT, SIG_IGN); /* ignore SIGINT, e.g. if parent process gets ctrl-c */

    char *line = NULL;
    size_t len = 0;
    while (1) {
        //debug fprintf(stderr, "shf.log: everyday I'm logging to %s\n", shf_log_path_name);
        usleep(10000); /* wait 1/100th of a second */
        while (is_ready(fileno(stdin))) {
            ssize_t chars_read = getline(&line, &len, stdin);
            if (-1 == chars_read) {
                goto EARLY_OUT;
            }
            append_line(line);
        }
        if (feof(stdin)) {
EARLY_OUT:;
            char finish_message[256];
            SHF_SNPRINTF(0, finish_message, "shf.log: finished logging to file '%s' on behalf of '%s'\n", shf_log_path_name, shf_log_peer);
            append_line(finish_message);
            exit(EXIT_SUCCESS);
        }
    }
}
