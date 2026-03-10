/*********************************************************
 * Copyright (C) 2002-2017 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_PGTABLE_H__
#   define __COMPAT_PGTABLE_H__


#if defined(CONFIG_PARAVIRT) && defined(CONFIG_HIGHPTE)
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)
#      include <asm/paravirt.h>
#      undef paravirt_map_pt_hook
#      define paravirt_map_pt_hook(type, va, pfn) do {} while (0)
#   endif
#endif
#include <asm/pgtable.h>


/*
 * p4d level appeared in 4.12.
 * Kernel 6.9+: pXd_large renamed to pXd_leaf (pgd/p4d_leaf may not exist on x86)
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
#   define compat_p4d_offset(pgd, address)  p4d_offset(pgd, address)
#   define compat_p4d_present(p4d)          p4d_present(p4d)
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 9, 0)
#      define compat_pgd_large(pgd)         (0)  /* x86: no pgd_leaf, PGD huge not used */
#      define compat_p4d_large(p4d)         (0)  /* x86: no p4d_leaf when folded */
#      define compat_pud_large(pud)         pud_leaf(pud)
#      define compat_pmd_large(pmd)         pmd_leaf(pmd)
#   else
#      define compat_pgd_large(pgd)         (0)
#      define compat_p4d_large(p4d)         p4d_large(p4d)
#      define compat_pud_large(pud)         pud_large(pud)
#      define compat_pmd_large(pmd)         pmd_large(pmd)
#   endif
#   define compat_p4d_pfn(p4d)              p4d_pfn(p4d)
#   define COMPAT_P4D_MASK                  P4D_MASK
typedef p4d_t compat_p4d_t;
#else
#   define compat_p4d_offset(pgd, address)  (pgd)
#   define compat_p4d_present(p4d)          (1)
#   define compat_p4d_large(p4d)            (0)
#   define compat_p4d_pfn(p4d)              INVALID_MPN  /* Not used */
#   define COMPAT_P4D_MASK                  0            /* Not used */
typedef pgd_t compat_p4d_t;
#endif
/* pud_pfn did not exist before 3.8. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
#   define pud_pfn(pud)  INVALID_MPN
#endif


/*
 * Define VM_PAGE_KERNEL_EXEC for vmapping executable pages.
 *
 * On ia32 PAGE_KERNEL_EXEC was introduced in 2.6.8.1.  Unfortunately it accesses
 * __PAGE_KERNEL_EXEC which is not exported for modules.  So we use
 * __PAGE_KERNEL and just cut _PAGE_NX bit from it.
 *
 * For ia32 kernels before 2.6.8.1 we use PAGE_KERNEL directly, these kernels
 * do not have noexec support.
 *
 * On x86-64 situation is a bit better: they always supported noexec, but
 * before 2.6.8.1 flag was named PAGE_KERNEL_EXECUTABLE, and it was renamed
 * to PAGE_KERNEL_EXEC when ia32 got noexec too (see above).
 */
#ifdef CONFIG_X86
#ifdef _PAGE_NX
#define VM_PAGE_KERNEL_EXEC __pgprot(__PAGE_KERNEL & ~_PAGE_NX)
#else
#define VM_PAGE_KERNEL_EXEC PAGE_KERNEL
#endif
#else
#define VM_PAGE_KERNEL_EXEC PAGE_KERNEL_EXEC
#endif


#endif /* __COMPAT_PGTABLE_H__ */
