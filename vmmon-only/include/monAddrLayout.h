/*********************************************************
 * Copyright (C) 2005,2007,2009,2013,2015 VMware, Inc. All rights reserved.
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
 *    Address layout of the monitor
 */

#ifndef _MON_ADDR_LAYOUT_H
#define _MON_ADDR_LAYOUT_H

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
   uint64 mmuRootStart;       // MMU_ROOT_START
   uint32 mmuRootLen;         // MMU_ROOT_LEN
   uint64 mmuL3Start;         // MMU_L3_START
   uint32 mmuL3Len;           // MMU_L3_LEN
   uint64 mmuL2Start;         // MMU_L2_START
   uint32 mmuL2Len;           // MMU_L2_LEN
   uint64 monPageTableStart;  // MON_PAGE_TABLE_START
   uint32 monPageTableLen;    // MON_PAGE_TABLE_LEN
#ifndef VMX86_SERVER
   uint64 monCpStart;         // CROSS_PAGE_START
#endif
} VMM64_AddrLayout;
#pragma pack(pop)

#define VMM_SCRATCHAS_FIRST_L4OFF   1
#define VMM_SCRATCHAS_LAST_L4OFF  127

#endif
