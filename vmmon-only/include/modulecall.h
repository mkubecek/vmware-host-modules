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
 * modulecall.h
 *
 *        Monitor <-->  Module (kernel driver) interface
 */

#ifndef _MODULECALL_H
#define _MODULECALL_H

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "x86types.h"
#include "x86desc.h"
#include "ptsc.h"
#include "vcpuid.h"
#include "vcpuset.h"
#include "vmm_constants.h"
#include "contextinfo.h"
#include "rateconv.h"
#include "modulecallstructs.h"
#include "mon_assert.h"

#define NUM_EXCEPTIONS 20       /* EXC_DE ... EXC_XF. */
 
#define MODULECALL_TABLE                                                      \
   MC(INTR)                                                                   \
   MC(SEMAWAIT)                                                               \
   MC(SEMASIGNAL)                                                             \
   MC(SEMAFORCEWAKEUP)                                                        \
   MC(IPI)          /* Hit thread with IPI. */                                \
   MC(USERRETURN)   /* Return codes for user calls. */                        \
   MC(GET_RECYCLED_PAGES)                                                     \
   MC(RELEASE_ANON_PAGES)                                                     \
   MC(LOOKUP_MPN)                                                             \
   MC(COSCHED)                                                                \
   MC(ALLOC_VMX_PAGE)                                                         \
   MC(ALLOC_TMP_GDT)                                                          \
   MC(PIN_MPN)                                                                \
   MC(VMCLEAR_VMCS_ALL_CPUS)                                                  \
   MC(GET_PAGE_ROOT)

/*
 *----------------------------------------------------------------------
 *
 * ModuleCallType --
 *
 *      Enumeration of support calls done by the module.
 *
 *      If anything changes in the enum, please update kstatModuleCallPtrs
 *      for stats purposes.
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

/*
 * Define VMX86_UCCOST in the makefiles (Local.mk,
 * typically) if you want a special build whose only purpose
 * is to measure the overhead of a user call and its
 * breakdown.
 *
 * WINDOWS NOTE: I don't know how to pass VMX86_UCCOST to
 * the driver build on Windows.  It must be defined by hand.
 *
 * ESX Note: we don't have a crosspage in which to store these
 * timestamps.  Such a feature would perhaps be nice (if we
 * ever tire of the argument that esx does so few usercalls
 * that speed doesn't matter).
 */

