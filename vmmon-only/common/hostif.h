/*********************************************************
 * Copyright (C) 1998-2020 VMware, Inc. All rights reserved.
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
 *  hostif.h - Platform dependent interface for supporting
 *             the vmx86 device driver.
 */


#ifndef _HOSTIF_H_
#define _HOSTIF_H_

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vmx86.h"
#include "vcpuset.h"

#include "hostifMem.h"
#include "hostifGlobalLock.h"

/*
 * Host-specific definitions.
 */
#if !__linux__ && !defined(WINNT_DDK) && !defined __APPLE__
#error "Only Linux or NT or Mac OS defined for now."
#endif


/*
 * The default monitor spin time for crosscalls is 50 usec
 * in vmcore/vmx/main/monitor_init.c.  This value is used
 * in vmx86_YieldToSet to decide whether to block and wait
 * for another vCPU to process our crosscall, or just wake
 * up the other vCPUs and go back to monitor.
 */

#define CROSSCALL_SPIN_SHORT_US 50

/*
 * Sleep timeout in usec, see above comment for CROSSCALL_SPIN_SHORT_US
 */

#define CROSSCALL_SLEEP_US 1000

typedef struct HostIFContigMemMap {
   MPN mpn;
   void *addr;
   PageCnt pages;
   struct HostIFContigMemMap *next;
} HostIFContigMemMap;

EXTERN Bool  HostIF_Init(VMDriver *vm, uint32 numVCPUs);
EXTERN int   HostIF_LookupUserMPN(VMDriver *vm, VA64 uAddr, MPN *mpn);

EXTERN MPN   HostIF_GetCrossPageDataMPN(VMCrossPageData *crosspageData);
EXTERN MPN   HostIF_GetCrossPageCodeMPN(void);

EXTERN void *HostIF_AllocPage(void);
EXTERN void  HostIF_FreePage(void *ptr);

EXTERN VPN   HostIF_MapPage(MPN mpn);
EXTERN void  HostIF_UnmapPage(VPN vpn);
EXTERN int   HostIF_LockPage(VMDriver *vm, VA64 uAddr,
                             Bool allowMultipleMPNsPerVA, MPN *mpn);
EXTERN int   HostIF_UnlockPage(VMDriver *vm, VA64 uAddr);
EXTERN int   HostIF_UnlockPageByMPN(VMDriver *vm, MPN mpn, VA64 uAddr);
EXTERN Bool  HostIF_IsLockedByMPN(VMDriver *vm, MPN mpn);
EXTERN void  HostIF_FreeAllResources(VMDriver *vm);
EXTERN uint64 HostIF_ReadUptime(void);
EXTERN uint64 HostIF_UptimeFrequency(void);
EXTERN PageCnt HostIF_EstimateLockedPageLimit(const VMDriver *vm,
                                              PageCnt lockedPages);
EXTERN void  HostIF_Wait(unsigned int timeoutMs);
EXTERN void  HostIF_WaitForFreePages(unsigned int timeoutMs);
EXTERN void *HostIF_AllocKernelPages(PageCnt numPages, MPN *mpns);
EXTERN void  HostIF_FreeKernelPages(PageCnt numPages, void *ptr);
EXTERN HostIFContigMemMap *HostIF_AllocContigPages(VMDriver *vm,
                                                   PageCnt numPages);
EXTERN void  HostIF_FreeContigPages(VMDriver *vm, HostIFContigMemMap *mapping);
EXTERN void  HostIF_VMLock(VMDriver *vm, int callerID);
EXTERN void  HostIF_VMUnlock(VMDriver *vm, int callerID);
#ifdef VMX86_DEBUG
EXTERN Bool HostIF_VMLockIsHeld(VMDriver *vm);
#endif

EXTERN void  HostIF_APICInit(VMDriver *vm);
EXTERN uint8 HostIF_GetMonitorIPIVector(void);
EXTERN uint8 HostIF_GetHVIPIVector(void);
EXTERN uint8 HostIF_GetPerfCtrVector(void);
EXTERN void  HostIF_GetTimerVectors(uint8 *v0, uint8 *v1);

EXTERN int   HostIF_SemaphoreWait(VMDriver *vm,
                                  Vcpuid vcpuid,
                                  uint64 *args);

EXTERN int   HostIF_SemaphoreSignal(VMDriver *vm, uint64 *args);

EXTERN void  HostIF_SemaphoreForceWakeup(VMDriver *vm, const VCPUSet *vcs);
EXTERN void  HostIF_IPI(VMDriver *vm, const VCPUSet *vcs);
EXTERN void  HostIF_OneIPI(VMDriver *vm, Vcpuid v);

EXTERN uint32 HostIF_GetCurrentPCPU(void);
EXTERN void HostIF_CallOnEachCPU(void (*func)(void *), void *data);

EXTERN Bool HostIF_PrepareWaitForThreads(VMDriver *vm, Vcpuid currVcpu);
EXTERN void HostIF_WaitForThreads(VMDriver *vm, Vcpuid currVcpu);
EXTERN void HostIF_CancelWaitForThreads(VMDriver *vm, Vcpuid currVcpu);
EXTERN void HostIF_WakeUpYielders(VMDriver *vm, Vcpuid currVcpu);

EXTERN int64 HostIF_AllocLockedPages(VMDriver *vm, VA64 addr,
                                     PageCnt numPages, Bool kernelMPNBuffer);
EXTERN int HostIF_FreeLockedPages(VMDriver *vm, MPN *mpns, PageCnt numPages);
EXTERN MPN HostIF_GetNextAnonPage(VMDriver *vm, MPN mpn);
EXTERN PageCnt HostIF_GetNumAnonPages(VMDriver *vm);
EXTERN MPN HostIF_AllocLowPage(VMDriver *vm);

EXTERN int HostIF_ReadPhysical(VMDriver *vm, MA ma, VA64 addr,
                               Bool kernelBuffer, size_t len);
EXTERN int HostIF_WritePhysical(VMDriver *vm, MA ma, VA64 addr,
                                Bool kernelBuffer, size_t len);
EXTERN int HostIF_WriteMachinePage(MPN mpn, VA64 addr);
#if defined __APPLE__
// There is no need for a fast clock lock on Mac OS.
#define HostIF_FastClockLock(_callerID) do {} while (0)
#define HostIF_FastClockUnlock(_callerID) do {} while (0)
#else
EXTERN void HostIF_FastClockLock(int callerID);
EXTERN void HostIF_FastClockUnlock(int callerID);
#endif
EXTERN int HostIF_SetFastClockRate(unsigned rate);

EXTERN MPN HostIF_AllocMachinePage(void);
EXTERN void HostIF_FreeMachinePage(MPN mpn);

EXTERN int HostIF_SafeRDMSR(uint32 msr, uint64 *val);

EXTERN int HostIF_CopyFromUser(void *dst, VA64 src, size_t len);
EXTERN int HostIF_CopyToUser(VA64 dst, const void *src, size_t len);

#if defined __APPLE__
EXTERN void HostIF_PageUnitTest(void);
#endif

#endif // ifdef _HOSTIF_H_
