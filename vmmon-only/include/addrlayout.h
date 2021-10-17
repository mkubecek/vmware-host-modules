/*********************************************************
 * Copyright (C) 2007-2020 VMware, Inc. All rights reserved.
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

#ifndef _ADDRLAYOUT_H_
#define _ADDRLAYOUT_H_

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMX
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "vm_assert.h"
#include "address_defs.h"
#include "vmm_constants.h"

#define DIRECT_EXEC_USER_RPL    3
#define BINARY_TRANSLATION_RPL  1
typedef char x86_PAGE[4096];

#define MAX_VADDR                    CONST64U(0xffffffffffffffff)
#define MONITOR_SIZE                 (64 * 1024 * 1024)
#define MONITOR_LINEAR_START         (MAX_VADDR - MONITOR_SIZE + 1)
#define MONITOR_LINEAR_END           (MAX_VADDR)
#define NUM_MONITOR_PAGES            (MONITOR_SIZE / PAGE_SIZE)
#define MONITOR_BASE_VPN             (MONITOR_LINEAR_START >> PAGE_SHIFT)

#define MONITOR_AS_START             MONITOR_BASE_VPN
#define MONITOR_AS_LEN               NUM_MONITOR_PAGES

/*
 * Regions and items necessarily overlap.  Build parallel structures for each.
 * These 'field' structures are used to determine page numbers and offsets.
 * AddrLayout contains zero-length regions used for alignment-checking.  (These
 * are used by gcc as Windows prohibits zero-length arrays.)
 */

/* Item layout. */
typedef struct {
#if defined __GNUC__
#define REGION(name, length) x86_PAGE name##_REGION_MARKER_FIELD[0];
#else
#define REGION(name, length)
#endif
#define ITEM(name, length) x86_PAGE name##_FIELD[length];
#include "addrlayout_table.h"
} AddrLayout;
#undef REGION
#undef ITEM
#define FIELD_PAGE_NUMBER(name) PAGE_NUMBER(&(((AddrLayout *)0)->name##_FIELD))


