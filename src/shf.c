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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>      /* for va_start() et al */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>      /* for offsetof() */
#include <sys/time.h>    /* for gettimeofday() */
#include <sys/statvfs.h> /* for statvfs() */
#include <signal.h>      /* for kill() */
#include <pthread.h>     /* for pthread_create() */
#include <math.h>        /* for floor() */
#include <syslog.h>
#include <sys/syscall.h> /* for syscall() */
#include <sys/resource.h>/* for setrlimit() */

#include "shf.private.h"
#include "shf.h"

#include "murmurhash3.h"

                      SHF      * shf_log_thread_instance   = NULL;
                      uint32_t   shf_init_called           = 0   ;

       __thread       SHF_HASH   shf_hash                        ;

       __thread       uint32_t   shf_uid                         ;
       __thread       uint32_t   shf_qiid                        ; /*!< Set by                           shf_q_pull_tail(), and shf_q_push_head_pull_tail(). */
       __thread       char     * shf_qiid_addr                   ; /*!< Set by shf_q_new(), shf_q_get(), shf_q_pull_tail(), and shf_q_push_head_pull_tail(). */
       __thread       uint32_t   shf_qiid_addr_len               ; /*!< Set by shf_q_new(), shf_q_get(), shf_q_pull_tail(), and shf_q_push_head_pull_tail(). */

static __thread       uint8_t  * shf_val_addr                    ;
static __thread       uint32_t   shf_val_size                    ; /* mmap() size */
       __thread       char     * shf_val                   = NULL; /* mmap() */
       __thread       uint32_t   shf_val_len                     ;

static __thread const char     * shf_key                         ; /* used by shf_make_hash() */
static __thread       uint32_t   shf_key_len                     ; /* used by shf_make_hash() */

static __thread       uint32_t   shf_data_needed_factor    = 1   ;

static __thread       char     * shf_backticks_buffer      = NULL; /* mmap() */
static __thread       uint32_t   shf_backticks_buffer_size = 0   ; /* mmap() size */
static __thread       uint32_t   shf_backticks_buffer_used       ;

static __thread       int     (* shf_upd_callback)(const char * val, uint32_t val_len) = shf_upd_callback_copy;
static __thread       uint32_t   shf_upd_callback_failsafe                             = 0;
static __thread const char     * shf_upd_callback_copy_val                             = NULL;
static __thread       uint32_t   shf_upd_callback_copy_val_len                         = 0;

/**
 * @brief Spawn a child process & return its pid.
 * - Uses fork() & execl() under the covers.
 *
 * @param[in] child_path        e.g. "/usr/bin/python".
 * @param[in] child_file        e.g. "python".
 * @param[in] child_argument_1  e.g. "my-script.py".
 * @param[in] child_argument_2  e.g.  my-script-command-line-arguments.
 * @retval    pid               The pid of the spawned child.
 *
 * Example usage:
 * @code
 * pid_t child_pid = shf_exec_child(shf_backticks("which python"), "python", "my-script.py", my-script-command-line-arguments);
 * @endcode
 */
pid_t
shf_exec_child(
    const char  * child_path      ,
    const char  * child_file      ,
    const char  * child_argument_1,
          char  * child_argument_2)
{
    pid_t pid_child = fork(); SHF_ASSERT(pid_child >= 0, "fork(): %d: ", errno);

    if(0 == pid_child) {
        execl(child_path, child_file, child_argument_1, child_argument_2, NULL);
        /* should never come here unless error! */
        SHF_ASSERT(0, "execl(child_path='%s', ...): %d: ", child_path, errno);
   }
   else {
        SHF_DEBUG("parent forked child child with pid %u\n", pid_child);
   }
   return pid_child;
} /* shf_exec_child() */

/**
 * @brief Spawn a child process, block while it executes, capture its output, trim trailing whitespace.
 * - Pointer is held in thread local storage to an mmap().
 * - The mmap() is automatically resized bigger to hold all the output.
 * - The mmap() is never freed.
 * - Intention is a convenient way to capture relatively small command output without wasting too much memory.
 *
 * @param[in] command  Command to be executed, e.g. `"ls -la | grep \.log"`.
 * @retval    Pointer  Pointer to captured output.
 *
 * Example usage:
 * @code
 * char * buf = shf_backticks("ls -la | grep \.log");
 * @endcode
 */
char *
shf_backticks(const char * command)
{
    FILE     * fp;
    uint32_t   bytes_read;

    if (0 == shf_backticks_buffer_size) {
        shf_backticks_buffer_size = SHF_SIZE_PAGE;
        shf_backticks_buffer      = mmap(NULL, shf_backticks_buffer_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE, -1, 0); SHF_ASSERT(MAP_FAILED != shf_backticks_buffer, "mmap(): %u: ", errno);
    }

    SHF_DEBUG("%s('%s')\n", __FUNCTION__, command);
    fp = popen(command, "r"); SHF_ASSERT(NULL != fp, "popen('%s'): %u: ", command, errno);

    shf_backticks_buffer_used = 0;
    while ((bytes_read = fread(&shf_backticks_buffer[shf_backticks_buffer_used], sizeof(char), (shf_backticks_buffer_size - shf_backticks_buffer_used - 1), fp)) != 0) {
        SHF_DEBUG("- read %u bytes from the pipe\n", bytes_read);
        shf_backticks_buffer_used += bytes_read;
        if (shf_backticks_buffer_size - shf_backticks_buffer_used <= 1) {
            shf_backticks_buffer       = mremap(shf_backticks_buffer, shf_backticks_buffer_size, SHF_SIZE_PAGE + shf_backticks_buffer_size, MREMAP_MAYMOVE); SHF_ASSERT(MAP_FAILED != shf_backticks_buffer, "mremap(): %u: ", errno);
            shf_backticks_buffer_size += SHF_SIZE_PAGE;
            SHF_DEBUG("- increased buffer size to %u\n", shf_backticks_buffer_size);
        }
    }

    while (shf_backticks_buffer_used > 0 && ('\n' == shf_backticks_buffer[shf_backticks_buffer_used - 1] || '\r' == shf_backticks_buffer[shf_backticks_buffer_used - 1] || ' ' == shf_backticks_buffer[shf_backticks_buffer_used - 1])) {
        shf_backticks_buffer_used --; /* trim trailing whitespace */
    }
    shf_backticks_buffer[shf_backticks_buffer_used] = '\0';
    // todo: why in value 2 if 'which does-not-exist'? int value = pclose(fp); SHF_ASSERT(0 == value, "pclose(): %u: ", errno);
    pclose(fp);

    return shf_backticks_buffer;
} /* shf_backticks() */

double
shf_get_time_in_seconds(void)
{
    struct timeval tv;

    SHF_ASSERT(gettimeofday(&tv, NULL) >= 0, "gettimeofday(): %u: ", errno);
    return (double)tv.tv_sec + 1.e-6 * (double)tv.tv_usec;
} /* shf_get_time_in_seconds() */

uint64_t
shf_get_vfs_available(const char * shf_path)
{
    struct statvfs mystatvfs;
    SHF_ASSERT(0 == statvfs(shf_path, &mystatvfs), "statvfs(): %u: ", errno);
    uint64_t vfs_available = mystatvfs.f_bsize * mystatvfs.f_bfree;
    SHF_DEBUG("%s(shf_path=%s) // %lu\n", __FUNCTION__, shf_path, vfs_available);
    return vfs_available;
} /* shf_get_vfs_available() */

void
shf_init(void)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    if (shf_init_called) {
        SHF_DEBUG("%s() already called; early out\n", __FUNCTION__);
        goto EARLY_OUT;
    }
    shf_init_called = 1;

#ifdef SHF_DEBUG_VERSION
    SHF_DEBUG("turning on core dump for debug\n");

    struct rlimit core_limit;
    core_limit.rlim_cur = RLIM_INFINITY;
    core_limit.rlim_max = RLIM_INFINITY;

    if (setrlimit(RLIMIT_CORE, &core_limit) < 0) {
        SHF_DEBUG("setrlimit(): %s; WARN: core dumps may be truncated or non-existant\n", strerror(errno));
    }
#endif

    SHF_DEBUG("- SHF_SIZE_PAGE        :%u\n" , SHF_SIZE_PAGE          );
    SHF_DEBUG("- SHF_SIZE_CACHE_LINE  :%u\n" , SHF_SIZE_CACHE_LINE    );
    SHF_DEBUG("- SHF_REFS_PER_ROW     :%lu\n", SHF_REFS_PER_ROW       );
    SHF_DEBUG("- SHF_REFS_PER_ROW_BITS:%u\n" , SHF_REFS_PER_ROW_BITS  );
    SHF_DEBUG("- SHF_SIZE_ROW         :%lu\n", SHF_SIZE_ROW           );
    SHF_DEBUG("- SHF_SIZE_TAB         :%u\n" , SHF_SIZE_TAB           );
    SHF_DEBUG("- SHF_ROWS_PER_TAB     :%lu\n", SHF_ROWS_PER_TAB       );
    SHF_DEBUG("- SHF_ROWS_PER_TAB_BITS:%u\n" , SHF_ROWS_PER_TAB_BITS  );
    SHF_DEBUG("- SHF_REFS_PER_TAB     :%lu\n", SHF_REFS_PER_TAB       );
    SHF_DEBUG("- SHF_TABS_PER_WIN_BITS:%u\n" , SHF_TABS_PER_WIN_BITS  );
    SHF_DEBUG("- SHF_TABS_PER_WIN     :%u\n" , SHF_TABS_PER_WIN       );
    SHF_DEBUG("- SHF_REFS_PER_WIN     :%lu\n", SHF_REFS_PER_WIN       );
    SHF_DEBUG("- SHF_WINS_PER_SHF_BITS:%u\n" , SHF_WINS_PER_SHF_BITS  );
    SHF_DEBUG("- SHF_WINS_PER_SHF     :%u\n" , SHF_WINS_PER_SHF       );
    SHF_DEBUG("- SHF_REFS_PER_SHF     :%lu\n", SHF_REFS_PER_SHF       );
    SHF_DEBUG("- SHF_TABS_PER_SHF     :%u\n" , SHF_TABS_PER_SHF       );
    SHF_DEBUG("- sizeof(SHF_REF_MMAP) :%lu\n", sizeof(SHF_REF_MMAP)   );
    SHF_DEBUG("- sizeof(SHF_ROW_MMAP) :%lu\n", sizeof(SHF_ROW_MMAP)   );
    SHF_DEBUG("- sizeof(SHF_TAB_MMAP) :%lu\n", sizeof(SHF_TAB_MMAP)   );
    SHF_DEBUG("- sizeof(SHF_OFF_MMAP) :%lu\n", sizeof(SHF_OFF_MMAP)   );
    SHF_DEBUG("- sizeof(SHF_OFF)      :%lu\n", sizeof(SHF_OFF)        );
    SHF_DEBUG("- sizeof(SHF_WIN_MMAP) :%lu\n", sizeof(SHF_WIN_MMAP)   );
    SHF_DEBUG("- sizeof(SHF_SHF_MMAP) :%lu\n", sizeof(SHF_SHF_MMAP)   );
    SHF_DEBUG("- sizeof(SHF_UID)      :%lu\n", sizeof(SHF_UID)        );
    SHF_DEBUG("- sizeof(SHF)          :%lu\n", sizeof(SHF)            );

    SHF_ASSERT(SHF_SIZE_PAGE         == getpagesize(), "Expected page size %u but kernel has page size %u", SHF_SIZE_PAGE, getpagesize());
    SHF_ASSERT(SHF_SIZE_CACHE_LINE   == 64           , "Expected cacheline size %u but CPU has cache line size %u", SHF_SIZE_CACHE_LINE, 64); /* todo: how to calculate cache line size at run-time? */
    SHF_ASSERT(sizeof(SHF_DATA_TYPE) == 1            , "Experted sizeof(SHF_DATA_TYPE) 1 but got %lu", sizeof(SHF_DATA_TYPE));

    SHF_ASSERT((1 << SHF_REFS_PER_ROW_BITS) == SHF_REFS_PER_ROW, "INTERNAL: SHF_REFS_PER_ROW_BITS: 2^%u is not %lu", SHF_REFS_PER_ROW_BITS, SHF_REFS_PER_ROW);
    SHF_ASSERT((1 << SHF_ROWS_PER_TAB_BITS) == SHF_ROWS_PER_TAB, "INTERNAL: SHF_ROWS_PER_TAB_BITS: 2^%u is not %lu", SHF_ROWS_PER_TAB_BITS, SHF_ROWS_PER_TAB);
    SHF_ASSERT(32 == (SHF_REFS_PER_ROW_BITS + SHF_ROWS_PER_TAB_BITS + SHF_TABS_PER_WIN_BITS + SHF_WINS_PER_SHF_BITS), "INTERNAL: SHF_*_PER_*_BITS should add up to 32, not %u", SHF_REFS_PER_ROW_BITS + SHF_ROWS_PER_TAB_BITS + SHF_TABS_PER_WIN_BITS + SHF_WINS_PER_SHF_BITS);

    shf_val_size = 4096;
    shf_val = mmap(NULL, shf_val_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE, -1, 0); SHF_ASSERT(MAP_FAILED != shf_val, "mmap(): %u: ", errno);

#ifdef SHF_DEBUG_VERSION
    shf_debug_file = fopen("/tmp/debug.shf", "wb"); SHF_ASSERT(NULL != shf_debug_file, "fopen(): %u: ", errno); /* shorten debug file */
    fclose(shf_debug_file);
#endif
EARLY_OUT:;
} /* shf_init() */

void
shf_detach( /* free any (c|m)alloc()d memory & munmap() any mmap()s */
    SHF * shf)
{
    int      value;
    uint32_t count_munmap = 0;
    uint32_t count_free   = 0;

    SHF_DEBUG("%s(shf=?)\n", __FUNCTION__);
    SHF_ASSERT_INTERNAL(shf, "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");

    if (shf->log_thread_acive)
        shf_log_thread_del(shf);

    if (shf->q.qids_nolock_push) { free(shf->q.qids_nolock_push); shf->count_xalloc --; }
    if (shf->q.qids_nolock_pull) { free(shf->q.qids_nolock_pull); shf->count_xalloc --; }

    for (uint32_t win = 0; win < SHF_WINS_PER_SHF; win++) {
        for (uint32_t tab = 0; tab < shf->shf_mmap->wins[win].tabs_used; tab++) {
            if (shf->tabs[win][tab].tab_size >= SHF_SIZE_PAGE) {
                SHF_ASSERT_INTERNAL(shf->tabs[win][tab].tab_mmap, "ERROR: INTERNAL: attempting to %p=munmap() NULL pointer at win=%u, tab=%u", shf->tabs[win][tab].tab_mmap, win, tab);
                value = munmap(shf->tabs[win][tab].tab_mmap, shf->tabs[win][tab].tab_size); count_munmap ++; SHF_ASSERT(0 == value, "ERROR: munmap(<win=%u>, <tab=%u>): %u: ", win, tab, errno);
            }
        }
    }

    if (shf->path) { free(shf->path); count_free ++; }
    if (shf->name) { free(shf->name); count_free ++; }
    value = munmap(shf->shf_mmap, SHF_MOD_PAGE(sizeof(SHF_SHF_MMAP))); count_munmap ++; SHF_ASSERT(0 == value, "ERROR: munmap(<shf_mmap>): %u: ", errno);
    SHF_ASSERT_INTERNAL(count_munmap == shf->count_mmap  , "ERROR: INTERNAL: called munmap() %u times but needed to call it %u times", count_munmap, shf->count_mmap  );
    SHF_ASSERT_INTERNAL(count_free   == shf->count_xalloc, "ERROR: INTERNAL: called free() %u times but needed to call it %u times"  , count_free  , shf->count_xalloc);

    free(shf);
    //debug fprintf(stderr, "%s() // successful\n", __FUNCTION__);
} /* shf_detach() */

