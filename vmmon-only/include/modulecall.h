/*********************************************************
 * Copyright (C) 1998-2021 VMware, Inc. All rights reserved.
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
 * modulecall.h
 *
 *        Monitor <-->  Module (kernel driver) interface
 */

#ifndef _MODULECALL_H
#define _MODULECALL_H

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "cpu_types.h"
#include "x86desc.h"
#include "ptsc.h"
#include "vcpuid.h"
#include "vcpuset.h"
#include "vmm_constants.h"
#include "contextinfo.h"
#include "rateconv.h"
#include "mon_assert.h"
#include "uccost.h"

#define NUM_EXCEPTIONS   20     /* EXC_DE ... EXC_XF. */

#define MODULECALL_TABLE                                                      \
   MC(INTR)                                                                   \
   MC(SEMAWAIT)                                                               \
   MC(SEMASIGNAL)                                                             \
   MC(SEMAFORCEWAKEUP)                                                        \
   MC(IPI)          /* Hit multiple threads with IPI. */                      \
   MC(ONE_IPI)      /* Hit a single thread with IPI. */                       \
   MC(USERRETURN)   /* Return codes for user calls. */                        \
   MC(GET_RECYCLED_PAGES)                                                     \
   MC(RELEASE_ANON_PAGES)                                                     \
   MC(LOOKUP_MPN)                                                             \
   MC(COSCHED)                                                                \
   MC(ALLOC_CONTIG_PAGES)                                                     \
   MC(ALLOC_VMX_PAGE)                                                         \
   MC(ALLOC_TMP_GDT)                                                          \
   MC(PIN_MPN)                                                                \
   MC(VMCLEAR_VMCS_ALL_CPUS)                                                  \
   MC(GET_PAGE_ROOT)                                                          \
   MC(ALLOC_ANON_LOW_PAGE)                                                    \
   MC(GET_MON_IPI_VECTOR)                                                     \
   MC(GET_HV_IPI_VECTOR)                                                      \
   MC(GET_PERF_CTR_VECTOR)                                                    \
   MC(GET_HOST_TIMER_VECTORS)                                                 \
   MC(BOOTSTRAP_CLEANUP)                                                      \
   MC(GET_SHARED_AREA)                                                        \
   MC(GET_STAT_VARS)                                                          \
   MC(GET_NUM_PTP_PAGES)                                                      \
   MC(GET_HV_IO_BITMAP)                                                       \
   MC(GET_MSR)                                                                \

/*
 *----------------------------------------------------------------------
 *
 * ModuleCallType --
 *
 *      Enumeration of support calls done by the module.
 *
 *----------------------------------------------------------------------
 */

typedef enum ModuleCallType {
   MODULECALL_NONE = 100,
#define MC(_modulecall) MODULECALL_##_modulecall,
   MODULECALL_TABLE
#undef MC
   MODULECALL_LAST                   // Number of entries. Must be the last one
} ModuleCallType;

#define MODULECALL_USERCALL_NONE     300

#define SHADOW_DR(cpData, n)    (cpData)->shadowDR[n].ureg64


/*----------------------------------------------------------------------
 *
 * MAX_SWITCH_PT_PATCHES
 *
 *   This is the maximum number of patches that must be placed into
 *   the monitor page tables so that the host GDT, crosspage data, and
 *   crosspage code can be accessed during worldswitch.
 *
 *----------------------------------------------------------------------
 */
#define MAX_SWITCH_PT_PATCHES 3

/*----------------------------------------------------------------------
 *
 * WS_INTR_STRESS
 *
 *   When set to non-zero, world switch code will enable single-step
 *   debugging across much of the switch path in both directions.  The
 *   #DB handler detects single-stepping and induces a simulated NMI per
 *   instruction.  This verifies that interrupts and exceptions are safe
 *   across the switch path, even if an NMI were raised during handling
 *   of another exception.
 *
 *   When set to zero, normal worldswitch operation occurs.
 *
 *   See the worldswitch assembly code for details.
 *
 *----------------------------------------------------------------------
 */
#define WS_INTR_STRESS 0


/*----------------------------------------------------------------------
 *
 * VMMPageTablePatch
 *
 *    Describes an entry in the monitor page table which needs to be
 *    patched during the back-to-host worldswitch.
 *
 *    o A patch can appear at any place in the page table, and so four
 *      items are required to uniquely describe the patch:
 *
 *      o level
 *
 *        This is the level in the page table to which the patch must
 *        be applied: L4, L3, L2, L1.  This information is used to
 *        determine the base of the region of memory which must be
 *        patched. The value zero is reserved to indicate an empty spot
 *        in the array of patches.
 *
 *      o index
 *
 *        The index of the PTE at the given page table.
 *
 *      o ptIdx
 *
 *        The index of the page table at the given level.
 *
 *      o pte
 *
 *        This is the PTE value which will be patched into the monitor
 *        page table.
 *
 *----------------------------------------------------------------------
 */
