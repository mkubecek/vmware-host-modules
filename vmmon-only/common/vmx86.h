/*********************************************************
 * Copyright (C) 1998-2017 VMware, Inc. All rights reserved.
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
 *  vmx86.h - Platform independent data and interface for supporting
 *            the vmx86 device driver.
 */

#ifndef VMX86_H
#define VMX86_H

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "x86apic.h"
#include "x86msr.h"
#include "modulecall.h"
#include "vcpuid.h"
#include "initblock.h"
#include "iocontrols.h"
#include "numa_defs.h"
#include "rateconv.h"
#include "vmmem_shared.h"
#include "apic.h"

typedef struct PseudoTSCOffsetInfo {
   uint32 inVMMCnt;                  /* Number of vcpus executing in the VMM. */
   uint32 vcpuid;                    /* Index into VMDriver.ptscOffsets. */
} PseudoTSCOffsetInfo;

typedef struct TSCDelta {
   Atomic_uint64 min;
   Atomic_uint64 max;
} TSCDelta;

/*
 * VMDriver - the main data structure for the driver side of a
 *            virtual machine.
 */

typedef struct VMDriver {
   /* Unique (in the driver), strictly positive, VM ID used by userland. */
   int                          userID;

   struct VMDriver             *nextDriver;   /* Next on list of all VMDrivers */

   Vcpuid                       numVCPUs;     /* Number of vcpus in VM. */
   struct VMHost               *vmhost;       /* Host-specific fields. */

   /* Pointers to the crossover pages shared with the monitor. */
   struct VMCrossPage          *crosspage[MAX_INITBLOCK_CPUS];
   volatile uint32              currentHostCpu[MAX_INITBLOCK_CPUS];
   VCPUSet                      crosscallWaitSet[MAX_INITBLOCK_CPUS];
   APICDescriptor               hostAPIC;

   struct MemTrack             *memtracker;   /* Memory tracker pointer */
   Bool                         checkFuncFailed;
   struct PerfCounter          *perfCounter;
   VMMemMgmtInfo                memInfo;
   unsigned                     fastClockRate;/* Protected by FastClockLock. */
   int                          fastSuspResFlag;

   volatile PseudoTSCOffsetInfo ptscOffsetInfo; /* Volatile per PR 699101#29. */
   Atomic_uint64                ptscLatest;
   int64                        ptscOffsets[MAX_INITBLOCK_CPUS];
} VMDriver;

typedef struct VmTimeStart {
   uint64 count;
   uint64 time;
} VmTimeStart;

typedef struct RefClockParams {
   RateConv_Ratio ratio;
   Atomic_uint64  add;
} RefClockParams;

typedef struct PseudoTSC {
   RefClockParams refClockToPTSC;
   int64          tscOffset;
   uint64         hz;
   volatile Bool  useRefClock;
   Bool           neverSwitchToRefClock;
   Bool           hwTSCsSynced;
   volatile Bool  initialized;
} PseudoTSC;

extern PseudoTSC pseudoTSC;

#define MAX_LOCKED_PAGES (-1)

extern VMDriver *Vmx86_CreateVM(void);
extern int Vmx86_LookupUserMPN(VMDriver *vm, VA64 uAddr, MPN *mpn);
extern int Vmx86_ReleaseVM(VMDriver *vm);
extern int Vmx86_InitVM(VMDriver *vm, InitBlock *initParams);
extern int Vmx86_LateInitVM(VMDriver *vm);
extern int Vmx86_RunVM(VMDriver *vm, Vcpuid vcpuid);
extern void Vmx86_YieldToSet(VMDriver *vm, Vcpuid currVcpu, const VCPUSet *req,
                             uint32 usecs, Bool skew);
