/*********************************************************
 * Copyright (C) 2018-2023 VMware, Inc. All rights reserved.
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
 * segs.h --
 *
 *      Describes segment descriptors and structures containing them for both
 *      the monitor (in hosted and ESX) and the vmkernel.
 */

#ifndef _SEGS_H_
#define _SEGS_H_

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "x86/cpu_types_arch.h"
#include "x86segdescrs.h"
#include "x86sel.h"
#include "addrlayout.h"
#include "vm_atomic.h"

/*
 * For each pcpu, a per-pcpu data area, the GDT, and the Task State
 * Segment reside consecutively on a page.
 */

#define PCPU_DATA_SIZE        (32 * CACHELINE_SIZE)

#define VMMON_GDT_SIZE        (sizeof(Descriptor) * NUM_VALID_SEGMENTS)
#define VMMON_GDT_LIMIT       (VMMON_GDT_SIZE - 1)

/*
 * This is one more than max GDT limit value.
 */
#define VMK_GDT_SIZE          0x10000

/*
 * In the vmkernel the GDTR limit is set to the maximum because this will help
 * the ULM/KLM to minimize the cost of a vmexit. After a vmexit the KLM doesn't
 * need to restore the GDTR if the host GDTR limit is set to the maximum
 * as Intel's VT restores the GDTR but sets the limit to the maximum value.
 * Please see details 27.5.2 Loading Host Segment and Descriptor-Table
 * Registers from the Intel manual.
 */
#define VMK_GDT_LIMIT         (VMK_GDT_SIZE - 1)

#define IRB_SIZE              32 /* Interrupt redirection bitmap. */
#define TSS_SIZE              (sizeof(Task64) + IRB_SIZE)

#define PCPU_DATA_VA          (VPN_2_VA(GDT_AND_TASK_START))
#define TASK_START_VA         (PCPU_DATA_VA + PCPU_DATA_SIZE)
#define GDT_START_VA          (TASK_START_VA + TSS_SIZE)

/*
 * vmkBoot uses some of the lower-numbered segments, as do host kernels on
 * hosted.  User segments could start earlier than 32; see bug #1904257.
 * Descriptor is used to describe boot, user and kernel segments.
 * Descriptor64 (twice as large) is used for task segments.
 */
#define NUM_BOOT_SEGMENTS     32
#define NUM_USER_SEGMENTS     (FIRST_SYSTEM_SEGMENT - FIRST_USER_SEGMENT)
#define NUM_SYSTEM_SEGMENTS   2
#define NUM_TASK_SEGMENTS     2

#define NUM_TOTAL_SEGMENTS    ((VMK_GDT_SIZE) / sizeof(Descriptor))
#define NUM_MAP_SEGMENTS      (NUM_BOOT_SEGMENTS + NUM_USER_SEGMENTS + \
                               NUM_SYSTEM_SEGMENTS +                   \
                               (2 * (sizeof(Descriptor64) /            \
                                sizeof(Descriptor))))
#define NUM_PAD_SEGMENTS      (NUM_TOTAL_SEGMENTS - NUM_MAP_SEGMENTS)

#define FIRST_USER_SEGMENT    NUM_BOOT_SEGMENTS
#define FIRST_SYSTEM_SEGMENT  (PAGE_SIZE / sizeof(Descriptor) - \
                               NUM_SYSTEM_SEGMENTS            - \
                               NUM_TASK_SEGMENTS * 2          - \
                               TSS_SIZE / sizeof(Descriptor)  - \
                               PCPU_DATA_SIZE / sizeof(Descriptor))

#define NULL_LDTR             0

/*
 * The vmkernel can use all lower-numbered segments for user-mode as
 * well as higher-numbered segments, though the vmkernel should not
 * use monitor-private segments.
 *
 * The monitor segments are placed at the end of the GDT.  The high
 * segment placement for the monitor ensures that there is no
 * selector-overlap with hosted kernel segments; the hosted world
 * switch code can be a bit faster then, as it can use a single cross
 * GDT.
 */