#pragma pack(push, 1)
typedef struct VMMPageTablePatch {
#define PTP_EMPTY    (0U) /* Unused array entry. (must be 0) */
#define PTP_LEVEL_L1 (1U)
#define PTP_LEVEL_L2 (2U)
#define PTP_LEVEL_L3 (3U)
#define PTP_LEVEL_L4 (4U)       /* root level */
   uint32   level;              /* [0, 4]  (maximal size: 3 bits) */
   uint32   pteIdx;             /* Index of the PTE in the page table. */
   uint64   pteGlobalIdx;       /* Global index of the PTE in 'level'. */
   LPN      lpn;                /* Logical page number mapped by patch. */
   VM_PDPTE pte;                /* PTE.                                */
} VMMPageTablePatch;
#pragma pack(pop)

#define MODULECALL_NUM_ARGS  4

/*
 * The cross page contains tiny stacks upon which interrupt and exception
 * handlers in the switch path may temporarily run.  Each stack must be
 * large enough for the sum of:
 *
 * - 1 #DB exception frame (5 * uint64)
 * - 1 #NMI exception frame (5 * uint64)
 * - 1 #MCE exception frame (5 * uint64)
 * - the largest stack use instantaneously possible by #MCE handling code
 * - the largest stack use instantaneously possible by #NMI handling code
 * - the largest stack use instantaneously possible by #DB handling code
 * - one high-water uint32 used to detect stack overflows when debugging
 *
 * A breakdown of the worst-case exception handler stack usage (SwitchUDHandler)
 * is: 5 * uint64 (Hardware) + 1 * uint64 (RAX) + 1 * uint64 (RBX) +
 *     1 * uint64 (RCX) + 1 * uint64 (call) + 2 * uint64 (sidt) = 11 * uint64
 * This is a slight over-estimate of the possible usage at any time but there
 * is plenty of space available in the cross page data area.
 *
 * 264 (11 * sizeof(uint64) * 3) bytes is slightly more than enough as of
 * 2020/06/14.
 */
#define TINY_STACK_SIZE      264

/*
 *----------------------------------------------------------------------
 *
 * VMCrossPageData --
 *
 *      Data which is stored on the cross page.
 *
 *----------------------------------------------------------------------
 */
