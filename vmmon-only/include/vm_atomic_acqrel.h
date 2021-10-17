/*********************************************************
 * Copyright (C) 2018-2021 VMware, Inc. All rights reserved.
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
 * vm_atomic_acqrel.h --
 *
 *       Declares atomics with acquire/release ordering.
 *
 * NOTE: Usage of these atomics should be rare and limited to implementations
 *       of lockless algorithms. Most scenarios are better solved with locks -
 *       which themselves are implemented using these atomics.
 *
 *       Inclusion of this header serves as a flag that a file needs careful
 *       review and auditing due to the difficulty of writing correct lockless
 *       code. [In the future, bora/doc/atomics will contain documentation
 *       to explain the subtleties of non-sequential atomics.]
 *
 *       A good overview of weak memory orderings and their caveats is from
 *       Hans-J. Boehm, who chaired the C++ technical committee that defined
 *       C++11/C11 atomics.
 *          http://schd.ws/hosted_files/cppcon2016/74/HansWeakAtomics.pdf
 *          https://hboehm.info/
 *
 *
 *       Acquire/Release can best be thought of using a "roach motel" model,
 *       defining a box:
 *           A[ xxxx ]R.
 *
 *       The acquire prevents the contents of the box (xxx) from jumping out
 *       the left side. The release prevents the contents (xxx) from jumping
 *       out the right side. Some reordering *is* allowed: stuff outside the
 *       box may legally jump into the box (where-as sequentially consistent,
 *       aka "full", memory barriers prevent that).
 *
 *       Fences are slightly stronger, and may be thought of as an operation
 *       applied to all memory locations (i.e. they have some
 *       bi-directionality). However, fences are rarely needed.
 *       For more information on the difference between operations and fences:
 *          https://preshing.com/20131125/acquire-and-release-fences-dont-work-the-way-youd-expect/
 *
 *       Publishing changes cross-thread is a release activity, consuming
 *       those changes is an acquire activity.
 *
 *
 *       Acquire/Release semantics are very hard to get right. For example,
 *       a naive implementor might think:
 *           IncRef ~= Acquire
 *           DecRef ~= Release
 *       struct { ref, x=0 } ptr
 *       T1: decrements 2->1          T2: decrements 1->0
 *       ptr->x = 5                   if (DecRef(ptr->ref) == 0)
 *       DecRef(ptr->ref)                return ptr->x;
 *
 *       This can indeterminately return 0 or 5. The 0 return value
 *       comes from this order, which is permitted by the barriers.
 *       T2: read ptr->x      <--- reads can can be lofted as long as
 *       T1: ptr->x = 5            they do not cross an Acquire
 *       T1: DecRef(ptr->ref)
 *       T2: DecRef(ptr->ref)
 *
 *       This race only occurs on the last DecRef, when the thread which
 *       dropped the refcount to zero actually must re-acquire the object
 *       before doing anything further to it (like reading or freeing).
 *       The simplest correct DecRef is this:
 *          DecRef(ptr) {
 *             if (0 == DecAcquireRelease(ptr->ref)) {
 *                 free(ptr);
 *             }
 *          }
 *       On some platforms (depending on what sort of barrier a decrement is),
 *       a more optimal DecRef may be:
 *          DecRef(ptr) {
 *             if (0 == DecRelease(ptr->ref)) {
 *                 ReadAcquire(ptr->ref);  // Force writes from other threads
 *                 free(ptr);              // to be visible to this thread
 *             }
 *          }
 */

#ifndef _ATOMIC_ACQREL_H_
#define _ATOMIC_ACQREL_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKDRIVERS
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_atomic.h"

