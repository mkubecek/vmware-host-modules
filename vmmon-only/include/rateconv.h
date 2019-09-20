/*********************************************************
 * Copyright (C) 2003-2016, 2018 VMware, Inc. All rights reserved.
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
 *
 * rateconv.h --
 *
 *     Parameters and functions for linear rate conversion of 64 bit
 *     counters:
 *
 *       y = ((x * mult) >> shift) + add.
 *
 */

#ifndef _VM_RATECONV_H_
#define _VM_RATECONV_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vm_basic_asm.h"
#include "vm_assert.h"
#include "vm_atomic.h"
#include "versioned_atomic.h"

#if defined __cplusplus
extern "C" {
#endif


/* RateConv_Params is part of vmx<->vmmon interface (INIT_PSEUDO_TSC ioctl) */
typedef struct RateConv_Params {
   uint32 mult;  /* mult == 1 implies shift == 0. */
   uint32 shift;
   int64  add;
} RateConv_Params;

typedef struct RateConv_ParamsVolatile {
   RateConv_Params p;
   VersionedAtomic vers;
} RateConv_ParamsVolatile;

typedef struct RateConv_Ratio {
   uint32 mult;
   uint32 shift;
} RateConv_Ratio;

#define RATE_CONV_IDENTITY { 1, 0, 0 }  /* Out = in. */

Bool RateConv_ComputeParams(uint64 inHz, uint64 inBase,
		            uint64 outHz, uint64 outBase,
		            RateConv_Params *conv);
void RateConv_LogParams(const char *prefix,
			uint64 inHz, uint64 inBase,
			uint64 outHz, uint64 outBase,
			const RateConv_Params *conv);
Bool RateConv_ComputeRatio(uint64 inHz, uint64 outHz,
                           RateConv_Ratio *ratio);
void RateConv_LogRatio(const char *prefix,
                       uint64 inHz, uint64 outHz,
                       const RateConv_Ratio *ratio);


/*
 *----------------------------------------------------------------------
 *
 * RateConv_Unsigned --
 *
 *      Apply rate conversion to an unsigned argument:
 *       y = ((x * mult) >> shift) + add.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint64
RateConv_Unsigned(const RateConv_Params *conv, uint64 x)
{
   return Mul64x3264(x, conv->mult, conv->shift) + (uint64)conv->add;
}


/*
 *----------------------------------------------------------------------
 *
 * RateConv_Signed --
 *
 *      Apply rate conversion to a signed argument:
 *       y = ((x * mult) >> shift) + add.
 *
 *----------------------------------------------------------------------
 */

static INLINE int64
RateConv_Signed(const RateConv_Params *conv, int64 x)
{
   return Muls64x32s64(x, conv->mult, conv->shift) + conv->add;
}


#if defined __cplusplus
} // extern "C"
#endif

#endif // _VM_RATECONV_H_
