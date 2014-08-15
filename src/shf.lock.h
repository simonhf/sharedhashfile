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

#ifndef __SHF_LOCK_H__
#define __SHF_LOCK_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE      /* See feature_test_macros(7) */
#endif
#include <unistd.h>
#include <sys/syscall.h> /* For SYS_xxx definitions; see man syscall */
#include <sched.h>
#include <sys/types.h>
#include <stdio.h>       /* for stderr */
#include <errno.h>       /* for errno */
#include <stdlib.h>      /* for exit() */
#include <sys/stat.h>    /* for stat() */

#include "shf.defines.h"

/* The following inline function is based on ReactOS; the license in the source file is MIT
 * See: http://code.reactos.org/browse/reactos/trunk/reactos/include/crt/mingw32/intrin_x86.h?r=62389
 */

#define __INTRIN_INLINE extern __inline__ __attribute__((__always_inline__,__gnu_inline__))

#if (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) > 40100

    static inline long
    InterlockedCompareExchange(volatile long * const Destination, const long Exchange, const long Comperand)
    {
        return __sync_val_compare_and_swap(Destination, Comperand, Exchange);
    }

    static inline long
    InterlockedExchangeAdd(long volatile * Addend, long Value)
    {
        return __sync_add_and_fetch(Addend, Value);
    }

    #define SHF_GETTID() syscall(SYS_gettid)
    #define SHF_YIELD() sched_yield()

#else

    #error "'gcc --version' must be at least 4.1.0"

#endif

#define SHF_SPIN_LOCK_SPIN_MAX (1000000) /* number of yields before lock fails */

typedef enum SHF_SPIN_LOCK_STATUS {
    SHF_SPIN_LOCK_STATUS_FAILED        , /* Locking failed (reached max spin count) */
    SHF_SPIN_LOCK_STATUS_LOCKED        , /* Locked successfully                     */
    SHF_SPIN_LOCK_STATUS_LOCKED_ALREADY  /* Already has lock - don't double unlock! */
}  SHF_LOCK_STATUS;

typedef struct SHF_SPIN_LOCK {
    volatile long     lock;
    volatile pid_t    pid; /* todo: make long lock hold tid & pid to save memory */
#ifdef SHF_DEBUG_VERSION
    volatile uint32_t line;
    volatile uint64_t macro;
    volatile uint64_t conflicts; /* count how often lock did not lock on first try */
#endif
} SHF_SPIN_LOCK;

static inline void
shf_spin_unlock_force(SHF_SPIN_LOCK * lock, long old_tid)
{
    InterlockedCompareExchange(&lock->lock, 0, old_tid);
} /* shf_spin_unlock_force() */

static inline SHF_LOCK_STATUS
shf_spin_lock(SHF_SPIN_LOCK * lock)
{
    uint64_t spin;
    long     our_tid = SHF_GETTID();
    long     old_tid;

RETRY_LOCK_AFTER_FORCE:;

    spin = 0;

    while ((spin < SHF_SPIN_LOCK_SPIN_MAX) && ((old_tid = InterlockedCompareExchange(&lock->lock, our_tid, 0)) != 0)) {
        if (old_tid == our_tid) {
            fprintf(stderr, "lock %p already held by our tid %ld\n", &lock->lock, our_tid);
            return SHF_SPIN_LOCK_STATUS_LOCKED_ALREADY;
        }
        spin ++;
        SHF_YIELD();
    }

#ifdef SHF_DEBUG_VERSION
    if (spin) {
        lock->conflicts ++;
    }
#endif

    if (spin == SHF_SPIN_LOCK_SPIN_MAX) {
        char proc_pid_task_tid[256];
        SHF_SNPRINTF(1, proc_pid_task_tid, "/proc/%u/task/%lu", lock->pid, old_tid);
        struct stat sb;
        int value = stat(proc_pid_task_tid, &sb);
        if (-1 == value && ENOENT == errno) {
#ifdef SHF_DEBUG_VERSION
            fprintf(stderr, "WARN: lock reached max %u spins for tid %lu of pid %u because of tid %lu of pid %u which went poof after line %u at macro pos %lu; forcing lock\n", SHF_SPIN_LOCK_SPIN_MAX, our_tid, getpid(), old_tid, lock->pid, lock->line, lock->macro);
#else
            fprintf(stderr, "WARN: lock reached max %u spins for tid %lu of pid %u because of tid %lu of pid %u which went poof; forcing lock\n", SHF_SPIN_LOCK_SPIN_MAX, our_tid, getpid(), old_tid, lock->pid);
#endif
            shf_spin_unlock_force(lock, old_tid); /* force lock to be unlocked */
            goto RETRY_LOCK_AFTER_FORCE; /* because several threads / processes might try to force at the same time */
        }
        else {
            fprintf(stderr, "WARN: lock reached max %u spins for tid %lu of pid %u because of tid %lu of pid %u which is waaay too busy... why?\n", SHF_SPIN_LOCK_SPIN_MAX, our_tid, getpid(), old_tid, lock->pid);
            return SHF_SPIN_LOCK_STATUS_FAILED;
        }
    }

    lock->pid = getpid();
    return SHF_SPIN_LOCK_STATUS_LOCKED;
} /* shf_spin_lock() */

