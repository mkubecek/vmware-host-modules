/*********************************************************
 * Copyright (C) 2005-2019 VMware, Inc. All rights reserved.
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
 * monAddrLayout.h --
 *
 *    Address layout of the monitor.
 */

#ifndef _MON_ADDR_LAYOUT_H
#define _MON_ADDR_LAYOUT_H

#include "vm_pagetable.h"

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#pragma pack(push, 1)
typedef struct VMM64_AddrLayout {
   /*
    *All address are VPNs and all lengths are numPages
    */
   uint64 monBase;            // MONITOR_BASE_VPN
   uint64 monL5Start;         // MON_PAGE_TABLE_L5_START
   uint32 monL5Len;           // MON_PAGE_TABLE_L5_LEN
   uint64 monL4Start;         // MON_PAGE_TABLE_L4_START
   uint32 monL4Len;           // MON_PAGE_TABLE_L4_LEN
   uint64 monL3Start;         // MON_PAGE_TABLE_L3_START
   uint32 monL3Len;           // MON_PAGE_TABLE_L3_LEN
   uint64 monL2Start;         // MON_PAGE_TABLE_L2_START
   uint32 monL2Len;           // MON_PAGE_TABLE_L2_LEN
   uint64 monL1Start;         // MON_PAGE_TABLE_L1_START
   uint32 monL1Len;           // MON_PAGE_TABLE_L1_LEN
#ifndef VMX86_SERVER
   uint64 monCpStart;         // CROSS_PAGE_START
#endif
} VMM64_AddrLayout;
#pragma pack(pop)

#define VMM_MONAS_4LP_FIRST_L4OFF   1
#define VMM_MONAS_4LP_LAST_L4OFF  130

#define VMM_MONAS_5LP_FIRST_L5OFF   1
#define VMM_MONAS_5LP_LAST_L5OFF   34

static INLINE PT_Level
MonAS_GetPagingLevel(void)
{
   return PT_LEVEL_4;
}

static INLINE Bool
MonAS_Uses5LevelPaging(void)
{
   return MonAS_GetPagingLevel() == PT_LEVEL_5;
}
#endif
