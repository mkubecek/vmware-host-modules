/*********************************************************
 * Copyright (C) 2020-2021 VMware, Inc. All rights reserved.
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

#ifndef _X86CET_H_
#define _X86CET_H_

/*
 * x86cet.h --
 *
 *    Contains definitions for the x86 Control-flow Enforcement Technology
 *    (CET) features: Shadow Stacks and Indirect Branch Tracking.
 */

#include "x86paging_64.h"

/* Error codes for #CP causes. */
#define CP_NEAR_RET               1
#define CP_FAR_RET_IRET           2
#define CP_ENDBRANCH              3
#define CP_RSTORSSP               4
#define CP_SETSSBSY               5
#define CP_ENCL                   (1 << 15)

/* Bits for token checks when switching %ssp. */
#define SSP_SUPERVISOR_TOKEN_BUSY (1 << 0)
#define SSP_RSTOR_TOKEN_LM        (1 << 0)
#define SSP_PREV_TOKEN            (1 << 1)

/* The Shadow Stack pointer is always 4-byte aligned. */
#define SSP_ALIGN_MASK            0x3
#define INVALID_SSP               SSP_ALIGN_MASK

/*
 * Shadow Frame --
 *    Pushed onto the new shadow stack upon exceptions, interrupts, and
 *    far calls except User->Supervisor.
 *    Popped off the old shadow stack upon lret or iret, except
 *    Supervisor->User.
 */
typedef struct {
   uint64   ssp;
   uint64   lip;
   Selector cs, __cs_unused[3];
} ShadowFrame64;

typedef ALIGNED(8) struct {
   uint32   ssp, __ssp_unused;
   uint32   lip, __lip_unused;
   Selector cs, __cs_unused[3];
} ShadowFrame32;


/*
 * CET_IBTComputeLegacyByte --
 *   Calculates the bit position in the legacy bitmap for the given LA.
 *   LA [63  ...  48][47 .... 15][14 13 12][11  ...  0]
 *         unused       byteNum    bitNum
 */
static INLINE void
CET_IBTComputeLegacyByte(LA la, uint64 *byteNum, uint8 *byteMask)
{
   const unsigned bitsPerByte = 8;
   uint64 bitNum = LA_2_LPN((LA64)la & MASK64(VA64_IMPL_BITS));
   *byteNum = bitNum / bitsPerByte;
   *byteMask = (uint8)(1 << (bitNum % bitsPerByte));
}

#endif /* ifndef _X86CET_H_ */
