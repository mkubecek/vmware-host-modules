/*********************************************************
 * Copyright (C) 2002,2014-2017,2023 VMware, Inc. All rights reserved.
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
 * PgtblVa2MPNLocked --
 *
 *    Walks through the hardware page tables to try to find the pte
 *    associated to a virtual address.  Then maps PTE to MPN.
 *
 * Results:
 *    INVALID_MPN on failure
 *    mpn         on success
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if COMPAT_LINUX_VERSION_CHECK_LT(6, 5, 0) // only used by PgtblVa2MPN() below
static INLINE MPN
PgtblVa2MPNLocked(struct mm_struct *mm, // IN: Mm structure of a process
                  VA addr)              // IN: Address in the virtual address
                                        //     space of that process
{
   compat_p4d_t *p4d;
   MPN mpn;
   pgd_t *pgd = pgd_offset(mm, addr);

   if (pgd_present(*pgd) == 0) {
      return INVALID_MPN;
   }
   if (pgd_large(*pgd)) {
      /* Linux kernel does not support PGD huge pages. */
      /* return pgd_pfn(*pgd) + ((addr & PGD_MASK) >> PAGE_SHIFT); */
      return INVALID_MPN;
   }

   p4d = compat_p4d_offset(pgd, addr);
   if (compat_p4d_present(*p4d) == 0) {
      return INVALID_MPN;
   }
   if (compat_p4d_large(*p4d)) {
      mpn = compat_p4d_pfn(*p4d) + ((addr & ~COMPAT_P4D_MASK) >> PAGE_SHIFT);
   } else {
      pud_t *pud = pud_offset(p4d, addr);

      if (pud_present(*pud) == 0) {
         return INVALID_MPN;
      }
      if (pud_large(*pud)) {
         mpn = pud_pfn(*pud) + ((addr & ~PUD_MASK) >> PAGE_SHIFT);
      } else {
         pmd_t *pmd = pmd_offset(pud, addr);

         if (pmd_present(*pmd) == 0) {
            return INVALID_MPN;
         }
         if (pmd_large(*pmd)) {
            mpn = pmd_pfn(*pmd) + ((addr & ~PMD_MASK) >> PAGE_SHIFT);
         } else {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,5,0)
            pte_t *pte = pte_offset_kernel(pmd, addr);
#else
            pte_t *pte = pte_offset_map(pmd, addr);
#endif

            if (pte_present(*pte) == 0) {
               pte_unmap(pte);
               return INVALID_MPN;
            }
            mpn = pte_pfn(*pte);
            pte_unmap(pte);
         }
      }
   }
   if (mpn >= INVALID_MPN) {
      mpn = INVALID_MPN;
   }
   return mpn;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * PgtblVa2MPN --
 *
 *    Walks through the hardware page tables of the current process to try to
 *    find the page structure associated to a virtual address.
 *
 * Results:
 *    Same as PgtblVa2MPNLocked()
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if COMPAT_LINUX_VERSION_CHECK_LT(6, 5, 0)

static INLINE MPN
PgtblVa2MPN(VA addr)  // IN
{
   struct mm_struct *mm;
   MPN mpn;

   /* current->mm is NULL for kernel threads, so use active_mm. */
   mm = current->active_mm;
   spin_lock(&mm->page_table_lock);
   mpn = PgtblVa2MPNLocked(mm, addr);
   spin_unlock(&mm->page_table_lock);
   return mpn;
}

#else /* COMPAT_LINUX_VERSION_CHECK_LT(6, 5, 0) */

static INLINE MPN
PgtblVa2MPN(VA addr)  // IN
{
   struct page *page;
   int npages;
   MPN mpn;

   npages = get_user_pages_unlocked(addr, 1, &page, FOLL_HWPOISON);
   if (npages != 1)
	   return INVALID_MPN;
   mpn = page_to_pfn(page);
   put_page(page);

   return mpn;
}

#endif /* COMPAT_LINUX_VERSION_CHECK_LT(6, 5, 0) */

#endif /* __PGTBL_H__ */
