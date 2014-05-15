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

int
main(int argc, char **argv) {
    pid_t pid = getpid();
    SHF_DEBUG("pid %u started\n", pid);

    uint32_t navl_data_length = 8;
    if (argc >= 2) {
        navl_data_length = atoi(argv[1]);
    }
    SHF_DEBUG("navl_data_length=%u\n", navl_data_length);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0); SHF_ASSERT(-1 != fd, "socket(): %d: ", errno);

    char unix_domain_socket_path[PATH_MAX];
    SHF_SNPRINTF(1, unix_domain_socket_path, "./test-%05u-test-ipc-socket-js.socket", pid);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, unix_domain_socket_path, sizeof(addr.sun_path)-1);

    unlink(unix_domain_socket_path);

    int status =   bind(fd, (struct sockaddr*)&addr, sizeof(addr)); SHF_ASSERT(-1 != status,   "bind(): %d: ", errno);
        status = listen(fd, 5                                    ); SHF_ASSERT(-1 != status, "listen(): %d: ", errno);

    shf_exec_child(shf_backticks("which node || which nodejs"), shf_backticks("which node || which nodejs"), "TestIpcSocket.js", unix_domain_socket_path);

    SHF_DEBUG("waiting for nodejs to connect\n");
    int client = accept(fd, NULL, NULL); SHF_ASSERT(-1 != client, "accept(): %d: ", errno);

    unlink(unix_domain_socket_path); /* everything connected; clean up .socket file already */

    char navl_data[16384]; /* fake NAVL data */
    for (uint32_t i = 0; i < sizeof(navl_data); i++) {
        navl_data[i] = '!';
    }
    double start_time = shf_get_time_in_seconds();
    uint32_t reads = 0;
    ssize_t bytes_read;
    do {
        char bytes[1024];
        bytes_read = read(client, bytes, sizeof(bytes)); SHF_ASSERT(-1 != bytes_read, "read(): %d: ", errno);
        if (0 == bytes_read) {
            shf_debug_verbosity_more();
            SHF_DEBUG("EOF\n");
            close(client);
        }
        else {
            reads ++;
            SHF_DEBUG("read: %lu bytes: %.*s\n", bytes_read, (uint32_t) bytes_read, bytes);
            ssize_t bytes_written = write(client, navl_data, navl_data_length); SHF_ASSERT(-1 != bytes_written, "write(): %d: ", errno);
            if (reads == 3) {
                SHF_DEBUG("disabling debug log\n");
                shf_debug_verbosity_less();
            }
        }
    } while (bytes_read);
    shf_debug_verbosity_more();
    double elapsed_time = shf_get_time_in_seconds() - start_time;
    fprintf(stderr, "sent %u hello messages in %f seconds or %.1f reads per second\n", reads, elapsed_time, reads / elapsed_time);

    SHF_DEBUG("ending\n");
    return 0;
} /* main() */
