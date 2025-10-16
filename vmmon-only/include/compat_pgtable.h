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

#ifndef __COMPAT_PGTABLE_H__
#   define __COMPAT_PGTABLE_H__


#if defined(CONFIG_PARAVIRT) && defined(CONFIG_HIGHPTE)
#   include <asm/paravirt.h>
#   undef paravirt_map_pt_hook
#   define paravirt_map_pt_hook(type, va, pfn) do {} while (0)
#endif
#include <asm/pgtable.h>


/*
 * p*d_leaf replaced p*d_large in 6.9.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 9, 0)
#   define compat_pgd_leaf(pgd)     pgd_leaf(pgd)
#   define compat_pmd_leaf(pmd)     pmd_leaf(pmd)
#   define compat_pud_leaf(pud)     pud_leaf(pud)
#   define compat_p4d_leaf(p4d)     p4d_leaf(p4d)
#else
#   define compat_pgd_leaf(pgd)     pgd_large(pgd)
#   define compat_pmd_leaf(pmd)     pmd_large(pmd)
#   define compat_pud_leaf(pud)     pud_large(pud)
#   define compat_p4d_leaf(p4d)     p4d_large(p4d)
#endif

/*
 * p4d level appeared in 4.12.
 */
#   define compat_p4d_offset(pgd, address)  p4d_offset(pgd, address)
#   define compat_p4d_present(p4d)          p4d_present(p4d)
#   define compat_p4d_pfn(p4d)              p4d_pfn(p4d)
#   define COMPAT_P4D_MASK                  P4D_MASK
typedef p4d_t compat_p4d_t;

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
