/*********************************************************
 * Copyright (C) 2002-2020 VMware, Inc. All rights reserved.
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
 * vcpuset.h --
 *
 *      ADT for a set of VCPUs.  Implemented as an array of bitmasks.
 *
 */

#ifndef _VCPUSET_H_
#define _VCPUSET_H_


#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_asm.h"
#include "vm_atomic.h"
#include "vcpuid.h"
#include "vcpuset_types.h"

#if defined VMX86_VMX || defined MONITOR_APP
#   include <stdio.h>   /* libc snprintf */
#   define VCS_SNPRINTF
#elif defined VMM || defined VMKERNEL
#   include "vm_libc.h" /* vmcore snprintf */
#   define VCS_SNPRINTF
#endif

#ifdef VMX86_VMX
#include "vmx.h"
#endif


/*
 * A buffer for logging a VCPUSet must fit a maximally-populated set.  To
 * balance brevity and readability, sets are formatted for printing like long
 * hexadecimal numbers, with a '.' at every 64-VCPU subset boundary.  The
 * highest-numbered VCPU in the set is printed first, followed by all slots
 * for lower-numbered VCPUs, populated or not.  Leading zeroes are not printed.
 *
 * Examples, assuming a VCS_SUBSET_COUNT of 2:
 * An empty set:                                            "0x0\0"
 * A full set:              "0xffffffffffffffff.ffffffffffffffff\0"
 * A set with only VCPU 50:                     "0x4000000000000\0"
 * A set with only VCPU 80:            "0x10000.0000000000000000\0"
 */
#define VCS_BUF_SIZE (2 +                                         /* "0x"  */ \
                      (VCS_SUBSET_COUNT * VCS_SUBSET_WIDTH / 4) + /* (hex) */ \
                      (VCS_SUBSET_COUNT - 1) +                    /* '.'   */ \
                      1)                                          /* NULL  */

extern VCPUSet  vcpuSetFull;

#define FOR_EACH_VCPU_IN_SET_WITH_MAX(_vcpuSet, _v, _numVcpus)                 \
   do {                                                                        \
      Vcpuid _v;                                                               \
      const VCPUSet *__vcs = (_vcpuSet);                                       \
      unsigned _subsetIdx = 0;                                                 \
      uint64 _subset = VCPUSet_Subset(__vcs, _subsetIdx);                      \
      unsigned _maxSubsets = VCS_VCPUID_SUBSET_IDX((_numVcpus) - 1) + 1;       \
      ASSERT(_maxSubsets <= VCS_SUBSET_COUNT);                                 \
      while ((_v = VCPUSet_FindFirstInSubset(__vcs, &_subset, &_subsetIdx,     \
                                             _maxSubsets)) != VCPUID_INVALID) {\

#define ROF_EACH_VCPU_IN_SET_WITH_MAX()                                 \
      }                                                                 \
   } while (0)


#define FOR_EACH_VCPU_IN_SET(_vcpuSet, _v)                              \
   FOR_EACH_VCPU_IN_SET_WITH_MAX(_vcpuSet, _v, MAX_VCPUS)

#define ROF_EACH_VCPU_IN_SET()                                          \
      }                                                                 \
   } while (0)

#define FOR_EACH_VCPU_IN_POPULATED_VCPUS(_vcpuSet, _v)                  \
   FOR_EACH_VCPU_IN_SET_WITH_MAX(_vcpuSet, _v, NumVCPUs())

#define ROF_EACH_VCPU_IN_POPULATED_VCPUS()                              \
      }                                                                 \
   } while (0)

#define FOR_EACH_SUBSET_IN_SET(_setIndex)                                     \
   do {                                                                       \
      unsigned _setIndex;                                                     \
      for (_setIndex = 0; _setIndex < VCS_SUBSET_COUNT; _setIndex++) {

#define ROF_EACH_SUBSET_IN_SET()                                              \
      }                                                                       \
   } while (0)


#define FOR_EACH_SUBSET_IN_SET_COUNTDOWN(_setIndex)                           \
   do {                                                                       \
      unsigned _setIndex = VCS_SUBSET_COUNT;                                  \
      while (_setIndex-- > 0) {

#define ROF_EACH_SUBSET_IN_SET_COUNTDOWN()                                    \
      }                                                                       \
   } while (0)


