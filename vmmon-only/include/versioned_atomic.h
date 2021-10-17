/*********************************************************
 * Copyright (C) 1998,2007-2021 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * versioned_atomic.h --
 *
 *      This file implements versioned atomic synchronization, which allows
 *      concurrent accesses to shared data by a single writer and multiple
 *      readers. The algorithm is lock-free (it does not use a mutual exclusion
 *      mechanism), and the writer is wait-free (it never waits for readers).
 *
 *      Many-writer/many-reader can be implemented on top of versioned atomics
 *      by adding an external mutual exclusion mechanism to serialize all
 *      writer threads into a single logical writer thread. This is preferable
 *      for cases where readers are expected to greatly outnumber writers.
 *
 *      In literature, this is called a 'seqlock' (Sequence Lock).
 *      Seqlocks are also the implementation used by Linux VDSO for fast
 *      clock_gettime implementation.
 *         https://en.wikipedia.org/wiki/Seqlock
 *         https://lwn.net/Articles/22818/
 *
 *      For a thorough discussion of C++11/C11 memory models with seqlock, see
 *      "Can Seqlocks Get Along with Programming Language Memory Models?" by
 *      Hans-J. Boehm (who chaired the C++ Concurrency Study Group when atomics
 *      were added):
 *         https://web.archive.org/web/20201130051130/http://safari.ece.cmu.edu/MSPC2012/slides_posters/boehm-slides.pdf
 *         https://www.hpl.hp.com/techreports/2012/HPL-2012-68.pdf
 *         https://dl.acm.org/doi/10.1145/2247684.2247688  [MSPC 2012]
 *         https://hboehm.info/
 *      Boehm's article uses relaxed atomics for 'data', due to a concern about
 *      C++11/C11 not fully defining the difference between non-atomic and
 *      relaxed-atomic and the possibility of torn non-atomic access.
 *      This implementation assumes no practical difference.
 *
 *      One well-known danger of seqlocks: reads of the data could be torn
 *      (and multiple reads of the same data could give different values) prior
 *      to the version being verified.
 *
 *      Implementation
 *      -------------
 *      The standard seqlock (WITHOUT synchronization) is:
 *         int seq;
 *         struct { ... } data
 *         reader() {
 *            int seq0, seq1;
 *            do {
 *               seq0 = seq;
 *               // read data
 *               seq1 = seq;
 *            } while (seq0 != seq1 || seq0 & 1);
 *            // use copy of data
 *         }
 *         writer(...) {
 *            seq++;
 *            // write data
 *            seq++;
 *         }
 *      The LSB of seq functions as a "busy" bit saying write is in progress.
 *      The counter will wrap eventually, but is assumed to be large enough
 *      that this is impractical.
 *
 *      Atomic Considerations
 *      ---------------------
 *      The implemenation below uses C11 atomics (acquire/release/relaxed),
 *      including one of very few scenarios where C11 fences make sense. The
 *      naive, intuitive implementation of just making all writes Release and
 *      all reads Acquire is explicitly *incorrect*: data writes can re-order
 *      before the first increment, and data reads can re-order after the
 *      second sequence read. In C11, reordering can happen from either the
 *      hardware or compiler.
 *
 *      There are three correct strategies to ensure data reads/writes stay
 *      within the seqlock critical sections. This will discuss reads;
 *      equivalent reasoning applies to writes.
 *      1) Convert all reads to acquire. Acquires cannot reorder around each
 *         other, so this provides sufficient ordering. Free on x86 (where all
 *         loads are acquires), very expensive on Arm (where acquires are
 *         explicit barriers), and awkward to write code.
 *      2) Spurious write with release, to prevent ops from reordering later.
 *         Loads cannot release, so this forces a read-modify-write. Cheap
 *         enough on Arm, very expensive on x86 where the write would make
 *         the cache line exclusive.
 *      3) C11 thread fence. A (very) rare scenario where thread fence is
 *         needed, the fence is a two-way barrier. An acquire fence, in
 *         to the normal acquire behavior of preventing later operations
 *         from moving before THAT load, also prevents later operations
 *         from moving before ANY prior load; in effect, an acquire fence
 *         is an acquire operation on all memory. This also permits making
 *         the final read be relaxed. Free on x86 (where all loads are
 *         acquires), cheap on Arm (where the barrier is unavoidable).
 *      The seqlock implemented here WITH atomics is:
 *         reader() {
 *            int seq0, seq1;
 *            do {
 *               seq0 = atomic_load(seq, memory_order_acquire);
 *               // NB: merging the next two lines into an acquire load
 *               // has different (and incorrect) behavior.
 *               atomic_thread_fence(memory_order_acquire);
 *               seq1 = atomic_load(seq, memory_order_relaxed);
 *            } while (seq0 != seq1 || seq0 & 1);
 *            // use copy of data
 *         }
 *         writer(...) {
 *            int seq0 = atomic_load(seq, memory_order_relaxed);
 *            // NB: merging the next two lines into a release store
 *            // has different (and incorrect) behavior.
 *            atomic_store(seq, seq0+1, memory_order_relaxed);
 *            atomic_thread_fence(memory_order_release);
 *            // write data
 *            atomic_store(seq, seq0+2, memory_order_release);
 *         }
 *      A few surprising facts deserve mention:
 *      - Reads of the sequence variable within the writer can be relaxed,
 *        because of the single-writer assumption.
 *      - The last read in the reader, and first write in the writer, can
 *        be relaxed; the thread fences take care of all necessary ordering.
 *      - Reads within the write critical section could be moved before the
 *        critical section (by hardware or compiler).
 *      - Writes within the read critical section could be delayed to after
 *        the critical section (by hardware or compiler).
 *
 *      Future Directions
 *      -----------------
 *      Multiple writers can be supported either by using the LSB of the
 *      version field as a spinlock (BeginWrite loops with a cmpxchg), or
 *      by putting the entire writer inside an external lock. The external
 *      lock is probably a better idea.
 *
 *      The reader can be slightly tuned by busy-waiting until the LSB is
 *      not set to avoid some spurious reads. Since contention is expected to
 *      be very low, this is not currently implemented; keeping the lock
 *      wait-free (so the caller can deal with contention as needed) is
 *      more desirable.
 *
 *      Recursive use of versioned atomics in writers is not possible.
 */