SHF * /* NULL if name does not exist */
shf_attach_existing(
    const char * path, /* e.g. '/dev/shm' */
    const char * name) /* e.g. 'myshf'    */
{
    SHF  * shf = NULL;
    char   file_name[256];
    int    fd;

    SHF_DEBUG("%s(path='%s', name='%s')\n", __FUNCTION__, path, name);
    SHF_ASSERT(shf_init_called, "shf_init() not previously called");

    SHF_SNPRINTF(1, file_name, "%s/%s.shf/%s.shf", path, name, name);
    fd = open(file_name, O_RDWR | O_CREAT, 0600);
    if (-1 != fd) {
        SHF_DEBUG("- allocating bytes for shf non mmap : %lu\n", sizeof(SHF));
        shf = calloc(1, sizeof(SHF)); SHF_ASSERT(shf != NULL, "calloc(1, %lu): %u: ", sizeof(SHF), errno);

        SHF_DEBUG("- allocating bytes for shf     mmap : %lu\n", SHF_MOD_PAGE(sizeof(SHF_SHF_MMAP)));
        shf->shf_mmap = mmap(NULL, SHF_MOD_PAGE(sizeof(SHF_SHF_MMAP)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NORESERVE | MAP_POPULATE, fd, 0); shf->count_mmap ++; SHF_ASSERT(MAP_FAILED != shf->shf_mmap, "mmap(): %u: ", errno);

        int value = close(fd); SHF_ASSERT(-1 != value, "close(): %u: ", errno);

        shf->path                 = strdup(path); shf->count_xalloc ++;
        shf->name                 = strdup(name); shf->count_xalloc ++;
        shf->is_lockable          = 1;
        shf->is_fixed_key_val_len = 0;
        shf->log                  = NULL;
    }

    SHF_DEBUG("- return %p // shf\n", shf);
    return shf;
} /* shf_attach_existing() */

#define SHF_TRUNCATE_FILE(PATH, FILE, SIZE, MKDIR) \
    { \
        int shf_truncate_file_value; \
        int shf_truncate_file_fd; \
        shf_truncate_file_value =     mkdir(PATH, 0777                  ); if (-1 == shf_truncate_file_value && EEXIST == errno) { /* path exists! */ } else SHF_ASSERT(1, "mkdir(): %u: ", errno); \
        shf_truncate_file_fd    =      open(FILE, O_RDWR | O_CREAT, 0600); SHF_ASSERT(-1 != shf_truncate_file_fd   ,      "open(): %u: ", errno); \
        shf_truncate_file_value = ftruncate(shf_truncate_file_fd, SIZE  ); SHF_ASSERT( 0 == shf_truncate_file_value, "ftruncate(): %u: ", errno); \
        shf_truncate_file_value =     close(shf_truncate_file_fd        ); SHF_ASSERT(-1 != shf_truncate_file_value,     "close(): %u: ", errno); \
    }

static void
shf_tab_create(
    const char * path, /* e.g. '/dev/shm' */
    const char * name, /* e.g. 'myshf'    */
    uint32_t     win ,
    uint16_t     tab ,
    uint32_t     show, /* show debug? */
    uint32_t     temp) /* append temp to folder? */
{
    char file_name_tab[256];
    char path_name_tab[256];
    char temp_name_tab[256] = "";
    if (temp) { SHF_SNPRINTF(0   , temp_name_tab, ".%05u"                    , getpid()                           ); }
                SHF_SNPRINTF(show, file_name_tab, "%s/%s.shf%s/%03d/%04u.tab", path, name, temp_name_tab, win, tab);
                SHF_SNPRINTF(show, path_name_tab, "%s/%s.shf%s/%03d"         , path, name, temp_name_tab, win     );
                SHF_TRUNCATE_FILE(path_name_tab, file_name_tab, SHF_MOD_PAGE(sizeof(SHF_TAB_MMAP)), temp /* mkdir? */);
} /* shf_create_tab() */

SHF *
shf_attach(
    const char * path                    , /* e.g. '/dev/shm' */
    const char * name                    , /* e.g. 'myshf'    */
    uint32_t     delete_upon_process_exit) /* 0 means do nothing, 1 means delete shf when calling process exits */
{
    char file_name[256];
    char path_name[256];
    int  fd;
    int  value;
    int  tabs = 0;

    SHF_DEBUG("%s(path='%s', name='%s', delete_upon_process_exit=%u)\n", __FUNCTION__, path, name, delete_upon_process_exit);
    SHF_ASSERT(shf_init_called, "shf_init() not previously called");

    SHF_SNPRINTF(1, file_name, "%s/%s.shf/%s.shf", path, name, name);
    SHF_SNPRINTF(1, path_name, "%s/%s.shf"       , path, name      );

    fd = open(file_name, O_RDWR | O_CREAT, 0600);
    if (-1 == fd) {
        SHF_ASSERT(/* not found */ 2 == errno, "open(): %u: ", errno);

        SHF_DEBUG("- name does not exist; creating new shf atomically: '%s'\n", name);
        char file_name_shf[256];
        char path_name_shf[256];
        SHF_SNPRINTF(1, file_name_shf, "%s/%s.shf.%05u/%s.shf", path, name, getpid(), name);
        SHF_SNPRINTF(1, path_name_shf, "%s/%s.shf.%05u"       , path, name, getpid()      );

        SHF_DEBUG("- creating file  for shf mmap : %lu\n", SHF_MOD_PAGE(sizeof(SHF_SHF_MMAP)));
        SHF_TRUNCATE_FILE(path_name_shf, file_name_shf, SHF_MOD_PAGE(sizeof(SHF_SHF_MMAP)), 1 /* mkdir */);

        SHF_DEBUG("- creating files for tab mmaps: %lu * %u\n", SHF_MOD_PAGE(sizeof(SHF_TAB_MMAP)), SHF_WINS_PER_SHF);
        tabs = 1;
        for (int win = 0; win < SHF_WINS_PER_SHF; win++) {
            for (int tab = 0; tab < tabs; tab++) {
                shf_tab_create(path, name, win, tab, 0 /* no show */, 1 /* temp/mkdir */);
            }
        }

        SHF_DEBUG("- atomically renaming folder from '%s' to '%s'\n", path_name_shf, path_name);
        value = rename(path_name_shf, path_name); SHF_ASSERT(-1 != value, "rename(): %u: ", errno);
    }
    else {
        value = close(fd); SHF_ASSERT(-1 != value, "close(): %u: ", errno);
    }

    SHF * shf = shf_attach_existing(path, name);
    if (shf && tabs) {
        for (int win = 0; win < SHF_WINS_PER_SHF; win++) {
            uint16_t next_tab = 0;
            for (int tab = 0; tab < SHF_TABS_PER_WIN; tab++) {
                shf->shf_mmap->wins[win].tabs[tab].tab = next_tab;
                next_tab ++;
                if (tabs == next_tab) {
                    next_tab = 0;
                }
            }
            shf->shf_mmap->wins[win].tabs_used = tabs;
        }
    }

    if (delete_upon_process_exit) {
        char pid[256];
        SHF_SNPRINTF(1, pid, "%u", getpid());

        const char * shf_monitor_path = shf_backticks("which shf.monitor");
        SHF_ASSERT_INTERNAL(strlen(shf_monitor_path), "ERROR: 'which shf.monitor' found nothing; ensure shf.monitor can be found in the PATH if calling %s() with delete_upon_process_exit=1; PATH=%s", __FUNCTION__, shf_backticks("echo $PATH"));
        SHF_DEBUG("- launching: '%s %s %s'\n", shf_monitor_path, pid, path_name);
        pid_t pid_child = shf_exec_child(shf_monitor_path, "shf.monitor", pid, path_name);
        SHF_ASSERT(0 == kill(pid_child, 0), "ERROR: INTERNAL: pid %u does not exist as expected: %u: ", pid_child, errno);
    }

    return shf;
} /* shf_attach() */

void
shf_make_hash(
    const char     * key    ,
          uint32_t   key_len)
{
    if (key) {
        MurmurHash3_x64_128(key, key_len, 12345 /* todo: handle seed better :-) */, &shf_hash.u64[0]);
        // todo: just generate the uid directly?
    }
    shf_key     = key    ;
    shf_key_len = key_len;
    SHF_DEBUG("%s(key=?, key_len=%u){} // %04x-%04x-%04x\n", __FUNCTION__, key_len, shf_hash.u16[0], shf_hash.u16[1], shf_hash.u16[2]);
} /* shf_make_hash() */

#define SHF_GET_TAB_MMAP(SHF, TAB) \
    if (0 == SHF->tabs[win][TAB].tab_mmap) { /* need to mmap() tab? */ \
        char file_tab[256]; \
        SHF_SNPRINTF(0, file_tab, "%s/%s.shf/%03u/%04u.tab", SHF->path, SHF->name, win, TAB); \
        struct stat sb; \
        int value = stat(file_tab, &sb); SHF_ASSERT(-1 != value, "stat(): %u: ", errno); \
        SHF_DEBUG("- mmap() %lu tab bytes @ 0x%02x-xxx[%03x]-xxx-x for '%s'\n", sb.st_size, win, TAB, file_tab); \
        SHF_ASSERT(sb.st_size == SHF_MOD_PAGE(sb.st_size), "INTERNAL: '%s' has an unexpected size of %lu\n", file_tab, sb.st_size); \
        int fd = open(file_tab, O_RDWR | O_CREAT, 0600); SHF_ASSERT(-1 != fd, "open(): %u: ", errno); \
        SHF->tabs[win][TAB].tab_mmap = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NORESERVE | MAP_POPULATE, fd, 0); shf->count_mmap ++; SHF_ASSERT(MAP_FAILED != SHF->tabs[win][TAB].tab_mmap, "mmap(): %u: ", errno); \
        value = close(fd); SHF_ASSERT(-1 != value, "close(): %u: ", errno); \
        SHF->tabs[win][TAB].tab_size = sb.st_size; \
        SHF_DEBUG_FILE("pid %5u, win-tab %u-%u: initial mmap() %u bytes\n", getpid(), win, TAB, SHF->tabs[win][TAB].tab_size); \
        SHF->shf_mmap->wins[win].tabs_mmaps ++; \
    } \
    tab_mmap = SHF->tabs[win][TAB].tab_mmap; \
    if (0 == tab_mmap->tab_size) { \
        SHF_DEBUG_FILE("pid %5u, win-tab %u-%u: init to %u bytes\n", getpid(), win, TAB, SHF->tabs[win][TAB].tab_size); \
        tab_mmap->tab_size                      = SHF->tabs[win][TAB].tab_size; \
        tab_mmap->tab_used                      = offsetof(SHF_TAB_MMAP, data); \
        SHF_DEBUG("- hack but works: mmap set tab size to %u and used to %u for the first time!\n", tab_mmap->tab_size, tab_mmap->tab_used); \
    } \
    if (SHF->tabs[win][TAB].tab_size != tab_mmap->tab_size) { \
        SHF_DEBUG("- tab was %u, now %u bytes; remapping\n", SHF->tabs[win][TAB].tab_size, tab_mmap->tab_size); \
        SHF_DEBUG_FILE("pid %5u, win-tab %u-%u: remap  from %7u to %7u bytes\n", getpid(), win, TAB, SHF->tabs[win][TAB].tab_size, tab_mmap->tab_size); \
        uint32_t new_tab_size = tab_mmap->tab_size; \
        if (1 /* reload replacement tab? */ == tab_mmap->tab_size) { \
            char file_tab[256]; \
            SHF_SNPRINTF(0, file_tab, "%s/%s.shf/%03u/%04u.tab", SHF->path, SHF->name, win, TAB); \
            struct stat sb; \
            int value = stat(file_tab, &sb); SHF_ASSERT(-1 != value, "stat(): %u: ", errno); \
            new_tab_size = sb.st_size; \
            int fd       =   open(file_tab, O_RDWR | O_CREAT, 0600); SHF_ASSERT(-1 != fd, "open(): %u: ", errno); \
                value    = munmap(SHF->tabs[win][TAB].tab_mmap, SHF->tabs[win][TAB].tab_size); SHF_ASSERT(-1 != value, "munmap(): %u: ", errno); \
                tab_mmap =   mmap(NULL, new_tab_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NORESERVE | MAP_POPULATE, fd, 0); SHF_ASSERT(MAP_FAILED != tab_mmap, "mmap(): %u: ", errno); \
                value    =  close(fd); SHF_ASSERT(-1 != value, "close(): %u: ", errno); \
            SHF->shf_mmap->wins[win].tabs_mmaps ++; \
        } \
        else { \
            tab_mmap = mremap(tab_mmap, SHF->tabs[win][TAB].tab_size, new_tab_size, MREMAP_MAYMOVE); SHF_ASSERT(MAP_FAILED != tab_mmap, "mremap(): %u: ", errno); \
            SHF->shf_mmap->wins[win].tabs_mremaps ++; \
        } \
        /* debug paranoia */ SHF_U08_AT(tab_mmap, new_tab_size - 1) ++; \
        /* debug paranoia */ SHF_U08_AT(tab_mmap, new_tab_size - 1) --; \
        SHF->tabs[win][TAB].tab_size = new_tab_size; \
        SHF->tabs[win][TAB].tab_mmap = tab_mmap; \
    }

#ifdef MADV_DONTDUMP /* since Linux 3.4 */
#define MYMADV_DONTDUMP MADV_DONTDUMP
#else
#define MYMADV_DONTDUMP 0
#endif

#define SHF_TAB_APPEND(SHF, TAB, TAB_MMAP, LEN_LEN, KEY, KEY_LEN, POS) \
    /* todo: examine if file append & remap is faster than remap & direct memory access */ \
    /* todo: consider special mode with is write only, e.g. for initial startup? */ \
    /* todo: faster to use remap_file_pages() instead of multiple mmap()s? */ \
    uint64_t data_needed    = sizeof(SHF_DATA_TYPE) + LEN_LEN + KEY_LEN + LEN_LEN + val_len; \
    uint64_t data_available = TAB_MMAP->tab_size - TAB_MMAP->tab_used; \
    SHF_DEBUG("- appending %lu bytes for ref @ 0x%02x-xxx[%03x]-%03x-%x // key,value are %u,%u bytes @ pos %u // todo: use SHF_DATA_TYPE instead of hard coding\n", data_needed, win, TAB, row, ref, KEY_LEN, val_len, TAB_MMAP->tab_used); \
    SHF_LOCK_DEBUG_MACRO(&SHF->shf_mmap->wins[win].lock, 1); \
    if ((0 == LEN_LEN                    )    /* if ->is_fixed_key_val_len */ \
    &&  (0 != TAB_MMAP->tab_data_free_pos)) { /* and single linked list chain link exists */ \
        /* come here to reuse deleted key,value pair on deleted link list */ \
                      POS = TAB_MMAP->tab_data_free_pos; \
        uint32_t next_pos = SHF_U32_AT(TAB_MMAP, POS+1+sizeof(KEY_LEN)); \
        TAB_MMAP->tab_data_free_pos = next_pos; \
        SHF_DATA_TYPE data_type; \
                      data_type.as_type.key_type = SHF_KEY_TYPE_KEY_IS_STR32; \
                      data_type.as_type.val_type = SHF_KEY_TYPE_VAL_IS_STR32; \
        SHF_U08_AT(TAB_MMAP, POS                                                      )    =    data_type.as_u08; \
        SHF_MEM_AT(TAB_MMAP, POS + sizeof(SHF_DATA_TYPE) + LEN_LEN                    , /* = */ KEY_LEN         , /* bytes at */ KEY); \
        if (val) { \
        SHF_MEM_AT(TAB_MMAP, POS + sizeof(SHF_DATA_TYPE) + LEN_LEN + KEY_LEN + LEN_LEN, /* = */ val_len         , /* bytes at */ val); \
        } \
        TAB_MMAP->tab_data_free -= 1 + KEY_LEN + val_len; \
        goto SKIP_APPEND_COS_REUSE; \
    } else if (data_needed > data_available) { \
        SHF_LOCK_DEBUG_MACRO(&SHF->shf_mmap->wins[win].lock, 2); \
        uint64_t new_tab_size = SHF_MOD_PAGE(TAB_MMAP->tab_size + (data_needed * shf_data_needed_factor)); \
        uint64_t vfs_available = shf_get_vfs_available(SHF->path); \
        SHF_ASSERT_INTERNAL(new_tab_size - TAB_MMAP->tab_size <= vfs_available, "ERROR: requesting to expand tab by %lu but only %lu bytes available on '%s'; need an extra %lu bytes; if /dev/shm consider increasing RAM via e.g. sudo mount -o remount,size=4g /dev/shm", new_tab_size - TAB_MMAP->tab_size, vfs_available, SHF->path, new_tab_size - TAB_MMAP->tab_size - vfs_available); \
        char file_tab[256]; \
        SHF_SNPRINTF(0, file_tab, "%s/%s.shf/%03u/%04u.tab", SHF->path, SHF->name, win, TAB); \
        SHF_DEBUG("- grow tab from %u to %lu; need %lu bytes but %lu bytes available in '%s'\n", TAB_MMAP->tab_size, new_tab_size, data_needed, data_available, file_tab); \
        SHF_DEBUG_FILE("pid %5u, win-tab %u-%u: grow   from %7u to %7lu bytes\n", getpid(), win, TAB, TAB_MMAP->tab_size, new_tab_size); \
        int fd    =      open(file_tab, O_RDWR | O_CREAT, 0600); SHF_ASSERT(-1 != fd, "open(): %u: ", errno); \
        int value = ftruncate(fd, new_tab_size); SHF_ASSERT(-1 != value, "ftruncate(): %u: ", errno); \
        if (1) { \
            /* note: why does mremap() not reflect in statvfs()? */ \
            value    = munmap(SHF->tabs[win][TAB].tab_mmap, SHF->tabs[win][TAB].tab_size); SHF_ASSERT(-1 != value, "munmap(): %u: ", errno); \
            TAB_MMAP =   mmap(NULL, new_tab_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NORESERVE | MAP_POPULATE, fd, 0); SHF_ASSERT(MAP_FAILED != TAB_MMAP, "mmap(): %u: ", errno); \
        } \
        else { \
            TAB_MMAP = mremap(SHF->tabs[win][TAB].tab_mmap, SHF->tabs[win][TAB].tab_size, new_tab_size, MREMAP_MAYMOVE); SHF_ASSERT(MAP_FAILED != TAB_MMAP, "mremap(): %u: ", errno); \
            madvise(TAB_MMAP, new_tab_size, MADV_RANDOM | MYMADV_DONTDUMP); /* todo: test if madvise() makes any performance difference */ \
            SHF->shf_mmap->wins[win].tabs_mremaps ++; \
        } \
        /* debug paranoia */ SHF_U08_AT(TAB_MMAP, new_tab_size - 1) ++; \
        /* debug paranoia */ SHF_U08_AT(TAB_MMAP, new_tab_size - 1) --; \
        value = close(fd); SHF_ASSERT(-1 != value, "close(): %u: ", errno); \
        SHF->tabs[win][TAB].tab_mmap = TAB_MMAP; \
        TAB_MMAP->tab_size           = new_tab_size; \
        SHF->tabs[win][TAB].tab_size = new_tab_size; \
    } \
    SHF_ASSERT(TAB_MMAP->tab_used+1+LEN_LEN+KEY_LEN                 <= TAB_MMAP->tab_size, "INTERNAL: expected key < %u but pos is %u at win %u, tab %u; key_len %u\n", TAB_MMAP->tab_size, TAB_MMAP->tab_used+1+LEN_LEN+KEY_LEN                , win, TAB, KEY_LEN          ); \
    SHF_ASSERT(TAB_MMAP->tab_used+1+LEN_LEN+KEY_LEN+LEN_LEN+val_len <= TAB_MMAP->tab_size, "INTERNAL: expected val < %u but pos is %u at win %u, tab %u; xxx_len %u\n", TAB_MMAP->tab_size, TAB_MMAP->tab_used+1+LEN_LEN+KEY_LEN+LEN_LEN+val_len, win, TAB, KEY_LEN + val_len); \
    SHF_DATA_TYPE data_type; \
                  data_type.as_type.key_type = SHF_KEY_TYPE_KEY_IS_STR32; \
                  data_type.as_type.val_type = SHF_KEY_TYPE_VAL_IS_STR32; \
    SHF_U08_AT(TAB_MMAP, TAB_MMAP->tab_used                                                               )    =    data_type.as_u08; \
    if (0 == LEN_LEN) { \
        /* store key value *without* size data */ \
    } \
    else { \
        /* store key value *with* size data */ \
    SHF_U32_AT(TAB_MMAP, TAB_MMAP->tab_used + sizeof(SHF_DATA_TYPE)                                       )    =    KEY_LEN         ; \
    SHF_U32_AT(TAB_MMAP, TAB_MMAP->tab_used + sizeof(SHF_DATA_TYPE) + LEN_LEN + KEY_LEN                   )    =    val_len         ; \
    } \
    SHF_MEM_AT(TAB_MMAP, TAB_MMAP->tab_used + sizeof(SHF_DATA_TYPE) + LEN_LEN                             , /* = */ KEY_LEN         , /* bytes at */ KEY); \
    if (val) { \
    SHF_MEM_AT(TAB_MMAP, TAB_MMAP->tab_used + sizeof(SHF_DATA_TYPE) + LEN_LEN + KEY_LEN + LEN_LEN         , /* = */ val_len         , /* bytes at */ val); \
    } \
    TAB_MMAP->tab_used += data_needed; \
    SKIP_APPEND_COS_REUSE:; \
    TAB_MMAP->tab_refs_used ++; \
    TAB_MMAP->tab_data_used += data_needed;