#if defined(VMX86_UCCOST) && !defined(VMX86_SERVER)
#define UCTIMESTAMP(cp, stamp) \
             do { (cp)->ucTimeStamps[UCCOST_ ## stamp] = RDTSC(); } while (0)
#else
#define UCTIMESTAMP(cp, stamp)
#endif

#ifdef VMX86_SERVER
typedef struct UCCostResults {
   uint32 vmksti;
   uint32 vmkcli;
   uint32 ucnop;
} UCCostResults;
#else

typedef struct UCCostResults {
   uint32 htom;
   uint32 mtoh;
   uint32 ucnop;
} UCCostResults;

typedef enum UCCostStamp {
#define UC(x) UCCOST_ ## x,
#include "uccostTable.h"
   UCCOST_MAX
} UCCostStamp;
#endif // VMX86_SERVER

typedef
#include "vmware_pack_begin.h"
struct CodeOffsets {
   uint16 hostToVmm;
   uint16 vmmToHost;
}
#include "vmware_pack_end.h"
CodeOffsets;

#define SHADOW_DR(cpData, n)    (cpData)->shadowDR[n].ureg64


/*----------------------------------------------------------------------
 *
 * MAX_SWITCH_PT_PATCHES
 *
 *   This is the maximum number of patches that must be placed into
 *   the monitor page tables so that two pages of the host GDT and the
 *   crosspage can be accessed during worldswitch.
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
typedef
#include "vmware_pack_begin.h"
struct VMMPageTablePatch {
#define PTP_EMPTY    (0U) /* Unused array entry. (must be 0) */
#define PTP_LEVEL_L1 (1U)
#define PTP_LEVEL_L2 (2U)
#define PTP_LEVEL_L3 (3U)
#define PTP_LEVEL_L4 (4U)       /* root level */
   uint32   level;              /* [0, 4]  (maximal size: 3 bits) */
   uint32   pteIdx;             /* Index of the PTE in the page table. */
   uint64   pteGlobalIdx;       /* Global index of the PTE in 'level'. */
   VM_PDPTE pte;                /* PTE.                                */
}
#include "vmware_pack_end.h"
VMMPageTablePatch;

#define MODULECALL_NUM_ARGS  4

/*
 *----------------------------------------------------------------------
 *
 * VMCrossPageData --
 *
 *      Data which is stored on the VMCrossPage.
 *
 *----------------------------------------------------------------------
 */
typedef
#include "vmware_pack_begin.h"
struct VMCrossPageData {
   /*
    * A tiny stack upon which interrupt and exception handlers in the switch
    * path temporarily run.  Keep the end 16-byte aligned.  This stack must
    * be large enough for the sum of:
    *
    * - 1 #DB exception frame (5 * uint64)
    * - 1 #NMI exception frame (5 * uint64)
    * - 1 #MCE exception frame (5 * uint64)
    * - the largest stack use instantaneously possible by #MCE handling code
    * - the largest stack use instantaneously possible by #NMI handling code
    * - the largest stack use instantaneously possible by #DB handling code
    * - one high-water uint32 used to detect stack overflows when debugging
    * - remaining pad bytes to align to 16 bytes
    *
    * 184 bytes is slightly more than enough as of 2015/03/17 -- fjacobs.
    */
   uint32   tinyStack[46];

   uint64   hostCR3;
   uint32   crosspageMA;

   uint8    hostDRSaved;        // Host DR spilled to hostDR[x].
   uint8    hostDRInHW;         // 0 -> shadowDR in h/w, 1 -> hostDR in h/w.
                                //   contains host-sized DB,NMI,MCE entries
   uint16   hostSS;
   uint64   hostRSP;
   uint64   hostDR[8];
   uint64   hostRBX;
   uint64   hostRSI;
   uint64   hostRDI;
   uint64   hostRBP;
   uint64   hostR12;
   uint64   hostR13;
   uint64   hostR14;
   uint64   hostR15;
   LA64     hostCrossPageLA;   // where host has crosspage mapped
   uint16   hostInitial64CS;
   uint16   _pad0[3];

   uint64   wsCR0;
   uint64   wsCR4;

   DTR64    crossGDTHKLADesc;   // always uses host kernel linear address
   uint16   _pad1[3];
   DTR64    mon64GDTR;
   uint16   mon64ES;
   uint16   mon64SS;
   uint16   mon64DS;
   uint64   mon64CR3;
   uint64   mon64RBX;
   uint64   mon64RSP;
   uint64   mon64RBP;
   uint64   mon64RSI;
   uint64   mon64RDI;
   uint64   mon64R12;
   uint64   mon64R13;
   uint64   mon64R14;
   uint64   mon64R15;
   uint64   mon64RIP;
   Task64   monTask64;          /* vmm64's task */

   VMMPageTablePatch vmmPTP[MAX_SWITCH_PT_PATCHES]; /* page table patch */
   LA64              vmm64CrossPageLA;
   LA64              vmm64CrossGDTLA;   // where crossGDT mapped by PT patch
                                        //  64-bit host: host kernel linear
                                        // address

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

   VCPUSet        yieldVCPUs;

#if !defined(VMX86_SERVER)
   uint64 ucTimeStamps[UCCOST_MAX];
#endif

   /*
    * The values in the shadow debug registers must match those in the
    * hardware debug register immediately after a task switch in
    * either direction.  They are used to minimize moves to and from
    * the debug registers.
    */
   SharedUReg64     shadowDR[8];
   uint8            shadowDRInHW; // bit n set iff %DRn == shadowDR[n]

   SwitchedMSRState switchedMSRState;
   uint8            _pad2[7];

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
   uint8    _pad3[6];

   DTR64    switchHostIDTR;   // baseLA = switchHostIDT's host kernel LA
   uint16   _pad4[3];
   DTR64    switchMonIDTR;    // baseLA = switchMonIDT's monitor LA
   uint16   _pad5[3];

   /*
    * Descriptors and interrupt tables for switchNMI handlers.  Each
    * IDT has only enough space for the hardware exceptions; they are
    * sized to accommodate 64-bit descriptors.
    */
   uint8 switchHostIDT[sizeof(Gate64) * NUM_EXCEPTIONS]; // hostCS:hostVA
   uint8 switchMonIDT[sizeof(Gate64) * NUM_EXCEPTIONS];  // monCS:monVA

   volatile Bool wsException[NUM_EXCEPTIONS]; // Tracks faults in worldswitch.
   uint64        wsUD2;                       // IP of ud2 instr or 0 if unset.
}
#include "vmware_pack_end.h"
VMCrossPageData;


/*
 *----------------------------------------------------------------------
 *
 * VMCrossPageCode --
 *
 *      Code which is stored on the VMCrossPage.
 *
 *----------------------------------------------------------------------
 */
typedef
#include "vmware_pack_begin.h"
struct VMCrossPageCode {
   uint16        size;           // Size of the code module.
   uint32        vmmonVersion;   // VMMON_VERSION
   CodeOffsets   offsets;        // Offsets to handlers.
   uint8         codeBlock[512]; // Code for worldswitch and fault handling.
}
#include "vmware_pack_end.h"
VMCrossPageCode;


/*
 *----------------------------------------------------------------------
 *
 * VMCrossPage --
 *
 *      Data structure shared between the monitor and the module
 *      that is used for crossing between the two.
 *      Accessible as vm->cross (kernel module) and CROSS_PAGE
 *      (monitor)
 *
 *      Exactly one page long
 *
 *----------------------------------------------------------------------
 */

typedef
#include "vmware_pack_begin.h"
struct VMCrossPage {
   uint32          version;         /* 4 bytes. Must be at offset zero. */
   uint32          crosspage_size;  /* 4 bytes. Must be at offset 4.    */
   VMCrossPageData crosspageData;
   VMCrossPageCode crosspageCode;
   uint8           _pad[PAGE_SIZE - (sizeof(uint32) /* version */        +
                                     sizeof(uint32) /* crosspage_size */ +
                                     sizeof(VMCrossPageData)             +
                                     sizeof(VMCrossPageCode))];
}
#include "vmware_pack_end.h"
VMCrossPage;

#define CROSSPAGE_VERSION_BASE 0xbfb /* increment by 1 */
#define CROSSPAGE_VERSION    ((CROSSPAGE_VERSION_BASE << 1) + WS_INTR_STRESS)

#if !defined(VMX86_SERVER) && defined(VMM)
#define CROSS_PAGE  ((VMCrossPage * const) VPN_2_VA(CROSS_PAGE_START))
#define VMM_SWITCH_SHARED_DATA ((VMCrossPageData *)&CROSS_PAGE->crosspageData)
#endif

#define NULLPAGE_LINEAR_START  (MONITOR_LINEAR_START + \
                                PAGE_SIZE * CPL0_GUARD_PAGE_START)

#define MX_WAITINTERRUPTED     3
#define MX_WAITTIMEDOUT        2
#define MX_WAITNORMAL          1  // Must equal one; see linux module code.
#define MX_WAITERROR           0  // Use MX_ISWAITERROR() to test for error.

// Any zero or negative value denotes error.
#define MX_ISWAITERROR(e)      ((e) <= MX_WAITERROR)
#endif