/* Region layout. */
typedef struct {
#define REGION(name, length) x86_PAGE name##_REGION_FIELD[length];
#define ITEM(name, length)
#include "addrlayout_table.h"
} RegionAddrLayout;
#undef REGION
#undef ITEM
#define REGION_FIELD_PAGE_NUMBER(name) \
   PAGE_NUMBER(&(((RegionAddrLayout *)0)->name##_REGION_FIELD))

#if defined __GNUC__
/*
 * The sum of sizes of items in a region must add up to the region's size.
 * AddrLayout contains zero-sized region markers at their correct offsets.
 * RegionAddrLayout contains full-sized regions (but no items).  Comparing the
 * marker and region offsets ensures that all items sizes in the previous
 * region sum to the size of the previous region.  To check the final region
 * (as it has no successor) we assert the sizes of the tables match.
 *
 * We also assert 2MB region granularity.
 *
 * This checking is only performed when using gcc.
 */
#define REGION(name, length) static INLINE void name(void) { \
   ASSERT_ON_COMPILE(FIELD_PAGE_NUMBER(name##_REGION_MARKER) == \
                     REGION_FIELD_PAGE_NUMBER(name)); \
   ASSERT_ON_COMPILE((length % 512) == 0); \
   ASSERT_ON_COMPILE(sizeof(AddrLayout) == sizeof(RegionAddrLayout)); \
   ASSERT_ON_COMPILE(sizeof(RegionAddrLayout) == MONITOR_SIZE); \
}
#define ITEM(name, length)
#include "addrlayout_table.h"
#undef REGION
#undef ITEM
#endif

/*
 * Sizes.
 */

enum {
#define REGION(name, length) name##_REGION_LEN = length,
#define ITEM(name, length) name##_LEN = length,
#include "addrlayout_table.h"
};
#undef REGION
#undef ITEM

/*
 * Starts.
 */

#if defined __GNUC__
/* Windows compiler and gdb do not accept 64-bit enumerations, but gcc
 * does.  These are used by geninfo.c.
 */
enum {
#define REGION(name, length) _asm_##name##_REGION_START = \
    0ULL + MONITOR_BASE_VPN + REGION_FIELD_PAGE_NUMBER(name),
#define ITEM(name, length) _asm_##name##_START = \
    0ULL + MONITOR_BASE_VPN + FIELD_PAGE_NUMBER(name),
#include "addrlayout_table.h"
};
#undef REGION
#undef ITEM

enum {
#define REGION(name, length) name##_REGION_START = \
   MONITOR_BASE_VPN + REGION_FIELD_PAGE_NUMBER(name),
#define ITEM(name, length) name##_START = \
   MONITOR_BASE_VPN + FIELD_PAGE_NUMBER(name),
#include "addrlayout_table.h"
};
#undef REGION
#undef ITEM

#else
#define REGION(name, length) \
   static const uint64 name##_REGION_START = \
   MONITOR_BASE_VPN + REGION_FIELD_PAGE_NUMBER(name);
#define ITEM(name, length) \
   static const uint64 name##_START = \
   MONITOR_BASE_VPN + FIELD_PAGE_NUMBER(name);
#include "addrlayout_table.h"
#undef REGION
#undef ITEM
#endif

enum {
#define REGION(name, length) name##_REGION_start_page = \
   REGION_FIELD_PAGE_NUMBER(name),
#define ITEM(name, length) name##_start_page = \
   FIELD_PAGE_NUMBER(name),
#include "addrlayout_table.h"
};
#undef REGION
#undef ITEM


/* Derive the bootstrap start address from table contents. */
#define MONITOR_BOOTSTRAP_START_LA   VPN_2_VA(BS_TXT_START)

#ifndef VMX86_SERVER
#define SWITCH_PAGE_TABLE_LEN          12
#endif

#define VMM_STKTOP_LRET_LEN        sizeof(LretFrame64)
#define VMM_STKTOP_LRET_OFFSET     (PAGE_SIZE - VMM_STKTOP_LRET_LEN)

#define MON_STACK_BASE             VPN_2_VA(MON_STACK_PAGES_START)
#define MON_STACK_TOP              VPN_2_VA(MON_STACK_PAGES_START + \
                                            MON_STACK_PAGES_LEN)
#define DF_STACK_BASE              VPN_2_VA(DF_STACK_PAGES_START)
#define DF_STACK_TOP               VPN_2_VA(DF_STACK_PAGES_START + \
                                            DF_STACK_PAGES_LEN)
#define MC_STACK_BASE              VPN_2_VA(MC_STACK_PAGES_START)
#define MC_STACK_TOP               VPN_2_VA(MC_STACK_PAGES_START + \
                                            MC_STACK_PAGES_LEN)
#define NMI_STACK_BASE             VPN_2_VA(NMI_STACK_PAGES_START)
#define NMI_STACK_TOP              VPN_2_VA(NMI_STACK_PAGES_START + \
                                            NMI_STACK_PAGES_LEN)

#define NUM_MON_PTABS   MON_PAGE_TABLE_L1_LEN

#define MON_START_L5OFF PT_LA_2_L5OFF(MONITOR_LINEAR_START)
#define MON_START_L4OFF PT_LA_2_L4OFF(MONITOR_LINEAR_START)
#define MON_START_L3OFF PT_LA_2_L3OFF(MONITOR_LINEAR_START)
#define MON_START_L2OFF PT_LA_2_L2OFF(MONITOR_LINEAR_START)

/*
 * Allocated wired pages for the monitor.
 * Currently we allocate wired pages for the:
 *  0) The shared area between user and monitor;
 *  a) GDT of the monitor
 *  a1) BT (CP1) stack
 *  b) The monitor's page table with one page directory under 4 GB
 *      and two page table pages.
 *  c) Pages for the monitor's stack.
 *  d) The BT area pages
 *  e) The cross driver/monitor page.
 *  f) The physical memory array for the machine.
 *  g) The SMRAM memory
 *
 * We allocate the shared area separately since it must be read/write
 * in the monitor.   See SharedArea_PowerOn().
 */

