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
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h> /* for offsetof() */
#include <sys/time.h> /* for gettimeofday() */

#include "shf.private.h"
#include "shf.h"

#include "murmurhash3.h"

                      uint32_t   shf_init_called           = 0   ;
                       int32_t   shf_debug_disabled        = 0   ;
#ifdef SHF_DEBUG_VERSION
static                 FILE    * shf_debug_file            = 0   ;
#endif

       __thread       SHF_HASH   shf_hash                        ;

       __thread       uint32_t   shf_uid                         ;

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

char *
shf_backticks(const char * command) /* e.g. buf_used = shf_backticks("ls -la | grep \.log", buf, sizeof(buf)); */
{
    FILE     * fp;
    uint32_t   bytes_read;

    if (0 == shf_backticks_buffer_size) {
        shf_backticks_buffer_size = SHF_SIZE_PAGE;
        shf_backticks_buffer      = mmap(NULL, shf_backticks_buffer_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE, -1, 0); SHF_ASSERT(MAP_FAILED != shf_backticks_buffer, "mmap(): %u: ", errno);
    }

    fp = popen(command, "r"); SHF_ASSERT(NULL != fp, "popen('%s'): %u: ", command, errno);

    shf_backticks_buffer_used = 0;
    while ((bytes_read = fread(&shf_backticks_buffer[shf_backticks_buffer_used], sizeof(char), (shf_backticks_buffer_size - shf_backticks_buffer_used - 1), fp)) != 0) {
        SHF_DEBUG("read %u bytes from the pipe\n", bytes_read);
        shf_backticks_buffer_used += bytes_read;
        if (shf_backticks_buffer_size - shf_backticks_buffer_used <= 1) {
            shf_backticks_buffer       = mremap(shf_backticks_buffer, shf_backticks_buffer_size, SHF_SIZE_PAGE + shf_backticks_buffer_size, MREMAP_MAYMOVE); SHF_ASSERT(MAP_FAILED != shf_backticks_buffer, "mremap(): %u: ", errno);
            shf_backticks_buffer_size += SHF_SIZE_PAGE;
            SHF_DEBUG("shf_backticks() increased buffer size to %u\n", shf_backticks_buffer_size);
        }
    }

    while (shf_backticks_buffer_used > 0 && ('\n' == shf_backticks_buffer[shf_backticks_buffer_used - 1] || '\r' == shf_backticks_buffer[shf_backticks_buffer_used - 1] || ' ' == shf_backticks_buffer[shf_backticks_buffer_used - 1])) {
        shf_backticks_buffer_used --; /* trim trailing whitespace */
    }
    shf_backticks_buffer[shf_backticks_buffer_used] = '\0';
    int value = pclose(fp); SHF_ASSERT(0 == value, "pclose(): %u: ", errno);

    return shf_backticks_buffer;
} /* shf_backticks() */

double
shf_get_time_in_seconds(void)
{
    struct timeval tv;

    SHF_ASSERT(gettimeofday(&tv, NULL) >= 0, "gettimeofday(): %u: ", errno);
    return (double)tv.tv_sec + 1.e-6 * (double)tv.tv_usec;
} /* shf_get_time_in_seconds() */