#define FOR_EACH_POPULATED_SUBSET_IN_SET(_setIndex, _numVcpus)                \
   do {                                                                       \
      unsigned _setIndex;                                                     \
      unsigned _maxSubsets = VCS_VCPUID_SUBSET_IDX(_numVcpus - 1);            \
      for (_setIndex = 0; _setIndex <= _maxSubsets; _setIndex++) {

#define ROF_EACH_POPULATED_SUBSET_IN_SET()                                    \
      }                                                                       \
   } while (0)


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_Empty --
 *
 *      Clear all bits in a VCPUSet.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_Empty(VCPUSet *vcs)
{
   FOR_EACH_SUBSET_IN_SET(idx) {
      vcs->subset[idx] = 0;
   } ROF_EACH_SUBSET_IN_SET();
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_IsEmpty --
 *
 *      Return TRUE iff a VCPUSet has no bits set.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
VCPUSet_IsEmpty(const VCPUSet *vcs)
{
   FOR_EACH_SUBSET_IN_SET(idx) {
      if (vcs->subset[idx] != 0) {
         return FALSE;
      }
   } ROF_EACH_SUBSET_IN_SET();
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_Full --
 *
 *      Returns a pointer to a VCPUSet containing all valid VCPUs.
 *
 *----------------------------------------------------------------------
 */
static INLINE const VCPUSet *
VCPUSet_Full(void)
{
   /*
    * Read too early, we may get the wrong notion of how many
    * vcpus the VM has. Cf. pr286243 and pr289186.
    */
#if defined (VMX86_VMX)
   ASSERT(NumVCPUs() != 0 && !VCPUSet_IsEmpty(&vcpuSetFull));
#endif
   return &vcpuSetFull;
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_Copy --
 *
 *      Copy one VCPUSet's contents to another.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_Copy(VCPUSet *dest, const VCPUSet *src)
{
   FOR_EACH_SUBSET_IN_SET(idx) {
      dest->subset[idx] = src->subset[idx];
   } ROF_EACH_SUBSET_IN_SET();
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_Equals --
 *
 *      Compare two VCPUSets, return TRUE iff their contents match.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
VCPUSet_Equals(const VCPUSet *vcs1, const VCPUSet *vcs2)
{
   FOR_EACH_SUBSET_IN_SET(idx) {
      if (vcs1->subset[idx] != vcs2->subset[idx]) {
         return FALSE;
      }
   } ROF_EACH_SUBSET_IN_SET();
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_IsMember --
 *
 *      Return TRUE iff the given Vcpuid is present in a VCPUSet.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
VCPUSet_IsMember(const VCPUSet *vcs, Vcpuid v)
{
   ASSERT(v < MAX_VCPUS);
   return (vcs->subset[VCS_VCPUID_SUBSET_IDX(v)] &
           VCS_VCPUID_SUBSET_BIT(v)) != 0;
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_AtomicIsMember --
 *
 *      Return TRUE iff the given Vcpuid is present in a VCPUSet.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
VCPUSet_AtomicIsMember(VCPUSet *vcs, Vcpuid v)
{
   volatile uint64 *subset = &vcs->subset[VCS_VCPUID_SUBSET_IDX(v)];
   ASSERT(v < MAX_VCPUS);
   return (Atomic_Read64(Atomic_VolatileToAtomic64(subset)) &
           VCS_VCPUID_SUBSET_BIT(v)) != 0;
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_FindFirst --
 * VCPUSet_FindLast --
 *
 *      Find the first (lowest-numbered) or last (highest-numbered)
 *      Vcpuid in a VCPUSet.
 *
 * Results:
 *      Vcpuid if at least one is present in a set.
 *      VCPUID_INVALID if the set is empty.
 *
 *----------------------------------------------------------------------
 */

static INLINE Vcpuid
VCPUSet_FindFirst(const VCPUSet *vcs)
{
   FOR_EACH_SUBSET_IN_SET(idx) {
      uint64 subset = vcs->subset[idx];
      if (subset != 0) {
         return lssb64(subset) - 1 + (idx << VCS_SUBSET_SHIFT);
      }
   } ROF_EACH_SUBSET_IN_SET();
   return VCPUID_INVALID;
}

static INLINE Vcpuid
VCPUSet_FindLast(const VCPUSet *vcs)
{
   FOR_EACH_SUBSET_IN_SET_COUNTDOWN(idx) {
      uint64 subset = vcs->subset[idx];
      if (subset != 0) {
         return mssb64(subset) - 1 + (idx << VCS_SUBSET_SHIFT);
      }
   } ROF_EACH_SUBSET_IN_SET_COUNTDOWN();
   return VCPUID_INVALID;
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_FindFirstInSubset --
 *
 *      Find the first (lowest-numbered) Vcpuid in the given subset of a
 *      VCPUSet object.  It is required that *subset is initialized to
 *      a subset of *vcs and *subsetIdx must hold the index of the subset
 *      stored in *subset.
 *
 * Results:
 *      Vcpuid if at least one is present in a set.
 *      VCPUID_INVALID if the set is empty.
 *
 * Side effects:
 *      This function is intended to be used for iterating over all bits
 *      of a VCPUSet, it will modify *subset to clear the bit associated
 *      with the returned Vcpuid (if any).  The *subsetIdx argument may
 *      also be updated.
 *
 *----------------------------------------------------------------------
 */

static INLINE Vcpuid
VCPUSet_FindFirstInSubset(const VCPUSet *vcs, uint64 *subset,
                          unsigned *subsetIdx, unsigned maxSubsets)
{
   ASSERT(*subsetIdx < maxSubsets && maxSubsets <= VCS_SUBSET_COUNT);
   do {
      if (*subset != 0) {
         unsigned bit = (unsigned)lssb64_0(*subset);
         *subset &= ~(CONST64U(1) << bit);
         return bit + (*subsetIdx << VCS_SUBSET_SHIFT);
      }
      ++*subsetIdx;
      if (*subsetIdx < maxSubsets) {
         *subset = vcs->subset[*subsetIdx];
      }
   } while (*subsetIdx < maxSubsets);
   return VCPUID_INVALID;
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_Remove --
 * VCPUSet_AtomicRemove --
 *
 *      Remove or atomically remove a single Vcpuid from a VCPUSet.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_Remove(VCPUSet *vcs, Vcpuid v)
{
   ASSERT(v < MAX_VCPUS);
   vcs->subset[VCS_VCPUID_SUBSET_IDX(v)] &= ~VCS_VCPUID_SUBSET_BIT(v);
}


static INLINE void
VCPUSet_AtomicRemove(VCPUSet *vcs, Vcpuid v)
{
   volatile uint64 *subset = &vcs->subset[VCS_VCPUID_SUBSET_IDX(v)];
   ASSERT(v < MAX_VCPUS);
   Atomic_And64(Atomic_VolatileToAtomic64(subset), ~VCS_VCPUID_SUBSET_BIT(v));
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_IncludeSet --
 * VCPUSet_RemoveSet --
 *
 *      Add/remove all vcpus present in the set 'src' to/from the set 'dest'.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_IncludeSet(VCPUSet *dest, const VCPUSet *src)
{
   FOR_EACH_SUBSET_IN_SET(idx) {
      dest->subset[idx] |= src->subset[idx];
   } ROF_EACH_SUBSET_IN_SET();
}


static INLINE void
VCPUSet_RemoveSet(VCPUSet *dest, const VCPUSet *src)
{
   FOR_EACH_SUBSET_IN_SET(idx) {
      dest->subset[idx] &= ~src->subset[idx];
   } ROF_EACH_SUBSET_IN_SET();
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_Include --
 * VCPUSet_AtomicInclude --
 *
 *      Add or atomically add a single Vcpuid to a VCPUSet.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_Include(VCPUSet *vcs, Vcpuid v)
{
   ASSERT(v < MAX_VCPUS);
   vcs->subset[VCS_VCPUID_SUBSET_IDX(v)] |= VCS_VCPUID_SUBSET_BIT(v);
}


static INLINE void
VCPUSet_AtomicInclude(VCPUSet *vcs, Vcpuid v)
{
   volatile uint64 *subset = &vcs->subset[VCS_VCPUID_SUBSET_IDX(v)];
   ASSERT(v < MAX_VCPUS);
   Atomic_Or64(Atomic_VolatileToAtomic64(subset), VCS_VCPUID_SUBSET_BIT(v));
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_AtomicTestInclude  --
 *
 *      Atomically add a single Vcpuid to a VCPUSet, and
 *      return TRUE iff the given Vcpuid was present in the VCPUSet.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
VCPUSet_AtomicTestInclude(VCPUSet *vcs, Vcpuid v)
{
   volatile uint64 *subset = &vcs->subset[VCS_VCPUID_SUBSET_IDX(v)];
   ASSERT(v < MAX_VCPUS);
   return Atomic_TestSetBit64(Atomic_VolatileToAtomic64(subset),
                              v & VCS_SUBSET_MASK);
}


#if defined(VMM) && !defined(MONITOR_APP)
/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_PackCareful --
 *
 *      Pack a VCPUSet into the bytes at "ptr".
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_PackCareful(unsigned numVCPUs, const VCPUSet *vcs, void *ptr)
{
   memcpy(ptr, vcs->subset, (numVCPUs + 7) / 8);
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_UnpackCareful --
 *
 *      Unpack a VCPUSet from the bytes at "ptr".
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_UnpackCareful(unsigned numVCPUs, VCPUSet *vcs, const void *ptr)
{
   memcpy(vcs->subset, ptr, (numVCPUs + 7) / 8);
}
#endif /* VMM && !MONITOR_APP */


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_PopulateRange --
 *
 *  Populates the given set with 'numVCPUs' VCPUs starting at 'firstVCPU'.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_PopulateRange(VCPUSet *vcs, unsigned firstVCPU,
                          unsigned numVCPUs)
{
   unsigned sub;
   unsigned lastVCPU      = firstVCPU + numVCPUs - 1;
   unsigned firstSubset   = firstVCPU / VCS_SUBSET_WIDTH;
   unsigned lastSubset    = lastVCPU / VCS_SUBSET_WIDTH;
   unsigned lowMaskShift  = firstVCPU % VCS_SUBSET_WIDTH;
   unsigned highMaskShift = VCS_SUBSET_WIDTH - 1 - lastVCPU % VCS_SUBSET_WIDTH;

   ASSERT(firstSubset <= lastSubset && lastSubset < VCS_SUBSET_COUNT);

   VCPUSet_Empty(vcs);
   for (sub = firstSubset; sub <= lastSubset; sub++) {
      vcs->subset[sub] = CONST64U(-1);
   }
   vcs->subset[firstSubset] &= (CONST64U(-1) << lowMaskShift);
   vcs->subset[lastSubset] &= (CONST64U(-1) >> highMaskShift);
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_Populate --
 *
 *  Populates the given set with the VCPUs in [0, numVCPUs).
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_Populate(VCPUSet *vcs, unsigned numVCPUs)
{
   VCPUSet_PopulateRange(vcs, 0, numVCPUs);
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_Subset --
 *
 *      Return the specified subset of a VCPUSet.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint64
VCPUSet_Subset(const VCPUSet *vcs,
                   unsigned subset)
{
   ASSERT(subset < VCS_SUBSET_COUNT);
   return vcs->subset[subset];
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_SubsetPtr --
 *
 *      Return a pointer to the specified subset of a VCPUSet.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint64 *
VCPUSet_SubsetPtr(VCPUSet *vcs, unsigned subset)
{
   ASSERT(subset < VCS_SUBSET_COUNT);
   return &vcs->subset[subset];
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_IsSupersetOrEqual --
 *
 *      Return TRUE iff vcs1 contains a superset of the VCPUs in vcs2
 *      or vcs1 and vcs2 contain exactly the same VCPUs.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
VCPUSet_IsSupersetOrEqual(const VCPUSet *vcs1, const VCPUSet *vcs2)
{
   FOR_EACH_SUBSET_IN_SET(idx) {
      if (vcs2->subset[idx] & ~vcs1->subset[idx]) {
         return FALSE;
      }
   } ROF_EACH_SUBSET_IN_SET();
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_IsSubsetOrEqual --
 *
 *      Return TRUE iff vcs1 contains a subset of the VCPUs in vcs2
 *      or vcs1 and vcs2 contain exactly the same VCPUs.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
VCPUSet_IsSubsetOrEqual(const VCPUSet *vcs1, const VCPUSet *vcs2)
{
   return VCPUSet_IsSupersetOrEqual(vcs2, vcs1);
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_MakeSingleton --
 *
 *      Add a single Vcpuid to a VCPUSet and remove all others.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_MakeSingleton(VCPUSet *vcs, Vcpuid v)
{
   VCPUSet_Empty(vcs);
   VCPUSet_Include(vcs, v);
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_FindSingleton --
 *
 *      Return the VCPU in the set iff vcs contains exactly one VCPU.
 *      Return VCPUID_INVALID otherwise.
 *
 *----------------------------------------------------------------------
 */

static INLINE Vcpuid
VCPUSet_FindSingleton(const VCPUSet *vcs)
{
   uint64 foundSub = 0;
   uint32 foundIdx = 0;
   FOR_EACH_SUBSET_IN_SET(idx) {
      uint64 sub = vcs->subset[idx];
      if (sub != 0) {
         if (foundSub != 0 || (sub & (sub - 1)) != 0) {
            return VCPUID_INVALID;
         }
         foundSub = sub;
         foundIdx = idx;
      }
   } ROF_EACH_SUBSET_IN_SET();
   if (foundSub != 0) {
      return lssb64(foundSub) - 1 + (foundIdx << VCS_SUBSET_SHIFT);
   } else {
      return VCPUID_INVALID;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_IsFull --
 *
 *  Returns true iff vcs contains the set of all vcpus.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
VCPUSet_IsFull(const VCPUSet *vcs)
{
   return VCPUSet_Equals(vcs, VCPUSet_Full());
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_AtomicReadWriteSubset --
 *
 *      For the nth aligned 64-VCPU subset of a VCPU set, atomically
 *      read then write.  Return the contents read.  Set 0 is VCPUs
 *      0-63 and set 1 is VCPUs 64-127.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint64
VCPUSet_AtomicReadWriteSubset(VCPUSet *vcs, uint64 vcpus,
                                  unsigned n)
{
   ASSERT(n < VCS_SUBSET_COUNT);
   return Atomic_ReadWrite64(Atomic_VolatileToAtomic64(&vcs->subset[n]),
                             vcpus);
}

/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_Size --
 *
 *    Return the number of VCPUs in this set.
 *
 *----------------------------------------------------------------------
 */
static INLINE int
VCPUSet_Size(const VCPUSet *vcs)
{
   int n = 0;
   FOR_EACH_SUBSET_IN_SET(idx) {
      uint64 bits = vcs->subset[idx];
      while (bits != 0) {
         bits = bits & (bits - 1);
         n++;
      }
   } ROF_EACH_SUBSET_IN_SET();
   return n;
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_UnionSubset --
 *
 *      Given an 64-bit value and a subset number, add the VCPUs
 *      represented to the set.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_UnionSubset(VCPUSet *vcs, uint64 vcpus, unsigned n)
{
   ASSERT(n < VCS_SUBSET_COUNT);
   vcs->subset[n] |= vcpus;
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_SubtractSubset --
 *
 *      Given an 64-bit value and a subset number, remove the VCPUs
 *      represented in the subset from the set.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_SubtractSubset(VCPUSet *vcs, uint64 vcpus, unsigned n)
{
   ASSERT(n < VCS_SUBSET_COUNT);
   vcs->subset[n] &= ~vcpus;
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_AtomicUnionSubset --
 *
 *      Given a 64-bit value and a subset number, atomically add
 *      the VCPUs represented to the set.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_AtomicUnionSubset(VCPUSet *vcs, uint64 vcpus, unsigned n)
{
   uint64 *subsetPtr = &vcs->subset[n];
   ASSERT(n < VCS_SUBSET_COUNT);
   Atomic_Or64(Atomic_VolatileToAtomic64(subsetPtr), vcpus);
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_Invert --
 *
 *      Makes all non-present valid VCPUs in a set present and all
 *      VCPUs present non-present.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_Invert(VCPUSet *vcs)
{
   VCPUSet temp;
   VCPUSet_Copy(&temp, VCPUSet_Full());
   VCPUSet_RemoveSet(&temp, vcs);
   VCPUSet_Copy(vcs, &temp);
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_Intersection
 *
 *      Given two VCPUSets, populate the destination set with only the
 *      VCPUs common to both.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VCPUSet_Intersection(VCPUSet *dest, const VCPUSet *src)
{
   FOR_EACH_SUBSET_IN_SET(idx) {
      dest->subset[idx] &= src->subset[idx];
   } ROF_EACH_SUBSET_IN_SET();
}


/*
 *----------------------------------------------------------------------
 *
 * VCPUSet_LogFormat --
 *
 *      Given a buffer of at least VCS_BUF_SIZE to fill, write into it a
 *      string suitable for use in Log() or LOG().
 *      Returns the buffer which was passed as an argument, after
 *      writing the string.
 *
 *----------------------------------------------------------------------
 */

#ifdef VCS_SNPRINTF
static INLINE char *
VCPUSet_LogFormat(char *buf, size_t size, const VCPUSet *vcs)
{
   unsigned offset  = 0;
   Vcpuid   highest = VCPUSet_FindLast(vcs);
   unsigned idx     = (highest == VCPUID_INVALID) ? 0 : highest / 8;

   ASSERT(size >= VCS_BUF_SIZE);
#define VCS_LOGF(...)                                                   \
   {                                                                    \
      int ret = snprintf(buf + offset, size - offset, __VA_ARGS__);     \
      ASSERT(0 <= ret && size >= offset && ret < (int)(size - offset)); \
      offset += (unsigned)ret;                                          \
   }
   /* Print the leading value with no zero-extension. */
   VCS_LOGF("%#x", ((unsigned char *)vcs)[idx]);

   while (idx-- > 0) {
      if ((idx + 1) % (VCS_SUBSET_WIDTH / 8) == 0) {
         VCS_LOGF(".");
      }
      VCS_LOGF("%02x", ((unsigned char *)vcs)[idx]);
   }
   return buf;
}
#undef VCS_LOGF
#endif


#endif /* _VCPUSET_H_ */
