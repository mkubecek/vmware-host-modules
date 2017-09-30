/*********************************************************
 * Copyright (C) 1998-2015 VMware, Inc. All rights reserved.
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
 * vm_asm_x86_64.h
 *
 *	x86-64 asm macros
 */

#ifndef _VM_ASM_X86_64_H_
#define _VM_ASM_X86_64_H_


#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#ifdef __GNUC__
#include "vm_asm_x86.h"
#endif

/*
 * This file contains inline assembly routines used by x86_64 code.
 */

#ifdef __GNUC__

  /* nop; prevents #error for no compiler definition from firing */

#elif defined _MSC_VER  /* !__GNUC__ */

/*
 * x86-64 windows doesn't support inline asm so we have to use these
 * intrinsic functions defined in the compiler.  Not all of these are well
 * documented.  There is an array in the compiler dll (c1.dll) which has
 * an array of the names of all the intrinsics minus the leading
 * underscore.  Searching around in the ntddk.h file can also be helpful.
 *
 * The declarations for the intrinsic functions were taken from the DDK.
 * Our declarations must match the ddk's otherwise the 64-bit c++ compiler
 * will complain about second linkage of the intrinsic functions.
 * We define the intrinsic using the basic types corresponding to the
 * Windows typedefs. This avoids having to include windows header files
 * to get to the windows types.
 */

#ifdef _WIN64
#ifdef __cplusplus
extern "C" {
#endif
unsigned __int64  __readmsr(unsigned long);
void              __writemsr(unsigned long, unsigned __int64);
#pragma intrinsic(__readmsr, __writemsr)
#ifdef __cplusplus
}
#endif


static INLINE uint64
RDPMC(int counter)
{
   return __readpmc(counter);
}


static INLINE void
WRMSR(uint32 msrNum, uint32 lo, uint32 hi)
{
   uint64 value = QWORD(hi, lo);
   __writemsr(msrNum, value);
}


static INLINE uint64
__GET_MSR(int input)
{
   return __readmsr((unsigned long)input);
}


static INLINE void
__SET_MSR(int cx, uint64 val)
{
  __writemsr((unsigned long)cx, (unsigned __int64)val);
}

#endif

#else
#error No compiler defined for get/set
#endif /* !__GNUC__ && !_MSC_VER */


#ifdef __GNUC__
static INLINE void
SWAPGS(void)
{
   __asm__ __volatile__("swapgs");
}


static INLINE uint64
RDTSCP_AuxOnly(void)
{
   uint64 tscLow, tscHigh, tscAux;

   __asm__ __volatile__(
      "rdtscp"
      : "=a" (tscLow), "=d" (tscHigh), "=c" (tscAux)
   );

   return tscAux;
}
#endif

#endif
