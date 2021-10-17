/*********************************************************
 * Copyright (C) 2018 VMware, Inc. All rights reserved.
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
 * vm_atomic_relaxed.h --
 *
 *       Declares atomics with relaxed (i.e. "unordered" / "no-fence")
 *       ordering.
 *
 * NOTE: Usage of these atomics should be rare and limited to adjusting
 *       unrelated stat counters and possibly reading values in prep for
 *       computing then atomically writing out an updated values using
 *       stronger semantics.
 *
 *       Inclusion of this header serves as a flag that a file needs careful
 *       review and auditing due to the difficulty of writing correct lockless
 *       code. [In the future, bora/doc/atomics will contain documentation
 *       to explain the subtleties of non-sequential atomics.]
 *
 *       Relaxed differ from other atomics in that they may be reordered
 *       w.r.t. other atomics (even volatiles) and w.r.t. other non-atomic
 *       memory accesses. For synchronization with signal handlers, this is
 *       overkill: see vm_uninterruptible.h.
 */

#ifndef _ATOMIC_RELAXED_H_
#define _ATOMIC_RELAXED_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKDRIVERS
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_atomic_acqrel.h"

#if defined __cplusplus
extern "C" {
#endif

#if defined VM_ARM_64
#   include "vm_atomic_arm64_begin.h"
#endif
#include "vm_basic_asm.h"


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Read8Relaxed --
 *
 *      Read the value of the specified object atomically (relaxed ordering).
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
Atomic_Read8Relaxed(Atomic_uint8 const *var)  // IN:
{
   uint8 val;

#if defined __GNUC__
#  if defined __i386__ || defined __x86_64__
   __asm__ __volatile__(
      "movb %1, %0"
      : "=q" (val)
      : "m" (var->value)
   );
#  elif defined VM_ARM_64
   val = _VMATOM_X(R_NF, 8, &var->value);
#  else
   /* Acquire read until better code is available */
   val = Atomic_Read8Acquire(var);
#  endif

#elif defined _MSC_VER
#  if defined VM_ARM_64
   val = (uint8)__iso_volatile_load8((volatile char *)&var->value);
#  elif defined __i386__ || defined __x86_64__
   val = var->value;   // volatile reads are documented not to tear on MSVC
#  else
#  error Unimplemented MSVC arch Atomic_Read8Relaxed
#  endif

#else
#error No compiler defined for Atomic_Read8Relaxed
#endif

   return val;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Read16Relaxed --
 *
 *      Read the value of the specified object atomically (relaxed ordering).
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
Atomic_Read16Relaxed(Atomic_uint16 const *var)  // IN:
{
   uint16 val;

   ASSERT((uintptr_t)var % 2 == 0);

#if defined __GNUC__
#  if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "movw %1, %0"
      : "=r" (val)
      : "m" (var->value)
   );
#  elif defined VM_ARM_64
   val = _VMATOM_X(R_NF, 16, &var->value);
#  else
   /* Acquire read until better code is available */
   val = Atomic_Read16Acquire(var);
#  endif

#elif defined _MSC_VER
#  if defined VM_ARM_64
   val = (uint16)__iso_volatile_load16((volatile short *)&var->value);
#  elif defined __i386__ || defined __x86_64__
   val = var->value;   // volatile reads are documented not to tear on MSVC
#  else
#  error Unimplemented MSVC arch Atomic_Read16Relaxed
#  endif

#else
#error No compiler defined for Atomic_Read16Relaxed
#endif

   return val;
}

/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Read32Relaxed --
 *
 *      Read the value of the specified object atomically (relaxed ordering).
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
Atomic_Read32Relaxed(Atomic_uint32 const *var)  // IN:
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
   );
#  elif defined VM_ARM_64
   val = _VMATOM_X(R_NF, 32, &var->value);
#  else
   /* Acquire read until better code is available */
   val = Atomic_Read32Acquire(var);
#  endif

#elif defined _MSC_VER
#  if defined VM_ARM_64
   val = (uint32)__iso_volatile_load32((volatile int *)&var->value);
#  elif defined __i386__ || defined __x86_64__
   val = var->value;   // volatile reads are documented not to tear on MSVC
