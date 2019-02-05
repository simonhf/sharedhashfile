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
 * application program. APIs for C & C++.
 *
 * ![Nailed It](http://simonhf.github.io/sharedhashfile/images/10m-tps-nailed-it.jpeg)
 *
 * @section kv_sec Key Value Store / Hash Table
 *
 * What makes SHF different from other key value stores?
 * - Exists in shared memory and avoids C pointers.
 * - Expands gracefully without having to rehash all keys.
 * - Hash table is 64 bit but only uses 32 bit offsets to save space.
 * - Uses lock sharding to minimize lock contention.
 * - Uses high performance, fair, read write spin locks.
 * - Every key stored can be accessed by the key or a 32 bit UID.
 * - Can be incrementally updated without suffering from memory holes.
 *
 * What does the SHF folder structure look like?
 * - In the example below the SHF instance is named `myname`.
 * - There is a folder called `myname.shf`.
 *   - This folder can be on `/dev/shm` or on an SSD.
 *   - Deleting this folder deletes the SHF instance.
 * - A file called `myname.shf` holding global data, e.g. locks.
 * - There are 256 'window' folders.
 * - Each window folder contains up to 2,048 'tables'.
 * - Each table contains up to 8192 keys.
 * - The maximum files in the folder are 256 * 2,048 = 524,288 files.
 * - The maximum keys storable are 256 * 2,048 * 8,192 = 2^32 keys.
 * - In each window, tables 1 to 2047 get added as expansion occurs.
 *
 * @code
 * - /dev/shm/myname.shf/
 * - /dev/shm/myname.shf/myname.shf    <-- global data, e.g. locks
 * - /dev/shm/myname.shf/000/
 * - ...
 * - /dev/shm/myname.shf/255/          <-- 256 windows
 * - /dev/shm/myname.shf/000/0000.tab
 * - ...
 * - /dev/shm/myname.shf/000/2047.tab  <-- 2,048 tables
 * @endcode
 *
 * What does a table file look like?
 * - A fixed part part holds key reference data in 512 rows.
 * - Each new key value is appended to the growable data part.
 * - If new key meta data does not fit in a row, expansion occurs:
 *   - The table is split into two tables.
 *   - About half the keys remain in one table, the rest in the other.
 *   - Rehashing keys never happens to save CPU.
 *   - Keys are copied during the split; deleted keys not copied.
 *   - Due to the 8,192 key limit then splitting uses little CPU.
 *
 * @code
 * +---+---+---+---+---+
 * |   |   |   |   |   |  Fixed sized meta data for 8,192 keys
 * |  0|  1| ..|511|512|  512 rows of 16 key references
 * +---+---+---+---+---+
 * |                   |  Growable data for keys & value data
 * :                   :
 * |                   |
 * +---+---+---+---+---+
 * @endcode
 *
 * Walk me through what happens when adding a key and value:
 * - The key is hashed producing indexes for window, table, and row.
 * - Because the table count grows, table index is looked up indirectly.
 * - The necessary window is locked aka lock sharding.
 * - The necessary table is memory mapped if not already mapped.
 * - Find a free slot in the row for the key reference data.
 * - If no free slot found then split table and find again.
 * - Add key reference to row; append key value data to growable data.
 * - The key UID (window|table|row|slot) is returned.
 * - The 32 bit key UID can be used for key access without hashing.
 *
 * How does the indirect table index work?
 * - A per-window array holds the maximum 2,048 table indexes.
 * - If there is only 1 table then all values are initially 0.
 * - If table 0 splits, half of the 0 values become 1.
 * - If table 1 splits, half of the 1 values become 2.
 * - If table 0 splits again, half of the 0 values become 3.
 * - And so on...
 *
 * How does the fair read write locking work?
 * - Any number of threads or processes can read at the same time.
 * - Only one thread or process can write at one time.
 * - Read or write starvation cannot occur due to a ticketing system.
 * - Threads should be pinned to CPU cores to avoid context switching.
 *
 * How can I access value bytes if the memory address changes?
 * - The shared memory address of a key or value can change any time.
 *   - E.g. another process might trigger a table split simultaneously.
 * - Therefore most API functions return a copy of the key value data.
 * - It is possible to get the direct memory address of key value data:
 *   - But do not add or delete any more keys to ensure no table splits.
 *   - Only table splits cause addresses to change.
 *   - todo: Allow large key value data to exist outside of tables.
 *   - E.g. IPC queues and logging use shared memory directly.
 *
 * @section ipc_sec Zero-Copy IPC Queues
 *
 * Which data structures are used by SHF IPC queues?
 * - Create an array of x fixed-sized (caller decides) queue items.
 * - The index to a particular queue item is called a qiid.
 * - Create y queues (think: queue item states).
 * - A queue is called a qid.
 * - All qiids are initially pushed onto qid #0.
 * - A qid can be given a name (key) to identify it:
 *   - e.g. qid #0: "qid-free".
 *   - e.g. qid #1: "qid-a2b".
 *   - e.g. qid #2: "qid-b2a".
 *
 * @code
 * +---+---+- - -+---+---+
 * |   |   |     |   |   |  Array of x queue items of size s bytes
 * |  0|  1|     |x-1|  x|  indexed by (q)ueue (i)tem (ID) aka qiid
 * +---+---+- - -+---+---+
 *
 * +---+---+- - -+---+---+
 * |   |   |     |   |   |  Array of y queues (double linked lists)
 * |  0|  1|     |y-1|  y|  indexed by (q)ueue (ID) aka qid
 * +---+---+- - -+---+---+
 * |  0|                    All qiids are pushed onto qid 0
 * +---+
 * :   :
 * +---+
 * |  x|
 * +---+
 * @endcode
 *
 * Abstract example of moving a qiid from qid #0 to qid #1:
 * - Pull next qiid from the tail of qid #0.
 * - Push      qiid onto     head of qid #1.
 *
 * @code
 * +---+---+- - -+---+---+
 * |   |   |     |   |   |  Array of y queues (double linked lists)
 * |  0|  1|     |y-1|  y|  indexed by (q)ueue (ID) aka qid
 * +---+---+- - -+---+---+
 * |  1|  0|                qiid pulled from qid 0 is pushed onto qid 1
 * +---+---+
 * :   :
 * +---+
 * |  x|
 * +---+
 * @endcode
 *
 * How is this useful for IPC between `Process A` & `Process B`?:
 * - Both the queue items and queues exist in shared memory.
 * - Queue items themselves never need to be copied (aka zero copy):
 *   - `Process A` might write bytes into shared memory @ qiid #0.
 *   - `Process B` might read  bytes from shared memory @ qiid #0.
 * - Pushing/pulling qiids to/from qids updates double linked lists.
 * - Example of IPC of a qiid from `Process A` to `Process B` and back:
 *   - `Process A` might pull a   qiid from "qid-free".
 *   - `Process A` might push the qiid onto "qid-a2b".
 *   - `Process B` might pull the qiid from "qid-a2b".
 *   - `Process B` might push the qiid onto "qid-b2a".
 *   - `Process A` might pull the qiid from "qid-b2a".
 *   - `Process A` might push the qiid onto "qid-free".
 * - There does not need to be only 3 qids as in this example.
 * - The number and/or name of qids is defined by the caller.
 * - A more complicated example might have (chains of?) more processes
 *   and/or fewer processes and more qids (think: states).
 * - The number of qids and qiids is only limited by available memory.
 * - Between pulling & pushing a qiid then it 'belongs' to no qid.
 *   - This is when the shared memory @ qiid can be accessed safely.
 *
 * How many qiids per second can be pulled & pushed between 2 processes?
 * - On a Lenovo W530 then ~ 90 million per second; C(++) <-> C(++).
 * - Note: Figures based upon minimal qid memory access.
 * - Note: Figures based upon the hybrid shf_q_push_head_pull_tail().
 *
 * Performance notes / how these figures were achieved:
 * - Pushing or pulling requires an expensive lock for each operation.
 * - So 1 million pushes & 1 million pulls == 2 million locks total.
 * - However, the hybrid shf_q_push_head_pull_tail() uses 1 lock.
 * - So 1 million hybrid calls == 1 million locks total.
 * - However, shf_q_new() takes `qids_nolock_max`:
 *   - Each queue caller (e.g. `Process A`) has an own pre-queue.
 *   - This pre-queue is unlocked and contains `qids_nolock_max` items.
 *   - Upon containing `qids_nolock_max` items then:
 *     - A lock is taken.
 *     - And `qids_nolock_max` items are moved to the qid.
 *     - Only double linked list ends are updated to move many items.
 *   - Therefore, if `qids_nolock_max` is set to 1,000 then:
 *     - 1 million hybrid calls == only 1,000 locks total.
 *   - The lockless pre-queues are not stored in shared memory.
 * - The architecture is designed for maximum possible performance.
 *
 * Suggestion for caller hybrid poll / notification approach:
 * - Processes poll for (& expect there to be) a new qiid to pull.
 * - E.g. similar to how a busy NIC driver polls for the next packet.
 * - For non-busy pollers, consider multi stage notification, e.g.:
 *   - Process greedily polls until nothing left to poll.
 *   - Use much slower OS notification mechanism to batch notify:
 *     - E.g. let's say OS mechanism only manages 100k per second.
 *     - But SHF IPC queues manages 90 millon per second.
 *     - So pusher only need notify every 90M / 100k == 900 items.
 *     - Otherwise throughput will lower to unwanted 100k per second.
 *   - Use poller timer to sweep for last item sent.
 *     - i.e. pusher sent 2 items in last single OS notify period.
 *
 * Suggestion for run-time order of operations / pattern:
 * - `Process A` wants to IPC with (not yet started) `Process B`.
 * - `Process A` creates unique SHF instance for later `Process B` IPC.
 * - `Process A` creates necessary IPC queue in the unique SHF instance.
 * - `Process A` spawns `Process B`; telling SHF instance to attach to.
 * - `Process B` uses the queue embedded into the attached SHF instance.
 * - `Process A` & `Process B` 'queue nirvana' at up to 90 million TPS.
 * - `Process A` eventually exits and cleanly tells `Process B` to exit.
 * - `Process A` deletes the SHF instance after `Process B` exited.
 * - `Process A` exits.
 * - Notes:
 *   - `Process A` has created the unique SHF & its queue before
 *     `Process B` exists so there is no race condition.
 *   - The IPC queue is only used while both processes are alive.
 *   - It is left up to `Process A` to decide how to communicate the SHF
 *     instance to `Process B` (e.g. command line argument?).
 *   - It is left up to `Process A` to decide that `Process B` is no
 *     longer using the SHF instance, so that the SHF instance can be
 *     deleted.
 *   - If `Process B` exits (e.g. unexpectedly) then it should not be
 *     expected that `Process A` can launch another `Process B` to
 *     continue where the old `Process B` left off. This is because of
 *     `qids_nolock_max`; see above.
 *
 * Caveats:
 * - Once shf_q_new() or shf_q_get() has been called then the key value
 *   store should no longer be used, or a seg fault might result. This
 *   limitation will be fixed in a future update.
 *
 * @section log_sec Multiplexed IPC Logging
 *
 * - todo: describe the big picture of how the logging works.
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

/**
 * @brief TODO: Change API to support these key types.
 */
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

/**
 * @brief TODO: Change API to support these value types.
 */
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

/* UINT32_MAX; note: defined here for use with either C or C++ clients */
#define SHF_DATA_TYPE_DELETED (0xff)
#define SHF_UID_NONE          (4294967295U) /*!< Value used to represent no uid */
#define SHF_QID_NONE          (4294967295U) /*!< Value used to represent no qid */
#define SHF_QIID_NONE         (4294967295U) /*!< Value used to represent no qiid */

extern __thread uint32_t   shf_uid          ;
extern __thread char     * shf_val          ;
extern __thread uint32_t   shf_val_len      ;
extern __thread uint32_t   shf_qiid         ;
extern __thread char     * shf_qiid_addr    ;
extern __thread uint32_t   shf_qiid_addr_len;

extern pid_t      shf_exec_child           (const char * child_path, const char * child_file, const char * child_argument_1, char  * child_argument_2);
extern char     * shf_backticks            (const char * command);
extern double     shf_get_time_in_seconds  (void);
extern void       shf_init                 (void);
extern void       shf_detach               (SHF * shf);
extern uint64_t   shf_get_vfs_available    (const char * path);
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
extern int        shf_upd_key_val          (SHF * shf);
extern int        shf_upd_uid_val          (SHF * shf, uint32_t uid);
extern void       shf_upd_callback_set     (int (*shf_upd_callback_new)(const char * val, uint32_t val_len));
extern int        shf_upd_callback_copy    (const char * val, uint32_t val_len);
extern char     * shf_del                  (SHF * shf);
extern uint64_t   shf_debug_get_garbage    (SHF * shf);
extern void       shf_debug_verbosity_less (void);
extern void       shf_debug_verbosity_more (void);
extern void       shf_set_data_need_factor (uint32_t data_needed_factor);
extern void       shf_set_is_lockable      (SHF * shf, uint32_t is_lockable);
extern void       shf_set_is_fixed_len     (SHF * shf, uint32_t fixed_key_len, uint32_t fixed_val_len);
extern void     * shf_q_new                (SHF * shf, uint32_t shf_qs, uint32_t shf_q_items, uint32_t shf_q_item_size, uint32_t qids_nolock_max);
extern void     * shf_q_get                (SHF * shf);
extern void       shf_q_del                (SHF * shf);
extern uint32_t   shf_q_new_name           (SHF * shf, const char * name, uint32_t name_len);
extern uint32_t   shf_q_get_name           (SHF * shf, const char * name, uint32_t name_len);
extern void       shf_q_push_head          (SHF * shf, uint32_t      qid, uint32_t qiid);
extern uint32_t   shf_q_pull_tail          (SHF * shf, uint32_t      qid                                       );
extern uint32_t   shf_q_push_head_pull_tail(SHF * shf, uint32_t push_qid, uint32_t push_qiid, uint32_t pull_qid);
extern uint32_t   shf_q_take_item          (SHF * shf, uint32_t      qiid                                      );
extern void       shf_q_flush              (SHF * shf, uint32_t pull_qid);
extern void       shf_q_size               (SHF * shf, uint32_t      qid);
extern uint32_t   shf_q_is_ready           (SHF * shf);
extern void       shf_race_init            (SHF * shf, const char * name, uint32_t name_len                 );
extern void       shf_race_start           (SHF * shf, const char * name, uint32_t name_len, uint32_t horses);
extern void       shf_log_init             (void);
extern void       shf_log_thread_new       (SHF * shf, uint32_t log_size, int log_fd);
extern void       shf_log_thread_del       (SHF * shf);
extern void       shf_log_attach_existing  (SHF * shf);
extern void       shf_log_append           (SHF * shf, const char * log_line, uint32_t log_line_len);
extern void       shf_log_output_set       (void (*shf_log_output_new)(char * log_line, uint32_t log_line_len));
extern int        shf_log_vfprintf         (FILE * stream, const char * format, va_list ap);
extern void       shf_log_fprintf          (FILE * stream, const char * format, ...);
extern void       shf_log_fputs            (const char * string, FILE * stream);
extern void       shf_log_fputc            (int character, FILE * stream);

// Useful links regarding /dev/shm:
// http://gerardnico.com/wiki/linux/shared_memory - Linux - Shared Memory (SHM) (/dev/shm)
// http://www.cyberciti.biz/tips/what-is-devshm-and-its-practical-usage.html - What Is /dev/shm And Its Practical Usage
// http://www.cyberciti.biz/files/linux-kernel/Documentation/filesystems/tmpfs.txt - Tmpfs is a file system which keeps all files in virtual memory

#endif /* __SHF_H__ */