#if defined __cplusplus
extern "C" {
#endif

/*
 * Enable use of movq (_mm_loadl_epi64 and _mm_storel_epi64). These SSE2
 * instructions allow 64bit reads and writes even in 32bit contexts and
 * are much more efficient than lock cmpxchg64 calls.
 *
 * Note: we do *not* enable the use of these intrinsics in x86 kernels.
 * Why: Both Windows and Linux drivers must explicitly call kernel functions
 *      to save/restore FPU registers when using the FPU. In Linux, this
 *      places the thread in "atomic context" where preemption is disabled.
 *      These functions mustn't place such constraints on the caller.
 */
#if !defined USE_XMM_ATOMICS_ON_I386 && defined __i386__ && defined USERLEVEL
   /* _M_IX86_FP >= 2 is the MSVC equivalent of defined(__SSE2__) */
#  if (defined __GNUC__ && defined __SSE2__) || \
      (defined _MSC_VER && _M_IX86_FP >= 2)
#     define USE_XMM_ATOMICS_ON_I386 1
#  endif
#endif
#if !defined USE_XMM_ATOMICS_ON_I386
#  define USE_XMM_ATOMICS_ON_I386 0
#endif

#if USE_XMM_ATOMICS_ON_I386
#  include <emmintrin.h>
#endif


#if defined VM_ARM_64
#   include "vm_atomic_arm64_begin.h"
#endif
#include "vm_basic_asm.h"


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Read8Acquire --
 *
 *      Read the value of the specified object atomically (acquire ordering).
 *
 * Results:
 *      The value of the atomic variable.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint8
Atomic_Read8Acquire(Atomic_uint8 const *var)  // IN:
{
   uint8 val;

#if defined __GNUC__
#  if defined __i386__ || defined __x86_64__
   __asm__ __volatile__(
      "movb %1, %0"
      : "=q" (val)
      : "m" (var->value)
      : "memory" // Must minimally prevent #LoadLoad/#LoadStore compiler reordering
   );

#  elif defined VM_ARM_64
   val = _VMATOM_X(R_SC, 8, &var->value);
#  else
   /* seq-cst until better code is available */
   val = Atomic_ReadIfEqualWrite8((Atomic_uint8 *)var, 0, 0);
#  endif

#elif defined _MSC_VER
#  if defined VM_ARM_64 && defined _ISO_VOLATILE
   val = (uint8)__iso_volatile_load8((volatile char *)&var->value);
#  elif defined __i386__ || defined __x86_64__ || defined VM_ARM_64
   val = var->value;   // "cl.exe /volatile:ms": volatile reads have acquire semantics
#  else
#  error Unimplemented MSVC arch Atomic_Read8Acquire
#  endif
#  if defined _ISO_VOLATILE
   SMP_R_BARRIER_RW(); // Prevent #LoadLoad/#LoadStore reordering
#  endif

#else
#error No compiler defined for Atomic_Read8Acquire
#endif

   return val;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Read16Acquire --
 *
 *      Read the value of the specified object atomically (acquire ordering).
 *
 * Results:
 *      The value of the atomic variable.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint16
Atomic_Read16Acquire(Atomic_uint16 const *var)  // IN:
{
   uint16 val;

   ASSERT((uintptr_t)var % 2 == 0);

#if defined __GNUC__
#  if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "movw %1, %0"
      : "=r" (val)
      : "m" (var->value)
      : "memory" // Must minimally prevent #LoadLoad/#LoadStore compiler reordering
   );

#  elif defined VM_ARM_64
   val = _VMATOM_X(R_SC, 16, &var->value);
#  else
   /* seq-cst until better code is available */
   val = Atomic_ReadIfEqualWrite16((Atomic_uint16 *)var, 0, 0);
#  endif

#elif defined _MSC_VER
#  if defined VM_ARM_64 && defined _ISO_VOLATILE
   val = (uint16)__iso_volatile_load16((volatile short *)&var->value);
#  elif defined __i386__ || defined __x86_64__ || defined VM_ARM_64
   val = var->value;   // "cl.exe /volatile:ms": volatile reads have acquire semantics
#  else
#  error Unimplemented MSVC arch Atomic_Read16Acquire
#  endif
#  if defined _ISO_VOLATILE
   SMP_R_BARRIER_RW(); // Prevent #LoadLoad/#LoadStore reordering
#  endif

#else
#error No compiler defined for Atomic_Read16Acquire
#endif

   return val;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Read32Acquire --
 *
 *      Read the value of the specified object atomically (acquire ordering).
 *
 * Results:
 *      The value of the atomic variable.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
Atomic_Read32Acquire(Atomic_uint32 const *var)  // IN:
{
   uint32 val;

   ASSERT((uintptr_t)var % 4 == 0);

#if defined __GNUC__
   /*
    * Use inline assembler to force using a single load instruction to
    * ensure that the compiler doesn't split a transfer operation into multiple
    * instructions.
    */

#  if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "movl %1, %0"
      : "=r" (val)
      : "m" (var->value)
      : "memory" // Must minimally prevent #LoadLoad/#LoadStore compiler reordering
   );
#  elif defined VM_ARM_64
   val = _VMATOM_X(R_SC, 32, &var->value);
#  else
   /* seq-cst until better code is available */
   val = Atomic_ReadIfEqualWrite32((Atomic_uint32 *)var, 0, 0);
#  endif

#elif defined _MSC_VER
#  if defined VM_ARM_64 && defined _ISO_VOLATILE
   val = (uint32)__iso_volatile_load32((volatile int *)&var->value);
#  elif defined __i386__ || defined __x86_64__ || defined VM_ARM_64
   val = var->value;   // "cl.exe /volatile:ms": volatile reads have acquire semantics
#  else
#  error Unimplemented MSVC arch Atomic_Read32Acquire
#  endif
#  if defined _ISO_VOLATILE
   SMP_R_BARRIER_RW(); // Prevent #LoadLoad/#LoadStore reordering
#  endif

#else
#error No compiler defined for Atomic_Read32Acquire
#endif

   return val;
}
#define Atomic_ReadAcquire Atomic_Read32Acquire


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Read64Acquire --
 *
 *      Read the value of the specified object atomically (acquire ordering).
 *
 * Results:
 *      The value of the atomic variable.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint64
Atomic_Read64Acquire(Atomic_uint64 const *var)  // IN:
{
   uint64 val;
#if defined __i386__ && USE_XMM_ATOMICS_ON_I386
   __m128i xmmScratch;
#endif

   ASSERT((uintptr_t)var % 8 == 0);

#if defined __GNUC__
#  if defined __x86_64__
   __asm__ __volatile__(
      "movq %1, %0"
      : "=r" (val)
      : "m" (var->value)
      : "memory" // Must minimally prevent #LoadLoad/#LoadStore compiler reordering
   );
#  elif defined __i386__ && USE_XMM_ATOMICS_ON_I386
   /* Use SSE2 to perform the atomic read */
   __asm__ __volatile__(
      "movq %2, %1"   "\n\t" // mem to xmm
      "movq %1, %0"          // xmm to mem (likely stack)
      : "=m" (val), "=x" (xmmScratch)
      : "m" (var->value)
      : "memory" // Must minimally prevent #LoadLoad/#LoadStore compiler reordering
   );
#  elif defined __i386__
   /*
    * We can't use SSE2, so we use cmpxchg8b.
    *
    * Since cmpxchg8b will replace the contents of EDX:EAX with the
    * value in memory if there is no match, we need only execute the
    * instruction once in order to atomically read 64 bits from
    * memory.  The only constraint is that ECX:EBX must have the same
    * value as EDX:EAX so that if the comparison succeeds.  We
    * intentionally don't tell gcc that we are using ebx and ecx as we
    * don't modify them and do not care what value they store.
    */
   __asm__ __volatile__(
      "mov %%ebx, %%eax"   "\n\t"
      "mov %%ecx, %%edx"   "\n\t"
      "lock; cmpxchg8b %1"
      : "=&A" (val)
      : "m" (*var)
      : "cc", // no need for "ebx" or "ecx", see above
        "memory" // Must minimally prevent #LoadLoad/#LoadStore compiler reordering
   );
#  elif defined VM_ARM_64
   val = _VMATOM_X(R_SC, 64, &var->value);
#  else
   /* seq-cst until better code is available */
   val = Atomic_ReadIfEqualWrite64((Atomic_uint64 *)var, 0, 0);
#  endif

#elif defined _MSC_VER
#  if defined VM_ARM_64 && defined _ISO_VOLATILE
   val = (uint64)__iso_volatile_load64((volatile long long *)&var->value);
#  elif defined VM_ARM_64 || defined __x86_64__
   val = var->value;   // "cl.exe /volatile:ms": volatile reads have acquire semantics
#  elif defined __i386__ && USE_XMM_ATOMICS_ON_I386
   /* Use SSE2 to perform the atomic read and let MSVC unpack the Xmm register */
   xmmScratch = _mm_loadl_epi64((__m128i const*)&var->value);
   SMP_R_BARRIER_RW(); // Prevent #LoadLoad/#LoadStore reordering
   val = xmmScratch.m128i_u64[0];
#  elif defined __i386__
   /* We can't use SSE2, so we use cmpxchg8b */
   val = (uint64)_InterlockedCompareExchange64((__int64 volatile * )&var->value, 0, 0);
#  else
#  error Unimplemented MSVC arch Atomic_Read64Acquire
#  endif
#  if !defined __i386__ && defined _ISO_VOLATILE
   SMP_R_BARRIER_RW(); // Prevent #LoadLoad/#LoadStore reordering
#  endif

#else
#error No compiler defined for Atomic_Read64Acquire
#endif

   return val;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Write8Release --
 *
 *      Write the specified value to the specified object atomically (release
 *      ordering).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Write8Release(Atomic_uint8 *var,  // OUT:
                     uint8 val)          // IN:
{
#if defined __GNUC__
#  if defined __i386__ || defined __x86_64__
   __asm__ __volatile__(
      "movb %1, %0"
      : "=m" (var->value)
      : "qn" (val)
      : "memory" // Must minimally prevent #LoadStore and #StoreStore compiler reordering
   );
#  elif defined VM_ARM_64
   _VMATOM_X(W_SC, 8, &var->value, val);
#  else
   /* seq-cst until better code is available */
   (void)Atomic_ReadWrite8(var, val);
#  endif

#elif defined _MSC_VER
#  if defined _ISO_VOLATILE
   SMP_RW_BARRIER_W(); // Prevent #LoadStore and #StoreStore reordering
#  endif
#  if defined __x86_64__ || defined __i386__
   var->value = val;   // "cl.exe /volatile:ms": volatile writes have release semantics
#  elif defined VM_ARM_64
   __iso_volatile_store8((volatile char *)&var->value, val);
#  else
#  error Unimplemented MSVC arch Atomic_Write8Release
#  endif

#else
#error No compiler defined for Atomic_Write8Release
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Write16Release --
 *
 *      Write the specified value to the specified object atomically (release
 *      ordering).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Write16Release(Atomic_uint16 *var,  // OUT:
                      uint16 val)          // IN:
{
   ASSERT((uintptr_t)var % 2 == 0);

#if defined __GNUC__
#  if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "movw %1, %0"
      : "=m" (var->value)
      : "r" (val)
      : "memory" // Must minimally prevent #LoadStore and #StoreStore compiler reordering
   );
#  elif defined VM_ARM_64
   _VMATOM_X(W_SC, 16, &var->value, val);
#  else
   /* seq-cst until better code is available */
   (void)Atomic_ReadWrite16(var, val);
#  endif

#elif defined _MSC_VER
#  if defined _ISO_VOLATILE
   SMP_RW_BARRIER_W(); // Prevent #LoadStore and #StoreStore reordering
#  endif
#  if defined __x86_64__ || defined __i386__
   var->value = val;   // "cl.exe /volatile:ms": volatile writes have release semantics
#  elif defined VM_ARM_64
   __iso_volatile_store16((volatile short *)&var->value, val);
#  else
#  error Unimplemented MSVC arch Atomic_Write16Release
#  endif

#else
#error No compiler defined for Atomic_Write16Release
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Write32Release --
 *
 *      Write the specified value to the specified object atomically (release
 *      ordering).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Write32Release(Atomic_uint32 *var,  // OUT:
                      uint32 val)          // IN:
{
   ASSERT((uintptr_t)var % 4 == 0);

#if defined __GNUC__
#  if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "movl %1, %0"
      : "=m" (var->value)
      : "r" (val)
      : "memory" // Must minimally prevent #LoadStore and #StoreStore compiler reordering
   );
#  elif defined VM_ARM_64
   _VMATOM_X(W_SC, 32, &var->value, val);
#  else
   /* seq-cst until better code is available */
   (void)Atomic_ReadWrite32(var, val);
#  endif

#elif defined _MSC_VER
#  if defined _ISO_VOLATILE
   SMP_RW_BARRIER_W(); // Prevent #LoadStore and #StoreStore reordering
#  endif
#  if defined __x86_64__ || defined __i386__
   var->value = val;   // "cl.exe /volatile:ms": volatile writes have release semantics
#  elif defined VM_ARM_64
   __iso_volatile_store32((volatile int *)&var->value, val);
#  else
#  error Unimplemented MSVC arch Atomic_Write32Release
#  endif

#else
#error No compiler defined for Atomic_Write32Release
#endif
}
#define Atomic_WriteRelease Atomic_Write32Release


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Write64Release --
 *
 *      Write the specified value to the specified object atomically (release
 *      ordering).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Write64Release(Atomic_uint64 *var,  // OUT:
                      uint64 val)          // IN:
{
#if defined __i386__ && USE_XMM_ATOMICS_ON_I386
   __m128i xmmScratch;
#endif

   ASSERT((uintptr_t)var % 8 == 0);

#if defined __GNUC__
#  if defined __x86_64__
   __asm__ __volatile__(
      "movq %1, %0"
      : "=m" (var->value)
      : "r" (val)
      : "memory" // Must minimally prevent #LoadStore and #StoreStore compiler reordering
   );
#  elif defined __i386__ && USE_XMM_ATOMICS_ON_I386
   /* Use SSE2 to perform the atomic write */
   __asm__ __volatile__(
      "movq %2, %1"   "\n\t"   // Memory (likely stack) to xmm
      "movq %1, %0"            // xmm to target
      : "=m" (var->value), "=x" (xmmScratch)
      : "m" (val)
      : "memory" // Must minimally prevent #LoadStore and #StoreStore compiler reordering
   );
#  elif defined __i386__
   /* We can't use SSE2, so we use something that turns into cmpxchg8b */
   (void)Atomic_ReadWrite64(var, val);
#  elif defined VM_ARM_64
   _VMATOM_X(W_SC, 64, &var->value, val);
#  else
   /* seq-cst until better code is available */
   (void)Atomic_ReadWrite64(var, val);
#  endif

#elif defined _MSC_VER
#  if !defined __i386__ && defined _ISO_VOLATILE
   SMP_RW_BARRIER_W(); // Prevent #LoadStore and #StoreStore reordering
#  endif
#  if defined __x86_64__
   var->value = val;   // "cl.exe /volatile:ms": volatile writes have release semantics
#  elif defined __i386__ && USE_XMM_ATOMICS_ON_I386
   /* Use SSE2 to perform the atomic read and let MSVC unpack the Xmm register */
    xmmScratch.m128i_u64[0] = val;
   SMP_RW_BARRIER_W(); // Prevent #LoadStore and #StoreStore reordering
   _mm_storel_epi64((__m128i*)&var->value, xmmScratch);
#  elif defined __i386__
   /* We can't use SSE2, so we use something that turns into cmpxchg8b */
   (void)Atomic_ReadWrite64(var, val);
#  elif defined VM_ARM_64
   __iso_volatile_store64((volatile long long *)&var->value, val);
#  else
#  error Unimplemented MSVC arch Atomic_Write64Release
#  endif

#else
#error No compiler defined for Atomic_Write64Release
#endif
}

#define MAKE_ATOMIC_ACQREL_FUNCS(name, size, in, out, cast)                   \
   static INLINE out                                                          \
   Atomic_Read ## name ## Acquire(Atomic_ ## name const *var)                 \
   {                                                                          \
      return (out)(cast)Atomic_Read ## size ## Acquire(var);                  \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE void                                                         \
   Atomic_Write ## name ## Release(Atomic_ ## name *var,                      \
                                   in val)                                    \
   {                                                                          \
      Atomic_Write ## size ## Release(var, (uint ## size)(cast)val);          \
   }

/*
 * Since we use a macro to generate these definitions, it is hard to look for
 * them. So DO NOT REMOVE THIS COMMENT and keep it up-to-date.
 *
 * Atomic_Ptr
 * Atomic_ReadPtrAcquire --
 * Atomic_WritePtrRelease --
 *
 * Atomic_Int
 * Atomic_ReadIntAcquire --
 * Atomic_WriteIntRelease --
 *
 * Atomic_Bool
 * Atomic_ReadBoolAcquire --
 * Atomic_WriteBoolRelease --
 */


#if defined VM_64BIT
MAKE_ATOMIC_ACQREL_FUNCS(Ptr, 64, void const *, void *, uintptr_t)
#else
MAKE_ATOMIC_ACQREL_FUNCS(Ptr, 32, void const *, void *, uintptr_t)
#endif
MAKE_ATOMIC_ACQREL_FUNCS(Int, 32, int, int, int)
MAKE_ATOMIC_ACQREL_FUNCS(Bool, 8, Bool, Bool, Bool)


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_FenceAcquire --
 * Atomic_FenceRelease --
 * Atomic_FenceAcqRel --
 * Atomic_FenceSeqCst --
 *
 *      Explicit memory fence (barrier) with specified memory ordering.
 *      Equivalent to C11 atomic_thread_fence(<barrier>).
 *
 *
 *      Use VERY SPARINGLY; actual need for full barriers are extremely rare.
 *      Atomic operations (e.g. read-acquire or write-release) are more
 *      efficient.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May emit memory barrier (arch-dependent).
 *      Disallows compiler re-orderings.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_FenceAcquire(void)
{
   // C11 atomic_thread_fence(memory_order_acquire);
   SMP_R_BARRIER_RW();
}

static INLINE void
Atomic_FenceRelease(void)
{
   // C11 atomic_thread_fence(memory_order_release);
   SMP_RW_BARRIER_W();
}

static INLINE void
Atomic_FenceAcqRel(void)
{
   // C11 atomic_thread_fence(memory_order_acq_rel);
   /* R_RW + RW_W is generally cheaper than RW_RW (W_R is expensive) */
   SMP_R_BARRIER_RW();
   SMP_RW_BARRIER_W();
}

static INLINE void
Atomic_FenceSeqCst(void)
{
   // C11 atomic_thread_fence(memory_order_seq_cst);
   SMP_RW_BARRIER_RW();
}

#ifdef VM_ARM_64
#   include "vm_atomic_arm64_end.h"
#endif

#if defined __cplusplus
}  // extern "C"
#endif

#endif // ifndef _ATOMIC_ACQREL_H_