#pragma pack(push, 1)
typedef struct VMCrossPageData {
   uint32   version;           // CROSSPAGE_VERSION
   uint32   vmmonVersion;      // VMMON_VERSION

   /*
    * The following stacks and contexts are ordered for performance and code
    * simplicity.  Both HostToVmm and VmmToHost strictly require this ordering.
    * For any change to this data, both functions must be updated.
    * Use VMX86_UCCOST to measure performance when changing the layout.
    */

   /* A tiny stack and the host context. */
   uint64   hostTinyStack[TINY_STACK_SIZE / sizeof(uint64)];
   uint64   hostCR3; /* Edge of context saved/restored in assembly. */
   uint64   hostRBX;
   uint64   hostRBP;
   uint64   hostRSI;
   uint64   hostRDI;
   uint64   hostR12;
   uint64   hostR13;
   uint64   hostR14;
   uint64   hostR15;
   uint64   hostRSP;
   uint16   hostSS;  /* Edge of context saved/restored in assembly. */
   uint16   hostDS;  /* Not saved/restored in assembly switch */
   uint16   hostES;  /* Not saved/restored in assembly switch */
   uint16   hostPad;

   /* A tiny stack and the monitor context. */
   uint64   monTinyStack[TINY_STACK_SIZE / sizeof(uint64)];
   uint64   monCR3; /* Edge of context saved/restored in assembly. */
   uint64   monRBX;
   uint64   monRBP;
   uint64   monR12;
   uint64   monR13;
   uint64   monR14;
   uint64   monR15;
   uint64   monRSP;
   uint16   monSS;  /* Edge of context saved/restored in assembly. */
   uint16   monDS;  /* Not saved/restored in assembly switch */
   uint16   monES;  /* Not saved/restored in assembly switch */
   uint16   monPad;

   uint64   crosspageDataMA;

   uint64   hostDR[8];
   LA64     crosspageDataLA;   // where host/PTP map the cross data page
   LA64     crossGDTLA;        // where host/PTP map the cross GDT
   LA64     crosspageCodeLA;   // where host/PTP map the cross code page
   LA64     vmmToHostLA;       // where host has placed the VmmToHost function
   uint16   hostInitial64CS;
   uint8    hostDRSaved;       // Host DR spilled to hostDR[x].
   uint8    hostDRInHW;        // 0 -> shadowDR in h/w, 1 -> hostDR in h/w.
   uint32   _pad0;

   uint64   wsCR0;
   uint64   wsCR4;

   DTR64    crossGDTHKLADesc;   // always uses host kernel linear address
   uint16   _pad1[3];
   DTR64    monGDTR;
   uint16   _pad2[3];
   /* A hardcoded value for monitor %rip which facilitates backtraces. */
   uint64   monRIP;
   Task64   monTask;          /* vmm's task */

   VMMPageTablePatch vmmPTP[MAX_SWITCH_PT_PATCHES]; /* page table patch */

   /*
    * The monitor may requests up to two actions when returning to the
    * host.  The moduleCallType field and args encode a request for
    * some action in the driver.  The vcpuSet field is an additional
    * argument used in some calls.  The userCallType field (together
    * with the RPC block) encodes a user call request.  The two
    * requests are independent.  The user call is executed first, with
    * the exception of MODULECALL_INTR which has a special effect.
    */
   ModuleCallType moduleCallType;
   uint32         retval;
   uint64         args[MODULECALL_NUM_ARGS];
#if !defined(VMX86_SERVER)
   VCPUSet        vcpuSet;
#endif
   int            userCallType;
   uint32         pcpuNum;   /* Used as extra module call arg within vmmon. */

#if !defined(VMX86_SERVER)
   uint64 ucTimeStamps[UCCOST_MAX];
   uint8  _ucPad[8];
#endif

   /*
    * The values in the shadow debug registers must match those in the
    * hardware debug register immediately after a task switch in
    * either direction.  They are used to minimize moves to and from
    * the debug registers.
    */
   SharedUReg64     shadowDR[8];
   uint8            shadowDRInHW; // bit n set iff %DRn == shadowDR[n]

   /* TRUE if no bits are set in yieldVCPUs, FALSE otherwise. */
   Bool             yieldVCPUsIsEmpty;
   uint8            _pad3[6];

   VCPUSet          yieldVCPUs;

   /*
    * Adjustment for machines where the hardware TSC does not run
    * constantly (laptops) or is out of sync between different PCPUs.
    * Updated as needed by vmmon.  See VMK_SharedData for the ESX
    * analog, which is updated by the vmkernel.
    */
   RateConv_ParamsVolatile pseudoTSCConv;
   VmAbsoluteTS            worldSwitchPTSC; // PTSC value immediately before
                                            // last worldswitch.

   VmAbsoluteTS timerIntrTS;    // PTSC of timer interrupt while in the vmm
   VmAbsoluteTS hstTimerExpiry; // PTSC of host timer interrupt
   VmAbsoluteTS monTimerExpiry; // PTSC of next MonTimer callback

   Bool     retryWorldSwitch;   // TRUE -> return to host on host->vmm switch
   /*
    * TRUE if moduleCall was interrupted by signal. Only
    * vmmon uses this field to remember that it should
    * restart RunVM call, nobody else should look at it.
    */
   Bool     moduleCallInterrupted;
   uint8    _pad4[6];

   DTR64    switchHostIDTR;   // baseLA = switchHostIDT's host kernel LA
   uint16   _pad5[3];
   DTR64    switchMonIDTR;    // baseLA = switchMonIDT's monitor LA
   uint16   _pad6[3];

   /*
    * Descriptors and interrupt tables for switchNMI handlers.  Each
    * IDT has only enough space for the hardware exceptions; they are
    * sized to accommodate 64-bit descriptors.
    */
   Gate64 switchHostIDT[NUM_EXCEPTIONS];      // hostCS:hostVA
   Gate64 switchMonIDT[NUM_EXCEPTIONS];       // monCS:monVA
   volatile Bool wsException[NUM_EXCEPTIONS]; // Tracks faults in worldswitch.
   uint8         _pad7[4];
   uint64        wsUD2;                       // IP of ud2 instr or 0 if unset.
   uint64        specCtrl; /* host MSR_SPEC_CTRL value before world switch. */
} VMCrossPageData;
#pragma pack(pop)

#define CROSSPAGE_VERSION_BASE 0xc14 /* increment by 1 */
#define CROSSPAGE_VERSION    ((CROSSPAGE_VERSION_BASE << 1) + WS_INTR_STRESS)

#if !defined(VMX86_SERVER) && defined(VMM)
#define CROSS_PAGE             ((VMCrossPageData *) \
                                VPN_2_VA(CROSS_PAGE_DATA_START))
#define VMM_SWITCH_SHARED_DATA ((VMCrossPageData * const) \
                                VPN_2_VA(CROSS_PAGE_DATA_START))
#endif

#define MX_WAITINTERRUPTED     3
#define MX_WAITTIMEDOUT        2
#define MX_WAITNORMAL          1
#define MX_WAITERROR           0

#endif