#define SHF_TAB_REF_MARK_AS_DELETED(TAB_MMAP, LEN_LEN) \
    uint32_t old_pos = TAB_MMAP->tab_data_free_pos; \
    /* mark data in old tab as deleted */ \
    SHF_U08_AT(TAB_MMAP, TAB_MMAP->row[row].ref[ref].pos) = SHF_DATA_TYPE_DELETED; \
    if (0 == LEN_LEN) {                                                                                        } /* if ->is_fixed_key_val_len */ \
    else              { SHF_U32_AT(TAB_MMAP, TAB_MMAP->row[row].ref[ref].pos+1) = key_len + LEN_LEN + val_len; } /* store total length of deleted key,value */ \
    if ((                  0 == LEN_LEN        )    /* if ->is_fixed_key_val_len */ \
    &&  ((key_len + val_len) >= sizeof(old_pos))) { /* and enough space to store single linked list chain link */ \
        /* come here to add deleted key,value pair to deleted link list */ \
        SHF_U32_AT(TAB_MMAP, TAB_MMAP->row[row].ref[ref].pos+1+sizeof(key_len)) = old_pos; \
        TAB_MMAP->tab_data_free_pos = TAB_MMAP->row[row].ref[ref].pos; \
        /* todo: consider having multiple linked lists for key,value powers of two sizes for use in non-fixed size key,vale mode */ \
    } \
    TAB_MMAP->tab_data_used -= 1 + LEN_LEN + key_len + LEN_LEN + val_len; \
    TAB_MMAP->tab_data_free += 1 + LEN_LEN + key_len + LEN_LEN + val_len; \
    /* mark ref in old tab as unused */ \
    TAB_MMAP->row[row].ref[ref].pos = 0; \
    TAB_MMAP->tab_refs_used --;

#define SHF_TAB_REF_COPY(LEN_LEN) \
    uint32_t tab_used_new = tab_mmap_new->tab_used; \
    /* determine length of old key,value */ \
    uint32_t     key_len = 0 == LEN_LEN ? shf->fixed_key_len :                         SHF_U32_AT(tab_mmap_old, tab_mmap_old->row[row].ref[ref].pos+1                        ) ; \
    uint32_t     val_len = 0 == LEN_LEN ? shf->fixed_val_len :                         SHF_U32_AT(tab_mmap_old, tab_mmap_old->row[row].ref[ref].pos+1+LEN_LEN+key_len        ) ; \
    const char * key     =                                     SHF_CAST(const char *, &SHF_U08_AT(tab_mmap_old, tab_mmap_old->row[row].ref[ref].pos+1+LEN_LEN                )); \
    const char * val     =                                     SHF_CAST(const char *, &SHF_U08_AT(tab_mmap_old, tab_mmap_old->row[row].ref[ref].pos+1+LEN_LEN+key_len+LEN_LEN)); \
    /* copy data from old tab to new tab */ \
    shf_debug_disabled ++; \
    SHF_TAB_APPEND(shf, tab, tab_mmap_new, LEN_LEN, key, key_len, tab_used_new); \
    shf_debug_disabled --; \
    SHF_TAB_REF_MARK_AS_DELETED(tab_mmap_old, LEN_LEN); \
    /* copy ref from old tab to new tab */ \
    tab_mmap_new->row[row].ref[ref].pos = tab_used_new; \
    tab_mmap_new->row[row].ref[ref].tab = tab_mmap_old->row[row].ref[ref].tab; \
    tab_mmap_new->row[row].ref[ref].rnd = tab_mmap_old->row[row].ref[ref].rnd; \
    tab_mmap_new->tab_refs_used ++;

#ifdef SHF_DEBUG_VERSION
#define SHF_LOCK_DEBUG_LINE(LOCK)        (LOCK)->line = __LINE__;
#define SHF_LOCK_DEBUG_MACRO(LOCK,MACRO) (LOCK)->line = __LINE__; (LOCK)->macro = MACRO;
#else
#define SHF_LOCK_DEBUG_LINE(ARGS...)
#define SHF_LOCK_DEBUG_MACRO(LOCK,MACRO)
#endif

#ifdef SHF_DEBUG_VERSION
static void
shf_tab_validate(SHF * shf, SHF_TAB_MMAP * tab_mmap, uint32_t tab_size, uint32_t win, uint16_t tab)
{
    SHF_DEBUG("%s(tab_mmap=%p, tab_size=%u, win=%u, tab=%u) {\n", __FUNCTION__, tab_mmap, tab_size, win, tab);
    uint32_t len_len        = shf->is_fixed_key_val_len ? 0 : sizeof(shf->fixed_key_len);
    uint32_t refs_validated = 0;
    for (uint32_t row = 0; row < SHF_ROWS_PER_TAB; row ++) {
        for (uint32_t ref = 0; ref < SHF_REFS_PER_ROW; ref ++) {
            uint32_t pos = tab_mmap->row[row].ref[ref].pos;
            if (pos) { /* if ref */
                refs_validated ++;
                uint32_t key_len = 0 == len_len ? shf->fixed_key_len : SHF_U32_AT(tab_mmap, pos+1                );
                uint32_t val_len = 0 == len_len ? shf->fixed_val_len : SHF_U32_AT(tab_mmap, pos+1+len_len+key_len);
                //debug shf_debug_disabled --; SHF_DEBUG("row %4u, ref %2d: pos %u, key_len %u\n", row, ref, pos, key_len); shf_debug_disabled ++;
                SHF_ASSERT(key_len                               != 0       , "INTERNAL: VALIDATION FAILURE: expected key_len > 0 at win %u, tab %u, row %u, ref %u, pos %u\n"                                                                      , win, tab, row, ref, pos                   );
                SHF_ASSERT(pos                                   <= tab_size, "INTERNAL: VALIDATION FAILURE: expected pos < %u but pos is %u at win %u, tab %u, row %u, ref %u, pos %u\n"            , tab_size, pos                                , win, tab, row, ref, pos                   );
                SHF_ASSERT(pos+1+len_len+key_len                 <= tab_size, "INTERNAL: VALIDATION FAILURE: expected key < %u but pos is %u at win %u, tab %u, row %u, ref %u, pos %u; key_len %u\n", tab_size, pos+len_len+key_len                , win, tab, row, ref, pos, key_len          );
                SHF_ASSERT(pos+1+len_len+key_len+len_len+val_len <= tab_size, "INTERNAL: VALIDATION FAILURE: expected val < %u but pos is %u at win %u, tab %u, row %u, ref %u, pos %u; xxx_len %u\n", tab_size, pos+len_len+key_len+len_len+val_len, win, tab, row, ref, pos, key_len + val_len);
            }
        }
    }
    SHF_DEBUG("- refs_validated=%u\n", refs_validated);
    SHF_DEBUG("}\n");
} /* shf_tab_validate() */
#endif

static void
shf_tab_shrink(SHF * shf, uint32_t win, uint16_t tab)
{
    SHF_DEBUG("%s(shf=?, win=%u, tab=%u) {\n", __FUNCTION__, win, tab);

    shf->shf_mmap->wins[win].tabs_shrunk ++;

    SHF_TAB_MMAP * tab_mmap;
    SHF_GET_TAB_MMAP(shf, tab);
    SHF_TAB_MMAP * tab_mmap_old = tab_mmap;

    {
        SHF_DEBUG("- un-linking  old tab %u; %u bytes; missing on disk but still memory mapped for now!\n", tab, shf->tabs[win][tab].tab_size);
        char file_tab[256];
        SHF_SNPRINTF(0, file_tab, "%s/%s.shf/%03u/%04u.tab", shf->path, shf->name, win, tab);
        int value = unlink(file_tab); SHF_ASSERT(0 == value, "unlink(): %u: ", errno);
        shf->tabs[win][tab].tab_mmap = 0;
    }

    SHF_DEBUG("- re-creating new tab %u\n", tab);
    shf_tab_create(shf->path, shf->name, win, tab, 1 /* show */, 0 /* no temp/mkdir */);
    SHF_GET_TAB_MMAP(shf, tab);
    SHF_TAB_MMAP * tab_mmap_new = tab_mmap;

    uint32_t len_len = shf->is_fixed_key_val_len ? 0 : sizeof(shf->fixed_key_len);
    SHF_DEBUG("- copying %u bytes data from old tab (excluding %u bytes marked as deleted) to new tab\n", tab_mmap_old->tab_data_used, tab_mmap_old->tab_data_free);
    for (uint32_t row = 0; row < SHF_ROWS_PER_TAB; row ++) {
        for (uint32_t ref = 0; ref < SHF_REFS_PER_ROW; ref ++) {
            if (tab_mmap_old->row[row].ref[ref].pos) { /* if ref */
                SHF_TAB_REF_COPY(len_len);
            }
        }
    }
    SHF_DEBUG_FILE("pid %5u, win-tab %u-%u: shrunk from %7u to %7u bytes; deleting old tab\n", getpid(), win, tab, tab_mmap_old->tab_size, tab_mmap_new->tab_size);
    uint32_t tab_size_old = tab_mmap_old->tab_size;
    tab_mmap_old->tab_size = 1; /* force other processes to munmap() this file & load the replacement */
    int value = munmap(tab_mmap_old, tab_size_old); SHF_ASSERT(0 == value, "munmap(): %u", errno);
    shf->count_mmap --; /* because we re-created it (see above) */

#ifdef SHF_DEBUG_VERSION
    shf_tab_validate(shf, tab_mmap_new, shf->tabs[win][tab].tab_size, win, tab);
#endif
} /* shf_tab_shrink() */

static void
shf_tab_part(SHF * shf, uint32_t win, uint16_t tab_old)
{
    uint16_t tab_new = shf->shf_mmap->wins[win].tabs_used;
    SHF_DEBUG_FILE("pid %5u, win-tab %u-%u: parting to new tab %u\n", getpid(), win, tab_old, tab_new);

    SHF_DEBUG("%s(shf=?, win=%u, tab_old=%u) {\n", __FUNCTION__, win, tab_old);

    shf_tab_create(shf->path, shf->name, win, tab_new, 1 /* show */, 0 /* no temp/mkdir */);

    SHF_TAB_MMAP * tab_mmap;
    SHF_GET_TAB_MMAP(shf, tab_new);

    shf->shf_mmap->wins[win].tabs_parted ++;
    shf->shf_mmap->wins[win].tabs_used   ++;

    SHF_DEBUG("- parting #%lu: tab redirects\n", shf->shf_mmap->wins[win].tabs_parted);
    SHF_ASSERT(tab_new < SHF_TABS_PER_WIN, "ERROR: tab overflow; too many keys? consider sharding between multiple sharedhashfiles?");
    uint32_t tab_switch = 0;
    for (uint16_t tab2 = 0; tab2 < SHF_TABS_PER_WIN; tab2++) {
        uint16_t   tab =  shf->shf_mmap->wins[win].tabs[tab2].tab;
        SHF_ASSERT(tab <  tab_new, "INTERNAL: expected tab < %u but got %u @ win %u, tab2 %u\n", tab_new, tab, win, tab2);
        if        (tab == tab_old) {
            shf->shf_mmap->wins[win].tabs[tab2].tab = tab_switch ? tab_new : tab_old;
            tab_switch                              = tab_switch ? 0       : 1      ;
        }
    }

#ifdef SHF_DEBUG_VERSION
    uint64_t tabs_parted_old = shf->shf_mmap->wins[win].tabs_parted_old;
    uint64_t tabs_parted_new = shf->shf_mmap->wins[win].tabs_parted_new;
#endif
    SHF_DEBUG("- parting #%lu: tab refs\n", shf->shf_mmap->wins[win].tabs_parted);
    uint32_t       len_len      = shf->is_fixed_key_val_len ? 0 : sizeof(shf->fixed_key_len);
    SHF_TAB_MMAP * tab_mmap_old = shf->tabs[win][tab_old].tab_mmap;
    SHF_TAB_MMAP * tab_mmap_new = shf->tabs[win][tab_new].tab_mmap;
    for (uint32_t row = 0; row < SHF_ROWS_PER_TAB; row ++) {
        for (uint32_t ref = 0; ref < SHF_REFS_PER_ROW; ref ++) {
            if (tab_mmap_old->row[row].ref[ref].pos) { /* if ref */
                uint16_t tab2 = tab_mmap_old->row[row].ref[ref].tab;
                uint16_t tab  = shf->shf_mmap->wins[win].tabs[tab2].tab;
                SHF_ASSERT( tab < SHF_TABS_PER_WIN             , "INTERNAL: expected tab < %u but got %u\n", SHF_TABS_PER_WIN, tab);
                SHF_ASSERT((tab == tab_old) || (tab == tab_new), "INTERNAL: expected tab %u or %u but got %u during parting @ row %u, ref %u with tab2 %u\n", tab_old, tab_new, tab, row, ref, tab2);
                if (tab == tab_new) {
                    shf->shf_mmap->wins[win].tabs_parted_new ++;
                    SHF_TAB_REF_COPY(len_len);
                }
                else {
                    shf->shf_mmap->wins[win].tabs_parted_old ++;
                }
            }
        }
    }
    SHF_DEBUG("- parted  #%lu: tab refs; %lu in old & %lu in new tab\n", shf->shf_mmap->wins[win].tabs_parted, shf->shf_mmap->wins[win].tabs_parted_old - tabs_parted_old, shf->shf_mmap->wins[win].tabs_parted_new - tabs_parted_new);

    SHF_DEBUG_FILE("pid %5u, win-tab %u-%u: shrink after part\n", getpid(), win, tab_old);
    shf_tab_shrink(shf, win, tab_old);
} /* shf_tab_part() */

