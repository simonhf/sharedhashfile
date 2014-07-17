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

/** @file shf.h
 * @brief SharedHashFile API
 *
 * @mainpage SharedHashFile (S)hared Memory (H)ash Table Using (F)iles (SHF) & IPC queue
 *
 * @section intro_sec Introduction
 * 
 * SharedHashFile is a lightweight NoSQL key value store / hash table, &
 * a zero-copy IPC queue library written in C for Linux. There is no
 * server process. Data is read and written directly from/to shared
 * memory or SSD; no sockets are used between SharedHashFile and the
 * application program. APIs for C, C++, & nodejs.
 * 
 * ![Nailed It](http://simonhf.github.io/sharedhashfile/images/10m-tps-nailed-it.jpeg)
 * 
 * @section ipc_sec Zero-Copy IPC Queues
 * 
 * How does it work? Create X fixed-sized queue elements, and Y queues
 * to push & pull those queue elements to/from.
 * 
 * Example: Imagine two processes ```Process A``` & ```Process B```.
 * ```Process A``` creates 100,000 queue elements and 3 queues;
 * ```queue-free```, ```queue-a2b```, and ```queue-b2a```. Intitally,
 * all queue elements are pushed onto ```queue-free```. ```Process A```
 * then spawns ```Process B``` which attaches to the SharedHashFile in
 * order to pull from ```queue-a2b```. To perform zero-copy IPC then
 * ```Process A``` can pull queue elements from ```queue-free```,
 * manipulate the fixed size, shared memory queue elements, and push the
 * queue elements into ```queue-a2b```. ```Process B``` does the
 * opposite; pulls queue elements from ```queue-a2b```, manipulates the
 * fixed size, shared memory queue queue elements, and pushes the queue
 * elements into ```queue-b2a```. ```Process A``` can also pull queue
 * items from ```queue-b2a``` in order to digest the results from
 * ```Process B```.
 * 
 * So how many queue elements per second can be moved back and forth by
 * ```Processes A``` & ```Process B```? On a Lenovo W530 laptop then
 * about 90 million per second if both ```Process A``` & ```Process B```
 * are written in C. Or about 7 million per second if  ```Process A```
 * is written in C and ```Process B``` is written in javascript for
 * nodejs.
 * 
 * Note: When a queue element is moved from one queue to another then it
 *  is not copied, only a reference is updated.
 *
 * @author
 * 
 * Simon Hardy-Francis, Hardy-Francis Enterprises Inc.
 *
 * @copyright
 * 
 * Copyright (c) 2014 Hardy-Francis Enterprises Inc.
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
 *
 * To use SharedHashFile in a closed-source product, commercial licenses are
 * available; email office [@] sharedhashfile [.] com for more information.
 */

#ifndef __SHF_H__
#define __SHF_H__

#include <stdint.h>

#include "shf.defines.h"

typedef enum SHF_KEY_TYPES {
    SHF_KEY_TYPE_KEY_IS_UID     , /*  0: no key, UID accesses directly <- todo */
    SHF_KEY_TYPE_KEY_AT_UID     , /*  1:    key is value at UID        <- todo */
    SHF_KEY_TYPE_KEY_IS_U32     , /*  2:    key is U32 number          <- todo */
    SHF_KEY_TYPE_KEY_IS_U64     , /*  3:    key is U64 number          <- todo */
    SHF_KEY_TYPE_KEY_IS_STR08   , /*  4:    key is  8bit length string <- todo */
    SHF_KEY_TYPE_KEY_IS_STR16   , /*  5:    key is 16bit length string <- todo */
    SHF_KEY_TYPE_KEY_IS_STR32   , /*  6:    key is 32bit length string */
    SHF_KEY_TYPE_KEY_IS_DELETED , /*  7:    0xFF == deleted            */
} SHF_KEY_TYPES;

typedef enum SHF_VAL_TYPES {
    SHF_VAL_TYPE_UNUSED         =     0,
    SHF_VAL_TYPE_VAL_AT_UID     , /*  1:    val is value at UID            <- todo */
    SHF_VAL_TYPE_VAL_IS_U32     , /*  2:    val is U32 number              <- todo */
    SHF_VAL_TYPE_VAL_IS_U64     , /*  3:    val is U64 number              <- todo */
    SHF_VAL_TYPE_VAL_IS_SHM     , /*  4:    val is mmap() at 64bit pointer <- todo */
    SHF_KEY_TYPE_VAL_IS_STR08   , /*  5:    val is  8bit length string     <- todo */
    SHF_KEY_TYPE_VAL_IS_STR16   , /*  6:    val is 16bit length string     <- todo */
    SHF_KEY_TYPE_VAL_IS_STR32   , /*  7:    val is 32bit length string     */
    SHF_KEY_TYPE_VAL_IS_UNUSED08, /*  8:    val is yet to be defined       */
    SHF_KEY_TYPE_VAL_IS_UNUSED09, /*  9:    val is yet to be defined       */
    SHF_KEY_TYPE_VAL_IS_UNUSED10, /* 10:    val is yet to be defined       */
    SHF_KEY_TYPE_VAL_IS_UNUSED11, /* 11:    val is yet to be defined       */
    SHF_KEY_TYPE_VAL_IS_UNUSED12, /* 12:    val is yet to be defined       */
    SHF_KEY_TYPE_VAL_IS_UNUSED13, /* 13:    val is yet to be defined       */
    SHF_KEY_TYPE_VAL_IS_UNUSED14, /* 14:    val is yet to be defined       */
    SHF_KEY_TYPE_VAL_IS_UNUSED15, /* 15:    val is yet to be defined       */
} SHF_VAL_TYPES;