extern void Vmx86_ReadTSCAndUptime(VmTimeStart *st);
extern uint32 Vmx86_ComputekHz(uint64 cycles, uint64 uptime);
extern uint32 Vmx86_GetkHzEstimate(VmTimeStart *st);
extern int Vmx86_SetHostClockRate(VMDriver *vm, unsigned rate);
extern int Vmx86_LockPage(VMDriver *vm,
                          VA64 uAddr,
                          Bool allowMultipleMPNsPerVA,
                          MPN *mpn);
extern int Vmx86_UnlockPage(VMDriver *vm, VA64 uAddr);
extern int Vmx86_UnlockPageByMPN(VMDriver *vm, MPN mpn, VA64 uAddr);
extern MPN Vmx86_GetRecycledPage(VMDriver *vm);
extern int Vmx86_ReleaseAnonPage(VMDriver *vm, MPN mpn);
extern int Vmx86_AllocLockedPages(VMDriver *vm, VA64 addr,
				  unsigned numPages, Bool kernelMPNBuffer,
                                  Bool ignoreLimits);
extern int Vmx86_FreeLockedPages(VMDriver *vm, VA64 addr,
				 unsigned numPages, Bool kernelMPNBuffer);
extern MPN Vmx86_GetNextAnonPage(VMDriver *vm, MPN mpn);
extern int Vmx86_GetLockedPageList(VMDriver *vm, VA64 uAddr,
				   unsigned int numPages);

extern int32 Vmx86_GetNumVMs(void);
extern int32 Vmx86_GetTotalMemUsage(void);
extern Bool Vmx86_SetConfiguredLockedPagesLimit(unsigned limit);
extern void Vmx86_SetDynamicLockedPagesLimit(unsigned limit);
extern Bool Vmx86_GetMemInfo(VMDriver *curVM,
                             Bool curVMOnly,
                             VMMemInfoArgs *args,
                             int outArgsLength);
extern void Vmx86_Admit(VMDriver *curVM, VMMemInfoArgs *args);
extern Bool Vmx86_Readmit(VMDriver *curVM, OvhdMem_Deltas *delta);
extern void Vmx86_UpdateMemInfo(VMDriver *curVM,
                                const VMMemMgmtInfoPatch *patch);
extern void Vmx86_Add2MonPageTable(VMDriver *vm, VPN vpn, MPN mpn,
				   Bool readOnly);
extern Bool Vmx86_PAEEnabled(void);
extern Bool Vmx86_VMXEnabled(void);
extern Bool Vmx86_GetAllMSRs(MSRQuery *query);
extern void Vmx86_MonTimerIPI(void);
extern void Vmx86_InitIDList(void);
extern VMDriver *Vmx86_LookupVMByUserID(int userID);
extern Bool Vmx86_FastSuspResSetOtherFlag(VMDriver *vm, int otherVmUserId);
extern int  Vmx86_FastSuspResGetMyFlag(VMDriver *vm, Bool blockWait);
extern void Vmx86_Open(void);
extern void Vmx86_Close(void);

static INLINE Bool
Vmx86_HwTSCsSynced(void)
{
   return pseudoTSC.hwTSCsSynced;
}

static INLINE Bool
Vmx86_PseudoTSCUsesRefClock(void)
{
   return pseudoTSC.useRefClock;
}

static INLINE Bool
Vmx86_SetPseudoTSCUseRefClock(void)
{
   if (!pseudoTSC.useRefClock && !pseudoTSC.neverSwitchToRefClock) {
      pseudoTSC.useRefClock = TRUE;
      return TRUE;
   }
   return FALSE;
}

static INLINE uint64
Vmx86_GetPseudoTSCHz(void)
{
   return pseudoTSC.hz;
}

static INLINE uint64
Vmx86_GetPseudoTSCOffset(void)
{
   return pseudoTSC.tscOffset;
}

extern void Vmx86_InitPseudoTSC(PTSCInitParams *params);
extern Bool Vmx86_CheckPseudoTSC(uint64 *lastTSC, uint64 *lastRC);
extern uint64 Vmx86_GetPseudoTSC(void);

extern uint64 Vmx86_GetUnavailablePerfCtrs(void);

#endif 