#  else
#  error Unimplemented MSVC arch Atomic_Read32Relaxed
#  endif

#else
#error No compiler defined for Atomic_Read32Relaxed
#endif

   return val;
}
#define Atomic_ReadRelaxed Atomic_Read32Relaxed


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Read64Relaxed --
 *
 *      Read the value of the specified object atomically (relaxed ordering).
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
Atomic_Read64Relaxed(Atomic_uint64 const *var)  // IN:
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
   );
#  elif defined __i386__ && USE_XMM_ATOMICS_ON_I386
   /* Use SSE2 to perform the atomic read */
   __asm__ __volatile__(
      "movq %2, %1"   "\n\t" // mem to xmm
      "movq %1, %0"          // xmm to mem (likely stack)
      : "=m" (val), "=x" (xmmScratch)
      : "m" (var->value)
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
      : "cc" // no need for "ebx" or "ecx", see above
   );
#  elif defined VM_ARM_64
   val = _VMATOM_X(R_NF, 64, &var->value);
#  else
   /* Acquire read until better code is available */
   val = Atomic_Read64Acquire(var);
#  endif

#elif defined _MSC_VER
#  if defined __x86_64__
   val = var->value;   // volatile reads are documented not to tear on MSVC
#  elif defined __i386__ && USE_XMM_ATOMICS_ON_I386
   /* Use SSE2 to perform the atomic read and let MSVC unpack the Xmm register */
   xmmScratch = _mm_loadl_epi64((__m128i const*)&var->value);
   val = xmmScratch.m128i_u64[0];
#  elif defined __i386__
   /* We can't use SSE2, so we use cmpxchg8b */
   val = (uint64)_InterlockedCompareExchange64((__int64 volatile * )&var->value, 0, 0);
#  elif defined VM_ARM_64
   val = (uint64)__iso_volatile_load64((volatile long long *)&var->value);
#  else
#  error Unimplemented MSVC arch Atomic_Read64Relaxed
#  endif

