/*********************************************************
 * Copyright (C) 2014-2021 VMware, Inc. All rights reserved.
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
 * address_defs.h --
 *
 *	Macros for virtual/physical/machine address/page conversions, page types.
 */

#ifndef _ADDRESS_DEFS_H_
#define _ADDRESS_DEFS_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "vm_basic_defs.h" // For PAGE_SHIFT

#if defined __cplusplus
extern "C" {
#endif


/*
 * Virtual, physical, machine address and page conversion macros
 */

#define VA_2_VPN(_va)  ((_va) >> PAGE_SHIFT)
#define PTR_2_VPN(_ptr) VA_2_VPN((VA)(_ptr))
#define VPN_2_VA(_vpn) ((_vpn) << PAGE_SHIFT)
#define VPN_2_PTR(_vpn) ((void *)VPN_2_VA(_vpn))

/*
 * Notice that we don't cast PA_2_PPN's argument to an unsigned type, because
 * we would lose compile-time checks for pointer operands and byte-sized
 * operands. If you use a signed quantity for _pa, ones may be shifted into the
 * high bits of your ppn.
 */

#define PA_2_PPN(_pa)      ((_pa) >> PAGE_SHIFT)
#define PPN_2_PA(_ppn)     ((PA)(_ppn) << PAGE_SHIFT)

#define PA_2_PPN_4KB(_pa)  ((_pa) >> PAGE_SHIFT_4KB)
#define PPN_2_PA_4KB(_ppn) ((PA)(_ppn) << PAGE_SHIFT_4KB)

#define PA_2_PPN_16KB(_pa)  ((_pa) >> PAGE_SHIFT_16KB)
#define PPN_2_PA_16KB(_ppn) ((PA)(_ppn) << PAGE_SHIFT_16KB)

static INLINE MA    MPN_2_MA(MPN mpn)     { return  (MA)mpn << PAGE_SHIFT;  }
static INLINE MPN   MA_2_MPN(MA ma)       { return (MPN)(ma >> PAGE_SHIFT); }

static INLINE IOA   IOPN_2_IOA(IOPN iopn) { return (IOA)(iopn << PAGE_SHIFT); }
static INLINE IOPN  IOA_2_IOPN(IOA ioa)   { return (IOPN)(ioa >> PAGE_SHIFT); }

/*
 *----------------------------------------------------------------------
 *
 * IsGoodMPN --
 *
 *      Is the given MPN valid?
 *
 * Results:
 *      Return TRUE if "mpn" looks plausible. We could make this stricter on
 *      a per-architecture basis.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
IsGoodMPN(MPN mpn)
{
   return mpn <= MAX_MPN;
}

static INLINE Bool
IsGoodMPNOrMemref(MPN mpn)
{
   return IsGoodMPN(mpn) || mpn == MEMREF_MPN;
}


#if defined __cplusplus
} // extern "C"
#endif

#endif // _ADDRESS_DEFS_H_
