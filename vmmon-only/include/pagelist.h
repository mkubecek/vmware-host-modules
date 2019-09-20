/*********************************************************
 * Copyright (C) 2010-2014,2017 VMware, Inc. All rights reserved.
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
 * pagelist.h -- 
 *
 *      Definitions of operations on BPNs used in communicating page info
 *	between VMKernel/VMX and VMM.
 */

#ifndef	_PAGELIST_H
#define	_PAGELIST_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "vm_assert.h"
#include "vmcore_types.h"

#if defined __cplusplus
extern "C" {
#endif


/*
 * Sets of pages are passed between the monitor and the platform to be 
 * shared, invalidated, remapped, or swapped.
 *
 * A set is sized so that it fits in a 4KB page.
 */

#pragma pack(push, 1)
typedef struct PageListEntry {
   CompressedBPN cbpn;
   Bool voided;
   uint8 _pad[1];
} PageListEntry;
#pragma pack(pop) 

#define PAGELIST_MAX     (PAGE_SIZE / sizeof(PageListEntry))

static INLINE void
PageList_SetEntry(PageListEntry *ple, BPN bpn)
{
   CompressedBPN_Write(&ple->cbpn, bpn);
   ple->voided = FALSE;
}   

static INLINE BPN
PageList_BPN(const PageListEntry *ple)
{
   return CompressedBPN_Read(&ple->cbpn);
}

static INLINE Bool
PageList_IsVoid(const PageListEntry *ple)
{
   ASSERT(ple->voided == TRUE || ple->voided == FALSE);
   return ple->voided;
}

static INLINE void
PageList_VoidEntry(PageListEntry *ple)
{
   ple->voided = TRUE;
}


/*
 * This function inspects the set of BPN between entry [0,i) in the page list
 * and returns TRUE if any of them matches the provided BPN.
 */
static INLINE Bool
PageList_IsBPNDup(const PageListEntry *pageList, unsigned i, BPN bpn)
{
   unsigned k;
   for (k = 0; k < i; k++) {
      if (PageList_BPN(&pageList[k]) == bpn) {
         return TRUE;
      }
   }
   return FALSE;
}


#if defined __cplusplus
} // extern "C"
#endif

#endif // _PAGELIST_H
