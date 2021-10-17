/*********************************************************
 * Copyright (C) 1998-2020 VMware, Inc. All rights reserved.
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
 * x86paging_common.h --
 *
 *      Contains definitions for the x86 page table layout that are common
 *      to legacy and long modes.
 */

#ifndef _X86PAGING_COMMON_H_
#define _X86PAGING_COMMON_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_types.h"


/*
 *   page tables
 */

#define PTE_P           0x00000001
#define PTE_RW          0x00000002
#define PTE_US          0x00000004
#define PTE_PWT         0x00000008
#define PTE_PCD         0x00000010 // careful: MMU_PTE_EX has same value.
#define PTE_A           0x00000020
#define PTE_D           0x00000040
#define PTE_PS          0x00000080
#define PTE_PAT         0x00000080
#define PTE_G           0x00000100
#define PTE_LARGE_PAT   0x00001000
#define PTE_NX          CONST64(0x8000000000000000)
#define PTE_NX_SHIFT    63

#define PTE_PK_MASK     CONST64(0x7800000000000000)
#define PTE_PK_SHIFT    59

#define PTE_LARGE_PAT_2_PS(_pte) (((_pte) & PTE_LARGE_PAT) >> 5)

#define PTE_PROT_FLAGS  (PTE_P|PTE_RW|PTE_US)
#define PTE_FLAGS       (PTE_PROT_FLAGS|PTE_G)

#define PTE_KERNEL              (PTE_P|PTE_RW)
#define PTE_KERNEL_NX           (PTE_P|PTE_RW|PTE_NX)

#define PTE_PRESENT(_pte)   (((_pte) & PTE_P) != 0)
#define PTE_WRITEABLE(_pte) (((_pte) & PTE_RW) != 0)
#define PTE_ACCESS(_pte)    (((_pte) & PTE_A) != 0)
#define PTE_DIRTY(_pte)     (((_pte) & PTE_D) != 0)
#define PTE_USER(_pte)      (((_pte) & PTE_US) != 0)
#define PTE_GLOBAL(_pte)    (((_pte) & PTE_G) != 0)
#define PTE_LARGEPAGE(_pte) (((_pte) & PTE_PS) != 0)
#define PTE_UNCACHED(_pte)  (((_pte) & PTE_PCD) != 0)
#define PTE_NOEXECUTE(_pte) (((_pte) & PTE_NX) != 0)
#define PTE_PK(_pte)        (((_pte) & PTE_PK_MASK) >> PTE_PK_SHIFT)

#define PK_AD         1 /* Access disable bit */
#define PK_WD         2 /* Write disable bit */
#define PKR_WIDTH     2
#define PKR_MASK      3
#define PK_RIGHTS(_pkru, _key) (((_pkru) >> ((_key) * PKR_WIDTH)) & PKR_MASK);

/* Error code flags */
#define PF_P            (1 << 0)
#define PF_RW           (1 << 1)
#define PF_US           (1 << 2)
#define PF_RSVD         (1 << 3)
#define PF_ID           (1 << 4)
#define PF_PK           (1 << 5)
#define PF_SS           (1 << 6)
#define PF_SGX          (1 << 15)
#define PF_RMP          (1 << 31)

/*
 * Operand definitions for the INVPCID instruction. See SDM Vol. 2A.
 */
#define INVPCID_EXTENT_ADDR               0
#define INVPCID_EXTENT_PCID_CTX           1
#define INVPCID_EXTENT_ALL_CTX            2
#define INVPCID_EXTENT_ALL_CTX_LOCAL      3
#define INVPCID_EXTENT_MAX                3
#define INVPCID_MAX_PCID              0xfff

typedef struct {
   uint64   pcid;    /* Bits 63:12 reserved.                   */
   uint64   la;      /* Used for single address invalidation.  */
} InvpcidArg;

typedef enum {
   PM_FLAT,        /* non paging mode                                      */
   PM_2_LEVEL,     /* classic 32 bit non-pae style paging mode.            */
   PM_3_LEVEL,     /* classic 32 bit pae style paging mode.                */
   PM_4_LEVEL,     /* long mode style paging mode (including compat mode). */
   PM_NUM     = 4, /* number of different paging modes.                    */
} PagingMode;

#endif /* _X86PAGING_COMMON_H_ */