typedef enum VmwSegs {
   NULL_SEGMENT             = 0,
   /* (... reserved for host operating system or vmkBoot segments). */

   USER_DATA_STACK_SEGMENT  = FIRST_USER_SEGMENT,
   USER_CODE_SEGMENT,

   AFTER_LAST_USER_SEGMENT,

   SYSTEM_CODE_SEGMENT      = FIRST_SYSTEM_SEGMENT,
   SYSTEM_DATA_SEGMENT,
   VMKERNEL_TASK_SEGMENT,
   VMKERNEL_TASK_SEGMENT_HI,
   MONITOR_TASK_SEGMENT,
   MONITOR_TASK_SEGMENT_HI,

   NUM_VALID_SEGMENTS
} VmwSegs;

#define GDT_SYSTEM_SEL(x) MAKE_SELECTOR(x##_SEGMENT, SELECTOR_GDT, 0)
#define GDT_SYSTEM_SEL_UNCHECKED(x) \
   MAKE_SELECTOR_UNCHECKED(x##_SEGMENT, SELECTOR_GDT, 0)
#define GDT_USER_SEL(x) MAKE_SELECTOR(x##_SEGMENT, SELECTOR_GDT, 3)
#define GDT_USER_SEL_UNCHECKED(x) \
   MAKE_SELECTOR_UNCHECKED(x##_SEGMENT, SELECTOR_GDT, 3)

/* Selectors used statically in code or in assembly must be unchecked. */
#define SYSTEM_NULL_SELECTOR     GDT_SYSTEM_SEL(NULL)
#ifdef VMKERNEL
#define USER_CODE_SELECTOR       GDT_USER_SEL_UNCHECKED(USER_CODE)
#define USER_DATA_STACK_SELECTOR GDT_USER_SEL_UNCHECKED(USER_DATA_STACK)
#endif
#define SYSTEM_CODE_SELECTOR     GDT_SYSTEM_SEL_UNCHECKED(SYSTEM_CODE)
#define SYSTEM_DATA_SELECTOR     GDT_SYSTEM_SEL_UNCHECKED(SYSTEM_DATA)
#define MONITOR_TASK_SELECTOR    GDT_SYSTEM_SEL(MONITOR_TASK)
#define VMKERNEL_TASK_SELECTOR   GDT_SYSTEM_SEL(VMKERNEL_TASK)

/*
 * This struct is shared between the vmkernel and the monitor. Since
 * the vmm and vmk always run as a matched set, the layout can be
 * changed down the line as needed.
 */
#pragma pack(push, 1)
typedef struct PcpuData {
   Bool         inVMM;       /* TRUE iff vmm world running in vmm context. */

   /*
    * nmiIPIFromVMkernel is set when vmkernel detects a CPU
    * lockup inside any of the worlds. This done before
    * vmkernel sends an NMI IPI to the CPU.
    */
   Atomic_Bool  nmiIPIFromVMkernel;
   uint8        _unused[PCPU_DATA_SIZE - sizeof(Atomic_Bool) - sizeof(Bool)];
} PcpuData;
#pragma pack(pop)

/*
 * The VMM GDT is comprised of many segment descriptors with one initial
 * Task State Segment system descriptor.  The VMM Task State Segment is
 * on the same page just before GDT start VA. The base address of the
 * GDT in GDTR register is set to address of empty descriptor.
 */
#pragma pack(push, 1)
typedef struct StaticGDTPage {
   PcpuData     pcpuData;                        /* Non-architectural. */
   Task64       monTSS;
   uint8        TSSIRBitmap[IRB_SIZE];
   Descriptor   empty[NUM_BOOT_SEGMENTS + NUM_USER_SEGMENTS];
   Descriptor   systemSegs[NUM_SYSTEM_SEGMENTS];
   Descriptor64 vmkTask;
   Descriptor64 monTask;
} StaticGDTPage;
#pragma pack(pop)

/*
 * The base address of the GDT in GDTR register is set to VmkernelGDT.
 * The size of VmkernelGDT is 64K with 239 mapped entries and 7953
 * pad entries (8 bytes per entry). We only reserve VA space for 7953
 * pad entries and not map them in PTE. This is done so that we could
 * set GDTR limit to maximum value (that is 64K - 1). Please see
 * bora/main/doc/gdtLim.txt for more details.
 */
#pragma pack(push, 1)
typedef struct VmkernelGDT {
   Descriptor   bootSegs[NUM_BOOT_SEGMENTS];
   Descriptor   userSegs[NUM_USER_SEGMENTS];
   Descriptor   systemSegs[NUM_SYSTEM_SEGMENTS]; /* VMM/VMK-shared. */
   Descriptor64 vmkTask;
   Descriptor64 monTask;
   Descriptor   padSegs[NUM_PAD_SEGMENTS];
} VmkernelGDT;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VmkernelGDTInfo {
   PcpuData     pcpuData;                        /* Non-architectural */
   Task64       vmkTSS;
   uint8        TSSIRBitmap[IRB_SIZE];
   VmkernelGDT  vmkGDT;
} VmkernelGDTInfo;
#pragma pack(pop)

MY_ASSERTS(segs,
           ASSERT_ON_COMPILE(SYSTEM_CODE_SEGMENT + 1 == SYSTEM_DATA_SEGMENT);
           ASSERT_ON_COMPILE(AFTER_LAST_USER_SEGMENT - FIRST_USER_SEGMENT <=
                             NUM_USER_SEGMENTS);
           ASSERT_ON_COMPILE(AFTER_LAST_USER_SEGMENT <= FIRST_SYSTEM_SEGMENT);
)

/*
 * Invariants:
 * 1) PcpuData, task and GDT mapped segments resides in the same page.
 *    VMM maps this 4KB page and refers it to locate task and GDT segments.
 *    HTSched maps this 4KB page of each host PCPU's GDT.
 * 2) GDT_START_VA should point to the mapped segments in VmkernelGDTInfo
 *    struct and also it should point to mapped segments in StaticGDTPage.
 *    The offset of mapped segments in VmkernelGDTInfo and StaticGDTPage
 *    should remain same.
 * 3) The key data structures like systemSegs, vmkTask and monTask in
 *    the StaticGDTPage and VmkernelGDTInfo structs align.
 */
MY_ASSERTS(pcpuData,
           ASSERT_ON_COMPILE(sizeof(PcpuData) == PCPU_DATA_SIZE);
           ASSERT_ON_COMPILE(offsetof(VmkernelGDTInfo, vmkGDT) ==
                             PCPU_DATA_SIZE + TSS_SIZE);
           ASSERT_ON_COMPILE(sizeof(VmkernelGDTInfo) == PCPU_DATA_SIZE +
                             TSS_SIZE + VMK_GDT_SIZE);
           ASSERT_ON_COMPILE((PCPU_DATA_SIZE + TSS_SIZE +
                              sizeof(Descriptor) * NUM_MAP_SEGMENTS) ==
                              PAGE_SIZE);
           ASSERT_ON_COMPILE(sizeof(VmkernelGDTInfo) -
                             sizeof(Descriptor) * NUM_PAD_SEGMENTS ==
                             PAGE_SIZE);
           ASSERT_ON_COMPILE(sizeof(StaticGDTPage) == PAGE_SIZE);
           ASSERT_ON_COMPILE((PCPU_DATA_SIZE + TSS_SIZE +
                              offsetof(VmkernelGDT, systemSegs)) ==
                              offsetof(StaticGDTPage, systemSegs));
           ASSERT_ON_COMPILE((PCPU_DATA_SIZE + TSS_SIZE +
                              offsetof(VmkernelGDT, vmkTask)) ==
                              offsetof(StaticGDTPage, vmkTask));
           ASSERT_ON_COMPILE((PCPU_DATA_SIZE + TSS_SIZE +
                              offsetof(VmkernelGDT, monTask)) ==
                              offsetof(StaticGDTPage, monTask));
           ASSERT_ON_COMPILE(offsetof(VmkernelGDTInfo, vmkTSS) ==
                             offsetof(StaticGDTPage, monTSS));
)

#endif /* _SEGS_H_ */
