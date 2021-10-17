/*********************************************************
 * Copyright (C) 1998-2014,2016,2018-2020 VMware, Inc. All rights reserved.
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
#include "x86/cpu_types_arch.h"
#include "vm_pagetable.h"
#include "x86paging_common.h"
#include "vm_assert.h"

#define LM_PTE_PFN_MASK      CONST64U(0xffffffffff000)
#define LM_PTE_2_PFN(_pte)   (((_pte) & LM_PTE_PFN_MASK) >> PT_PTE_PFN_SHIFT)

#define LM_PDE_PFN_MASK      0xfffffffe00000LL
#define LM_PDPTE_PFN_MASK    0xfffffc0000000LL

#define LM_AVAIL_SHIFT         9

#define LM_AVAIL_MASK         (CONST64(0x7) << LM_AVAIL_SHIFT)
#define LM_FLAGS_MASK         CONST64(0x80000000000001ff)
#define LM_CR3_FLAGS_MASK     CONST64(0x18)
#define LM_L3_1G_RSVD_MASK    CONST64(0x3fffe000)

#define LM_MAKE_CR3(_mpfn, _flags) \
                   (((uint64)(_mpfn) << PT_PTE_PFN_SHIFT) | \
                   ((_flags) & LM_CR3_FLAGS_MASK))

#define LM_MAKE_PTE(_mpfn, _avail, _flags) \
                   (((uint64)(_mpfn) << PT_PTE_PFN_SHIFT) | \
                    (((_avail) << LM_AVAIL_SHIFT) & LM_AVAIL_MASK) | \
                    ((uint64)(_flags) & LM_FLAGS_MASK))

#define LM_MAKE_PDE(_pfn, _avail, _flags) LM_MAKE_PTE(_pfn, _avail, _flags)
#define LM_MAKE_L5E(_pfn, _avail, _flags) LM_MAKE_PTE(_pfn, _avail, _flags)
#define LM_MAKE_L4E(_pfn, _avail, _flags) LM_MAKE_PTE(_pfn, _avail, _flags)
#define LM_MAKE_L3E(_pfn, _avail, _flags) LM_MAKE_PTE(_pfn, _avail, _flags)
#define LM_MAKE_L2E(_pfn, _avail, _flags) LM_MAKE_PTE(_pfn, _avail, _flags)
#define LM_MAKE_L1E(_pfn, _avail, _flags) LM_MAKE_PTE(_pfn, _avail, _flags)


/*
 *----------------------------------------------------------------------
 *
 * LMPTEIsSafe --
 *
 *    A shadow PTE is considered "safe" if any of the following conditions are
 *    met:
 *
 *      a) It is not terminal (i.e., present with no reserved bits set)
 *      b) Terminal, but with MPN and PS fields set to zero
 *      c) Terminal, but with MPN field specifying an uncachable page
 *
 *    In practice, for condition c), we require that bits 45:43 of the EPTE are
 *    set to b'111.  The position of these bits is undocumented and not
 *    architectural; they are truly magic.
 *
 *----------------------------------------------------------------------
 */
#ifdef VMX86_DEBUG

#define LM_SAFE_BITS_MASK (MASK64(3) << 43)
#define LM_SAFE_BITS(_v)  ((_v) & LM_SAFE_BITS_MASK)

static INLINE Bool
LMPTEIsTerminal(VM_PAE_PTE pte, PT_Level level, uint64 physMask)
{
   uint64 rsvd2m = (MASK(PT_LEVEL_SHIFT) << PAGE_SHIFT) & ~PTE_LARGE_PAT;
   uint64 rsvd1g = (MASK(2 * PT_LEVEL_SHIFT) << PAGE_SHIFT) & ~PTE_LARGE_PAT;
   uint64 rsvdl4 = PTE_PS;
   uint64 rsvd = physMask & MASK64(52);

   return (!PTE_PRESENT(pte) ||
           (pte & LM_PTE_PFN_MASK) & rsvd) != 0 ||
           ((pte & PTE_PS) != 0 &&
            ((level == PT_LEVEL_2 && (pte & rsvd2m) != 0) ||
             (level == PT_LEVEL_3 && (pte & rsvd1g) != 0))) ||
           (level == PT_LEVEL_4 && (pte & rsvdl4) != 0);
}