uint32_t /* SHF_UID; SHF_UID_NONE means something went wrong and key not appended */
shf_put_key_val(
    SHF        * shf    ,
    const char * val    , /* NULL means reserve val_len bytes */
    uint32_t     val_len)
{
    SHF_UID uid;

    uid.as_u32 = SHF_UID_NONE;

    SHF_ASSERT_INTERNAL(shf, "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");

    uint32_t len_len = shf->is_fixed_key_val_len ? 0 : sizeof(shf->fixed_key_len);

SHF_NEED_NEW_TAB_AFTER_PARTING:;

    // todo: consider implementing maximum size for shf here

    uint32_t win  = shf_hash.u16[0] %             SHF_WINS_PER_SHF       ;
    uint32_t tab2 = shf_hash.u16[1] %             SHF_TABS_PER_WIN       ;
    uint32_t row  = shf_hash.u16[2] %             SHF_ROWS_PER_TAB       ;
    uint32_t rnd  = shf_hash.u32[2] % (1 << (32 - SHF_TABS_PER_WIN_BITS));

    if (shf->is_lockable) { SHF_LOCK_WRITER(&shf->shf_mmap->wins[win].lock); }
    SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);

    uint16_t tab = shf->shf_mmap->wins[win].tabs[tab2].tab;

    SHF_TAB_MMAP * tab_mmap;
    SHF_GET_TAB_MMAP(shf, tab);
    SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);

    for (uint32_t ref = 0; ref < SHF_REFS_PER_ROW; ref ++) {
        if (tab_mmap->row[row].ref[ref].pos) {
            /* row used */
        }
        else {
            uid.as_part.win = win;
            uid.as_part.tab = tab2;
            uid.as_part.row = row;
            uid.as_part.ref = ref;
            uint32_t pos = tab_mmap->tab_used;
            SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
            SHF_TAB_APPEND(shf, tab, tab_mmap, len_len, shf_key, shf_key_len, pos);
            SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
            tab_mmap->row[row].ref[ref].pos = pos;
            tab_mmap->row[row].ref[ref].tab = tab2;
            tab_mmap->row[row].ref[ref].rnd = rnd;
            goto SHF_SKIP_ROW_FULL_CHECK;
        }
    }

    SHF_DEBUG("- row full; parting tab\n");
    SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
    shf_tab_part(shf, win, tab);
    SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
    if (shf->is_lockable) { SHF_UNLOCK_WRITER(&shf->shf_mmap->wins[win].lock); }
    goto SHF_NEED_NEW_TAB_AFTER_PARTING;

SHF_SKIP_ROW_FULL_CHECK:;

    if (tab_mmap->tab_data_free > (tab_mmap->tab_data_used * 20 / 100)) {
        SHF_DEBUG_FILE("pid %5u, win-tab %u-%u: shrink after put\n", getpid(), win, tab);
        SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
        shf_tab_shrink(shf, win, tab);
        SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
    }

    if (shf->is_lockable) { SHF_UNLOCK_WRITER(&shf->shf_mmap->wins[win].lock); }

    SHF_DEBUG("%s(shf=?, val=?, val_len=%u){} // return 0x%08x=%02x-%03x-%03x-%01x=%s\n", __FUNCTION__, val_len, uid.as_u32, uid.as_part.win, uid.as_part.tab, uid.as_part.row, uid.as_part.ref, SHF_UID_NONE == uid.as_u32 ? "failure" : "success");

    return uid.as_u32;
} /* shf_put_val() */

typedef enum SHF_FIND_KEY_AND {
    SHF_FIND_KEY_OR_UID              = 0,
    SHF_FIND_KEY_OR_UID_AND_COPY_VAL    ,
    SHF_FIND_KEY_OR_UID_AND_DELETE      ,
    SHF_FIND_KEY_OR_UID_AND_UPDATE
} SHF_FIND_KEY_AND;

static int /* bit0=0 means key does not exist, bit0=1 means key found, bit1=1 means callback error */
shf_find_key_internal(
    SHF              * shf ,
    uint32_t           uid ,
    SHF_FIND_KEY_AND   what)
{
    int           result = 0;
    SHF_UID       tmp_uid   ;
    uint32_t      win       ;
    uint32_t      tab2      ;
    uint32_t      row       ;
    uint32_t      rnd       ;
    uint16_t      tab       ;
    SHF_DATA_TYPE data_type ;
    uint32_t      ref       ;
    uint32_t      pos       ;
    uint32_t      key_len   ;
    uint32_t      val_len   ;

    SHF_ASSERT_INTERNAL(shf, "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");

    uint32_t len_len = shf->is_fixed_key_val_len ? 0 : sizeof(shf->fixed_key_len);

    if (SHF_UID_NONE == uid) {
        win  = tmp_uid.as_part.win = shf_hash.u16[0] %             SHF_WINS_PER_SHF       ;
        tab2 = tmp_uid.as_part.tab = shf_hash.u16[1] %             SHF_TABS_PER_WIN       ;
        row  = tmp_uid.as_part.row = shf_hash.u16[2] %             SHF_ROWS_PER_TAB       ;
        rnd  =                       shf_hash.u32[2] % (1 << (32 - SHF_TABS_PER_WIN_BITS));
    }
    else {
               tmp_uid.as_u32 = uid;
        win  = tmp_uid.as_part.win;
        tab2 = tmp_uid.as_part.tab;
        row  = tmp_uid.as_part.row;
    }

    if    ((SHF_FIND_KEY_OR_UID              == what)
    ||     (SHF_FIND_KEY_OR_UID_AND_COPY_VAL == what)) { if (shf->is_lockable) { SHF_LOCK_READER(&shf->shf_mmap->wins[win].lock); }}
    else /* SHF_FIND_KEY_OR_UID_AND_(DELETE|UPDATE) */ { if (shf->is_lockable) { SHF_LOCK_WRITER(&shf->shf_mmap->wins[win].lock); }}
    SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);

    tab = shf->shf_mmap->wins[win].tabs[tab2].tab; /* important that this is looked up after the lock! */

    SHF_TAB_MMAP * tab_mmap;
    SHF_GET_TAB_MMAP(shf, tab);
    SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);

    if (SHF_UID_NONE == uid) {
        for (ref = 0; ref < SHF_REFS_PER_ROW; ref ++) { /* search for ref in row */
            if ((tab_mmap->row[row].ref[ref].pos != 0   ) /* if ref in row is valid looking key... */
            &&  (tab_mmap->row[row].ref[ref].rnd == rnd )
            &&  (tab_mmap->row[row].ref[ref].tab == tab2)) {
                SHF_DEBUG("- todo: use SHF_DATA_TYPE instead of hard coding\n");
                pos              =  tab_mmap->row[row].ref[ref].pos; SHF_ASSERT(pos < tab_mmap->tab_size, "INTERNAL: expected pos < %u but pos is %u at win %u, tab %u\n", tab_mmap->tab_size, pos, win, tab);
                data_type.as_u08 =  SHF_U08_AT(tab_mmap, pos);
                if (0 == len_len) { key_len = shf->fixed_key_len         ; val_len =  shf->fixed_val_len                         ; }
                else              { key_len = SHF_U32_AT(tab_mmap, pos+1); val_len =  SHF_U32_AT(tab_mmap, pos+1+len_len+key_len); }
                SHF_ASSERT(pos+1+len_len+key_len                 <= tab_mmap->tab_size, "INTERNAL: expected key < %u but pos is %u at win %u, tab %u; pos %u, len_len %u, key_len %u, val_len %u\n", tab_mmap->tab_size, pos+1+len_len+key_len                , win, tab, pos, len_len, key_len, val_len);
                SHF_ASSERT(pos+1+len_len+key_len+len_len+val_len <= tab_mmap->tab_size, "INTERNAL: expected val < %u but pos is %u at win %u, tab %u; pos %u, len_len %u, key_len %u, val_len %u\n", tab_mmap->tab_size, pos+1+len_len+key_len+len_len+val_len, win, tab, pos, len_len, key_len, val_len);
                shf_val_addr = &SHF_U08_AT(tab_mmap, pos+1+len_len+key_len+len_len);
                SHF_UNUSE(data_type); // todo: remove hard coding of types
                if (key_len != shf_key_len                                              ) { shf->shf_mmap->wins[win].keylen_misses ++; continue; }
                if (0       != SHF_CMP_AT(tab_mmap, pos+1+len_len, shf_key_len, shf_key)) { shf->shf_mmap->wins[win].memcmp_misses ++; continue; }
                result = 1; /* aka key found */
                break;
            }
        }
    }
    else {
        ref  = tmp_uid.as_part.ref;
        if ((tab_mmap->row[row].ref[ref].pos != 0   ) /* if uid points to valid looking ref... */
        &&  (tab_mmap->row[row].ref[ref].tab == tab2)) {
            SHF_DEBUG("- todo: use SHF_DATA_TYPE instead of hard coding\n");
            pos              =  tab_mmap->row[row].ref[ref].pos; SHF_ASSERT(pos < tab_mmap->tab_size, "INTERNAL: expected pos < %u but pos is %u at win %u, tab %u\n", tab_mmap->tab_size, pos, win, tab);
            data_type.as_u08 =  SHF_U08_AT(tab_mmap, pos);
            if (0 == len_len) { key_len = shf->fixed_key_len         ; val_len =  shf->fixed_val_len                         ; }
            else              { key_len = SHF_U32_AT(tab_mmap, pos+1); val_len =  SHF_U32_AT(tab_mmap, pos+1+len_len+key_len); }
            SHF_ASSERT(pos+1+len_len+key_len                 <= tab_mmap->tab_size, "INTERNAL: expected key < %u but pos is %u at win %u, tab %u; pos %u, len_len %u, key_len %u, val_len %u\n", tab_mmap->tab_size, pos+1+len_len+key_len                , win, tab, pos, len_len, key_len, val_len);
            SHF_ASSERT(pos+1+len_len+key_len+len_len+val_len <= tab_mmap->tab_size, "INTERNAL: expected val < %u but pos is %u at win %u, tab %u; pos %u, len_len %u, key_len %u, val_len %u\n", tab_mmap->tab_size, pos+1+len_len+key_len+len_len+val_len, win, tab, pos, len_len, key_len, val_len);
            shf_val_addr = &SHF_U08_AT(tab_mmap, pos+1+len_len+key_len+len_len);
            SHF_UNUSE(data_type); // todo: remove hard coding of types
            result = 1; /* aka uid found */
        }
    }

    if (1 == result) {
        SHF_DEBUG("- found %lu bytes for key @ 0x%02x-%03x[%03x]-%03x-%x // key,value are %u,%u bytes @ pos %u\n", sizeof(SHF_DATA_TYPE) + len_len + key_len + len_len + val_len, win, tab2, tab, row, ref, key_len, val_len, pos);
        if (SHF_FIND_KEY_OR_UID == what) {
            /* nothing to do here! */
        }
        else if (SHF_FIND_KEY_OR_UID_AND_COPY_VAL == what) {
            if (val_len > shf_val_size) {
                SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
                shf_val = mremap(shf_val, shf_val_size, SHF_MOD_PAGE(val_len), MREMAP_MAYMOVE); SHF_ASSERT(MAP_FAILED != shf_val, "mremap(): %u: ", errno);
                shf_val_size = SHF_MOD_PAGE(val_len);
                SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
            }
            memcpy(shf_val, shf_val_addr, val_len);
            shf_val_len = val_len;

            if (tab_mmap->tab_data_free > (tab_mmap->tab_data_used / 4)) { // todo: allow flexibility WRT how garbage collection gets triggered
                SHF_DEBUG_FILE("pid %5u, win-tab %u-%u: shrink after get\n", getpid(), win, tab);
                SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
                shf_tab_shrink(shf, win, tab);
                SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
            }
        }
        else if (SHF_FIND_KEY_OR_UID_AND_DELETE == what) {
            SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
            SHF_TAB_REF_MARK_AS_DELETED(tab_mmap, len_len);
            SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
        }
        else if (SHF_FIND_KEY_OR_UID_AND_UPDATE == what) {
            shf_upd_callback_failsafe ++;
            SHF_SYSLOG_ASSERT_INTERNAL(1 == shf_upd_callback_failsafe, "ERROR: %s() recursive call detected! shf_upd*() functions should never use themselves recursively!", __FUNCTION__);
            result |= (*shf_upd_callback)(SHF_CAST(char *, shf_val_addr), val_len);
            shf_upd_callback_failsafe --;
        }
    }
    if    ((SHF_FIND_KEY_OR_UID              == what)
    ||     (SHF_FIND_KEY_OR_UID_AND_COPY_VAL == what)) { if (shf->is_lockable) { SHF_UNLOCK_READER(&shf->shf_mmap->wins[win].lock); }}
    else /* SHF_FIND_KEY_OR_UID_AND_(DELETE|UPDATE) */ { if (shf->is_lockable) { SHF_UNLOCK_WRITER(&shf->shf_mmap->wins[win].lock); }}

    SHF_DEBUG("%s(shf=?){} // return %u; 0x%08x=%02x-%03x[%03x]-%03x-%01x=%s\n", __FUNCTION__, result, tmp_uid.as_u32, tmp_uid.as_part.win, tmp_uid.as_part.tab, tab, tmp_uid.as_part.row, tmp_uid.as_part.ref, result ? "exists" : "not exists");

    return result;
} /* shf_find_key_internal() */

void * shf_get_key_val_addr(SHF * shf              ) { return shf_find_key_internal(shf, SHF_UID_NONE, SHF_FIND_KEY_OR_UID             ) ? shf_val_addr : NULL; } /* 64bit address of val itself                                                            */ // todo: add shf_freeze() to make addr safer to use; disables key,val locking!
void * shf_get_uid_val_addr(SHF * shf, uint32_t uid) { return shf_find_key_internal(shf,     uid     , SHF_FIND_KEY_OR_UID             ) ? shf_val_addr : NULL; } /* 64bit address of val itself                                                            */ // todo: add shf_freeze() to make addr safer to use; disables key,val locking!
int    shf_get_key_val_copy(SHF * shf              ) { return shf_find_key_internal(shf, SHF_UID_NONE, SHF_FIND_KEY_OR_UID_AND_COPY_VAL)                      ; } /* bit0=0 means key does not exist, bit0=1 means key found                                */
int    shf_get_uid_val_copy(SHF * shf, uint32_t uid) { return shf_find_key_internal(shf,     uid     , SHF_FIND_KEY_OR_UID_AND_COPY_VAL)                      ; } /* bit0=0 means key does not exist, bit0=1 means key found                                */
int    shf_del_key_val     (SHF * shf              ) { return shf_find_key_internal(shf, SHF_UID_NONE, SHF_FIND_KEY_OR_UID_AND_DELETE  )                      ; } /* bit0=0 means key does not exist, bit0=1 means key deleted                              */
int    shf_del_uid_val     (SHF * shf, uint32_t uid) { return shf_find_key_internal(shf,     uid     , SHF_FIND_KEY_OR_UID_AND_DELETE  )                      ; } /* bit0=0 means key does not exist, bit0=1 means key deleted                              */
int    shf_upd_key_val     (SHF * shf              ) { return shf_find_key_internal(shf, SHF_UID_NONE, SHF_FIND_KEY_OR_UID_AND_UPDATE  )                      ; } /* bit0=0 means key does not exist, bit0=1 means key updated, bit1=1 means callback error */
int    shf_upd_uid_val     (SHF * shf, uint32_t uid) { return shf_find_key_internal(shf,     uid     , SHF_FIND_KEY_OR_UID_AND_UPDATE  )                      ; } /* bit0=0 means key does not exist, bit0=1 means key updated, bit1=1 means callback error */

void
shf_upd_callback_set(int (*shf_upd_callback_new)(const char * val, uint32_t val_len))
{
    shf_upd_callback          = shf_upd_callback_new;
    shf_upd_callback_failsafe = 0                 ;
} /* shf_upd_callback_set() */

int
shf_upd_callback_copy(const char * val, uint32_t val_len)
{
    int result = 0;
    if (0 == shf_upd_callback_failsafe) {
        SHF_DEBUG("%s(val=?, val_len=%u){} // return %u; user land presetting val and val_len\n", __FUNCTION__, val_len, result);
        shf_upd_callback_copy_val     = val    ;
        shf_upd_callback_copy_val_len = val_len;
    }
    else {
        result = (val_len == shf_upd_callback_copy_val_len) ? result : 2;
        SHF_DEBUG("%s(val=?, val_len=%u){} // return %u; SHF callback copying preset val with preset val_len %u inplace\n", __FUNCTION__, val_len, result, shf_upd_callback_copy_val_len);
        if (val_len == shf_upd_callback_copy_val_len) {
            memcpy(SHF_CAST(char *, val), shf_upd_callback_copy_val, shf_upd_callback_copy_val_len);
        }
    }
    return result;
} /* shf_upd_callback_copy() */

void   shf_debug_verbosity_less(void) { shf_debug_disabled ++; }
void   shf_debug_verbosity_more(void) { shf_debug_disabled --; }

char *
shf_del( /* shf_detach() & then delete the folder structure on /dev/shm or disk */
    SHF      * shf)
{
    SHF_DEBUG("%s(shf=?)\n", __FUNCTION__);
    SHF_ASSERT_INTERNAL(shf, "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");

    char du_rm_folder[256]; SHF_SNPRINTF(1, du_rm_folder, "du -h -d 0 %s/%s.shf ; rm -rf %s/%s.shf/", shf->path, shf->name, shf->path, shf->name);

    shf_detach(shf);

    char * du_rm_output = shf_backticks(du_rm_folder);

    // todo: double check that rm command worked

    return du_rm_output;
} /* shf_del() */