#ifndef _VERSIONED_ATOMIC_H
#define _VERSIONED_ATOMIC_H

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "vm_basic_asm.h"
#include "vm_assert.h"
#include "vm_atomic.h"
#include "vm_atomic_acqrel.h"
#include "vm_atomic_relaxed.h"

#if defined __cplusplus
extern "C" {
#endif


/*
 * Users with attribute(packed) structs must ensure any
 * VersionedAtomic members are marked as ALIGNED(4).  Unfortunately
 * the compiler cannot be trusted to align our substruct correctly
 * (PR515329).  If the enclosing struct is packed, the VersionedAtomic
 * alignment requested below will be ignored!
 */

typedef struct {
   Atomic_uint32 atomic;
   uint32 _pad;   // temporary; don't change the size right now
} ALIGNED(4) VersionedAtomic;
typedef uint32 VersionedAtomicCookie;


/*
 *-----------------------------------------------------------------------------
 *
 * VersionedAtomic_Init --
 *
 *      Optional initialization function. (VersionedAtomic can also
 *      be zero-initialized safely).
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
VersionedAtomic_Init(VersionedAtomic *versions,    // OUT
                     VersionedAtomicCookie value)  // IN
{
   /* Later ASSERTs use LSB to indicate write in progress */
   ASSERT((value & 0x1) == 0);

   /*
    * VersionedAtomic assumes single-writer, so initialization does not
    * need to be atomic.
    */
   Atomic_Write32Relaxed(&versions->atomic, value);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VersionedAtomic_BeginWrite --
 *
 *      Called by the writer to indicate that it is about to write to the
 *      shared data protected by 'versions'. Effectively locks out all readers
 *      until VersionedAtomic_EndWrite() is called.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
VersionedAtomic_BeginWrite(VersionedAtomic *versions) // IN
{
   VersionedAtomicCookie v = Atomic_Read32Relaxed(&versions->atomic);

   /* Catch improper initialization or unsupported recursive use. */
   ASSERT((v & 0x1) == 0);

   /*
    * Do not use Atomic_Inc32() below: it is more expensive than
    * Atomic_Write32() and unnecessary: only the write needs to be atomic, not
    * the whole read + inc + write.
    * VersionedAtomic assumes single-writer, so reads are always local and
    * ONLY writes need to be synchronized with other threads.
    */
#if !defined VMM
   /*
    * Atomic_Write32() below requires the address to be naturally aligned, but
    * currently only checks that condition when VMM is defined (tracked by
    * PR1055836). So until that PR is fixed, we must check that condition here
    * when VMM is not defined.
    */
   ASSERT(((uintptr_t)&versions->atomic % sizeof versions->atomic) == 0);
#endif
   Atomic_Write32Relaxed(&versions->atomic, v + 1);
   Atomic_FenceRelease();  // Do not merge - see comment at top of file
}


/*
 *-----------------------------------------------------------------------------
 *
 * VersionedAtomic_EndWrite --
 *
 *      Called by the writer to indicate that it has finished writing to the
 *      shared data protected by 'versions'. Lets pending and new readers
 *      proceed.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
VersionedAtomic_EndWrite(VersionedAtomic *versions) // IN
{
   VersionedAtomicCookie v = Atomic_Read32Relaxed(&versions->atomic);

   /* Catch improper EndWrite without BeginWrite */
   ASSERT((v & 0x1) != 0);

   /*
    * VersionedAtomic assumes single-writer, so reads are always local and
    * ONLY writes need to be synchronized with other threads. If this isn't
    * clear, picture versions->atomic being cached from BeginWrite.
    */
   Atomic_Write32Release(&versions->atomic, v + 1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VersionedAtomic_BeginTryRead --
 *
 *      Called by a reader before it starts reading the shared data protected
 *      by 'versions'.
 *
 * Results:
 *      A version number for the caller to pass to the matching call to
 *      VersionedAtomic_EndTryRead(), if any.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE VersionedAtomicCookie
VersionedAtomic_BeginTryRead(VersionedAtomic const *versions) // IN
{
   return Atomic_Read32Acquire(&versions->atomic);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VersionedAtomic_EndTryRead --
 *
 *      Called by a reader after it has finished reading the shared data
 *      protected by 'versions', to confirm that the data is consistent, i.e.
 *      that the writer has not modified the data while the reader was reading
 *      it.
 *
 * Results:
 *      TRUE if the shared data read between VersionedAtomic_BeginTryRead() and
 *      this call is consistent.
 *      FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
VersionedAtomic_EndTryRead(VersionedAtomic const *versions,    // IN
                           VersionedAtomicCookie readVersion)  // IN
{
   /*
    * There is a theoretical ABA issue here: if the writer updates the shared
    * data exactly 2^31 times while a reader reads it, the read will
    * incorrectly be considered consistent. In practice, this issue is
    * unlikely, so we ignore it. But should we need it, we could make the issue
    * even less likely by using 64-bit version variables.
    */
   Atomic_FenceAcquire();  // Do not merge - see comment at top of file
   return LIKELY(Atomic_Read32Relaxed(&versions->atomic) == readVersion && // not torn
                 (readVersion & 0x1) == 0);  // not writing
}


#if defined __cplusplus
} // extern "C"
#endif

#endif //_VERSIONED_ATOMIC_H
