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

#ifndef __SHF_PRIVATE_H__
#define __SHF_PRIVATE_H__

#include <stdint.h>

#include "shf.lock.h"

#define SHF_SIZE_PAGE           (4096)
#define SHF_SIZE_CACHE_LINE     (64)
#define SHF_REFS_PER_ROW        ((SHF_SIZE_CACHE_LINE / 2 /* why? */) / sizeof(uint16_t)) /*      16  keys per row */
#define SHF_REFS_PER_ROW_BITS   (4)                                                       /*       4  bits         */
#define SHF_SIZE_ROW            (sizeof(SHF_ROW_MMAP))                                    /*     128  size per row */
#define SHF_SIZE_TAB            (1<<16)                                                   /*  65,536  size per tab */
#define SHF_ROWS_PER_TAB        (SHF_SIZE_TAB / SHF_SIZE_ROW)                             /*     512  rows per tab */
#define SHF_ROWS_PER_TAB_BITS   (9)                                                       /*       9  bits         */
#define SHF_REFS_PER_TAB        (SHF_ROWS_PER_TAB * SHF_REFS_PER_ROW)                     /*   8,192  refs per tab */
#define SHF_TABS_PER_WIN_BITS   (24 - SHF_ROWS_PER_TAB_BITS - SHF_REFS_PER_ROW_BITS)      /*      11  bits         */
#define SHF_TABS_PER_WIN        (1<<SHF_TABS_PER_WIN_BITS)                                /*   2,048  tabs per win */
#define SHF_REFS_PER_WIN        (SHF_REFS_PER_TAB * SHF_TABS_PER_WIN)                     /*      16M refs per win */
#define SHF_WINS_PER_SHF_BITS   (8)                                                       /*       8  bits         */
#define SHF_WINS_PER_SHF        (1<<SHF_WINS_PER_SHF_BITS)                                /*     256  wins per shf */
#define SHF_REFS_PER_SHF        (SHF_REFS_PER_WIN * SHF_WINS_PER_SHF)                     /*   4,096M refs per shf */
#define SHF_TABS_PER_SHF        (SHF_TABS_PER_WIN * SHF_WINS_PER_SHF)                     /* 524,288  tabs per shf */

typedef struct SHF_REF_MMAP { // todo: consider optimizing from 4+4 bytes to 3+3 bytes (maybe not due to useful atomic long?)
    volatile uint32_t tab :      SHF_TABS_PER_WIN_BITS; /* 11 bits or 2,048 tabs */
    volatile uint32_t rnd : 32 - SHF_TABS_PER_WIN_BITS; /* 21 bits or 2,097,152; /16 is 1 in 131,072 chance */
    volatile uint32_t pos                             ; /* 0 means ref UNused */
} __attribute__((packed)) SHF_REF_MMAP;

typedef struct SHF_ROW_MMAP {
    volatile SHF_REF_MMAP ref[SHF_REFS_PER_ROW];
} __attribute__((packed)) SHF_ROW_MMAP;

typedef struct SHF_TAB_MMAP {
    volatile uint32_t     tab_size             ; /* size of memory (mod 4KB) */
    volatile uint32_t     tab_used             ; /* size of memory */
    volatile uint32_t     tab_refs_used        ; /* refs allocated & used */
    volatile uint32_t     tab_data_free_pos    ; /* next data bytes marked free if shf->fixed_key_len */
    volatile uint32_t     tab_data_free        ; /*      data bytes marked free */
    volatile uint32_t     tab_data_used        ; /*      data bytes        used */
    volatile SHF_ROW_MMAP row[SHF_ROWS_PER_TAB];
    // todo: base to linked list of deleted key,value pairs
    volatile uint8_t      data[0];
} __attribute__((packed)) SHF_TAB_MMAP;

typedef struct SHF_OFF_MMAP {
    volatile uint16_t tab; /* index up to 2048 tabs; todo use extra bit for something :-) */
} __attribute__((packed)) SHF_OFF_MMAP;

typedef struct SHF_OFF {
    SHF_TAB_MMAP * tab_mmap; /* pointer to mremap()able ref & data memory */
    uint32_t       tab_size; /* size of memory (mod 4KB) */
} __attribute__((packed)) SHF_OFF;

typedef struct SHF_WIN_MMAP {
             SHF_LOCK     lock                  ;
    volatile SHF_OFF_MMAP tabs[SHF_TABS_PER_WIN]; /* 4KB == 2048 tabs * uint16_t */
    volatile uint32_t     tabs_used             ; /* number of tabs in win */
    volatile uint64_t     tabs_mmaps            ; /* times 1 tab mmapped */
    volatile uint64_t     tabs_mremaps          ; /* times 1 tab mremapped */
    volatile uint64_t     tabs_shrunk           ; /* times 1 tab shrunk */
    volatile uint64_t     tabs_parted           ; /* times 1 tab parted into 2 tabs */
    volatile uint64_t     tabs_parted_old       ; /* parted in old tab */
    volatile uint64_t     tabs_parted_new       ; /* parted in new tab */
    volatile uint64_t     keylen_misses         ; /* times hash   matched but keylen didn't match */
    volatile uint64_t     memcmp_misses         ; /* times keylen matched but key    didn't match */
} __attribute__((packed)) SHF_WIN_MMAP;