uint64_t
shf_debug_get_garbage( /* get total bytes marked as deleted aka garbage */
    SHF * shf)
{
    SHF_ASSERT_INTERNAL(shf, "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");
    uint64_t all_data_free = 0;
    for (uint32_t win = 0; win < SHF_WINS_PER_SHF; win++) {
        uint32_t tabs_used = shf->shf_mmap->wins[win].tabs_used;
        for (uint32_t tab = 0; tab < tabs_used; tab++) {
            all_data_free += shf->tabs[win][tab].tab_mmap->tab_data_free;
        }
    }
    return all_data_free;
} /* shf_debug_get_garbage() */

void
shf_set_data_need_factor(
    uint32_t data_needed_factor)
{
    SHF_DEBUG("%s(data_needed_factor=%u){}\n", __FUNCTION__, data_needed_factor);
    SHF_ASSERT(shf_data_needed_factor > 0, "ERROR: data_needed_factor must be > 0");
    shf_data_needed_factor = data_needed_factor;
} /* shf_set_data_need_factor() */

void
shf_set_is_lockable(
    SHF      * shf       ,
    uint32_t   is_lockable)
{
    SHF_ASSERT_INTERNAL(shf             , "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");
    SHF_ASSERT_INTERNAL(is_lockable <= 1, "ERROR: is_lockable must be 0 or 1");
    shf->is_lockable = is_lockable;
} /* shf_set_is_lockable() */

void
shf_set_is_fixed_len(
    SHF      * shf,
    uint32_t   fixed_key_len,
    uint32_t   fixed_val_len)
{
    SHF_ASSERT_INTERNAL(shf, "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");
    shf->fixed_key_len        = fixed_key_len;
    shf->fixed_val_len        = fixed_val_len;
    shf->is_fixed_key_val_len = 1;
} /* shf_set_is_fixed_len() */

/**
 * @brief Get a set of -- already created -- queue items & queues for those items to be pulled and pushed to.
 * - Sets the thread local variable @ref shf_qiid_addr to point to the first byte of the queue items array.
 * - Sets the thread local variable @ref shf_qiid_addr_len the length in bytes of the queue items array.
 * - Normally called by `Process B`; see @ref ipc_sec.
 *
 * @param[in] shf      Attached SHF.
 * @retval    Pointer  Process specific pointer to the array of queue items.
 *
 * Example usage:
 * @code
 * char * qiid_array = shf_q_get(shf);
 * @endcode
 */
void *
shf_q_get(
    SHF * shf)
{
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__qs"             )); SHF_ASSERT(                              shf_get_key_val_copy(shf) , "ERROR: could not get key '%s'", "__qs"             ); shf->q.qs              = SHF_CAST(uint32_t *, shf_val)[0];
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__q_items"        )); SHF_ASSERT(                              shf_get_key_val_copy(shf) , "ERROR: could not get key '%s'", "__q_items"        ); shf->q.q_items         = SHF_CAST(uint32_t *, shf_val)[0];
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__q_item_size"    )); SHF_ASSERT(                              shf_get_key_val_copy(shf) , "ERROR: could not get key '%s'", "__q_item_size"    ); shf->q.q_item_size     = SHF_CAST(uint32_t *, shf_val)[0];
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__qids_nolock_max")); SHF_ASSERT(                              shf_get_key_val_copy(shf) , "ERROR: could not get key '%s'", "__qids_nolock_max"); shf->q.qids_nolock_max = SHF_CAST(uint32_t *, shf_val)[0];
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__qids"           )); SHF_ASSERT(NULL != (shf->q.qids        = shf_get_key_val_addr(shf)), "ERROR: could not get key '%s'", "__qids"           );
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__qiids"          )); SHF_ASSERT(NULL != (shf->q.qiids       = shf_get_key_val_addr(shf)), "ERROR: could not get key '%s'", "__qiids"          );
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__q_lock"         )); SHF_ASSERT(NULL != (shf->q.q_lock      = shf_get_key_val_addr(shf)), "ERROR: could not get key '%s'", "__q_lock"         );
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__q_items_addr"   )); SHF_ASSERT(NULL != (shf->q.q_item_addr = shf_get_key_val_addr(shf)), "ERROR: could not get key '%s'", "__q_items_addr"   );

    shf_qiid_addr     = shf->q.q_item_addr                 ;
    shf_qiid_addr_len = shf->q.q_item_size * shf->q.q_items;

    SHF_DEBUG("%s(shf=?){} // 0x%p\n", __FUNCTION__, shf->q.q_item_addr);

#ifdef SHF_DEBUG_VERSION
    SHF_ASSERT(1234567 == shf->q.q_lock->debug_magic, "ERROR: INTERNAL: shf->q.q_lock->debug_magic has unexpected value %u\n", shf->q.q_lock->debug_magic);
#endif

#ifdef SHF_DEBUG_VERSION
    SHF_ASSERT(NULL == shf->q.qids_nolock_push, "ERROR: did %s() accidentally get called twice?", __FUNCTION__);
    SHF_ASSERT(NULL == shf->q.qids_nolock_pull, "ERROR: did %s() accidentally get called twice?", __FUNCTION__);
#endif
    shf->q.qids_nolock_push = malloc(shf->q.qs * sizeof(SHF_QID_MMAP)); shf->count_xalloc ++; SHF_ASSERT(shf->q.qids_nolock_push, "ERROR: malloc(%lu): %u", shf->q.qs * sizeof(SHF_QID_MMAP), errno);
    shf->q.qids_nolock_pull = malloc(shf->q.qs * sizeof(SHF_QID_MMAP)); shf->count_xalloc ++; SHF_ASSERT(shf->q.qids_nolock_pull, "ERROR: malloc(%lu): %u", shf->q.qs * sizeof(SHF_QID_MMAP), errno);
    for (uint32_t i = 0; i < shf->q.qs; i++) {
        shf->q.qids_nolock_push[i].tail = SHF_QID_NONE;
        shf->q.qids_nolock_push[i].head = SHF_QID_NONE;
        shf->q.qids_nolock_push[i].size = 0           ;
        shf->q.qids_nolock_pull[i].tail = SHF_QID_NONE;
        shf->q.qids_nolock_pull[i].head = SHF_QID_NONE;
        shf->q.qids_nolock_pull[i].size = 0           ;
    }

    shf->q.q_is_ready = 1;

    return shf->q.q_item_addr;
} /* shf_q_get() */

/**
 * @brief Delete the set of queue items & queues for an SHF instance.
 * - Normally called by `Process A`; see @ref ipc_sec.
 *
 * @param[in] shf  Attached SHF.
 *
 * Example usage:
 * @code
 * shf_q_del(shf);
 * @endcode
 */
void
shf_q_del(
    SHF * shf)
{
    SHF_ASSERT_INTERNAL(shf->q.q_is_ready, "ERROR: have you called shf_q_(new|get)()?");

    shf_make_hash(SHF_CONST_STR_AND_SIZE("__qs"          )); SHF_ASSERT(shf_del_key_val(shf), "ERROR: could not del key '%s'", "__qs"          );
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__q_items"     )); SHF_ASSERT(shf_del_key_val(shf), "ERROR: could not del key '%s'", "__q_items"     );
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__q_item_size" )); SHF_ASSERT(shf_del_key_val(shf), "ERROR: could not del key '%s'", "__q_item_size" );
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__qids"        )); SHF_ASSERT(shf_del_key_val(shf), "ERROR: could not del key '%s'", "__qids"        );
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__qiids"       )); SHF_ASSERT(shf_del_key_val(shf), "ERROR: could not del key '%s'", "__qiids"       );
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__q_lock"      )); SHF_ASSERT(shf_del_key_val(shf), "ERROR: could not del key '%s'", "__q_lock"      );
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__q_items_addr")); SHF_ASSERT(shf_del_key_val(shf), "ERROR: could not del key '%s'", "__q_items_addr");

    if (shf->q.qids_nolock_push) { free(shf->q.qids_nolock_push); shf->count_xalloc --; }
    if (shf->q.qids_nolock_pull) { free(shf->q.qids_nolock_pull); shf->count_xalloc --; }

    shf->q.q_is_ready = 0;

    SHF_DEBUG("%s(shf=?){}\n", __FUNCTION__);
} /* shf_q_del() */

/**
 * @brief Create a set of queue items & queues for those items to be pulled and pushed to.
 * - Sets the thread local variable @ref shf_qiid_addr to point to the first byte of the queue items array.
 * - Sets the thread local variable @ref shf_qiid_addr_len the length in bytes of the queue items array.
 * - Normally called by `Process A`; see @ref ipc_sec.
 * - Currently only one set of queues can be created per SHF instance.
 *
 * @param[in] shf              Attached SHF.
 * @param[in] qs               Number of queues to create.
 * @param[in] q_items          Number of queue items to create.
 * @param[in] q_item_size      Size of each queue item in bytes.
 * @param[in] qids_nolock_max  Number of queue items to push or pull without locking, e.g. 1.
 * @retval    Pointer          Process specific pointer to the array of queue items.
 *
 * Example usage:
 * @code
 * char * qiid_array = shf_q_new(shf, 3, 10, 1024, 1); // Create 10 qiids of size 1024 bytes each, to be shared between 3 qids; don't batch up locking
 * @endcode
 */
void *
shf_q_new(
    SHF      * shf            ,
    uint32_t   qs             ,
    uint32_t   q_items        ,
    uint32_t   q_item_size    ,
    uint32_t   qids_nolock_max)
{
    shf->q.qs              = qs             ;
    shf->q.q_items         = q_items        ;
    shf->q.q_item_size     = q_item_size    ;
    shf->q.qids_nolock_max = qids_nolock_max;

    SHF_ASSERT_INTERNAL(shf->q.q_is_ready == 0      , "ERROR: shf_q_new() already called"       );
    SHF_ASSERT_INTERNAL(qs                >  2      , "ERROR: qs must be > 2"                   );
    SHF_ASSERT_INTERNAL(q_items           >  1      , "ERROR: q_items must be > 2"              );
    SHF_ASSERT_INTERNAL(qids_nolock_max   >  0      , "ERROR: qids_nolock_max must be > 0"      );
    SHF_ASSERT_INTERNAL(qids_nolock_max   <  q_items, "ERROR: qids_nolock_max must be < q_items");

    shf_debug_verbosity_less();
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__qs"             )); uint32_t uid_qs              = shf_put_key_val(shf, SHF_CAST(const char *, &qs             ),           sizeof(qs             ));
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__q_items"        )); uint32_t uid_q_items         = shf_put_key_val(shf, SHF_CAST(const char *, &q_items        ),           sizeof(q_items        ));
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__q_item_size"    )); uint32_t uid_q_item_size     = shf_put_key_val(shf, SHF_CAST(const char *, &q_item_size    ),           sizeof(q_item_size    ));
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__qids_nolock_max")); uint32_t uid_qids_nolock_max = shf_put_key_val(shf, SHF_CAST(const char *, &qids_nolock_max),           sizeof(qids_nolock_max));
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__qids"           )); uint32_t uid_qids            = shf_put_key_val(shf, NULL                                    , qs      * sizeof(SHF_QID_MMAP   ));
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__qiids"          )); uint32_t uid_qiids           = shf_put_key_val(shf, NULL                                    , q_items * sizeof(SHF_QIID_MMAP  ));
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__q_lock"         )); uint32_t uid_q_lock          = shf_put_key_val(shf, NULL                                    ,           sizeof(SHF_Q_LOCK_MMAP));
    shf_make_hash(SHF_CONST_STR_AND_SIZE("__q_items_addr"   )); uint32_t uid_q_items_addr    = shf_put_key_val(shf, NULL                                    , q_items * q_item_size           ) ;
    shf_debug_verbosity_more();

    SHF_ASSERT(SHF_UID_NONE != uid_qs             , "ERROR: could not put key: __qs"             );
    SHF_ASSERT(SHF_UID_NONE != uid_q_items        , "ERROR: could not put key: __q_items"        );
    SHF_ASSERT(SHF_UID_NONE != uid_q_item_size    , "ERROR: could not put key: __q_item_size"    );
    SHF_ASSERT(SHF_UID_NONE != uid_qids_nolock_max, "ERROR: could not put key: __qids_nolock_max");
    SHF_ASSERT(SHF_UID_NONE != uid_qids           , "ERROR: could not put key: __qids"           );
    SHF_ASSERT(SHF_UID_NONE != uid_qiids          , "ERROR: could not put key: __qiids"          );
    SHF_ASSERT(SHF_UID_NONE != uid_q_lock         , "ERROR: could not put key: __q_lock"         );
    SHF_ASSERT(SHF_UID_NONE != uid_q_items_addr   , "ERROR: could not put key: __q_items_addr"   );

    // todo: freeze at this point; all memory has been allocated

    /* add qiid q items to default qid q 0 */
    shf->q.qids_nolock_push = malloc(shf->q.qs * sizeof(SHF_QID_MMAP)); shf->count_xalloc ++; SHF_ASSERT(shf->q.qids_nolock_push, "ERROR: malloc(%lu): %u", shf->q.qs * sizeof(SHF_QID_MMAP), errno);
    shf->q.qids_nolock_pull = malloc(shf->q.qs * sizeof(SHF_QID_MMAP)); shf->count_xalloc ++; SHF_ASSERT(shf->q.qids_nolock_pull, "ERROR: malloc(%lu): %u", shf->q.qs * sizeof(SHF_QID_MMAP), errno);
    shf->q.qids             = shf_get_uid_val_addr(shf, uid_qids        );
    shf->q.qiids            = shf_get_uid_val_addr(shf, uid_qiids       );
    shf->q.q_lock           = shf_get_uid_val_addr(shf, uid_q_lock      );
    shf->q.q_item_addr      = shf_get_uid_val_addr(shf, uid_q_items_addr);
    for (uint32_t i = 0; i < shf->q.qs; i++) {
        shf->q.qids_nolock_push[i].tail = SHF_QID_NONE;
        shf->q.qids_nolock_push[i].head = SHF_QID_NONE;
        shf->q.qids_nolock_push[i].size = 0           ;
        shf->q.qids_nolock_pull[i].tail = SHF_QID_NONE;
        shf->q.qids_nolock_pull[i].head = SHF_QID_NONE;
        shf->q.qids_nolock_pull[i].size = 0           ;
        shf->q.qids            [i].tail = SHF_QID_NONE;
        shf->q.qids            [i].head = SHF_QID_NONE;
        shf->q.qids            [i].size = 0           ;
    }
    for (uint32_t i = 0; i < q_items; i++) {
        shf->q.qiids[i].last = i - 1;
        shf->q.qiids[i].next = i + 1;

        SHF_ASSERT(shf->q.q_item_size >= 9, "ERROR: q_item_size is %u but must be at least 4", q_item_size);
        snprintf(&shf->q.q_item_addr[i * shf->q.q_item_size], 9, "%08x", i);
    }
    shf->q.qiids[0          ].last = SHF_QID_NONE;
    shf->q.qiids[q_items - 1].next = SHF_QID_NONE;
    shf->q.qids[0].tail            = 0           ;
    shf->q.qids[0].head            = q_items - 1 ;
    shf->q.qids[0].size            = q_items     ;

    shf_qiid_addr     = shf->q.q_item_addr                 ;
    shf_qiid_addr_len = shf->q.q_item_size * shf->q.q_items;

    SHF_DEBUG("%s(shf=?, qs=%u, q_items=%u, q_item_size=%u){} // 0x%p\n", __FUNCTION__, qs, q_items, q_item_size, shf->q.q_item_addr);

#ifdef SHF_DEBUG_VERSION
    shf->q.q_lock->debug_magic = 1234567;
#endif

    shf->q.q_is_ready = 1;

    return shf->q.q_item_addr;
} /* shf_q_new() */

/**
 * @brief Associate an unused qid with an ascii name.
 * - After calling shf_q_new(), call this function q_items times.
 * - Normally called by `Process A`; see @ref ipc_sec.
 *
 * @param[in] shf       Attached SHF.
 * @param[in] name      Ascii name of qid.
 * @param[in] name_len  Length of ascii name in bytes.
 * @retval    qid       qid which is now used.
 *
 * Example usage:
 * @code
 * uint32_t qid = shf_q_new_name(shf, "qid-free");
 * @endcode
 */
