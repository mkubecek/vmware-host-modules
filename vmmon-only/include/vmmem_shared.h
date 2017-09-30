/*********************************************************
 * Copyright (C) 2000-2015,2017 VMware, Inc. All rights reserved.
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
 * vmmem_shared.h --
 *
 *      This is the header file for machine memory manager.
 */


#ifndef _VMMEM_SHARED_H
#define _VMMEM_SHARED_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMX
#include "includeCheck.h"

/*
 * Page remapping definitions.
 */

#define VMMEM_FLAG_BIT(x) (1 << (x))

#define VMMEM_ANON_LOW_MEM        VMMEM_FLAG_BIT(0)
#define VMMEM_ANON_CONTIG         VMMEM_FLAG_BIT(1)
#define VMMEM_ANON_CAN_FAIL       VMMEM_FLAG_BIT(2)
#define VMMEM_ANON_USE_PREALLOC   VMMEM_FLAG_BIT(3)
#define VMMEM_ANON_IOABLE_PAGE    VMMEM_FLAG_BIT(4)
#define VMMEM_ANON_ALL_FLAGS      MASK(5)

#define VMMEM_GUEST_WRITEABLE     VMMEM_FLAG_BIT(0)
#define VMMEM_GUEST_BREAKCOW      VMMEM_FLAG_BIT(1)
#define VMMEM_GUEST_LARGE_PAGE    VMMEM_FLAG_BIT(2)
#define VMMEM_GUEST_CAN_FAIL      VMMEM_FLAG_BIT(3)
#define VMMEM_GUEST_TEST_ZEROCOW  VMMEM_FLAG_BIT(4)
#define VMMEM_GUEST_TRY_ZEROCOW   VMMEM_FLAG_BIT(5)
#define VMMEM_GUEST_TRY_POISONCOW VMMEM_FLAG_BIT(6)
#define VMMEM_GUEST_PREALLOC      VMMEM_FLAG_BIT(7)
#define VMMEM_GUEST_ALL_FLAGS     MASK(8)
#define VMMEM_GUEST_TRY_COW       (VMMEM_GUEST_TEST_ZEROCOW | \
                                   VMMEM_GUEST_TRY_ZEROCOW  | \
                                   VMMEM_GUEST_TRY_POISONCOW)

#define VMMEM_PLATFORM_CHECK_OK           VMMEM_FLAG_BIT(0)
#define VMMEM_PLATFORM_KEY_OK             VMMEM_FLAG_BIT(1)
#define VMMEM_PLATFORM_COW                VMMEM_FLAG_BIT(2)
#define VMMEM_PLATFORM_EXPOSED_TO_VMM     VMMEM_FLAG_BIT(3)
#define VMMEM_PLATFORM_P2M_UPDATE_PENDING VMMEM_FLAG_BIT(4)
#define VMMEM_PLATFORM_DIRTY              VMMEM_FLAG_BIT(5)
#define VMMEM_PLATFORM_IS_2M_PAGE         VMMEM_FLAG_BIT(6)
#define VMMEM_PLATFORM_IS_1G_PAGE         VMMEM_FLAG_BIT(7)
#define VMMEM_PLATFORM_LARGE_RETRY        VMMEM_FLAG_BIT(8)
#define VMMEM_PLATFORM_TRY_COW_SUCCESS    VMMEM_FLAG_BIT(9)

#define VMMEM_PLATFORM_BACKED_LARGE      (VMMEM_PLATFORM_IS_2M_PAGE |   \
                                          VMMEM_PLATFORM_IS_1G_PAGE)


#define MAX_PLATFORM_PAGE_INFO_PAGES  240

/*
 * Structure used to query platform about the page state.
 */
typedef struct PlatformPageInfoList {
   uint32 numPages;
   uint32 _pad;
   BPN    bpn[MAX_PLATFORM_PAGE_INFO_PAGES];    // bpns to check
   MPN    mpn[MAX_PLATFORM_PAGE_INFO_PAGES];    // filled in by host
   uint8  flags[MAX_PLATFORM_PAGE_INFO_PAGES];  // filled in by host
} PlatformPageInfoList;

#define VMMEM_SERVICES_TYPE_2_MASK(type)                  \
           (1 << type)
#define VMMEM_SERVICES_IN_MASK(typeMask, type)            \
           (VMMEM_SERVICES_TYPE_2_MASK(type) & typeMask)
#define VMMEM_SERVICES_CLEAR_MASK(typeMask, type)         \
           (typeMask & ~VMMEM_SERVICES_TYPE_2_MASK(type))

#define VMMEM_SERVICES_DEFS                               \
   MDEF(VMMEM_SERVICES_TYPE_P2M,  P2MUpdate_FilterPages)  \
   MDEF(VMMEM_SERVICES_TYPE_SWAP, BusMemSwap_FilterPages)

#define MDEF(_type, _cb) _type,
typedef enum VmMemServices_Type {
   VMMEM_SERVICES_DEFS
#undef MDEF
   VMMEM_SERVICES_TYPE_MAX
} VmMemServices_Type;
#define VMMEM_SERVICES_TYPE_INVALID (VMMEM_SERVICES_TYPE_MAX)

void VmMem_DisableLargePageAllocations(void);
void VmMem_EnableLargePageAllocations(void);

#endif
