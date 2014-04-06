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

#include <sched.h>
#include <sys/types.h>
#include <sys/syscall.h>

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

#define SHF_LOCK_SPIN_MAX (1000000) /* number of yields before lock fails */

typedef enum SHF_LOCK_STATUS {
    SHF_LOCK_STATUS_FAILED        , /* Locking failed (reached max spin count) */
    SHF_LOCK_STATUS_LOCKED        , /* Locked successfully                     */
    SHF_LOCK_STATUS_LOCKED_ALREADY  /* Already has lock - don't double unlock! */
}  SHF_LOCK_STATUS;

typedef struct SHF_LOCK {
    volatile long     lock;
    volatile pid_t    pid; /* todo: make long lock hold tid & pid to save memory */
#ifdef SHF_DEBUG_VERSION
    volatile uint32_t line;
    volatile uint64_t macro;
    volatile uint64_t conflicts; /* count how often lock did not lock on first try */
#endif
} SHF_LOCK;

static inline void
shf_lock_force(SHF_LOCK * lock, long tid)
{
    InterlockedCompareExchange(&lock->lock, tid, InterlockedCompareExchange(&lock->lock, tid, tid));
} /* shf_lock_force() */

static inline SHF_LOCK_STATUS
shf_lock(SHF_LOCK * lock)
{
    unsigned spin    = 0;
    long     our_tid = SHF_GETTID();
    long     old_tid;

    while ((spin < SHF_LOCK_SPIN_MAX) && ((old_tid = InterlockedCompareExchange(&lock->lock, our_tid, 0)) != 0)) {
        if (old_tid == our_tid) {
            fprintf(stderr, "lock %p already held by our tid %ld\n", &lock->lock, our_tid);
            return SHF_LOCK_STATUS_LOCKED_ALREADY;
        }
        spin ++;
        SHF_YIELD();
    }

#ifdef SHF_DEBUG_VERSION
    if (spin) {
        lock->conflicts ++;
    }
#endif

    if (spin == SHF_LOCK_SPIN_MAX) {
        char proc_pid_task_tid[256];
        SHF_SNPRINTF(1, proc_pid_task_tid, "/proc/%u/task/%lu", lock->pid, old_tid);
        struct stat sb;
        int value = stat(proc_pid_task_tid, &sb);
        if (-1 == value && ENOENT == errno) {
#ifdef SHF_DEBUG_VERSION
            fprintf(stderr, "WARN: lock reached max %u spins for tid %lu of pid %u because of tid %lu of pid %u which went poof after line %u at macro pos %lu; forcing lock\n", SHF_LOCK_SPIN_MAX, our_tid, getpid(), old_tid, lock->pid, lock->line, lock->macro);
#else
            fprintf(stderr, "WARN: lock reached max %u spins for tid %lu of pid %u because of tid %lu of pid %u which went poof; forcing lock\n", SHF_LOCK_SPIN_MAX, our_tid, getpid(), old_tid, lock->pid);
#endif
            shf_lock_force(lock, our_tid);
        }
        else {
            fprintf(stderr, "WARN: lock reached max %u spins for tid %lu of pid %u because of tid %lu of pid %u which is waaay too busy... why?\n", SHF_LOCK_SPIN_MAX, our_tid, getpid(), old_tid, lock->pid);
            return SHF_LOCK_STATUS_FAILED;
        }
    }

    lock->pid = getpid();
    return SHF_LOCK_STATUS_LOCKED;
} /* shf_lock() */

static inline void
shf_unlock(SHF_LOCK * lock)
{
    long our_tid = SHF_GETTID();
    long old_tid = InterlockedCompareExchange(&lock->lock, 0, our_tid);

    SHF_ASSERT(old_tid == our_tid, "lock %p is held by thread %ld, not our tid %ld\n", &lock->lock, old_tid, our_tid);
} /* shf_unlock() */

#endif /* __SHF_LOCK_H__ */