typedef struct SHF_SHF_MMAP {
    SHF_WIN_MMAP wins[SHF_WINS_PER_SHF]; /* 256 WINdows */
} __attribute__((packed)) SHF_SHF_MMAP;

typedef struct SHF_Q_LOCK_MMAP {
    SHF_LOCK lock;
#ifdef SHF_DEBUG_VERSION
    uint32_t debug_magic;
#endif
} __attribute__((packed)) SHF_Q_LOCK_MMAP;

typedef struct SHF_QID_MMAP {
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t size;
} __attribute__((packed)) SHF_QID_MMAP;

typedef struct SHF_QIID_MMAP {
    volatile uint32_t last;
    volatile uint32_t next;
} __attribute__((packed)) SHF_QIID_MMAP;

typedef struct SHF_Q {
    uint32_t          qs              ; /* number of queues, e.g. 3 if free, a2b, & b2a */
    uint32_t          q_next          ; /* next qid to assign via shf_q_new_name() */
    uint32_t          q_items         ; /* number of queue items to share between the queues */
    uint32_t          q_item_size     ; /* size of each queue item in bytes */
    char            * q_item_addr     ; /* address of array of queue items */
    uint32_t          qids_nolock_max ; /* max q items in qids_nolock_(pull|pull) */
    SHF_QID_MMAP    * qids_nolock_push; /* non-mmap qid for lockless pushing */
    SHF_QID_MMAP    * qids_nolock_pull; /* non-mmap qid for lockless pulling */
    SHF_QID_MMAP    * qids            ;
    SHF_QIID_MMAP   * qiids           ;
    SHF_Q_LOCK_MMAP * q_lock          ;
    uint32_t          q_is_ready      ; /* successfully called shf_q_(new|get)()? */
} __attribute__((packed)) SHF_Q;

#define SHF_PID_MAX (131072) /* todo: convert this to dynamically use cat /proc/sys/kernel/pid_max */

typedef struct SHF_LOG_MMAP {
#ifdef SHF_DEBUG_VERSION
             uint32_t magic            ;
#endif
             int      fd               ;
             SHF_LOCK lock             ;
             double   time_init        ;
             uint32_t second           ; /* remember this unique second */
             uint32_t write_fail       ; /* throttles write() failures */
    volatile uint32_t writing          ; /* write() loop in progress? */
             uint32_t used_hi          ; /* used high water mark */
             uint32_t used_hi_new      ;
    volatile uint32_t used             ;
    volatile uint32_t size             ;
    volatile uint32_t running          ;
    volatile uint32_t stopped          ;
    volatile uint8_t  tids[SHF_PID_MAX];
    volatile uint8_t  tid_id           ;
             char     bytes[0]         ;
} __attribute__((packed)) SHF_LOG_MMAP;

typedef struct SHF {
    uint32_t       version                                 ; /* todo: implement version */
    SHF_OFF        tabs[SHF_WINS_PER_SHF][SHF_TABS_PER_WIN]; /* 524,288 private tab pointers */
    SHF_SHF_MMAP * shf_mmap                                ; /* pointer to mremap()able memory */
    char         * path                                    ; /* e.g. '/dev/shm' */
    char         * name                                    ; /* e.g. 'myshf' */
    uint32_t       is_lockable                             ; /* 0 means single threaded use only, 1 means lockable */
    uint32_t       is_fixed_key_val_len                    ; /* 0 means key values can be any length, 1 means key values all the same length */
    uint32_t       fixed_key_len                           ; /* length of key   if is_fixed_key_val_len */
    uint32_t       fixed_val_len                           ; /* length of value if is_fixed_key_val_len */
    uint32_t       count_mmap                              ; /* number of mmap()s */
    uint32_t       count_xalloc                            ; /* number of (c|m)alloc()s */
    SHF_Q          q                                       ; /* for IPC q   */
    SHF_LOG_MMAP * log                                     ; /* for IPC log; value for key '__log' */
    uint32_t       log_thread_acive                        ; /* for IPC log; we have the log thread? */
} __attribute__((packed)) SHF;

typedef union SHF_UID {
    struct {
        uint64_t win : SHF_WINS_PER_SHF_BITS; /*  8 bits or   256 wins per shf */
        uint64_t tab : SHF_TABS_PER_WIN_BITS; /* 11 bits or 2,048 tabs per win */
        uint64_t row : SHF_ROWS_PER_TAB_BITS; /*  9 bits or   512 rows per tab */
        uint64_t ref : SHF_REFS_PER_ROW_BITS; /*  4 bits or    16 refs per row */
    } __attribute__((packed)) as_part;
    uint32_t as_u32;
} __attribute__((packed)) SHF_UID;

typedef union SHF_HASH { // todo: just use uid instead
    uint64_t u64[2];
    uint32_t u32[4];
    uint16_t u16[8];
    uint8_t  u08[16];
} __attribute__((packed)) SHF_HASH;

extern __thread SHF_HASH shf_hash;

#endif /* __SHF_PRIVATE_H__ */