static INLINE Bool
LMPTEIsSafe(VM_PAE_PTE pte, PT_Level level, uint64 physMask)
{
   MPN mpn = (pte & LM_PTE_PFN_MASK) >> PT_PTE_PFN_SHIFT;
   Bool safe = !LMPTEIsTerminal(pte, level, physMask) ||
               (mpn == 0 && !PTE_LARGEPAGE(pte)) ||
               LM_SAFE_BITS(pte) == LM_SAFE_BITS_MASK;
   return safe;
}
#endif /* VMX86_DEBUG */


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

static INLINE Bool
x86IsCanonicalC(VA64 va)
{
   return (va & VA64_CANONICAL_MASK) == 0 ||
          (va & VA64_CANONICAL_MASK) == VA64_CANONICAL_MASK;
}

#if defined(VM_X86_64) && defined(__GNUC__)
static INLINE Bool
x86IsCanonicalAsm(VA64 va)
{
   Bool out;
   /*
    * sarq $48, %0: Move 17 bits from position 63:47 into 15:0 and CF,
    *  sign-extending as we do so.
    * adcl $0, %0: Then add (0 + CF) to the shifted value.  The result is zero
    *  iff (CF == 1 && bits 31:0 == 0xffffffff) or (CF == 0 && bits 31:0 == 0).
    *  That is, if original bits 47 and higher were all 1s or all 0s.
    * setz %1: Create the boolean.
    *
    * The chain of AdcLong macros will emit one adc instruction; gcc
    * has no obvious way to force the size of a quad %0 to a long.
    */
#define AdcLong(reg64, reg32) \
   ".ifc  %0, %%" #reg64 "\n" \
   " adcl $0, %%" #reg32 "\n" \
   ".endif \n"

   asm ("sarq $48, %0 \n"
        AdcLong(rax, eax)
        AdcLong(rcx, ecx)
        AdcLong(rdx, edx)
        AdcLong(rbx, ebx)
        AdcLong(rsp, esp)
        AdcLong(rbp, ebp)
        AdcLong(rsi, esi)
        AdcLong(rdi, edi)
        AdcLong(r8,  r8d)
        AdcLong(r9,  r9d)
        AdcLong(r10, r10d)
        AdcLong(r11, r11d)
        AdcLong(r12, r12d)
        AdcLong(r13, r13d)
        AdcLong(r14, r14d)
        AdcLong(r15, r15d)
        "setz %1" : "+r"(va), "=r"(out));
   return out;
#undef AdcLong
}
#endif

static INLINE Bool
x86_IsCanonical(VA64 va)
{
#if defined(VM_X86_64) && defined(__GNUC__)
   if (__builtin_constant_p(va)) {
      return x86IsCanonicalC(va);
   } else {
      return x86IsCanonicalAsm(va);
   }
#else
   return x86IsCanonicalC(va);
#endif
}

static INLINE Bool
x86_IsCanonicalRange(VA64 va, unsigned size)
{
   /*
    * The check is simple as long as the size is less
    * than the number of implemented bits.
    *
    * The only case we don't handle is one where the VA goes from a
    * high canonical address and wraps to a non-canonical address
    * (e.g. 0x00008000_00000000) or higher.  Our test would falsely
    * consider this canonical.
    */
   ASSERT_ON_COMPILE(sizeof(size) * 8 < VA64_IMPL_BITS);

   /*
    * VA64_CANONICAL_MASK is the lowest canonical address with the
    * upper bits all set.
    *
    * VA64_CANONICAL_HOLE_START is one higher than the highest valid
    * canonical address with the upper bits all cleared.  Note that we
    * access up to (va + size - 1), not (va + size), so <= is correct.
    */
   return va >= VA64_CANONICAL_MASK ||
          va + size <= VA64_CANONICAL_HOLE_START;
}

#endif /* _X86PAGING_64_H_ */
