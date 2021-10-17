/*********************************************************
 * Copyright (C) 2008-2021 VMware, Inc. All rights reserved.
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

#ifndef _INTELVT_H_
#define _INTELVT_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

/*
 * intelVT.h -
 *
 *      Module to handle Intel VT configuration.
 *
 */

#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "msrCache.h"
#include "x86vt.h"
#include "virtual_vt.h"

#if defined(__cplusplus)
extern "C" {
#endif


#define INTELVT_MSRS                   \
   MSRNUM(MSR_FEATCTL)                 \
   MSRNUM(MSR_VMX_BASIC)               \
   MSRNUM(MSR_VMX_PINBASED_CTLS)       \
   MSRNUM(MSR_VMX_PROCBASED_CTLS)      \
   MSRNUM(MSR_VMX_EXIT_CTLS)           \
   MSRNUM(MSR_VMX_ENTRY_CTLS)          \
   MSRNUM(MSR_VMX_MISC)                \
   MSRNUM(MSR_VMX_CR0_FIXED0)          \
   MSRNUM(MSR_VMX_CR0_FIXED1)          \
   MSRNUM(MSR_VMX_CR4_FIXED0)          \
   MSRNUM(MSR_VMX_CR4_FIXED1)          \
   MSRNUM(MSR_VMX_VMCS_ENUM)           \
   MSRNUM(MSR_VMX_2ND_CTLS)            \
   MSRNUM(MSR_VMX_EPT_VPID)            \
   MSRNUM(MSR_VMX_TRUE_PINBASED_CTLS)  \
   MSRNUM(MSR_VMX_TRUE_PROCBASED_CTLS) \
   MSRNUM(MSR_VMX_TRUE_EXIT_CTLS)      \
   MSRNUM(MSR_VMX_TRUE_ENTRY_CTLS)     \
   MSRNUM(MSR_VMX_VMFUNC)              \
   MSRNUM(MSR_VMX_3RD_CTLS)


#define EXTRACT_FIELD(msr, basename) \
   (((msr) >> basename ## _SHIFT) & basename ## _MASK)

#define INSERT_FIELD(msr, basename, val) \
   (((msr) & ~((uint64)(basename ## _MASK) << basename ## _SHIFT)) |    \
    (uint64)(val) << basename ## _SHIFT)

#define INVALID_VMX_BASIC INSERT_FIELD(~0ULL, MSR_VMX_BASIC_VMCS_SIZE, 0)

typedef uint64 (*IntelVTMSRGet_Fn)(const MSRCache*, uint32, unsigned);

const uint32 *IntelVT_MSRs(void);
unsigned IntelVT_MSRCount(void);
Bool IntelVT_FindCommonMSRs(MSRCache *common, const MSRCache *data);
Bool IntelVT_IsValid(const MSRCache *common, uint32 pcpu);


/*
 *----------------------------------------------------------------------
 *
 * IntelVT_FindCommonCtls --
 *
 *      Computes and returns common VMX_*_CTLS feature MSR across all
 *      logical processors on the host.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint64
IntelVT_FindCommonCtls(const MSRCache *data,    // IN
                       IntelVTMSRGet_Fn getMSR, // IN
                       unsigned numCPUs,        // IN
                       uint32 msrNum)           // IN
{
   unsigned cpu;
   uint64 commonVal = getMSR(data, msrNum, 0);

   for (cpu = 1; cpu < numCPUs; cpu++) {
      uint64 thisCpu = getMSR(data, msrNum, cpu);
      uint32 zeros = LODWORD(commonVal) | LODWORD(thisCpu);
      uint32 ones = HIDWORD(commonVal) & HIDWORD(thisCpu);

      commonVal = QWORD(ones, zeros);
   }
   return commonVal;
}


/*
 *----------------------------------------------------------------------
 *
 * IntelVT_FindCommonBasic --
 *
 *      Computes and returns a common VMX_BASIC feature MSR across all
 *      logical processors on the host.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint64
IntelVT_FindCommonBasic(const MSRCache *data,    // IN
                        IntelVTMSRGet_Fn getMSR, // IN
                        unsigned numCPUs)        // IN
{
   unsigned cpu;
   const uint64 orFields = MSR_VMX_BASIC_32BITPA;
   const uint64 andFields = MSR_VMX_BASIC_TRUE_CTLS | MSR_VMX_BASIC_DUALVMM |
                            MSR_VMX_BASIC_ADVANCED_IOINFO;
   uint64 commonVal = getMSR(data, MSR_VMX_BASIC, 0);
   for (cpu = 1; cpu < numCPUs; cpu++) {
      uint64 thisCpu = getMSR(data, MSR_VMX_BASIC, cpu);
      uint64 diff = commonVal ^ thisCpu;

      if (EXTRACT_FIELD(diff, MSR_VMX_BASIC_VMCS_ID) ||
          EXTRACT_FIELD(diff, MSR_VMX_BASIC_VMCS_SIZE) ||
          EXTRACT_FIELD(diff, MSR_VMX_BASIC_MEMTYPE)) {
         commonVal = INVALID_VMX_BASIC;
         break;
      }
      commonVal |= thisCpu & orFields;
      commonVal &= ~andFields | (thisCpu & andFields);
   }

   return commonVal;
}


/*
 *----------------------------------------------------------------------
 *
 * IntelVT_FindCommonMisc --
 *
 *      Computes and returns a common VMX_MISC feature MSR across all
 *      logical processors on the host.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint64
IntelVT_FindCommonMisc(const MSRCache *data,    // IN
                       IntelVTMSRGet_Fn getMSR, // IN
                       unsigned numCPUs)        // IN
{
   unsigned cpu;
   const uint64 fieldMask =
      (MSR_VMX_MISC_TMR_RATIO_MASK << MSR_VMX_MISC_TMR_RATIO_SHIFT) |
      (MSR_VMX_MISC_CR3_TARGETS_MASK << MSR_VMX_MISC_CR3_TARGETS_SHIFT) |
      (MSR_VMX_MISC_MAX_MSRS_MASK << MSR_VMX_MISC_MAX_MSRS_SHIFT) |
      ((uint64)MSR_VMX_MISC_MSEG_ID_MASK << MSR_VMX_MISC_MSEG_ID_SHIFT);
   uint64 commonVal = getMSR(data, MSR_VMX_MISC, 0);
   unsigned cr3Targets = EXTRACT_FIELD(commonVal, MSR_VMX_MISC_CR3_TARGETS);
   unsigned maxMSRs = EXTRACT_FIELD(commonVal, MSR_VMX_MISC_MAX_MSRS);

   for (cpu = 1; cpu < numCPUs; cpu++) {
      uint64 thisCpu = getMSR(data, MSR_VMX_MISC, cpu);
      uint64 diff = commonVal ^ thisCpu;

      if (EXTRACT_FIELD(diff, MSR_VMX_MISC_MSEG_ID)) {
         commonVal = 0;
         break;
      }

      commonVal &= fieldMask | (thisCpu & ~fieldMask);
      cr3Targets = MIN(cr3Targets, EXTRACT_FIELD(thisCpu,
                       MSR_VMX_MISC_CR3_TARGETS));
      maxMSRs = MIN(maxMSRs, EXTRACT_FIELD(thisCpu, MSR_VMX_MISC_MAX_MSRS));
   }
   commonVal = INSERT_FIELD(commonVal, MSR_VMX_MISC_CR3_TARGETS, cr3Targets);
   commonVal = INSERT_FIELD(commonVal, MSR_VMX_MISC_MAX_MSRS, maxMSRs);

   return commonVal;
}


/*
 *----------------------------------------------------------------------
 *
 * IntelVTFindCommon --
 *
 *      Computes and returns a common value for a given MSR by a bitwise OR or
 *      AND operation, across all logical processors.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint64
IntelVTFindCommon(uint32 msrNum,           // IN
                  const MSRCache *data,    // IN
                  IntelVTMSRGet_Fn getMSR, // IN
                  unsigned numCPUs,        // IN
                  Bool isOpAND)            // IN
{
   uint64 commonVal = getMSR(data, msrNum, 0);
   unsigned cpu;

   for (cpu = 1; cpu < numCPUs; cpu++) {
      if (isOpAND) {
         commonVal &= getMSR(data, msrNum, cpu);
      } else {
         commonVal |= getMSR(data, msrNum, cpu);
      }
   }
   return commonVal;
}


/*
 *----------------------------------------------------------------------
 *
 * IntelVT_FindCommonEPT --
 *
 *      Computes and returns a common VMX_EPT_VPID feature MSR across
 *      all logical processors on the host.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint64
IntelVT_FindCommonEPT(const MSRCache *data,    // IN
                      IntelVTMSRGet_Fn getMSR, // IN
                      unsigned numCPUs)        // IN
{
   return IntelVTFindCommon(MSR_VMX_EPT_VPID, data, getMSR, numCPUs, TRUE);
}


/*
 *----------------------------------------------------------------------
 *
 * IntelVT_FindCommonFixed0 --
 *
 *      Computes and returns a common VMX_*_FIXED0 feature MSR across
 *      all logical processors on the host.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint64
IntelVT_FindCommonFixed0(const MSRCache *data,    // IN
                         IntelVTMSRGet_Fn getMSR, // IN
                         unsigned numCPUs,        // IN
                         uint32 msrNum)           // IN
{
   return IntelVTFindCommon(msrNum, data, getMSR, numCPUs, FALSE);
}


/*
 *----------------------------------------------------------------------
 *
 * IntelVT_FindCommonFixed1 --
 *
 *      Computes and returns a common VMX_*_FIXED1 feature MSR across
 *      all logical processors on the host.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint64
IntelVT_FindCommonFixed1(const MSRCache *data,     // IN
                         IntelVTMSRGet_Fn getMSR,  // IN
                         unsigned numCPUs,         // IN
                         uint32 msrNum)            // IN
{
   return IntelVTFindCommon(msrNum, data, getMSR, numCPUs, TRUE);
}


/*
 *----------------------------------------------------------------------
 *
 * IntelVT_FindCommonFeatureCtl --
 *
 *      Computes and returns a common MSR_FEATCTL MSR across all logical
 *      processors on the host.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint64
IntelVT_FindCommonFeatureCtl(const MSRCache *data,           // IN
                             IntelVTMSRGet_Fn getMSR,        // IN
                             unsigned numCPUs)               // IN
{
   return IntelVTFindCommon(MSR_FEATCTL, data, getMSR, numCPUs, TRUE);
}


/*
 *----------------------------------------------------------------------
 *
 * IntelVT_FindCommonVMFunc --
 *
 *      Computes and returns a common MSR_VMX_VMFUNC feature MSR across
 *      all logical processors on the host.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint64
IntelVT_FindCommonVMFunc(const MSRCache *data,    // IN
                         IntelVTMSRGet_Fn getMSR, // IN
                         unsigned numCPUs)        // IN
{
   return IntelVTFindCommon(MSR_VMX_VMFUNC, data, getMSR, numCPUs, TRUE);
}


/*
 *----------------------------------------------------------------------
 *
 * IntelVT_FindCommonEnum --
 *
 *      Computes and returns a common MSR_VMX_VMCS_ENUM feature MSR across
 *      all logical processors on the host.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint64
IntelVT_FindCommonEnum(const MSRCache *data,    // IN
                       IntelVTMSRGet_Fn getMSR, // IN
                       unsigned numCPUs)        // IN
{
   uint64 commonVal = getMSR(data, MSR_VMX_VMCS_ENUM, 0);
   unsigned cpu;
   unsigned maxIndex = EXTRACT_FIELD(commonVal, MSR_VMX_VMCS_ENUM_MAX_INDEX);

   for (cpu = 1; cpu < numCPUs; cpu++) {
      maxIndex = MIN(maxIndex,
                     EXTRACT_FIELD(getMSR(data, MSR_VMX_VMCS_ENUM, cpu),
                                   MSR_VMX_VMCS_ENUM_MAX_INDEX));
   }

   commonVal = INSERT_FIELD(commonVal, MSR_VMX_VMCS_ENUM_MAX_INDEX, maxIndex);
   return commonVal;
}


/*
 *----------------------------------------------------------------------
 *
 * IntelVT_FindCommon3rd --
 *
 *      Computes and returns a common MSR_VMX_3RD_CTLS feature MSR across
 *      all logical processors on the host.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint64
IntelVT_FindCommon3rd(const MSRCache *data,     // IN
                      IntelVTMSRGet_Fn getMSR,  // IN
                      unsigned numCPUs)         // IN
{
   return IntelVTFindCommon(MSR_VMX_3RD_CTLS, data, getMSR, numCPUs, TRUE);
}


#undef EXTRACT_FIELD
#undef INSERT_FIELD
#undef INVALID_VMX_BASIC


/*
 *----------------------------------------------------------------------
 *
 * IntelVT_Enabled --
 *
 *      Use the MSR cache to check the feature control MSR.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
IntelVT_Enabled(const MSRCache *cache, uint32 pcpu)
{
   return VT_EnabledFromFeatures(MSRCache_Get(cache, MSR_FEATCTL, pcpu));
}


/*
 *----------------------------------------------------------------------
 *
 * IntelVT_Supported --
 *
 *      Helper function to query all the MSRs needed by
 *      VT_SupportedFromFeatures().
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
IntelVT_Supported(const MSRCache *cache, uint32 pcpu)
{
   uint64 basic = MSRCache_Get(cache, MSR_VMX_BASIC, pcpu);
   if (basic & MSR_VMX_BASIC_TRUE_CTLS) {
      uint64 pin   = MSRCache_Get(cache, MSR_VMX_TRUE_PINBASED_CTLS, pcpu);
      uint64 cpu   = MSRCache_Get(cache, MSR_VMX_TRUE_PROCBASED_CTLS, pcpu);
      uint64 entry = MSRCache_Get(cache, MSR_VMX_TRUE_ENTRY_CTLS, pcpu);
      uint64 exit  = MSRCache_Get(cache, MSR_VMX_TRUE_EXIT_CTLS, pcpu);
      return VT_SupportedFromFeatures(pin, cpu, entry, exit, basic);
   } else {
      /* Bug 1914425 - VMM no longer supports CPUs without TRUE_xxx_CTLS */
      return FALSE;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * IntelVT_VVTSupported --
 *
 *      Helper function to query all the MSRs needed by
 *      VVT_SupportedFromFeatures().
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
IntelVT_VVTSupported(const MSRCache *cache, uint32 pcpu)
{
   uint64 basic = MSRCache_Get(cache, MSR_VMX_BASIC, pcpu);
   uint64 misc = MSRCache_Get(cache, MSR_VMX_MISC, pcpu);
   uint64 cr0Fixed0 = MSRCache_Get(cache, MSR_VMX_CR0_FIXED0, pcpu);
   uint64 cr0Fixed1 = MSRCache_Get(cache, MSR_VMX_CR0_FIXED1, pcpu);
   uint64 cr4Fixed0 = MSRCache_Get(cache, MSR_VMX_CR4_FIXED0, pcpu);
   uint64 cr4Fixed1 = MSRCache_Get(cache, MSR_VMX_CR4_FIXED1, pcpu);
   uint64 secondary = MSRCache_Get(cache, MSR_VMX_2ND_CTLS, pcpu);
   uint64 eptVPID = MSRCache_Get(cache, MSR_VMX_EPT_VPID, pcpu);
   uint64 truePin = MSRCache_Get(cache, MSR_VMX_TRUE_PINBASED_CTLS, pcpu);
   uint64 trueCPU = MSRCache_Get(cache, MSR_VMX_TRUE_PROCBASED_CTLS, pcpu);
   return VVT_SupportedFromFeatures(basic, misc, cr0Fixed0, cr0Fixed1,
                                    cr4Fixed0, cr4Fixed1, secondary, eptVPID,
                                    truePin, trueCPU);
}

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