static inline SHF_LOCK_STATUS
shf_spin_lock_try(SHF_SPIN_LOCK * lock)
{
    long our_tid = SHF_GETTID();
    long old_tid;

    if ((old_tid = InterlockedCompareExchange(&lock->lock, our_tid, 0)) != 0) {
        if (old_tid == our_tid) {
            fprintf(stderr, "lock %p already held by our tid %ld\n", &lock->lock, our_tid);
            return SHF_SPIN_LOCK_STATUS_LOCKED_ALREADY;
        }
#ifdef SHF_DEBUG_VERSION
        lock->conflicts ++;
#endif
        return SHF_SPIN_LOCK_STATUS_FAILED;
    }

    lock->pid = getpid();
    return SHF_SPIN_LOCK_STATUS_LOCKED;
} /* shf_spin_lock_try() */

static inline void
shf_spin_unlock(SHF_SPIN_LOCK * lock)
{
    long our_tid = SHF_GETTID();
    long old_tid = InterlockedCompareExchange(&lock->lock, 0, our_tid);

    SHF_ASSERT(old_tid == our_tid, "lock %p is held by thread %ld, not our tid %ld\n", &lock->lock, old_tid, our_tid);
} /* shf_spin_unlock() */

/* C version ported from assembly language 'fair read/write spinlocks' Copyright (c) 2002 David Howells (dhowells@redhat.com); See: https://lkml.org/lkml/2002/11/8/102 */

#define SHF_BARRIER()   asm volatile(       "": : :"memory")
#define SHF_CPU_PAUSE() asm volatile("pause\n": : :"memory")

union SHF_RW_LOCK_UNION
{
    volatile uint32_t as_u64;
    volatile uint32_t as_u32;
    volatile uint16_t as_u16;
    struct
    {
        volatile uint8_t     ticket_active_writer; /* starts at 1<<(0 * 8) in uint64_t */
        volatile uint8_t pad_ticket_active_writer; /* starts at 1<<(1 * 8) in uint64_t */
        volatile uint8_t     ticket_active_reader; /* starts at 1<<(2 * 8) in uint64_t */
        volatile uint8_t pad_ticket_active_reader; /* starts at 1<<(3 * 8) in uint64_t */
        volatile uint8_t     ticket_next         ; /* starts at 1<<(4 * 8) in uint64_t */
        volatile uint8_t pad_ticket_next         ; /* starts at 1<<(5 * 8) in uint64_t */
        volatile uint8_t     pad                 ; /* starts at 1<<(6 * 8) in uint64_t */
        volatile uint8_t pad_pad                 ; /* starts at 1<<(7 * 8) in uint64_t */
    } as_u08;
} __attribute__((packed));

typedef struct SHF_RW_LOCK {
    volatile union SHF_RW_LOCK_UNION lock;
    volatile pid_t                   pid;
#ifdef SHF_DEBUG_VERSION
    volatile uint32_t                line;
    volatile uint64_t                macro;
    volatile uint64_t                conflicts; /* count how often lock did not lock on first try */
#endif
} SHF_RW_LOCK;

/*                                                 0x0706050403020100 */
#define SHF_RW_LOCK_INC_TICKET_NEXT_ACTIVE_WRITER (0x0000000000000001)
#define SHF_RW_LOCK_INC_TICKET_NEXT_ACTIVE_READER (0x0000000000010000)
#define SHF_RW_LOCK_INC_TICKET_NEXT               (0x0000000100000000)

static inline void
shf_rw_lock_writer(SHF_RW_LOCK * lock)
{
    uint64_t spin                  = 0;
    uint64_t tmp                   = __sync_fetch_and_add_8(&lock->lock.as_u64, SHF_RW_LOCK_INC_TICKET_NEXT); /* atomic increment ticket_next */
             tmp                 >>= 32;
    uint8_t  ticket_next_pre_inc   = tmp & 0xff;

    lock->lock.as_u08.pad_ticket_next = 0; /* ensure overflow never gets too big */

    while (ticket_next_pre_inc != lock->lock.as_u08.ticket_active_writer) { SHF_CPU_PAUSE(); spin ++; }

#ifdef SHF_DEBUG_VERSION
    if (spin) {
        lock->conflicts ++;
    }
#endif
} /* shf_rw_lock_writer() */

