/*********************************************************
 * Copyright (C) 1998-2014,2019 VMware, Inc. All rights reserved.
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
 * vm_asm.h
 *
 *	asm macros
 */

#ifndef _VM_ASM_H_
#define _VM_ASM_H_


#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#ifdef VM_ARM_64
#include "vm_asm_arm64.h"
#else

#include "vm_basic_asm.h"
#include "x86msr.h"
#include "vm_asm_x86.h"


static INLINE void
SET_FS64(uint64 fs64)
{
   X86MSR_SetMSR(MSR_FSBASE, fs64);
}


static INLINE void
SET_GS64(uint64 gs64)
{
   X86MSR_SetMSR(MSR_GSBASE, gs64);
}

static INLINE void
SET_KernelGS64(uint64 kgs64)
{
   X86MSR_SetMSR(MSR_KERNELGSBASE, kgs64);
}


static INLINE uint64
GET_FS64(void)
{
   return X86MSR_GetMSR(MSR_FSBASE);
}


static INLINE uint64
GET_GS64(void)
{
   return X86MSR_GetMSR(MSR_GSBASE);
}


static INLINE uint64
GET_KernelGS64(void)
{
   return X86MSR_GetMSR(MSR_KERNELGSBASE);
}

#endif // VM_ARM_64
#endif
