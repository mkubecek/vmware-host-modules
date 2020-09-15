/*********************************************************
 * Copyright (C) 2018-2020 VMware, Inc. All rights reserved.
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
#include "x86types.h"
#include "x86segdescrs.h"
#include "x86sel.h"
#include "addrlayout.h"


/*
 * Segment map of the monitor.
 *
 * The GDT and Task State Segment reside consecutively on one page.  The
 * monitor segments are placed at the end of the GDT.  The vmkernel can use all
 * lower-numbered segments for user-mode as well as higher-numbered segments
 * (though the vmkernel should not use monitor-private segments).  The high
 * segment placement ensures that there is no selector-overlap with hosted
 * kernel segments (the hosted world switch code can be a bit faster then, as
 * it can use a single cross GDT).
 */

#define VMMON_GDT_SIZE        (sizeof(Descriptor) * NUM_VALID_SEGMENTS)
#define VMMON_GDT_LIMIT       (VMMON_GDT_SIZE - 1)

#define IRB_SIZE              32 /* Interrupt redirection bitmap. */
#define TSS_SIZE              (sizeof(Task64) + IRB_SIZE)

#define GDT_START_VA          (VPN_2_VA(GDT_AND_TASK_START))
#define TASK_START_VA         (VPN_2_VA(GDT_AND_TASK_START) + VMMON_GDT_SIZE)

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

#define FIRST_USER_SEGMENT    NUM_BOOT_SEGMENTS
#define FIRST_SYSTEM_SEGMENT  (PAGE_SIZE / sizeof(Descriptor) - \
                               NUM_SYSTEM_SEGMENTS            - \
                               NUM_TASK_SEGMENTS * 2          - \
                               TSS_SIZE / sizeof(Descriptor))

#define GDT_USER_TLS_MIN      USER_TLS_1_SEGMENT
#define GDT_USER_TLS_MAX      USER_TLS_3_SEGMENT
#define USER_TLS_COUNT        ((USER_TLS_3_SEGMENT - USER_TLS_1_SEGMENT) + 1)

#define FOREACH_USER_TLS_INDEX(_i)              \
   {                                            \
      unsigned _i;                              \
      for (_i = 0; _i < USER_TLS_COUNT; _i++) { \

#define FOREACH_USER_TLS_INDEX_DONE             \
      }                                         \
   }

#define NULL_LDTR             0

/*
 * The descriptor after SYSTEM_CODE_SEGMENT (loaded into %cs) must be
 * appropriate for %ss because of the syscall instruction for 64-bit
 * user worlds.  Thus SYSTEM_DATA_SEGMENT is directly after it.
 */
typedef enum VmwSegs {
   NULL_SEGMENT             = 0,
   /* (... reserved for host operating system or vmkBoot segments). */

   USER32_CODE_SEGMENT      = FIRST_USER_SEGMENT,
   USER_DATA_SEGMENT,
   USER64_SYSRET_SEGMENT,
   USER64_STACK_SEGMENT,
   USER64_CODE_SEGMENT,
   USER_TLS_1_SEGMENT,
   USER_TLS_2_SEGMENT,
   USER_TLS_3_SEGMENT,

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
#define SYSTEM_NULL_SELECTOR    GDT_SYSTEM_SEL(NULL)
#define USER32_CODE_SELECTOR    GDT_USER_SEL_UNCHECKED(USER32_CODE)
#define USER_DATA_SELECTOR      GDT_USER_SEL_UNCHECKED(USER_DATA)
#define USER64_CODE_SELECTOR    GDT_USER_SEL_UNCHECKED(USER64_CODE)
#define USER64_SYSRET_SELECTOR  GDT_USER_SEL(USER64_SYSRET)
#define SYSTEM_CODE_SELECTOR    GDT_SYSTEM_SEL_UNCHECKED(SYSTEM_CODE)
#define SYSTEM_DATA_SELECTOR    GDT_SYSTEM_SEL_UNCHECKED(SYSTEM_DATA)
#define MONITOR_TASK_SELECTOR   GDT_SYSTEM_SEL(MONITOR_TASK)
#define VMKERNEL_TASK_SELECTOR  GDT_SYSTEM_SEL(VMKERNEL_TASK)

/*
 * The VMM GDT is comprised of many segment descriptors with one initial
 * Task State Segment system descriptor.  The VMM Task State Segment is
 * on the same page sequentially after its GDT.
 */
#pragma pack(push, 1)
typedef struct StaticGDTAndTSS {
   Descriptor   empty[NUM_BOOT_SEGMENTS + NUM_USER_SEGMENTS];
   Descriptor   systemSegs[NUM_SYSTEM_SEGMENTS];
   Descriptor64 vmkTask;
   Descriptor64 monTask;
   Task64       monTSS;
   uint8        TSSIRBitmap[IRB_SIZE];
} StaticGDTAndTSS;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VmkernelGDT {
   Descriptor   bootSegs[NUM_BOOT_SEGMENTS];
   Descriptor   userSegs[NUM_USER_SEGMENTS];
   Descriptor   systemSegs[NUM_SYSTEM_SEGMENTS]; /* VMM/VMK-shared. */
   Descriptor64 vmkTask;
   Descriptor64 monTask;
} VmkernelGDT;
#pragma pack(pop)

MY_ASSERTS(segs,
           ASSERT_ON_COMPILE(SYSTEM_CODE_SEGMENT + 1 == SYSTEM_DATA_SEGMENT);
           ASSERT_ON_COMPILE(AFTER_LAST_USER_SEGMENT - FIRST_USER_SEGMENT <=
                             NUM_USER_SEGMENTS);
           ASSERT_ON_COMPILE(AFTER_LAST_USER_SEGMENT <= FIRST_SYSTEM_SEGMENT);
)

#endif /* _SEGS_H_ */