typedef union SHF_DATA_TYPE {
    struct {
        SHF_KEY_TYPES key_type : 3; /*  7 types of keys */
        SHF_VAL_TYPES val_type : 4; /* 16 types of values */
        uint8_t       extended : 1; /* is key,value data extended? */
    } __attribute__((packed)) as_type;
    uint8_t as_u08;
} __attribute__((packed)) SHF_DATA_TYPE;

#define SHF_DATA_TYPE_DELETED (0xff)
#define SHF_UID_NONE          (4294967295U) /* UINT32_MAX; note: defined here for use with either C or C++ clients */
#define SHF_QID_NONE          (4294967295U) /* UINT32_MAX; note: defined here for use with either C or C++ clients */
#define SHF_QIID_NONE         (4294967295U) /* UINT32_MAX; note: defined here for use with either C or C++ clients */

extern __thread uint32_t   shf_uid          ;
extern __thread char     * shf_val          ;
extern __thread uint32_t   shf_val_len      ;
extern __thread uint32_t   shf_qiid         ;
extern __thread char     * shf_qiid_addr    ;
extern __thread uint32_t   shf_qiid_addr_len;

/** @brief Spawn a child process & return its pid.
 *  @param[in] child_path       todo
 *  @param[in] child_file       todo
 *  @param[in] child_argument_1 todo
 *  @param[in] child_argument_2 todo
 *  @retval The pid of the spawned child.
 */
extern pid_t      shf_exec_child           (const char * child_path, const char * child_file, const char * child_argument_1, char  * child_argument_2);
extern char     * shf_backticks            (const char * command);
extern double     shf_get_time_in_seconds  (void);
extern uint64_t   shf_get_vfs_available    (SHF * shf);
extern void       shf_init                 (void);
extern void       shf_detach               (SHF * shf);
extern SHF      * shf_attach_existing      (const char * path, const char * name);
extern SHF      * shf_attach               (const char * path, const char * name, uint32_t delete_upon_process_exit);
extern void       shf_make_hash            (const char * key, uint32_t key_len);
extern uint32_t   shf_put_key_val          (SHF * shf, const char * val, uint32_t val_len);
extern int        shf_get_key_val_copy     (SHF * shf);
extern int        shf_get_uid_val_copy     (SHF * shf, uint32_t uid);
extern void     * shf_get_key_val_addr     (SHF * shf);
extern void     * shf_get_uid_val_addr     (SHF * shf, uint32_t uid);
extern int        shf_del_key_val          (SHF * shf);
extern int        shf_del_uid_val          (SHF * shf, uint32_t uid);
extern char     * shf_del                  (SHF * shf);
extern uint64_t   shf_debug_get_garbage    (SHF * shf);
extern void       shf_debug_verbosity_less (void);
extern void       shf_debug_verbosity_more (void);
extern void       shf_set_data_need_factor (uint32_t data_needed_factor);
extern void       shf_set_is_lockable      (SHF * shf, uint32_t is_lockable);
extern void     * shf_q_new                (SHF * shf, uint32_t shf_qs, uint32_t shf_q_items, uint32_t shf_q_item_size, uint32_t qids_nolock_max);
extern void     * shf_q_get                (SHF * shf);
extern void       shf_q_del                (SHF * shf);
extern uint32_t   shf_q_new_name           (SHF * shf, const char * name, uint32_t name_len);
extern uint32_t   shf_q_get_name           (SHF * shf, const char * name, uint32_t name_len);
extern void       shf_q_push_head          (SHF * shf, uint32_t      qid, uint32_t qiid);
extern uint32_t   shf_q_pull_tail          (SHF * shf, uint32_t      qid                                       ); /* sets shf_qiid & shf_qiid_addr & shf_qiid_addr_len */
extern uint32_t   shf_q_push_head_pull_tail(SHF * shf, uint32_t push_qid, uint32_t push_qiid, uint32_t pull_qid); /* sets shf_qiid & shf_qiid_addr & shf_qiid_addr_len */
extern uint32_t   shf_q_take_item          (SHF * shf, uint32_t      qid                                       ); /* sets shf_qiid & shf_qiid_addr & shf_qiid_addr_len */
extern void       shf_q_flush              (SHF * shf, uint32_t pull_qid);
extern void       shf_q_size               (SHF * shf, uint32_t      qid);
extern uint32_t   shf_q_is_ready           (SHF * shf);
extern void       shf_race_init            (SHF * shf, const char * name, uint32_t name_len                 );
extern void       shf_race_start           (SHF * shf, const char * name, uint32_t name_len, uint32_t horses);
extern void       shf_log_output_set       (void (*shf_log_output_new)(const char * log_line));

// Useful links regarding /dev/shm:
// http://gerardnico.com/wiki/linux/shared_memory - Linux - Shared Memory (SHM) (/dev/shm)
// http://www.cyberciti.biz/tips/what-is-devshm-and-its-practical-usage.html - What Is /dev/shm And Its Practical Usage
// http://www.cyberciti.biz/files/linux-kernel/Documentation/filesystems/tmpfs.txt - Tmpfs is a file system which keeps all files in virtual memory

#endif /* __SHF_H__ */