uint32_t
shf_q_new_name(
    SHF        * shf     ,
    const char * name    ,
    uint32_t     name_len)
{
    SHF_ASSERT_INTERNAL(shf              , "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");
    SHF_ASSERT_INTERNAL(shf->q.q_is_ready, "ERROR: have you called shf_q_(new|get)()?");

    uint32_t qid = shf->q.q_next; /* todo: enforce this function only called by the queue creator */

    shf->q.q_next ++; SHF_ASSERT(shf->q.q_next <= shf->q.qs, "ERROR: called %s() %u times but only allocated %u queues", __FUNCTION__, shf->q.q_next, shf->q.qs);

    shf_debug_verbosity_less();
    shf_make_hash(name, name_len);
    SHF_ASSERT_INTERNAL(0 == shf_get_key_val_copy(shf), "ERROR: %s(): unique queue name already exists!", __FUNCTION__);
    uint32_t uid_qid = shf_put_key_val(shf, SHF_CAST(const char *, &qid), sizeof(qid));
    SHF_ASSERT(SHF_UID_NONE != uid_qid, "ERROR: could not put key '%.*s'", name_len, name);
    // todo: somehow store name for use in debug messages
    shf_debug_verbosity_more();

    SHF_DEBUG("%s(shf=?, name='%.*s', name_len=%u){} // qid %u\n", __FUNCTION__, name_len, name, name_len, qid);

    return qid;
} /* shf_q_new_name() */

/**
 * @brief Associate an unused qid with an ascii name.
 * - After calling shf_q_get(), call this function for each expected qid.
 * - Normally called by `Process B`; see @ref ipc_sec.
 *
 * @param[in] shf       Attached SHF.
 * @param[in] name      Ascii name of qid.
 * @param[in] name_len  Length of ascii name in bytes.
 * @retval    qid       qid which is now used.
 *
 * Example usage:
 * @code
 * uint32_t qid = shf_q_get_name(shf, "qid-free");
 * @endcode
 */
uint32_t
shf_q_get_name(
    SHF        * shf     ,
    const char * name    ,
    uint32_t     name_len)
{
    SHF_ASSERT_INTERNAL(shf              , "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");
    SHF_ASSERT_INTERNAL(shf->q.q_is_ready, "ERROR: have you called shf_q_(new|get)()?");

    uint32_t qid;

    shf_debug_verbosity_less();
    shf_make_hash(name, name_len); SHF_ASSERT(shf_get_key_val_copy(shf), "ERROR: could not get key '%.*s'", name_len, name);
    shf_debug_verbosity_more();

    SHF_ASSERT(sizeof(qid) == shf_val_len, "ERROR: expected key '%.*s' to have size %lu but got size %u", name_len, name, sizeof(qid), shf_val_len);
    qid = SHF_CAST(uint32_t *, shf_val)[0];
    SHF_ASSERT(qid < shf->q.qs, "ERROR: expected 0 <= qid < %u but got %u; did you call shf_q_get() or shf_q_new()?", shf->q.qs, qid);

    SHF_DEBUG("%s(shf=?, name='%.*s', name_len=%u){} // qid %u\n", __FUNCTION__, name_len, name, name_len, qid);

    return qid;
} /* shf_q_get_name() */

// before:                  tail     next     ...      head
// before:                  ----     ----     ----     ----
// before: SHF_QID_NONE <-- last <-- last <-- last <-- last
// before:                  next --> next --> next --> next --> SHF_QID_NONE

// after :                           tail     ...      head
// after :                           ----     ----     ----
// after : SHF_QID_NONE <--      <-- last <-- last <-- last
// after :                           next --> next --> next --> SHF_QID_NONE

/**
 * @brief Take a qiid from a qid.
 *
 * @param[in] shf             Attached SHF.
 * @param[in] qiid            qiid to remove from (maybe middle) of qid double linked list.
 * @retval    #SHF_QIID_NONE  This function is not implemented yet!
 *
 * Example usage:
 * @code
 * uint32_t qiid = shf_q_take_item(shf, qiid);
 * @endcode
 */
uint32_t
shf_q_take_item(
    SHF      * shf ,
    uint32_t   qiid)
{
    SHF_ASSERT_INTERNAL(shf              , "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");
    SHF_ASSERT_INTERNAL(shf->q.q_is_ready, "ERROR: have you called shf_q_(new|get)()?");

    SHF_UNUSE(qiid);

    // todo: implement shf_q_take_item()

    return SHF_QIID_NONE;
} /* shf_q_take_item() */

/**
 * @brief Pull a qiid off qid tail.
 * - Sets the thread local variable @ref shf_qiid          to the qiid pulled if found, else #SHF_QIID_NONE.
 * - Sets the thread local variable @ref shf_qiid_addr     to point to the first byte of the queue item if found, else NULL.
 * - Sets the thread local variable @ref shf_qiid_addr_len to the length in bytes of the queue item if found, else zero.
 * - For a faster function see shf_q_push_head_pull_tail(); see @ref ipc_sec for why.
 *
 * @param[in] shf             Attached SHF.
 * @param[in] pull_qid        qid to try and pull qiid from.
 * @retval    qiid            qiid pulled from qid.
 * @retval    #SHF_QIID_NONE  If no qiid available.
 *
 * Example usage:
 * @code
 * uint32_t qiid = shf_q_pull_tail(shf, qid_free);
 * @endcode
 */
uint32_t
shf_q_pull_tail(
    SHF      * shf     ,
    uint32_t   pull_qid)
{
    SHF_ASSERT_INTERNAL(shf              , "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");
    SHF_ASSERT_INTERNAL(shf->q.q_is_ready, "ERROR: have you called shf_q_(new|get)()?");

    uint32_t pull_qiid = SHF_QIID_NONE;

#ifdef SHF_DEBUG_VERSION
    SHF_ASSERT(pull_qid  < shf->q.qs, "ERROR: expected 0 <= pull_qid < %u but got %u", shf->q.qs, pull_qid );
#endif

    if (shf->q.qids_nolock_pull[pull_qid].size == 0) {
        /* come here if local pull queue is emtpy */
        shf_q_flush(shf, pull_qid);
    }

    if (shf->q.qids_nolock_pull[pull_qid].size == 0) {
        shf_qiid          = pull_qiid;
        shf_qiid_addr     = NULL;
        shf_qiid_addr_len = 0;
#ifdef SHF_DEBUG_VERSION
        SHF_DEBUG("%s() // nolock pull q size=%u // nothing to pull\n", __FUNCTION__, shf->q.qids_nolock_pull[pull_qid].size);
#endif
    }
    else {
        shf->q.qids_nolock_pull[pull_qid].size --;

        pull_qiid = shf->q.qids_nolock_pull[pull_qid].tail;
#ifdef SHF_DEBUG_VERSION
        SHF_ASSERT(pull_qiid < shf->q.q_items, "ERROR: INTERNAL: in nolock pull tail qiid is %u but should be < %u\n", pull_qiid, shf->q.q_items);
#endif
        uint32_t qiid_next = shf->q.qiids[pull_qiid].next;
        shf->q.qids_nolock_pull[pull_qid].tail = qiid_next;
        if (SHF_QIID_NONE == qiid_next) { shf->q.qids_nolock_pull[pull_qid].head = SHF_QIID_NONE; }
        else                            { shf->q.qiids[qiid_next].last           = SHF_QIID_NONE; }
#ifdef SHF_DEBUG_VERSION
        SHF_DEBUG("%s() // nolock pull q size=%u, pull_qid=%u, qiid=%u, qiid_next=%u, head=%u, tail=%u\n", __FUNCTION__, shf->q.qids_nolock_pull[pull_qid].size, pull_qid, pull_qiid, qiid_next, shf->q.qids_nolock_pull[pull_qid].head, shf->q.qids_nolock_pull[pull_qid].tail);
#endif

        shf_qiid          =                       pull_qiid                      ;
        shf_qiid_addr     = shf->q.q_item_addr + (pull_qiid * shf->q.q_item_size);
        shf_qiid_addr_len =                                   shf->q.q_item_size ;
    }

    return pull_qiid;
} /* shf_q_pull_tail() */

/**
 * @brief Push a qiid on qid head.
 * - Sets the thread local variable @ref shf_qiid          to the qiid pulled if found, else #SHF_QIID_NONE.
 * - Sets the thread local variable @ref shf_qiid_addr     to point to the first byte of the queue item if found, else NULL.
 * - Sets the thread local variable @ref shf_qiid_addr_len to the length in bytes of the queue item if found, else zero.
 * - For a faster function see shf_q_push_head_pull_tail(); see @ref ipc_sec for why.
 *
 * @param[in] shf             Attached SHF.
 * @param[in] push_qid        qid to push on to.
 * @param[in] push_qiid       qiid to push.
 * @retval    qiid            qiid pulled from qid.
 * @retval    #SHF_QIID_NONE  If no qiid available.
 *
 * Example usage:
 * @code
 * uint32_t qiid = shf_q_pull_tail(shf, qid_free);
 * @endcode
 */
void
shf_q_push_head(
    SHF      * shf      ,
    uint32_t   push_qid ,
    uint32_t   push_qiid)
{
    SHF_ASSERT_INTERNAL(shf              , "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");
    SHF_ASSERT_INTERNAL(shf->q.q_is_ready, "ERROR: have you called shf_q_(new|get)()?");

#ifdef SHF_DEBUG_VERSION
                                      SHF_ASSERT(push_qid  < shf->q.qs     , "ERROR: expected 0 <= push_qid < %u but got %u" , shf->q.qs     , push_qid );
    if (SHF_QIID_NONE != push_qiid) { SHF_ASSERT(push_qiid < shf->q.q_items, "ERROR: expected 0 <= push_qiid < %u but got %u", shf->q.q_items, push_qiid); }
#endif

    if (SHF_QIID_NONE != push_qiid) {
        shf->q.qids_nolock_push[push_qid].size ++;

        uint32_t qiid_last = shf->q.qids_nolock_push[push_qid].head;
        shf->q.qiids[push_qiid].last = qiid_last;
        shf->q.qiids[push_qiid].next = SHF_QIID_NONE;
        shf->q.qids_nolock_push[push_qid].head = push_qiid;
        if (SHF_QIID_NONE == qiid_last) { shf->q.qids_nolock_push[push_qid].tail = push_qiid; }
        else                            { shf->q.qiids[qiid_last].next           = push_qiid; }
#ifdef SHF_DEBUG_VERSION
        SHF_DEBUG("%s() // nolock push q size=%u, push_qid=%u, push_qiid=%u, qiid_last=%u, head=%u, tail=%u\n", __FUNCTION__, shf->q.qids_nolock_push[push_qid].size, push_qid, push_qiid, qiid_last, shf->q.qids_nolock_push[push_qid].head, shf->q.qids_nolock_push[push_qid].tail);
#endif

        if (shf->q.qids_nolock_push[push_qid].size >= shf->q.qids_nolock_max) {
            /* come here if local push queue is full */
            shf_q_flush(shf, SHF_QID_NONE);
        }
    }
} /* shf_q_push_head() */

/**
 * @brief Diagnostic function for debugging to show qid queue sizes.
 * - Shows the queue size for queue qid, and its associated 'nolock' push & pull queues; ; see @ref ipc_sec.
 *
 * @param[in] shf  Attached SHF.
 * @param[in] qid  qid to show sizes for.
 */
void
shf_q_size( /* this function only used for debugging... */
    SHF      * shf,
    uint32_t   qid)
{
    SHF_UNUSE(qid); /* only used in debug version */

    SHF_ASSERT_INTERNAL(shf              , "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");
    SHF_ASSERT_INTERNAL(shf->q.q_is_ready, "ERROR: have you called shf_q_(new|get)()?");

    shf_debug_verbosity_less(); SHF_LOCK_WRITER(&shf->q.q_lock->lock); shf_debug_verbosity_more();

    SHF_DEBUG("%s(shf=?, qid=%u) // shf->q.qids_nolock_push[qid].size=%u, shf->q.qids[qid].size=%u, shf->q.qids_nolock_pull[qid].size=%u\n", __FUNCTION__, qid, shf->q.qids_nolock_push[qid].size, shf->q.qids[qid].size, shf->q.qids_nolock_pull[qid].size);

    shf_debug_verbosity_less(); SHF_UNLOCK_WRITER(&shf->q.q_lock->lock); shf_debug_verbosity_more();
} /* shf_q_size() */

/**
 * @brief Flushes nolock push & pull queues.
 * - This function is intended for internal use. Cannot think of a use case for the caller to use it.
 * - See @ref ipc_sec regarding nolock push & pull queues.
 *
 * @param[in] shf       Attached SHF.
 * @param[in] pull_qid  qid to flush from.
 */
void
shf_q_flush(
    SHF      * shf     ,
    uint32_t   pull_qid)
{
    SHF_ASSERT_INTERNAL(shf              , "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");
    SHF_ASSERT_INTERNAL(shf->q.q_is_ready, "ERROR: have you called shf_q_(new|get)()?");

    shf_debug_verbosity_less(); SHF_LOCK_WRITER(&shf->q.q_lock->lock); shf_debug_verbosity_more();

#ifdef SHF_DEBUG_VERSION
    SHF_DEBUG("%s(shf=?, pull_qid=%u) // shf->q.qids[pull_qid].size=%u, shf->q.qids_nolock_pull[pull_qid].size=%u\n", __FUNCTION__, pull_qid, SHF_QID_NONE == pull_qid ? 0 : shf->q.qids[pull_qid].size, SHF_QID_NONE == pull_qid ? 0 : shf->q.qids_nolock_pull[pull_qid].size);
#endif

    /* empty nolock push queues */
    for (uint32_t push_qid = 0; push_qid < shf->q.qs; push_qid++) { /* foreach q: */
        if (shf->q.qids_nolock_push[push_qid].size > 0) { /* push this nolock onto that locked q */
#ifdef SHF_DEBUG_VERSION
            SHF_DEBUG("%s() // push nolock q to locked q; push_qid=%u, shf->q.qids[push_qid].size=%u, shf->q.qids_nolock_push[push_qid].size=%u\n", __FUNCTION__, push_qid, shf->q.qids[push_qid].size, shf->q.qids_nolock_push[push_qid].size);
#endif
            uint32_t qiid_this_tail = shf->q.qids_nolock_push[push_qid].tail;
            uint32_t qiid_this_head = shf->q.qids_nolock_push[push_qid].head;
#ifdef SHF_DEBUG_VERSION
            SHF_ASSERT_INTERNAL(SHF_QIID_NONE != qiid_this_tail, "ERROR: INTERNAL: SHF_QIID_NONE == qiid_this_tail // nolock to lock push");
            SHF_ASSERT_INTERNAL(SHF_QIID_NONE != qiid_this_head, "ERROR: INTERNAL: SHF_QIID_NONE == qiid_this_head // nolock to lock push");
            SHF_ASSERT_INTERNAL(shf->q.q_items > qiid_this_tail, "ERROR: INTERNAL: in nolock push tail qiid is %u but should be < %u\n", qiid_this_tail, shf->q.q_items);
            SHF_ASSERT_INTERNAL(shf->q.q_items > qiid_this_head, "ERROR: INTERNAL: in nolock push head qiid is %u but should be < %u\n", qiid_this_head, shf->q.q_items);
#endif
            uint32_t qiid_last = shf->q.qids[push_qid].head;
            shf->q.qiids[qiid_this_tail].last = qiid_last;
            shf->q.qiids[qiid_this_head].next = SHF_QIID_NONE;
            shf->q.qids[push_qid].head = qiid_this_head;
            if (SHF_QIID_NONE == qiid_last) { shf->q.qids[push_qid].tail   = qiid_this_tail; }
            else                            { shf->q.qiids[qiid_last].next = qiid_this_tail; }
#ifdef SHF_DEBUG_VERSION
            SHF_DEBUG("%s() // locked push q size=%u, push_qid=%u, +size=%u, qiid_last=%u, head=%u, tail=%u\n", __FUNCTION__, shf->q.qids[push_qid].size, push_qid, shf->q.qids_nolock_push[push_qid].size, qiid_last, shf->q.qids[push_qid].head, shf->q.qids[push_qid].tail);
#endif

            shf->q.qids[push_qid].size             += shf->q.qids_nolock_push[push_qid].size;
            shf->q.qids_nolock_push[push_qid].size  = 0;
            shf->q.qids_nolock_push[push_qid].tail  = SHF_QIID_NONE;
            shf->q.qids_nolock_push[push_qid].head  = SHF_QIID_NONE;
        }
    }

    /* fill nolock pull queue from locked queue if possible */
    if (SHF_QID_NONE != pull_qid) {
        uint32_t qiids_to_pull_max = shf->q.qids_nolock_pull[pull_qid].size < shf->q.qids_nolock_max ? shf->q.qids_nolock_max - shf->q.qids_nolock_pull[pull_qid].size : 0                   ;
        uint32_t qiids_to_pull     = shf->q.qids[pull_qid].size             < qiids_to_pull_max      ?                          shf->q.qids[pull_qid].size             : qiids_to_pull_max   ;
        if (qiids_to_pull > 0) {
#ifdef SHF_DEBUG_VERSION
            SHF_DEBUG("%s() // pull locked q to nolock q; pull_qid=%u, shf->q.qids[pull_qid].size=%u, shf->q.qids_nolock_pull[pull_qid].size=%u, qiids_to_pull=%u\n", __FUNCTION__, pull_qid, shf->q.qids[pull_qid].size, shf->q.qids_nolock_pull[pull_qid].size, qiids_to_pull);
#endif
            uint32_t qiid_this_tail = shf->q.qids[pull_qid].tail;
            uint32_t qiid_this_head = shf->q.qids[pull_qid].tail; /* start off at tail */
#ifdef SHF_DEBUG_VERSION
            SHF_ASSERT_INTERNAL(SHF_QIID_NONE != qiid_this_tail, "ERROR: INTERNAL: SHF_QIID_NONE == qiid_this_tail // lock to nolock pull");
            SHF_ASSERT_INTERNAL(SHF_QIID_NONE != qiid_this_head, "ERROR: INTERNAL: SHF_QIID_NONE == qiid_this_head // lock to nolock pull");
            SHF_ASSERT_INTERNAL(shf->q.q_items > qiid_this_tail, "ERROR: INTERNAL: in nolock pull tail qiid is %u but should be < %u\n", qiid_this_tail, shf->q.q_items);
            SHF_ASSERT_INTERNAL(shf->q.q_items > qiid_this_head, "ERROR: INTERNAL: in nolock pull head qiid is %u but should be < %u\n", qiid_this_head, shf->q.q_items);
#endif
            for (uint32_t i = 0; i < (qiids_to_pull - 1); i++) { /* walk locked qiids until qiid_this_head is qiids_to_pull qiids along */
                uint32_t qiid_this_next = shf->q.qiids[qiid_this_head].next;
#ifdef SHF_DEBUG_VERSION
                SHF_ASSERT_INTERNAL(SHF_QIID_NONE != qiid_this_next, "ERROR: INTERNAL: SHF_QIID_NONE == qiid_this_next // locked walk @ i=%u of %u, qiid_this_tail=%u, qiid_this_head=%u", i, qiids_to_pull, qiid_this_tail, qiid_this_head);
                SHF_ASSERT_INTERNAL(shf->q.q_items > qiid_this_next, "ERROR: INTERNAL: next qiid is %u but should be < %u // locked walk @ i=%u of %u, qiid_this_tail=%u, qiid_this_head=%u\n", qiid_this_next, shf->q.q_items, i, qiids_to_pull, qiid_this_tail, qiid_this_head);
#endif
                qiid_this_head = qiid_this_next;
            }

            uint32_t qiid_last_nolock = shf->q.qids_nolock_pull[pull_qid].head;
            uint32_t qiid_next_locked = shf->q.qiids[qiid_this_head].next;
            shf->q.qiids[qiid_this_tail].last = qiid_last_nolock;
            shf->q.qiids[qiid_this_head].next = SHF_QIID_NONE;
            shf->q.qids_nolock_pull[pull_qid].head = qiid_this_head;
            if (SHF_QIID_NONE == qiid_last_nolock) { shf->q.qids_nolock_pull[pull_qid].tail = qiid_this_tail; }
            else                                   { shf->q.qiids[qiid_last_nolock].next    = qiid_this_tail; }
#ifdef SHF_DEBUG_VERSION
            SHF_DEBUG("%s() // nolock pull q size=%u, pull_qid=%u, +size=%u, qiid_last_nolock=%u, head=%u, tail=%u\n", __FUNCTION__, shf->q.qids_nolock_pull[pull_qid].size, pull_qid, qiids_to_pull, qiid_last_nolock, shf->q.qids[pull_qid].head, shf->q.qids[pull_qid].tail);
#endif

            shf->q.qids_nolock_pull[pull_qid].size += qiids_to_pull;
            shf->q.qids[pull_qid].size             -= qiids_to_pull;
            if (shf->q.qids[pull_qid].size) { shf->q.qids[pull_qid].tail = qiid_next_locked; }
            else                            { shf->q.qids[pull_qid].tail = SHF_QIID_NONE   ;
                                              shf->q.qids[pull_qid].head = SHF_QIID_NONE   ; }
        } /* if (qiids_to_pull > 0) */
    } /* if (SHF_QID_NONE != pull_qid)() */

    shf_debug_verbosity_less(); SHF_UNLOCK_WRITER(&shf->q.q_lock->lock); shf_debug_verbosity_more();
} /* shf_q_flush() */

