/*********************************************************
 * Copyright (c) 2002-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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

#ifndef __PGTBL_H__
#   define __PGTBL_H__


#include <linux/highmem.h>

#include "compat_pgtable.h"
#include "compat_spinlock.h"
#include "compat_page.h"
#include "compat_version.h"

/*
 *-----------------------------------------------------------------------------
 *
 * UserVa2MPN --
 *
 *    Walks through the hardware page tables of the current process to try to
 *    find the page structure associated to a virtual address.
 *
 * Results:
 *    MPN associated with the given virtual address
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE MPN
UserVa2MPN(VA addr)  // IN
{
   struct page *page;
   int npages;
   MPN mpn;

   npages = get_user_pages_unlocked(addr, 1, &page, 0);
   if (npages != 1) {
      return INVALID_MPN;
   }

   mpn = page_to_pfn(page);
   put_page(page);

   return mpn;
}

#endif /* __PGTBL_H__ */
