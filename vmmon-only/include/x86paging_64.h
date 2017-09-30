/*********************************************************
 * Copyright (C) 1998-2014,2016 VMware, Inc. All rights reserved.
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
 * x86paging_64.h --
 *
 *      Contains definitions for the x86 page table layout specific to
 *      long mode.
 */

#ifndef _X86PAGING_64_H_
#define _X86PAGING_64_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "x86types.h"
#include "vm_pagetable.h"

#define LM_PTE_PFN_MASK      CONST64U(0xffffffffff000)
#define LM_PTE_2_PFN(_pte)   (((_pte) & LM_PTE_PFN_MASK) >> PT_PTE_PFN_SHIFT)

#define LM_PDE_PFN_MASK      0xfffffffe00000LL
#define LM_PDPTE_PFN_MASK    0xfffffc0000000LL

#define LM_AVAIL_SHIFT         9

#define LM_AVAIL_MASK         (CONST64(0x7) << LM_AVAIL_SHIFT)
#define LM_FLAGS_MASK         CONST64(0x80000000000001ff)
#define LM_CR3_FLAGS_MASK     CONST64(0x18)
#define LM_L4_L3_RSVD_MASK    CONST64(0x180)
#define LM_L3_1G_RSVD_MASK    CONST64(0x3fffe000)

#define LM_MAKE_CR3(_mpfn, _flags) \
                   (((uint64)(_mpfn) << PT_PTE_PFN_SHIFT) | \
                   ((_flags) & LM_CR3_FLAGS_MASK))

#define LM_MAKE_PTE(_mpfn, _avail, _flags) \
                   (((uint64)(_mpfn) << PT_PTE_PFN_SHIFT) | \
                    (((_avail) << LM_AVAIL_SHIFT) & LM_AVAIL_MASK) | \
                    ((uint64)(_flags) & LM_FLAGS_MASK))

#define LM_MAKE_PDE(_pfn, _avail, _flags) LM_MAKE_PTE(_pfn, _avail, _flags)
#define LM_MAKE_L4E(_pfn, _avail, _flags) LM_MAKE_PTE(_pfn, _avail, _flags)
#define LM_MAKE_L3E(_pfn, _avail, _flags) LM_MAKE_PTE(_pfn, _avail, _flags)
#define LM_MAKE_L2E(_pfn, _avail, _flags) LM_MAKE_PTE(_pfn, _avail, _flags)
#define LM_MAKE_L1E(_pfn, _avail, _flags) LM_MAKE_PTE(_pfn, _avail, _flags)


/*
 * x86-64 architecture requires implementations supporting less than
 * full 64-bit VAs to ensure that all virtual addresses are in canonical
 * form. An address is in canonical form if the address bits from the
 * most significant implemented bit up to bit 63 are all ones or all
 * zeros. If this is not the case, the processor generates #GP/#SS. Our
 * VCPU implements 48 bits of virtual address space.
 */

#define VA64_IMPL_BITS             48
#define VA64_CANONICAL_MASK        ~((CONST64U(1) << (VA64_IMPL_BITS - 1)) - 1)
#define VA64_CANONICAL_HOLE_START   (CONST64U(1) << (VA64_IMPL_BITS - 1))
#define VA64_CANONICAL_HOLE_LEN  VA64_CANONICAL_MASK - VA64_CANONICAL_HOLE_START

#endif /* _X86PAGING_64_H_ */
