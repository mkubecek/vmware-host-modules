/*********************************************************
 * Copyright (C) 1998-2014,2017,2019-2021 VMware, Inc. All rights reserved.
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
 * ptsc.h --
 *
 *      Pseudo TSC
 */

#ifndef _PTSC_H_
#define _PTSC_H_

#define INCLUDE_ALLOW_VMX

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "rateconv.h"

/*
 * RDTSC and PTSC_Get.
 *
 * RDTSC reads the hardware timestamp counter on the current physical
 * CPU.  In general, the TSC is *not* a globally consistent timer that
 * runs at a constant rate.  Any code that still assumes it is should
 * be corrected; see PR 20499.
 *
 * PTSC_Get returns a pseudo-TSC that runs at approximately the
 * maximum speed of physical CPU 0's TSC and is approximately globally
 * consistent.  It is available both at userlevel and in the monitor,
 * with different implementations.  In the vmkernel, Timer_PseudoTSC
 * provides similar functionality.
 *
 */

/* TS stands for "timestamp", which is in units of "cycles" */
typedef uint64 VmAbsoluteTS; // a particular point in time (in cycles)
typedef int64  VmRelativeTS; // a signed delta in cycles
typedef uint64 VmIntervalTS; // an unsigned delta in cycles
typedef uint64 VmAbsoluteUS; // a particular point in time (in us)
typedef int64  VmRelativeUS; // a signed delta in us
typedef uint64 VmIntervalUS; // an unsigned delta in us

/*
 * Compare two VmAbsoluteTS's using comparison operator op, allowing
 * for wrap.  The assumption is that differences should not be more
 * than 2**63, so a larger difference is taken as negative.
 */
#define COMPARE_TS(ts1, op, ts2) (((int64) ((ts1) - (ts2))) op 0)

#define MAX_ABSOLUTE_TS \
   ((VmAbsoluteTS) CONST64U(0xffffffffffffffff))

/*
 * Largest possible unambiguous difference between two VmAbsoluteTS's
 * according to COMPARE_TS's method of comparison.
 */
#define MAX_RELATIVE_TS \
   ((VmRelativeTS) CONST64(0x7fffffffffffffff))

#define MAX_ABSOLUTE_US \
   ((VmAbsoluteUS) CONST64U(0xffffffffffffffff))

typedef struct PTSCInfo {
   Bool             hwTSCsSynced;
   Bool             hwTSCsAdjusted;
   uint8            _pad[6];
   int64            hz;
   uint32           kHz;
   uint32           mHz;

   RateConv_Ratio   usToCycles;
   RateConv_Ratio   cyclesToUs;
   RateConv_Ratio   msToCycles;
   RateConv_Ratio   cyclesToNs;
} PTSCInfo;

extern PTSCInfo ptscInfo;

Bool PTSC_Init(uint64 tscHz);
VmAbsoluteTS PTSC_InitialCount(const char *module,
                               const char *option,
                               Bool stress,
                               VmIntervalTS freq,
                               VmAbsoluteTS defaultCnt);
Bool PTSC_HasPerfectlySynchronizedTSCs(void);
VmRelativeTS PTSC_RefClockOffset(void);

static INLINE int64
PTSC_Hz(void)
{
   ASSERT(ptscInfo.hz);
   return ptscInfo.hz;
}

static INLINE uint32
PTSC_KHz(void)
{
   ASSERT(ptscInfo.kHz);
   return ptscInfo.kHz;
}

static INLINE uint32
PTSC_MHz(void)
{
   ASSERT(ptscInfo.mHz);
   return ptscInfo.mHz;
}

#if defined(VM_X86_64) || defined(VM_ARM_64)

/*
 * Conversions to/from cycles.  Note that the conversions operate on
 * signed values, so be careful when taking the difference of two
 * VmAbsoluteTS (which is unsigned) that that value is not out of range
 * of the signed type.
 */

static INLINE VmRelativeTS
PTSC_USToCycles(int64 us)
{
   return Muls64x32s64(us, ptscInfo.usToCycles.mult, ptscInfo.usToCycles.shift);
}

static INLINE VmRelativeTS
PTSC_MSToCycles(int64 ms)
{
   return Muls64x32s64(ms, ptscInfo.msToCycles.mult, ptscInfo.msToCycles.shift);
}

static INLINE int64
PTSC_CyclesToNS(VmRelativeTS ts)
{
   return Muls64x32s64(ts, ptscInfo.cyclesToNs.mult, ptscInfo.cyclesToNs.shift);
}

static INLINE int64
PTSC_CyclesToUS(VmRelativeTS ts)
{
   return Muls64x32s64(ts, ptscInfo.cyclesToUs.mult, ptscInfo.cyclesToUs.shift);
}

#else

/* 32-bit Muls64x32s64 too big to justify inlining. */
VmRelativeTS PTSC_USToCycles(int64 us);
VmRelativeTS PTSC_MSToCycles(int64 ms);
int64 PTSC_CyclesToNS(VmRelativeTS ts);
int64 PTSC_CyclesToUS(VmRelativeTS ts);

#endif

#if defined(VMX86_SERVER) && (defined(VMX86_VMX) || defined (ULM_ESX))

/*
 * ESX with userworld VMX
 */
#include "user_layout.h"

static INLINE VmAbsoluteTS
PTSC_Get(void)
{
   extern __thread User_ThreadData vmkUserTdata;
   VmAbsoluteTS ptsc;

   if (vmkUserTdata.magic != USER_THREADDATA_MAGIC) {
      return 0;
   }
   ptsc = vmkUserTdata.u.pseudoTSCGet(&vmkUserTdata);
   ASSERT((int64)ptsc >= 0);
   return ptsc;
}

#else

/*
 * Monitor and hosted VMX
 */

VmAbsoluteTS PTSC_Get(void);

#endif

/*
 *-----------------------------------------------------------------------------
 *
 * PTSC_HasSynchronizedTSCs --
 *
 *      Returns TRUE iff the platform TSCs are known to be synchronized.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
PTSC_HasSynchronizedTSCs(void)
{
   return ptscInfo.hwTSCsSynced;
}


/*
 *-----------------------------------------------------------------------------
 *
 * PTSC_HostAdjustedTSCs --
 *
 *      Returns TRUE if the platform may have adjusted TSCs in an attempt
 *      to sync them up.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
PTSC_HostAdjustedTSCs(void)
{
   return ptscInfo.hwTSCsAdjusted;
}


/*
 *----------------------------------------------------------------------
 *
 * PTSC_AdvanceTimer --
 *
 *      Advance '*deadline' in 'period' increments such that it is
 *      greater than 'now'.  Return the number of ticks incremented.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint64
PTSC_AdvanceTimer(VmAbsoluteTS now,
                  VmIntervalTS period,
                  VmAbsoluteTS *deadline)
{
   VmAbsoluteTS d = *deadline;
   if (now >= d) {
      uint64 count = 1;
      d += period;
      if (UNLIKELY(now >= d)) {
         uint64 t = (now - d) / period + 1;
         VmIntervalTS diff = t * period;
         count += t;
         d += diff;
      }
      *deadline = d;
      return count;
   }
   return 0;
}

#endif /* ifndef _PTSC_H_ */