static inline void
shf_rw_unlock_writer(SHF_RW_LOCK * lock)
{
    if (0) {
        SHF_RW_LOCK tmp;
        tmp.lock.as_u32 = lock->lock.as_u32;

        SHF_BARRIER();

        tmp.lock.as_u08.ticket_active_writer ++;
        tmp.lock.as_u08.ticket_active_reader ++;

        lock->lock.as_u32 = tmp.lock.as_u32;
    }
    else {
        __sync_fetch_and_add_8(&lock->lock.as_u64, SHF_RW_LOCK_INC_TICKET_NEXT_ACTIVE_WRITER + SHF_RW_LOCK_INC_TICKET_NEXT_ACTIVE_READER); /* atomic increment ticket_active_writer & ticket_active_reader */
        lock->lock.as_u08.pad_ticket_active_writer = 0; /* ensure overflow never gets too big */
        lock->lock.as_u08.pad_ticket_active_reader = 0; /* ensure overflow never gets too big */
    }
} /* shf_rw_unlock_writer() */

static inline void
shf_rw_lock_reader(SHF_RW_LOCK * lock)
{
    uint64_t spin                  = 0;
    uint64_t tmp                   = __sync_fetch_and_add_8(&lock->lock.as_u64, SHF_RW_LOCK_INC_TICKET_NEXT); /* atomic increment ticket_next */
             tmp                 >>= 32;
    uint8_t  ticket_next_pre_inc   = tmp & 0xff;

    lock->lock.as_u08.pad_ticket_next = 0; /* ensure overflow never gets too big */

    while (ticket_next_pre_inc != lock->lock.as_u08.ticket_active_reader)  { SHF_CPU_PAUSE(); spin ++; } /* todo: add pid to identify trouble-maker when spinned for too long */
    if (0) {
        lock->lock.as_u08.ticket_active_reader ++; /* so that other readers can read concurrently (if no writer) */
    }
    else {
        __sync_add_and_fetch_8(&lock->lock.as_u64, SHF_RW_LOCK_INC_TICKET_NEXT_ACTIVE_READER); /* atomic increment ticket_active_reader */
        lock->lock.as_u08.pad_ticket_active_reader = 0; /* ensure overflow never gets too big */
    }

#ifdef SHF_DEBUG_VERSION
    if (spin) {
        lock->conflicts ++;
        /* todo: record bucket stats for number of spins; consider using SHF_YIELD() instead of SHF_SHF_CPU_PAUSE() if too many spins */
    }
#endif
} /* shf_rw_lock_reader() */

static inline void
shf_rw_unlock_reader(SHF_RW_LOCK * lock)
{
    __sync_add_and_fetch_8(&lock->lock.as_u64, SHF_RW_LOCK_INC_TICKET_NEXT_ACTIVE_WRITER); /* atomic increment ticket_active_writer */
    lock->lock.as_u08.pad_ticket_active_writer = 0; /* ensure overflow never gets too big */
} /* shf_rw_unlock_reader() */

#if 0

#define SHF_LOCK                SHF_SPIN_LOCK
#define SHF_LOCK_READER(LOCK)   SHF_DEBUG("- locking for %s() // aka spinlock()\n", __FUNCTION__); while (SHF_SPIN_LOCK_STATUS_LOCKED != shf_spin_lock(LOCK)) { SHF_DEBUG("- failed to spin lock for %s; trying again\n", __FUNCTION__); }
#define SHF_UNLOCK_READER(LOCK)                                                                                                          shf_spin_unlock(LOCK); SHF_DEBUG("- unlocked for %s() // aka spinlock()\n", __FUNCTION__);
#define SHF_LOCK_WRITER(LOCK)   SHF_LOCK_READER(LOCK)
#define SHF_UNLOCK_WRITER(LOCK) SHF_UNLOCK_READER(LOCK)

#else

#define SHF_LOCK                SHF_RW_LOCK
#define SHF_LOCK_READER(LOCK)   SHF_DEBUG("- rw locking for reader %s()\n", __FUNCTION__); shf_rw_lock_reader(LOCK)
#define SHF_UNLOCK_READER(LOCK)                                                            shf_rw_unlock_reader(LOCK); SHF_DEBUG("- rw unlocked for reader %s()\n", __FUNCTION__);
#define SHF_LOCK_WRITER(LOCK)   SHF_DEBUG("- rw locking for writer %s()\n", __FUNCTION__); shf_rw_lock_writer(LOCK)
#define SHF_UNLOCK_WRITER(LOCK)                                                            shf_rw_unlock_writer(LOCK); SHF_DEBUG("- rw unlocked for writer %s()\n", __FUNCTION__);

#endif

#endif /* __SHF_LOCK_H__ */