/**
 * @brief If push_qiid is not #SHF_QIID_NONE then push it onto push_qid, then pull a qiid off pull_qid tail.
 * - Sets the thread local variable @ref shf_qiid          to the qiid pulled if found, else #SHF_QIID_NONE.
 * - Sets the thread local variable @ref shf_qiid_addr     to point to the first byte of the queue item if found, else NULL.
 * - Sets the thread local variable @ref shf_qiid_addr_len to the length in bytes of the queue item if found, else zero.
 * - This is faster than shf_q_push_head() & shf_q_pull_tail(); see @ref ipc_sec for why.
 *
 * @param[in] shf             Attached SHF.
 * @param[in] push_qid        qid to push on to.
 * @param[in] push_qiid       qiid to push.
 * @param[in] pull_qid        qid to try and pull qiid from.
 * @retval    qiid            qiid pulled from pull_qid.
 * @retval    #SHF_QIID_NONE  If no qiid available.
 *
 * Example usage:
 * @code
 * shf_qiid = SHF_QIID_NONE;
 * while(SHF_QIID_NONE != shf_q_push_head_pull_tail(shf, qid_b2a, shf_qiid, qid_a2b)) {
 *     // your business logic here
 * }
 * @endcode
 */
uint32_t
shf_q_push_head_pull_tail(
    SHF      * shf      ,
    uint32_t   push_qid ,
    uint32_t   push_qiid,
    uint32_t   pull_qid )
{
    SHF_ASSERT_INTERNAL(shf              , "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");
    SHF_ASSERT_INTERNAL(shf->q.q_is_ready, "ERROR: have you called shf_q_(new|get)()?");

    uint32_t pull_qiid = SHF_QIID_NONE;

#ifdef SHF_DEBUG_VERSION
                                      SHF_ASSERT(push_qid  < shf->q.qs     , "ERROR: expected 0 <= push_qid < %u but got %u" , shf->q.qs     , push_qid );
    if (SHF_QIID_NONE != push_qiid) { SHF_ASSERT(push_qiid < shf->q.q_items, "ERROR: expected 0 <= push_qiid < %u but got %u", shf->q.q_items, push_qiid); }
                                      SHF_ASSERT(pull_qid  < shf->q.qs     , "ERROR: expected 0 <= pull_qid < %u but got %u" , shf->q.qs     , pull_qid );
#endif

    if (SHF_QIID_NONE != push_qiid) {
        shf->q.qids_nolock_push[push_qid].size ++;

        uint32_t qiid_last = shf->q.qids_nolock_push[push_qid].head;
        shf->q.qiids[push_qiid].last = qiid_last;
        shf->q.qiids[push_qiid].next = SHF_QIID_NONE;
        shf->q.qids_nolock_push[push_qid].head = push_qiid;
        if (SHF_QIID_NONE == qiid_last) { shf->q.qids_nolock_push[push_qid].tail = push_qiid; }
        else                            { shf->q.qiids[qiid_last].next           = push_qiid; }
#ifdef SHF_DEBUG_VERSION
        SHF_DEBUG("%s() // nolock push q size=%u, push_qid=%u, push_qiid=%u, qiid_last=%u, head=%u, tail=%u\n", __FUNCTION__, shf->q.qids_nolock_push[push_qid].size, push_qid, push_qiid, qiid_last, shf->q.qids_nolock_push[push_qid].head, shf->q.qids_nolock_push[push_qid].tail);
#endif
    }

    if ((shf->q.qids_nolock_push[push_qid].size >= shf->q.qids_nolock_max)
    ||  (shf->q.qids_nolock_pull[pull_qid].size == 0                     )) {
        /* come here if local push queue is full or local pull queue is emtpy */
        shf_q_flush(shf, pull_qid);
    }

    if (shf->q.qids_nolock_pull[pull_qid].size == 0) {
        shf_qiid          = pull_qiid;
        shf_qiid_addr     = NULL;
        shf_qiid_addr_len = 0;
#ifdef SHF_DEBUG_VERSION
        SHF_DEBUG("%s() // nolock pull q size=%u // nothing to pull\n", __FUNCTION__, shf->q.qids_nolock_pull[pull_qid].size);
#endif
    }
    else {
        shf->q.qids_nolock_pull[pull_qid].size --;

        pull_qiid = shf->q.qids_nolock_pull[pull_qid].tail;
#ifdef SHF_DEBUG_VERSION
        SHF_ASSERT(pull_qiid < shf->q.q_items, "ERROR: INTERNAL: in nolock pull tail qiid is %u but should be < %u\n", pull_qiid, shf->q.q_items);
#endif
        uint32_t qiid_next = shf->q.qiids[pull_qiid].next;
        shf->q.qids_nolock_pull[pull_qid].tail = qiid_next;
        if (SHF_QIID_NONE == qiid_next) { shf->q.qids_nolock_pull[pull_qid].head = SHF_QIID_NONE; }
        else                            { shf->q.qiids[qiid_next].last           = SHF_QIID_NONE; }
#ifdef SHF_DEBUG_VERSION
        SHF_DEBUG("%s() // nolock pull q size=%u, pull_qid=%u, qiid=%u, qiid_next=%u, head=%u, tail=%u\n", __FUNCTION__, shf->q.qids_nolock_pull[pull_qid].size, pull_qid, pull_qiid, qiid_next, shf->q.qids_nolock_pull[pull_qid].head, shf->q.qids_nolock_pull[pull_qid].tail);
#endif

        shf_qiid          =                       pull_qiid                      ;
        shf_qiid_addr     = shf->q.q_item_addr + (pull_qiid * shf->q.q_item_size);
        shf_qiid_addr_len =                                   shf->q.q_item_size ;
    }

    return pull_qiid;
} /* shf_q_push_head_pull_tail() */

/**
 * @brief Checks if queue has been initialized via shf_q_new() or shf_q_get().
 * - This function is intended for internal use. Cannot think of a use case for the caller to use it.
 *
 * @param[in] shf  Attached SHF.
 * @retval    0    Queue is not initialized.
 * @retval    1    Queue is     initialized.
 */
uint32_t
shf_q_is_ready(SHF * shf)
{
    SHF_ASSERT_INTERNAL(shf, "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");

    return shf->q.q_is_ready;
} /* shf_q_is_ready() */

/**
 * @brief Initialize race between two or more threads and/or processes.
 * - Only one thread or processes should ever call this function; the race official.
 *
 * @param[in] shf       Attached SHF.
 * @param[in] name      Race unique name.
 * @param[in] name_len  Race unique name length.
 */
void
shf_race_init(
    SHF        * shf     ,
    const char * name    ,
    uint32_t     name_len)
{
    SHF_ASSERT_INTERNAL(shf, "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");

                                   shf_make_hash       (name, name_len);
    uint32_t uid                 = shf_put_key_val     (shf, NULL, 1  ); SHF_ASSERT_INTERNAL(SHF_UID_NONE != uid, "ERROR: %s() could not put key: '%.*s'", __FUNCTION__, name_len, name);
#ifdef SHF_DEBUG_VERSION
    volatile uint8_t * race_line = shf_get_key_val_addr(shf           ); SHF_ASSERT_INTERNAL(NULL != race_line, "ERROR: %s() could not get key: '%.*s'", __FUNCTION__, name_len, name);
    SHF_ASSERT_INTERNAL(0 == *race_line, "ERROR: %s() incorrectly initialized value", __FUNCTION__);
#endif
} /* shf_race_init() */

/**
 * @brief Every thread and/or process (aka horse) in the race should call this function.
 * - Once the race official has called shf_race_init() then each competing horse calls shf_race_start().
 * - shf_race_start() only fires the gun once all horses have called shf_race_start().
 * - If one or more horses take too long to call shf_race_start() then the waiting horses will assert.
 * - Once the race is started all competing horses return from shf_race_start() roughly at the same time.
 *
 * @param[in] shf       Attached SHF.
 * @param[in] name      Race unique name.
 * @param[in] name_len  Race unique name length.
 * @param[in] horses    Race horses competing.
 */
void
shf_race_start(
    SHF        * shf     ,
    const char * name    ,
    uint32_t     name_len,
    uint32_t     horses  )
{
    SHF_ASSERT_INTERNAL(shf, "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");

                                   shf_make_hash       (name, name_len);
    volatile uint8_t * race_line = shf_get_key_val_addr(shf); SHF_ASSERT(NULL != race_line, "ERROR: %s() could not get key: '%.*s'", __FUNCTION__, name_len, name);

    double start_time = shf_get_time_in_seconds();
    __sync_fetch_and_add_8(race_line, 1); /* atomic increment */
    while (horses != *race_line) {
        SHF_CPU_PAUSE();
        if (shf_get_time_in_seconds() - start_time > 6.0 /* arbitrary many seconds */) {
            SHF_ASSERT_INTERNAL(0, "ERROR: %s() timeout waiting for %u horses but only got %u", __FUNCTION__, horses, *race_line);
        }
    }
#ifdef SHF_DEBUG_VERSION
    shf_debug_verbosity_more();
    SHF_DEBUG("%s() // %u horses started after %f seconds\n", __FUNCTION__, horses, shf_get_time_in_seconds() - start_time);
    shf_debug_verbosity_less();
#endif
} /* shf_race_start() */

double (* shf_log_get_time_in_seconds)(void) = shf_get_time_in_seconds; /* indirect call for easier unit testing */

#define SHF_LOG_BUFFER_SIZE      4096
#define SHF_LOG_WRITE_THRESHOLD (64 * 1024)
#define SHF_LOG_DEFAULT_SIZE    (64 * 1024 * 10)
#define SHF_LOG_WRITE_INTERVAL   10000      /* usleep interval in shf_log_thread(); usleep(10,000) microseconds means wait 10ms */

static          double   shf_log_time_init                = 0;
static __thread uint32_t shf_log_tid                      = 0;
static __thread uint32_t shf_log_tid_id                   = 0;
static __thread char     shf_log_prefix[128]                 ; /* "000000.000000 12345 "; elapsed seconds & tid */
static __thread uint32_t shf_log_prefix_len                  ;
static __thread uint32_t shf_log_output_indirect_failsafe = 0;

//example static void
//example shf_log_output_stderr(char * log_line, uint32_t log_line_len)
//example {
//example     log_line[0] = '?' == log_line[0] ? '=' : log_line[0]; /* mark as logged output by stderr */
//example     fprintf(stderr, "%.*s", log_line_len, log_line);
//example } /* shf_log_output_stderr() */

void
shf_log_init(void)
{
    shf_log_tid    = 0;
    shf_log_tid_id = 0;
} /* shf_log_init() */

static void
shf_log_output_stdout(char * log_line, uint32_t log_line_len)
{
    log_line[0] = '?' == log_line[0] ? '=' : log_line[0]; /* mark as logged output by stdout */
    fprintf(stdout, "%.*s", log_line_len, log_line);
    fflush(stdout);
} /* shf_log_output_stdout() */

static void
shf_log_output_shf_log(char * log_line, uint32_t log_line_len)
{
    SHF_SYSLOG_ASSERT_INTERNAL(shf_log_thread_instance, "ERROR: shf_log_thread_instance must never be NULL; called shf_log_thread_new()?\n");
    log_line[0] = '?' == log_line[0] ? '#' : log_line[0]; /* mark as logged output by shf_log */
    shf_log_append(shf_log_thread_instance, log_line, log_line_len);
} /* shf_log_output_shf_log() */

void (*shf_log_output_indirect)(char * log_line, uint32_t log_line_len) = shf_log_output_stdout;

static int /* bool */
shf_log_safe_append(char * log_buffer, uint32_t log_buffer_size, uint32_t * index_ptr, int appended)
{
    if ((appended < 0) || ((unsigned)appended >= log_buffer_size - *index_ptr)) {
        log_buffer[log_buffer_size - 4] = '.';
        log_buffer[log_buffer_size - 3] = '.';
        log_buffer[log_buffer_size - 2] = '\n';
        log_buffer[log_buffer_size - 1] = '\0';
        return 0 /* false */;
    }

    *index_ptr += appended;
    return 1 /* true */;
} /* shf_log_safe_append() */