#else
#error No compiler defined for Atomic_Read64Relaxed
#endif

   return val;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Write8Relaxed --
 *
 *      Write the specified value to the specified object atomically (relaxed
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
Atomic_Write8Relaxed(Atomic_uint8 *var,  // OUT:
                     uint8 val)          // IN:
{
#if defined __GNUC__
#  if defined __i386__ || defined __x86_64__
   __asm__ __volatile__(
      "movb %1, %0"
      : "=m" (var->value)
      : "qn" (val)
   );
#  elif defined VM_ARM_64
   _VMATOM_X(W_NF, 8, &var->value, val);
#  else
   /* Release write until better code is available */
   Atomic_Write8Release(var, val);
#  endif

#elif defined _MSC_VER
#  if defined __x86_64__ || defined __i386__
   var->value = val;   // volatile reads are documented not to tear on MSVC
#  elif defined VM_ARM_64
   __iso_volatile_store8((volatile char *)&var->value, val);
#  else
#  error Unimplemented MSVC arch Atomic_Write8Relaxed
#  endif
#else

#error No compiler defined for Atomic_Write8Relaxed
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Write16Relaxed --
 *
 *      Write the specified value to the specified object atomically (relaxed
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
Atomic_Write16Relaxed(Atomic_uint16 *var,  // OUT:
                      uint16 val)          // IN:
{
   ASSERT((uintptr_t)var % 2 == 0);

#if defined __GNUC__
#  if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "movw %1, %0"
      : "=m" (var->value)
      : "r" (val)
   );
#  elif defined VM_ARM_64
   _VMATOM_X(W_NF, 16, &var->value, val);
#  else
   /* Release write until better code is available */
   Atomic_Write16Release(var, val);
#  endif

#elif defined _MSC_VER
#  if defined __x86_64__ || defined __i386__
   var->value = val;   // volatile reads are documented not to tear on MSVC
#  elif defined VM_ARM_64
   __iso_volatile_store16((volatile short *)&var->value, val);
#  else
#  error Unimplemented MSVC arch Atomic_Write16Relaxed
#  endif
#else

#error No compiler defined for Atomic_Write16Relaxed
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Write32Relaxed --
 *
 *      Write the specified value to the specified object atomically (relaxed
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
Atomic_Write32Relaxed(Atomic_uint32 *var, // OUT:
                      uint32 val)         // IN:
{
   ASSERT((uintptr_t)var % 4 == 0);

#if defined __GNUC__
#  if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "movl %1, %0"
      : "=m" (var->value)
      : "r" (val)
   );
#  elif defined VM_ARM_64
   _VMATOM_X(W_NF, 32, &var->value, val);
#  else
   /* Release write until better code is available */
   Atomic_Write32Release(var, val);
#  endif

#elif defined _MSC_VER
#  if defined __x86_64__ || defined __i386__
   var->value = val;   // volatile reads are documented not to tear on MSVC
#  elif defined VM_ARM_64
   __iso_volatile_store32((volatile int *)&var->value, val);
#  else
#  error Unimplemented MSVC arch Atomic_Write32Relaxed
#  endif

#else
#error No compiler defined for Atomic_Write32Relaxed
#endif
}
#define Atomic_WriteRelaxed Atomic_Write32Relaxed


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Write64Relaxed --
 *
 *      Write the specified value to the specified object atomically (relaxed
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
Atomic_Write64Relaxed(Atomic_uint64 *var, // OUT:
                      uint64 val)         // IN:
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
   );
#  elif defined __i386__ && USE_XMM_ATOMICS_ON_I386
   /* Use SSE2 to perform the atomic write */
   __asm__ __volatile__(
      "movq %2, %1"   "\n\t"   // Memory (likely stack) to xmm
      "movq %1, %0"            // xmm to target
      : "=m" (var->value), "=x" (xmmScratch)
      : "m" (val)
   );
#  elif defined __i386__
   /* We can't use SSE2, so we use something that turns into cmpxchg8b */
   Atomic_Write64Release(var, val);
#  elif defined VM_ARM_64
   _VMATOM_X(W_NF, 64, &var->value, val);
#  else
   /* Release write until better code is available */
   Atomic_Write64Release(var, val);
#  endif

#elif defined _MSC_VER
#  if defined __x86_64__
   var->value = val;   // volatile reads are documented not to tear on MSVC
#  elif defined __i386__ && USE_XMM_ATOMICS_ON_I386
   /* Use SSE2 to perform the atomic read and let MSVC unpack the Xmm register */
    xmmScratch.m128i_u64[0] = val;
   _mm_storel_epi64((__m128i*)&var->value, xmmScratch);
#  elif defined __i386__
   /* We can't use SSE2, so we use something that turns into cmpxchg8b */
   Atomic_Write64Release(var, val);
#  elif defined VM_ARM_64
   __iso_volatile_store64((volatile long long *)&var->value, val);
#  else
#  error Unimplemented MSVC arch Atomic_Write64Relaxed
#  endif

#else
#error No compiler defined for Atomic_Write64Relaxed
#endif
}

#define MAKE_ATOMIC_RELAXED_FUNCS(name, size, in, out, cast)                  \
   static INLINE out                                                          \
   Atomic_Read ## name ## Relaxed(Atomic_ ## name const *var)                 \
   {                                                                          \
      return (out)(cast)Atomic_Read ## size ## Relaxed(var);                  \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE void                                                         \
   Atomic_Write ## name ## Relaxed(Atomic_ ## name *var,                      \
                                   in val)                                    \
   {                                                                          \
      Atomic_Write ## size ## Relaxed(var, (uint ## size)(cast)val);          \
   }

/*
 * Since we use a macro to generate these definitions, it is hard to look for
 * them. So DO NOT REMOVE THIS COMMENT and keep it up-to-date.
 *
 * Atomic_Int
 * Atomic_ReadIntRelaxed --
 * Atomic_WriteIntRelaxed --
 *
 * Atomic_Bool
 * Atomic_ReadBoolRelaxed --
 * Atomic_WriteBoolRelaxed --
 */

MAKE_ATOMIC_RELAXED_FUNCS(Int, 32, int, int, int)
MAKE_ATOMIC_RELAXED_FUNCS(Bool, 8, Bool, Bool, Bool)

#ifdef VM_ARM_64
#   include "vm_atomic_arm64_end.h"
#endif

#if defined __cplusplus
}  // extern "C"
#endif

#endif // ifndef _ATOMIC_RELAXED_H_
