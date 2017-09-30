/*********************************************************
 * Copyright (C) 1998,2015-2016 VMware, Inc. All rights reserved.
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
 *      Implementation
 *      --------------
 *      This implementation is based on Leslie Lamport's paper "Concurrent
 *      Reading and Writing", Communications of the ACM, November 1977
 *      (http://url.eng.vmware.com/udcc ).
 *
 *      Lamport's algorithm was designed for systems in which the reader and
 *      the writers run on separate computers and where the version variables
 *      (v0 and v1 in this implementation) are stored on a shared hard disk. So
 *      it did not assume that the version variables could be accessed
 *      atomically. Instead, it assumed that the version variables were made of
 *      multiple basic units of data, called "digits", and that each digit
 *      could be accessed atomically. The upper-left corner of page 4 of the
 *      paper precisely and concisely describes the algorithm, and in
 *      particular the order in which digits of the version variables must be
 *      accessed (left to right, right to left, or no particular order).
 *
 *      This implementation is designed for systems in which the reader and the
 *      writers run on the same computer and where the version variables are
 *      stored in shared memory. So it assumes that the version variables can
 *      be accessed atomically. As a result, this implementation uses a
 *      simplified version of Lamport's algorithm, where:
 *      o Each left to right or right to left access in Lamport's algorithm is
 *        replaced with a single atomic access in this implementation.
 *      o Each access in no particular order in Lamport's algorithm is replaced
 *        with a single non-atomic access in this implementation.
 *      Note: The writer is the only thread which writes to version variables,
 *            so it is allowed to read them non-atomically.
 *
 *      Multiple concurrent writers to the version variables are not allowed.
 *      Even if writers are working on lock-free or disjoint data, the version
 *      variables are not interlocked for read-modify-write. See PR514764.
 *
 *      Recursive use of versioned atomics in writers is currently not
 *      supported. See PR514764.
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
   uint32 v0;
   uint32 v1;
} ALIGNED(4) VersionedAtomic;


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
   /* Catch improper initialization or unsupported recursive use. */
   ASSERT(versions->v1 == versions->v0);

   /*
    * Do not use Atomic_Inc32() below: it is more expensive than
    * Atomic_Write32() and unnecessary: only the write needs to be atomic, not
    * the whole read + inc + write.
    */
#if !defined VMM
   /*
    * Atomic_Write32() below requires the address to be naturally aligned, but
    * currently only checks that condition when VMM is defined (tracked by
    * PR1055836). So until that PR is fixed, we must check that condition here
    * when VMM is not defined.
    */
   ASSERT(((uintptr_t)&versions->v0 % sizeof versions->v0) == 0);
#endif
   Atomic_Write32((Atomic_uint32 *)&versions->v0, versions->v0 + 1);

   /*
    * The write to 'versions->v0' must be observed by all other OS CPUs before
    * the write(s) to the shared data.
    *
    * On arm64, this barrier is Full System, which is probably too strong:
    * Inner Shareable (== all OS CPUs) should work just fine.
    */
   SMP_W_BARRIER_W();
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
   /*
    * The write(s) to the shared data must be observed by all other OS CPUs
    * before the write to 'versions->v1'.
    *
    * On arm64, this barrier is Full System, which is probably too strong:
    * Inner Shareable (== all OS CPUs) should work just fine.
    */
   SMP_W_BARRIER_W();

   ASSERT(versions->v1 + 1 == versions->v0);

#if !defined VMM
   /*
    * Atomic_Write32() below requires the address to be naturally aligned, but
    * currently only checks that condition when VMM is defined (tracked by
    * PR1055836). So until that PR is fixed, we must check that condition here
    * when VMM is not defined.
    */
   ASSERT(((uintptr_t)&versions->v1 % sizeof versions->v1) == 0);
#endif
   Atomic_Write32((Atomic_uint32 *)&versions->v1, versions->v0);
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

static INLINE uint32
VersionedAtomic_BeginTryRead(VersionedAtomic const *versions) // IN
{
   uint32 readVersion = Atomic_Read32((Atomic_uint32 *)&versions->v1);

   /*
    * The read from 'versions->v1' must happen on this CPU before the read(s)
    * from the shared data.
    *
    * On arm64, this barrier is Full System, which is probably too strong:
    * Inner Shareable (== all OS CPUs) should work just fine, and maybe
    * Non-shareable (== current CPU) would work too.
    */
   SMP_R_BARRIER_R();

   return readVersion;
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
VersionedAtomic_EndTryRead(VersionedAtomic const *versions, // IN
                           uint32 readVersion)              // IN
{
   /*
    * The read(s) from the shared data must happen on this CPU before the read
    * from 'versions->v0'.
    *
    * On arm64, this barrier is Full System, which is probably too strong:
    * Inner Shareable (== all OS CPUs) should work just fine, and maybe
    * Non-shareable (== current CPU) would work too.
    */
   SMP_R_BARRIER_R();

   /*
    * There is a theoretical ABA issue here: if the writer updates the shared
    * data exactly 2^32 times while a reader reads it, the read will
    * incorrectly be considered consistent. In practice, this issue is
    * unlikely, so we ignore it. But should we need it, we could make the issue
    * even less likely by using 64-bit version variables.
    */
   return LIKELY(   Atomic_Read32((Atomic_uint32 *)&versions->v0)
                 == readVersion);
}


#if defined __cplusplus
} // extern "C"
#endif

#endif //_VERSIONED_ATOMIC_H