char *
shf_log_prefix_get(void)
{
    const char * tbd = "?";

    shf_log_prefix_len = 0;

    if (0 == shf_log_time_init) {
        shf_log_time_init = shf_get_time_in_seconds();
    }

    double time_elapsed_now = shf_get_time_in_seconds() - shf_log_time_init;

    if (0 == shf_log_tid) {
#ifdef SYS_gettid
        shf_log_tid = syscall(SYS_gettid);
#else
#error "ERROR: SYS_gettid unavailable on this system"
#endif
    }

    if (0 == shf_log_tid_id) {
        if (NULL != shf_log_thread_instance) {
            SHF * shf = shf_log_thread_instance;
            if (shf_log_tid >= sizeof(shf->log->tids)) { /* note: cannot use SHF_ASSERT_INTERNAL() because it itself uses this function to log! */
                printf("%05u:%s: ERROR: assertion: ERROR: internal: tid=%u >= sizeof(shf->log->tids)=%lu\n", __LINE__, __FILE__, shf_log_tid, sizeof(shf->log->tids));
                exit(EXIT_FAILURE);
            }
            if (0 == shf->log->tids[shf_log_tid]) {
                /* come here if no tid id assigned yet */
                shf_debug_verbosity_less();
                SHF_LOCK_WRITER(&shf->log->lock);   /* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */
                if (0 == shf->log->tids[shf_log_tid]) {
                    shf->log->tid_id ++;
                    shf->log->tids[shf_log_tid] = shf->log->tid_id;
                }
                SHF_UNLOCK_WRITER(&shf->log->lock); /* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
                shf_debug_verbosity_more();

                tbd = "#";
                shf_log_safe_append(shf_log_prefix, sizeof(shf_log_prefix), &shf_log_prefix_len, snprintf(&shf_log_prefix[shf_log_prefix_len], sizeof(shf_log_prefix) - shf_log_prefix_len, "?%.6f %u --> auto mapped to thread id %u\n", time_elapsed_now, shf->log->tid_id ? shf->log->tid_id : shf_log_tid, shf_log_tid));
            }
            shf_log_tid_id = shf->log->tids[shf_log_tid];
        }
    }

    /* todo: modify the %u for tid so that if more than 9 threads used then the field grows to at least 2 digits */
    shf_log_safe_append(shf_log_prefix, sizeof(shf_log_prefix), &shf_log_prefix_len,  snprintf(&shf_log_prefix[shf_log_prefix_len], sizeof(shf_log_prefix) - shf_log_prefix_len, "%s%.6f %u ", tbd, time_elapsed_now, shf_log_tid_id ? shf_log_tid_id : shf_log_tid));

    return &shf_log_prefix[0];
} /* shf_log_prefix_get() */

void
shf_log(char * prefix, const char * format_type, int line, const char * file, const char * str_error, const char * eol, int priority, const char * format_user, ...) /* see shf.defines.h for example usage */
{
    va_list      ap;
    char         log_buffer[SHF_LOG_BUFFER_SIZE];
    const char * file_only = strchr(file, '/') ? 1 + strrchr(file, '/') : file; /* if path, remove it */
    uint32_t     i         = 0;

    SHF_UNUSE(format_type); /* todo: decide if we really want to show the source line and *variable* length file */
    SHF_UNUSE(line       ); /* todo: decide if we really want to show the source line and *variable* length file */
    SHF_UNUSE(file_only  ); /* todo: decide if we really want to show the source line and *variable* length file */

    va_start(ap, format_user);

    if (shf_log_safe_append(log_buffer, sizeof(log_buffer), &i,  snprintf(&log_buffer[i], SHF_LOG_BUFFER_SIZE - i, "%s"       , prefix                    ))    /* append log prefix, e.g. seconds & tid  */
/*  &&  shf_log_safe_append(log_buffer, sizeof(log_buffer), &i,  snprintf(&log_buffer[i], SHF_LOG_BUFFER_SIZE - i, format_type, line, file_only           )) */ /* append log type  , e.g. debug or error */
    &&  shf_log_safe_append(log_buffer, sizeof(log_buffer), &i, vsnprintf(&log_buffer[i], SHF_LOG_BUFFER_SIZE - i, format_user, ap                        ))    /* append                user log message */
    &&  shf_log_safe_append(log_buffer, sizeof(log_buffer), &i,  snprintf(&log_buffer[i], SHF_LOG_BUFFER_SIZE - i, "%s"       , str_error ? str_error : ""))    /* append optional   system error message */
    &&  shf_log_safe_append(log_buffer, sizeof(log_buffer), &i,  snprintf(&log_buffer[i], SHF_LOG_BUFFER_SIZE - i, "%s"       , eol       ? eol       : ""))) { /* append optional            eol message */
        /* if we made it to here then SHF_LOG_BUFFER_SIZE is big enough! */
    }

    if (LOG_INFO == priority) {
        shf_log_output_indirect_failsafe ++;
        SHF_SYSLOG_ASSERT_INTERNAL(1 == shf_log_output_indirect_failsafe, "ERROR: %s() recursive call detected! shf_log*() functions should never use themselves to log! tried to log line '%.*s'", __FUNCTION__, i, log_buffer);
        (*shf_log_output_indirect)(log_buffer, i);
        shf_log_output_indirect_failsafe --;
    }
    else {
        setlogmask(LOG_UPTO(LOG_DEBUG));
        openlog(NULL /* use program name */, LOG_CONS | LOG_PID | LOG_NDELAY | LOG_PERROR, LOG_USER);
        syslog(priority, "%.*s", i, log_buffer);
        closelog();
    }

    va_end(ap);
} /* shf_log() */

void
shf_log_output_set(void (*shf_log_output_new)(char * log_line, uint32_t log_line_len)) /* todo: write a test for this! */
{
    shf_log_output_indirect          = shf_log_output_new;
    shf_log_output_indirect_failsafe = 0                 ;
} /* shf_log_output_set() */

#define SHF_LOG_THREAD_WRITE(SHARED_MEMORY_ADDR) \
    SHF_SYSLOG_DEBUG("%s() // attempting to write() %u bytes", __FUNCTION__, log_left); \
    char * log_work = SHARED_MEMORY_ADDR; \
    while (log_left > 0) { \
        shf->log->writing = 1; \
        ssize_t bytes_written = write(shf->log->fd, log_work, log_left); \
        if ((-1 == bytes_written && EAGAIN      == errno) \
        ||  (-1 == bytes_written && EWOULDBLOCK == errno) \
        ||  ( 0 == bytes_written                        )) { \
            last_loop_did_not_block = 0; \
            break; \
        } \
        else if (-1 == bytes_written && EINTR == errno) { \
            SHF_SYSLOG_WARNING("%s() // WARN: write() returned EINTR", __FUNCTION__); \
            break; \
        } \
        else if (bytes_written < 0) { \
            SHF_SYSLOG_WARNING("%s() // WARN: write() returned %ld: errno=%d=%s ", __FUNCTION__, bytes_written, errno, strerror(errno)); \
            shf->log->write_fail = second; /* throttle write errors during this second */ \
            break; \
        } \
        log_left -= bytes_written; \
        log_work += bytes_written; \
        SHF_SYSLOG_DEBUG("%s() // wrote %lu bytes", __FUNCTION__, bytes_written); \
    }

static void *
shf_log_thread(void *arg)
{
    SHF * shf = shf_log_thread_instance;

    SHF_UNUSE(arg);

    SHF_DEBUG("%s(shf=?){}\n", __FUNCTION__);

    SHF_ASSERT_INTERNAL(shf_log_thread_instance, "ERROR: INTERNAL: shf_log_thread_instance is NULL\n");

#ifdef SHF_DEBUG_VERSION
    SHF_ASSERT_INTERNAL(1234567 == shf->log->magic, "ERROR: INTERNAL: shf->log->magic has unexpected value %u\n", shf->log->magic);
#endif

    if (0 == shf->log->time_init) {
        shf->log->time_init = shf_log_time_init ? shf_log_time_init : shf_log_get_time_in_seconds(); /* use static shf_log_time_init if already used */
    }

    uint32_t last_loop_did_not_block = 1;
    while (shf->log->running) {
        if (last_loop_did_not_block)
            usleep(SHF_LOG_WRITE_INTERVAL);

        last_loop_did_not_block = 1;

        uint32_t second = floor(shf_log_get_time_in_seconds() - shf->log->time_init);

        shf_debug_verbosity_less();
        SHF_LOCK_WRITER(&shf->log->lock);   /* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */
        uint32_t log_used = shf->log->used;
        uint32_t log_left = shf->log->used;
        SHF_UNLOCK_WRITER(&shf->log->lock); /* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
        shf_debug_verbosity_more();

        /* NOTE: threads & processes can still log while this thread is writing! */
        if (log_used > 0 && shf->log->write_fail != second) {
            if (log_used > shf->log->used_hi) {
                shf->log->used_hi     = log_used;
                shf->log->used_hi_new = 1;
            }

            if      (shf->log->second != second                 ) {           } /* reached next second so fall thru & write the buffer                  */
            else if (log_used          < SHF_LOG_WRITE_THRESHOLD) { continue; } /* still this   second so only        write the buffer if threshold met */

            SHF_LOG_THREAD_WRITE(&shf->log->bytes[0]);

            if (log_used != log_left) { /* if something written then re-position any log not written */
                shf_debug_verbosity_less();
                SHF_LOCK_WRITER(&shf->log->lock);   /* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */
                uint32_t log_left_during_write = shf->log->used - log_used; /* might have increased during write() */
                shf->log->used = log_left + log_left_during_write;
                memmove(&shf->log->bytes[0], log_work, shf->log->used);
                SHF_UNLOCK_WRITER(&shf->log->lock); /* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
                shf_debug_verbosity_more();
            }
            shf->log->writing = 0;

            if (shf->log->second != second) { /* update log statistics once per second */
                if (shf->log->used_hi_new) {
                    shf->log->used_hi_new = 0;
                    double used_hi_percent = shf->log->used_hi * 100.0 / shf->log->size;
                    if (used_hi_percent > 75.0) { // todo: make this high water mark configurable somehow
                        SHF_SYSLOG_WARNING("%s() // log reached new hi: %u of %u bytes or %f%%; increase log shared memory?\n", __FUNCTION__, shf->log->used_hi, shf->log->size, used_hi_percent);
                    }
                }
                shf->log->second = second;
            }
        }
    } /* while(shf->log->running) */

    uint32_t second   = 0;
    uint32_t log_left = shf->log->used;
    SHF_SYSLOG_DEBUG("%s() // thread closing down; %u bytes log remain unwritten", __FUNCTION__, log_left);
    if (log_left > 0) {
        SHF_LOG_THREAD_WRITE(&shf->log->bytes[0]);
        if (log_left > 0) {
            SHF_SYSLOG_WARNING("%s() // WARN: failed to write %u bytes of remaining log lines when log thread shutting down", __FUNCTION__, log_left);
        }
    }

    shf_log_output_set(shf_log_output_stdout); /* output future log lines to stdout */

    shf->log->stopped ++;

    return NULL;
} /* shf_log_thread() */

void
shf_log_attach_existing(SHF * shf)
{
    SHF_ASSERT_INTERNAL(shf, "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");

    SHF_DEBUG("%s(shf=?)\n", __FUNCTION__);

               shf_debug_verbosity_less();
               shf_make_hash       (SHF_CONST_STR_AND_SIZE("__log"));
    shf->log = shf_get_key_val_addr(shf                            );
               SHF_ASSERT_INTERNAL(NULL != shf->log, "ERROR: shf->log must be not NULL; only call %s() after you have called shf_log_thread_new() in the other thread/process!", __FUNCTION__);
               shf_debug_verbosity_more();

    shf_log_thread_instance = shf;
    shf_log_output_set(shf_log_output_shf_log); /* output future log lines to shared memory */

    shf_log_time_init = shf->log->time_init;
} /* shf_log_attach_existing */

void
shf_log_thread_new(SHF * shf, uint32_t log_size, int log_fd)
{
    SHF_ASSERT_INTERNAL(shf, "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");

    SHF_DEBUG("%s(shf=?, log_size=%u, fd=%u) // SHF_LOG_DEFAULT_SIZE=%u\n", __FUNCTION__, log_size, log_fd, SHF_LOG_DEFAULT_SIZE);

                        shf_debug_verbosity_less();
                        shf_make_hash       (SHF_CONST_STR_AND_SIZE("__log")           );
             shf->log = shf_get_key_val_addr(shf                                       ); SHF_ASSERT_INTERNAL(NULL         == shf->log, "ERROR: shf->log must be NULL; only call %s() once!", __FUNCTION__);
    uint32_t  uid_log = shf_put_key_val     (shf, NULL, sizeof(SHF_LOG_MMAP) + log_size); SHF_ASSERT_INTERNAL(SHF_UID_NONE !=  uid_log, "ERROR: could not put key: __log");
             shf->log = shf_get_uid_val_addr(shf, uid_log                              );
                        shf_debug_verbosity_more();

    // todo: convert __log to being own mmap() where the addr never changes

    shf->log_thread_acive = 1       ;
    shf->log->stopped     = 0       ;
    shf->log->running     = 1       ;
    shf->log->fd          = log_fd  ;
    shf->log->size        = log_size ? log_size : SHF_LOG_DEFAULT_SIZE;
#ifdef SHF_DEBUG_VERSION
    shf->log->magic       = 1234567 ;
#endif

    shf_log_thread_instance = shf;
    shf_log_output_set(shf_log_output_shf_log); /* output future log lines to shared memory */

    pthread_t log_thread;
    errno = pthread_create(&log_thread, NULL, shf_log_thread, NULL); SHF_ASSERT(0 == errno, "pthread_create(): %d: ", errno);

    // todo: atexit for shf_log_thread_del()
} /* shf_log_thread_new() */

void
shf_log_thread_del(SHF * shf)
{
    SHF_ASSERT_INTERNAL(shf, "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");

    SHF_DEBUG("%s(shf=?) // %s\n", __FUNCTION__, shf->log && shf->log->running ? "waiting for log thread to end" : "log thread never run");

    SHF_ASSERT_INTERNAL(shf == shf_log_thread_instance, "ERROR: shf %p != shf_log_thread_instance %p");

    if (shf->log && shf->log->running) {
        shf->log->running = 0;            /* signal log thread to stop */
        while (0 == shf->log->stopped) {} /* wait for log thread to stop using shf */
    }

    shf_log_thread_instance = NULL;
} /* shf_log_thread_del() */

void
shf_log_append(SHF * shf, const char * log_line, uint32_t log_line_len)
{
    SHF_SYSLOG_ASSERT_INTERNAL(shf, "ERROR: shf must not be NULL; have you called shf_attach(_existing)()?");

    SHF_SYSLOG_DEBUG("%s(shf=?, log_line=?, log_line_len=%u)\n", __FUNCTION__, log_line, log_line_len);

    if (log_line_len >= shf->log->size) {
        SHF_SYSLOG_WARNING("%s() // WARN: ignoring log line with size %u >= log size %u\n", __FUNCTION__, log_line_len, shf->log->size);
        return;
    }

#ifdef SHF_DEBUG_VERSION
    SHF_ASSERT(1234567 == shf->log->magic, "ERROR: INTERNAL: shf->log->magic has unexpected value %u\n", shf->log->magic);
#endif

    shf_debug_verbosity_less();

    uint64_t waiting_for_writer_loop    = 1;
    uint64_t waiting_for_writer_loop_hi = 0;
    while   (waiting_for_writer_loop) {
        if  (waiting_for_writer_loop > 1) {
            SHF_SYSLOG_DEBUG("%s() // waiting for writer to finish; increase log shared memory?", __FUNCTION__);
            usleep(SHF_LOG_WRITE_INTERVAL); /* wait for shf_log_thread() to write() more log */
        }

        SHF_SYSLOG_DEBUG("%s() // waiting for lock", __FUNCTION__);
        SHF_LOCK_WRITER(&shf->log->lock);   /* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */

        if (shf->log->size - shf->log->used < shf_log_prefix_len + log_line_len) {
            waiting_for_writer_loop   ++; /* do nothing; loop & wait */
        }
        else {
            SHF_SYSLOG_DEBUG("%s() // memcpy %u bytes of log", __FUNCTION__, log_line_len);
            memcpy(&shf->log->bytes[shf->log->used], &log_line[0], log_line_len);
            shf->log->used += log_line_len;
            waiting_for_writer_loop_hi = waiting_for_writer_loop;
            waiting_for_writer_loop    = 0;
        }

        SHF_UNLOCK_WRITER(&shf->log->lock); /* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
    } /* while (not_appended) */
    if (waiting_for_writer_loop_hi > 1)
        SHF_SYSLOG_WARNING("%s() // waited %lu iterations for writer to finish; increase log shared memory?", __FUNCTION__, waiting_for_writer_loop_hi);

    shf_debug_verbosity_more();
} /* shf_log_append() */

static __thread char     shf_log_fprintf_buffer[SHF_LOG_BUFFER_SIZE];
static __thread uint32_t shf_log_fprintf_buffer_i = 0;

int
shf_log_vfprintf(FILE * stream, const char * format, va_list ap)
{
    int len = shf_log_fprintf_buffer_i;

    SHF_UNUSE(stream);

    shf_log_safe_append(shf_log_fprintf_buffer, sizeof(shf_log_fprintf_buffer), &shf_log_fprintf_buffer_i, vsnprintf(&shf_log_fprintf_buffer[shf_log_fprintf_buffer_i], SHF_LOG_BUFFER_SIZE - shf_log_fprintf_buffer_i, format, ap));
    len = shf_log_fprintf_buffer_i - len;

    if ('\n' == shf_log_fprintf_buffer[shf_log_fprintf_buffer_i - 1]) {
        SHF_PLAIN("%.*s", shf_log_fprintf_buffer_i, &shf_log_fprintf_buffer[0]);
        shf_log_fprintf_buffer_i = 0;
    }

    return len;
} /* shf_log_fprintf() */

void
shf_log_fprintf(FILE * stream, const char * format, ...)
{
    va_list ap;

    SHF_UNUSE(stream);

    va_start(ap, format);
    shf_log_vfprintf(stream, format, ap);
    va_end(ap);
} /* shf_log_fprintf() */

void
shf_log_fputs(const char * string, FILE * stream)
{
    shf_log_fprintf(stream, "%s", string);
} /* shf_log_fputs() */

void
shf_log_fputc(int character, FILE * stream)
{
    shf_log_fprintf(stream, "%c", character);
} /* shf_log_fputc() */