void
shf_init(void)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    if (shf_init_called) {
        SHF_DEBUG("%s() already called; early out\n", __FUNCTION__);
        goto EARLY_OUT;
    }
    shf_init_called = 1;

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
shf_detach(
    SHF * shf)
{
    SHF_UNUSE(shf); // todo: implement shf_detach()
    // todo free shf->path
    // todo free shf->name

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
        shf->shf_mmap = mmap(NULL, SHF_MOD_PAGE(sizeof(SHF_SHF_MMAP)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NORESERVE | MAP_POPULATE, fd, 0); SHF_ASSERT(MAP_FAILED != shf->shf_mmap, "mmap(): %u: ", errno);

        int value = close(fd); SHF_ASSERT(-1 != value, "close(): %u: ", errno);

        shf->path = strdup(path);
        shf->name = strdup(name);
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
    const char * path, /* e.g. '/dev/shm' */
    const char * name) /* e.g. 'myshf'    */
{
    char file_name[256];
    char path_name[256];
    int  fd;
    int  value;
    int  tabs = 0;

    SHF_DEBUG("%s(path='%s', name='%s')\n", __FUNCTION__, path, name);
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
        SHF->tabs[win][TAB].tab_mmap = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NORESERVE | MAP_POPULATE, fd, 0); SHF_ASSERT(MAP_FAILED != SHF->tabs[win][TAB].tab_mmap, "mmap(): %u: ", errno); \
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
        SHF->shf_mmap->wins[win].tabs_refs_size += SHF_REFS_PER_TAB; \
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

#define SHF_TAB_APPEND(SHF, TAB, TAB_MMAP, KEY, KEY_LEN) \
    /* todo: examine if file append & remap is faster than remap & direct memory access */ \
    /* todo: consider special mode with is write only, e.g. for initial startup? */ \
    /* todo: faster to use remap_file_pages() instead of multiple mmap()s? */ \
    uint32_t data_needed    = sizeof(SHF_DATA_TYPE) + sizeof(uint32_t) + KEY_LEN + sizeof(uint32_t) + val_len; \
    uint32_t data_available = TAB_MMAP->tab_size - TAB_MMAP->tab_used; \
    SHF_DEBUG("- appending %u bytes for ref @ 0x%02x-xxx[%03x]-%03x-%x // key,value are %u,%u bytes @ pos %u // todo: use SHF_DATA_TYPE instead of hard coding\n", data_needed, win, TAB, row, ref, KEY_LEN, val_len, TAB_MMAP->tab_used); \
    SHF_LOCK_DEBUG_MACRO(&SHF->shf_mmap->wins[win].lock, 1); \
    if (data_needed > data_available) { \
        SHF_LOCK_DEBUG_MACRO(&SHF->shf_mmap->wins[win].lock, 2); \
        uint32_t new_tab_size = SHF_MOD_PAGE(TAB_MMAP->tab_size + (data_needed * shf_data_needed_factor)); \
        char file_tab[256]; \
        SHF_SNPRINTF(0, file_tab, "%s/%s.shf/%03u/%04u.tab", SHF->path, SHF->name, win, TAB); \
        SHF_DEBUG("- grow tab from %u to %u; need %u bytes but %u bytes available in '%s'\n", TAB_MMAP->tab_size, new_tab_size, data_needed, data_available, file_tab); \
        SHF_DEBUG_FILE("pid %5u, win-tab %u-%u: grow   from %7u to %7u bytes\n", getpid(), win, TAB, TAB_MMAP->tab_size, new_tab_size); \
        int fd    =      open(file_tab, O_RDWR | O_CREAT, 0600); SHF_ASSERT(-1 != fd, "open(): %u: ", errno); \
        int value = ftruncate(fd, new_tab_size); SHF_ASSERT(-1 != value, "ftruncate(): %u: ", errno); \
        if (0) { \
            value    = munmap(SHF->tabs[win][TAB].tab_mmap, SHF->tabs[win][TAB].tab_size); SHF_ASSERT(-1 != value, "munmap(): %u: ", errno); \
            TAB_MMAP =   mmap(NULL, new_tab_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NORESERVE | MAP_POPULATE, fd, 0); SHF_ASSERT(MAP_FAILED != TAB_MMAP, "mmap(): %u: ", errno); \
        } \
        else { \
            TAB_MMAP = mremap(SHF->tabs[win][TAB].tab_mmap, SHF->tabs[win][TAB].tab_size, new_tab_size, MREMAP_MAYMOVE); SHF_ASSERT(MAP_FAILED != TAB_MMAP, "mremap(): %u: ", errno); \
            madvise(TAB_MMAP, new_tab_size, MADV_RANDOM | MADV_DONTDUMP); /* todo: test if madvise() makes any performance difference */ \
            SHF->shf_mmap->wins[win].tabs_mremaps ++; \
        } \
        /* debug paranoia */ SHF_U08_AT(TAB_MMAP, new_tab_size - 1) ++; \
        /* debug paranoia */ SHF_U08_AT(TAB_MMAP, new_tab_size - 1) --; \
        value     =     close(fd); SHF_ASSERT(-1 != value, "close(): %u: ", errno); \
        SHF->tabs[win][TAB].tab_mmap = TAB_MMAP; \
        TAB_MMAP->tab_size           = new_tab_size; \
        SHF->tabs[win][TAB].tab_size = new_tab_size; \
    } \
    SHF_ASSERT(TAB_MMAP->tab_used+1+4+KEY_LEN           <= TAB_MMAP->tab_size, "INTERNAL: expected key < %u but pos is %u at win %u, tab %u; key_len %u\n", TAB_MMAP->tab_size, TAB_MMAP->tab_used+1+4+KEY_LEN          , win, TAB, KEY_LEN); \
    SHF_ASSERT(TAB_MMAP->tab_used+1+4+KEY_LEN+4+val_len <= TAB_MMAP->tab_size, "INTERNAL: expected val < %u but pos is %u at win %u, tab %u; xxx_len %u\n", TAB_MMAP->tab_size, TAB_MMAP->tab_used+1+4+KEY_LEN+4+val_len, win, TAB, KEY_LEN + val_len); \
    SHF_DATA_TYPE data_type; \
                  data_type.as_type.key_type = SHF_KEY_TYPE_KEY_IS_STR32; \
                  data_type.as_type.val_type = SHF_KEY_TYPE_VAL_IS_STR32; \
    SHF_U08_AT(TAB_MMAP, TAB_MMAP->tab_used              )    =     data_type.as_u08; \
    SHF_U32_AT(TAB_MMAP, TAB_MMAP->tab_used+1            )    =     KEY_LEN         ; \
    SHF_MEM_AT(TAB_MMAP, TAB_MMAP->tab_used+1+4          , /* = */  KEY_LEN         , /* bytes at */ KEY); \
    SHF_U32_AT(TAB_MMAP, TAB_MMAP->tab_used+1+4+KEY_LEN  )    =     val_len         ; \
    SHF_MEM_AT(TAB_MMAP, TAB_MMAP->tab_used+1+4+KEY_LEN+4, /* = */  val_len         , /* bytes at */ val); \
    TAB_MMAP->tab_used += data_needed; \
    TAB_MMAP->tab_refs_used ++; \
    TAB_MMAP->tab_data_used += data_needed; \
    SHF->shf_mmap->wins[win].tabs_refs_used ++;

#define SHF_TAB_REF_MARK_AS_DELETED(TAB_MMAP) \
    /* mark data in old tab as deleted */ \
    SHF_U08_AT(TAB_MMAP, TAB_MMAP->row[row].ref[ref].pos  ) = SHF_DATA_TYPE_DELETED; \
    SHF_U32_AT(TAB_MMAP, TAB_MMAP->row[row].ref[ref].pos+1) = key_len + val_len; \
    TAB_MMAP->tab_data_used -= 1 + 4 + key_len + 4 + val_len; \
    TAB_MMAP->tab_data_free += 1 + 4 + key_len + 4 + val_len; \
    /* mark ref in old tab as unused */ \
    TAB_MMAP->row[row].ref[ref].pos = 0; \
    TAB_MMAP->tab_refs_used --;

#define SHF_TAB_REF_COPY() \
    uint32_t tab_used_new = tab_mmap_new->tab_used; \
    /* determine length of old key,value */ \
    uint32_t     key_len =                         SHF_U32_AT(tab_mmap_old, tab_mmap_old->row[row].ref[ref].pos+1            ) ; \
    const char * key     = SHF_CAST(const char *, &SHF_U08_AT(tab_mmap_old, tab_mmap_old->row[row].ref[ref].pos+1+4          )); \
    uint32_t     val_len =                         SHF_U32_AT(tab_mmap_old, tab_mmap_old->row[row].ref[ref].pos+1+4+key_len  ) ; \
    const char * val     = SHF_CAST(const char *, &SHF_U08_AT(tab_mmap_old, tab_mmap_old->row[row].ref[ref].pos+1+4+key_len+4)); \
    /* copy data from old tab to new tab */ \
    shf_debug_disabled ++; \
    SHF_TAB_APPEND(shf, tab, tab_mmap_new, key, key_len); \
    shf_debug_disabled --; \
    SHF_TAB_REF_MARK_AS_DELETED(tab_mmap_old); \
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
shf_tab_validate(SHF_TAB_MMAP * tab_mmap, uint32_t tab_size, uint32_t win, uint16_t tab)
{
    SHF_DEBUG("%s(tab_mmap=%p, tab_size=%u, win=%u, tab=%u) {\n", __FUNCTION__, tab_mmap, tab_size, win, tab);
    uint32_t refs_validated = 0;
    for (uint32_t row = 0; row < SHF_ROWS_PER_TAB; row ++) {
        for (uint32_t ref = 0; ref < SHF_REFS_PER_ROW; ref ++) {
            uint32_t pos = tab_mmap->row[row].ref[ref].pos;
            if (pos) { /* if ref */
                refs_validated ++;
                uint32_t key_len = SHF_U32_AT(tab_mmap, pos+1          );
                uint32_t val_len = SHF_U32_AT(tab_mmap, pos+1+4+key_len);
                //debug shf_debug_disabled --; SHF_DEBUG("row %4u, ref %2d: pos %u, key_len %u\n", row, ref, pos, key_len); shf_debug_disabled ++;
                SHF_ASSERT(key_len                   != 0       , "INTERNAL: VALIDATION FAILURE: expected key_len > 0 at win %u, tab %u, row %u, ref %u, pos %u\n"                       ,                                      win, tab, row, ref, pos);
                SHF_ASSERT(pos                       <= tab_size, "INTERNAL: VALIDATION FAILURE: expected pos < %u but pos is %u at win %u, tab %u, row %u, ref %u, pos %u\n"            , tab_size, pos                      , win, tab, row, ref, pos);
                SHF_ASSERT(pos+1+4+key_len           <= tab_size, "INTERNAL: VALIDATION FAILURE: expected key < %u but pos is %u at win %u, tab %u, row %u, ref %u, pos %u; key_len %u\n", tab_size, pos+1+4+key_len          , win, tab, row, ref, pos, key_len);
                SHF_ASSERT(pos+1+4+key_len+4+val_len <= tab_size, "INTERNAL: VALIDATION FAILURE: expected val < %u but pos is %u at win %u, tab %u, row %u, ref %u, pos %u; xxx_len %u\n", tab_size, pos+1+4+key_len+4+val_len, win, tab, row, ref, pos, key_len + val_len);
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

    SHF_DEBUG("- copying %u bytes data from old tab (excluding %u bytes marked as deleted) to new tab\n", tab_mmap_old->tab_data_used, tab_mmap_old->tab_data_free);
    for (uint32_t row = 0; row < SHF_ROWS_PER_TAB; row ++) {
        for (uint32_t ref = 0; ref < SHF_REFS_PER_ROW; ref ++) {
            if (tab_mmap_old->row[row].ref[ref].pos) { /* if ref */
                SHF_TAB_REF_COPY();
            }
        }
    }
    SHF_DEBUG_FILE("pid %5u, win-tab %u-%u: shrunk from %7u to %7u bytes; deleting old tab\n", getpid(), win, tab, tab_mmap_old->tab_size, tab_mmap_new->tab_size);
    uint32_t tab_size_old = tab_mmap_old->tab_size;
    tab_mmap_old->tab_size = 1; /* force other processes to munmap() this file & load the replacement */
    int value = munmap(tab_mmap_old, tab_size_old); SHF_ASSERT(0 == value, "munmap(): %u", errno);

#ifdef SHF_DEBUG_VERSION
    shf_tab_validate(tab_mmap_new, shf->tabs[win][tab].tab_size, win, tab);
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
    SHF_TAB_MMAP * tab_mmap_old    = shf->tabs[win][tab_old].tab_mmap;
    SHF_TAB_MMAP * tab_mmap_new    = shf->tabs[win][tab_new].tab_mmap;
    for (uint32_t row = 0; row < SHF_ROWS_PER_TAB; row ++) {
        for (uint32_t ref = 0; ref < SHF_REFS_PER_ROW; ref ++) {
            if (tab_mmap_old->row[row].ref[ref].pos) { /* if ref */
                uint16_t tab2 = tab_mmap_old->row[row].ref[ref].tab;
                uint16_t tab  = shf->shf_mmap->wins[win].tabs[tab2].tab;
                SHF_ASSERT( tab < SHF_TABS_PER_WIN             , "INTERNAL: expected tab < %u but got %u\n", SHF_TABS_PER_WIN, tab);
                SHF_ASSERT((tab == tab_old) || (tab == tab_new), "INTERNAL: expected tab %u or %u but got %u during parting @ row %u, ref %u with tab2 %u\n", tab_old, tab_new, tab, row, ref, tab2);
                if (tab == tab_new) {
                    shf->shf_mmap->wins[win].tabs_parted_new ++;
                    SHF_TAB_REF_COPY();
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
    const char * val    ,
    uint32_t     val_len)
{
    SHF_UID uid;

    uid.as_u32 = SHF_UID_NONE;

SHF_NEED_NEW_TAB_AFTER_PARTING:;

    // todo: consider implementing maximum size for shf here

    uint32_t win  = shf_hash.u16[0] %             SHF_WINS_PER_SHF       ;
    uint32_t tab2 = shf_hash.u16[1] %             SHF_TABS_PER_WIN       ;
    uint32_t row  = shf_hash.u16[2] %             SHF_ROWS_PER_TAB       ;
    uint32_t rnd  = shf_hash.u32[2] % (1 << (32 - SHF_TABS_PER_WIN_BITS));

    SHF_LOCK_WRITER(&shf->shf_mmap->wins[win].lock);
    SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);

    uint16_t tab = shf->shf_mmap->wins[win].tabs[tab2].tab;

    SHF_TAB_MMAP * tab_mmap;
    SHF_GET_TAB_MMAP(shf, tab);
    SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);

    for (uint32_t ref = 0; ref < SHF_REFS_PER_ROW; ref ++) {
        if (tab_mmap->row[row].ref[ref].pos) { /* row used */ } else {
            uid.as_part.win = win;
            uid.as_part.tab = tab2;
            uid.as_part.row = row;
            uid.as_part.ref = ref;
            uint32_t pos = tab_mmap->tab_used;
            SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
            SHF_TAB_APPEND(shf, tab, tab_mmap, shf_key, shf_key_len);
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
    SHF_UNLOCK_WRITER(&shf->shf_mmap->wins[win].lock);
    goto SHF_NEED_NEW_TAB_AFTER_PARTING;

SHF_SKIP_ROW_FULL_CHECK:;

    if (tab_mmap->tab_data_free > (tab_mmap->tab_data_used * 20 / 100)) {
        SHF_DEBUG_FILE("pid %5u, win-tab %u-%u: shrink after put\n", getpid(), win, tab);
        SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
        shf_tab_shrink(shf, win, tab);
        SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
    }

    SHF_UNLOCK_WRITER(&shf->shf_mmap->wins[win].lock);

    SHF_DEBUG("%s(shf=?, val=?, val_len=%u){} // return 0x%08x=%02x-%03x-%03x-%01x=%s\n", __FUNCTION__, val_len, uid.as_u32, uid.as_part.win, uid.as_part.tab, uid.as_part.row, uid.as_part.ref, SHF_UID_NONE == uid.as_u32 ? "failure" : "success");

    return uid.as_u32;
} /* shf_put_val() */

typedef enum SHF_FIND_KEY_AND {
    SHF_FIND_KEY_OR_UID              = 0,
    SHF_FIND_KEY_OR_UID_AND_COPY_VAL    ,
    SHF_FIND_KEY_OR_UID_AND_DELETE
} SHF_FIND_KEY_AND;

static int /* 0 means key does not exist, 1 means key found */
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

    if   ((SHF_FIND_KEY_OR_UID              == what)
    ||    (SHF_FIND_KEY_OR_UID_AND_COPY_VAL == what)) { SHF_LOCK_READER(&shf->shf_mmap->wins[win].lock); }
    else                                              { SHF_LOCK_WRITER(&shf->shf_mmap->wins[win].lock); }
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
                pos              =  tab_mmap->row[row].ref[ref].pos        ; SHF_ASSERT(pos < tab_mmap->tab_size, "INTERNAL: expected pos < %u but pos is %u at win %u, tab %u\n", tab_mmap->tab_size, pos, win, tab);
                data_type.as_u08 =  SHF_U08_AT(tab_mmap, pos              );
                key_len          =  SHF_U32_AT(tab_mmap, pos+1            ); SHF_ASSERT(pos+1+4+key_len           <= tab_mmap->tab_size, "INTERNAL: expected key < %u but pos is %u at win %u, tab %u\n", tab_mmap->tab_size, pos+1+4+key_len          , win, tab);
                val_len          =  SHF_U32_AT(tab_mmap, pos+1+4+key_len  ); SHF_ASSERT(pos+1+4+key_len+4+val_len <= tab_mmap->tab_size, "INTERNAL: expected val < %u but pos is %u at win %u, tab %u\n", tab_mmap->tab_size, pos+1+4+key_len+4+val_len, win, tab);
                shf_val_addr     = &SHF_U08_AT(tab_mmap, pos+1+4+key_len+4);
                SHF_UNUSE(data_type); // todo: remove hard coding of types
                if (key_len != shf_key_len                                        ) { shf->shf_mmap->wins[win].keylen_misses ++; continue; }
                if (0       != SHF_CMP_AT(tab_mmap, pos+1+4, shf_key_len, shf_key)) { shf->shf_mmap->wins[win].memcmp_misses ++; continue; }
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
            pos              =  tab_mmap->row[row].ref[ref].pos        ; SHF_ASSERT(pos < tab_mmap->tab_size, "INTERNAL: expected pos < %u but pos is %u at win %u, tab %u\n", tab_mmap->tab_size, pos, win, tab);
            data_type.as_u08 =  SHF_U08_AT(tab_mmap, pos              );
            key_len          =  SHF_U32_AT(tab_mmap, pos+1            ); SHF_ASSERT(pos+1+4+key_len           <= tab_mmap->tab_size, "INTERNAL: expected key < %u but pos is %u at win %u, tab %u\n", tab_mmap->tab_size, pos+1+4+key_len          , win, tab);
            val_len          =  SHF_U32_AT(tab_mmap, pos+1+4+key_len  ); SHF_ASSERT(pos+1+4+key_len+4+val_len <= tab_mmap->tab_size, "INTERNAL: expected val < %u but pos is %u at win %u, tab %u\n", tab_mmap->tab_size, pos+1+4+key_len+4+val_len, win, tab);
            shf_val_addr     = &SHF_U08_AT(tab_mmap, pos+1+4+key_len+4);
            SHF_UNUSE(data_type); // todo: remove hard coding of types
            result = 1; /* aka uid found */
        }
    }

    if (1 == result) {
        SHF_DEBUG("- found %lu bytes for key @ 0x%02x-%03x[%03x]-%03x-%x // key,value are %u,%u bytes @ pos %u\n", sizeof(SHF_DATA_TYPE) + sizeof(uint8_t) + key_len + sizeof(uint8_t) + val_len, win, tab2, tab, row, ref, key_len, val_len, pos);
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
            SHF_TAB_REF_MARK_AS_DELETED(tab_mmap);
            SHF_LOCK_DEBUG_LINE(&shf->shf_mmap->wins[win].lock);
        }
    }
    if   ((SHF_FIND_KEY_OR_UID              == what)
    ||    (SHF_FIND_KEY_OR_UID_AND_COPY_VAL == what)) { SHF_UNLOCK_READER(&shf->shf_mmap->wins[win].lock); }
    else                                              { SHF_UNLOCK_WRITER(&shf->shf_mmap->wins[win].lock); }

    SHF_DEBUG("%s(shf=?){} // return 0x%08x=%02x-%03x[%03x]-%03x-%01x=%s\n", __FUNCTION__, tmp_uid.as_u32, tmp_uid.as_part.win, tmp_uid.as_part.tab, tab, tmp_uid.as_part.row, tmp_uid.as_part.ref, result ? "exists" : "not exists");

    return result;
} /* shf_find_key_internal() */

void * shf_get_key_val_addr(SHF * shf              ) { return shf_find_key_internal(shf, SHF_UID_NONE, SHF_FIND_KEY_OR_UID             ) ? shf_val_addr : NULL; } /* 64bit address of val itself                     */ // todo: add shf_freeze() to make addr safer to use; disables key,val locking!
void * shf_get_uid_val_addr(SHF * shf, uint32_t uid) { return shf_find_key_internal(shf,     uid     , SHF_FIND_KEY_OR_UID             ) ? shf_val_addr : NULL; } /* 64bit address of val itself                     */ // todo: add shf_freeze() to make addr safer to use; disables key,val locking!
int    shf_get_key_val_copy(SHF * shf              ) { return shf_find_key_internal(shf, SHF_UID_NONE, SHF_FIND_KEY_OR_UID_AND_COPY_VAL)                      ; } /* 0 means key does not exist, 1 means key found   */
int    shf_get_uid_val_copy(SHF * shf, uint32_t uid) { return shf_find_key_internal(shf,     uid     , SHF_FIND_KEY_OR_UID_AND_COPY_VAL)                      ; } /* 0 means key does not exist, 1 means key found   */
int    shf_del_key_val     (SHF * shf              ) { return shf_find_key_internal(shf, SHF_UID_NONE, SHF_FIND_KEY_OR_UID_AND_DELETE  )                      ; } /* 0 means key does not exist, 1 means key deleted */
int    shf_del_uid_val     (SHF * shf, uint32_t uid) { return shf_find_key_internal(shf,     uid     , SHF_FIND_KEY_OR_UID_AND_DELETE  )                      ; } /* 0 means key does not exist, 1 means key deleted */

void   shf_debug_verbosity_less(void) { shf_debug_disabled ++; }
void   shf_debug_verbosity_more(void) { shf_debug_disabled --; }

void
shf_del(
    SHF * shf)
{
    SHF_UNUSE(shf);
    SHF_DEBUG("todo: implement shf_del(); delete entire shf");
} /* shf_del() */

uint64_t
shf_debug_get_bytes_marked_as_deleted(
    SHF * shf)
{
    uint64_t all_data_free = 0;
    for (uint32_t win = 0; win < SHF_WINS_PER_SHF; win++) {
        uint32_t tabs_used = shf->shf_mmap->wins[win].tabs_used;
        for (uint32_t tab = 0; tab < tabs_used; tab++) {
            all_data_free += shf->tabs[win][tab].tab_mmap->tab_data_free;
        }
    }
    return all_data_free;
} /* shf_debug_get_bytes_marked_as_deleted() */

void
shf_set_data_need_factor(
    uint32_t data_needed_factor)
{
    SHF_ASSERT(shf_data_needed_factor > 0, "ERROR: data_needed_factor must be > 0");
    shf_data_needed_factor = data_needed_factor;
} /* shf_set_data_need_factor() */