#define MON_BOOTSTRAP_PAGE_TABLE_PAGES (BOOTSTRAP_REGION_LEN     /  \
                                        PAE_PTES_PER_PGTBL)
#ifdef VMX86_SERVER
#define NUM_MONWIRED_PAGES     0

#define MON_PAGE_TABLE_ALLOC_PAGES 0
#define NUM_MONWIRED_BOOTSTRAP_PAGES 0

#define NUM_MONWIRED_NUMA_PAGES_ML (MON_STACK_PAGES_LEN  + \
                                    DF_STACK_PAGES_LEN   + \
                                    MC_STACK_PAGES_LEN   + \
                                    NMI_STACK_PAGES_LEN  + \
                                    HV_CURRENT_VMCB_LEN  + \
                                    MON_IDT_LEN          + \
                                    GDT_AND_TASK_LEN)

#else
#define NUM_MONWIRED_PAGES    (CROSS_PAGE_CODE_LEN   + \
                               CROSS_PAGE_DATA_LEN   + \
                               MON_PAGE_TABLE_L4_LEN + \
                               MON_PAGE_TABLE_L3_LEN + \
                               MON_PAGE_TABLE_L2_LEN)

#define MON_PAGE_TABLE_ALLOC_PAGES ((MONITOR_READONLY_REGION_LEN + \
                                     MONITOR_DATA_REGION_LEN     + \
                                     MONITOR_MISC_REGION_LEN)    / \
                                     PAE_PTES_PER_PGTBL)

#define NUM_MONWIRED_BOOTSTRAP_PAGES MON_BOOTSTRAP_PAGE_TABLE_PAGES

#define NUM_MONWIRED_NUMA_PAGES_ML (MON_STACK_PAGES_LEN + \
                                    DF_STACK_PAGES_LEN  + \
                                    MC_STACK_PAGES_LEN  + \
                                    NMI_STACK_PAGES_LEN + \
                                    GDT_AND_TASK_LEN    + \
                                    MON_IDT_LEN         + \
                                    HV_CURRENT_VMCB_LEN + \
                                    SWITCH_PAGE_TABLE_LEN)
#endif

#define MON_PAGE_TABLE_L5        VPN_2_VA(MON_PAGE_TABLE_L5_START)
#define MON_PAGE_TABLE_L4        VPN_2_VA(MON_PAGE_TABLE_L4_START)
#define VMM_LRET_STACK_TOP       (MON_STACK_TOP - VMM_STKTOP_LRET_LEN)
/*
 * Mark out a guard page for the VMM stack, this is not in the addrlayout_table
 * explicitly because it overlaps the last page of the MONITOR_READONLY region.
 */
#define VMM_STACK_GUARD_START    MON_STACK_PAGES_START
#define VMM_STACK_GUARD_LEN      PAGE_SIZE

/*
 * Following functions return true if the range [va, va+len) is within
 * range of a particular monitor stack.  (Don't adjust without
 * considering uint64 overflow when va is very high.)
 */
static INLINE Bool
AddrLayout_InMonStack(VA64 va, size_t len)
{
   return MON_STACK_BASE <= va && va <= MON_STACK_TOP - len;
}

static INLINE Bool
AddrLayout_InNMIStack(VA64 va, size_t len)
{
   return va >= NMI_STACK_BASE && va <= NMI_STACK_TOP - len;
}

static INLINE Bool
AddrLayout_InDFStack(VA64 va, size_t len)
{
   return va >= DF_STACK_BASE && va <= DF_STACK_TOP - len;
}

static INLINE Bool
AddrLayout_InMCStack(VA64 va, size_t len)
{
   return va >= MC_STACK_BASE && va <= MC_STACK_TOP - len;
}

static INLINE Bool
AddrLayout_InAMonitorStack(VA va, size_t len)
{
   return AddrLayout_InMonStack(va, len)  ||
          AddrLayout_InNMIStack(va, len)  ||
          AddrLayout_InDFStack(va, len)   ||
          AddrLayout_InMCStack(va, len);
}

#endif
