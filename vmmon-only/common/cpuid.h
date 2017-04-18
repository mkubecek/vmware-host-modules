/*********************************************************
 * Copyright (C) 1998,2016 VMware, Inc. All rights reserved.
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
 * cpuid.h --
 *
 *    wrap CPUID instruction
 */

#ifndef CPUID_H
#define CPUID_H

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "x86cpuid.h"

extern uint32 cpuidFeatures;

void CPUID_Init(void);
CpuidVendor CPUID_GetVendor(void);
uint32 CPUID_GetVersion(void);
Bool CPUID_SyscallSupported(void);
Bool CPUID_LongModeSupported(void);
Bool CPUID_AddressSizeSupported(void);

static INLINE uint32 
CPUID_GetFeatures(void)
{
   return cpuidFeatures;
}

static INLINE Bool
CPUID_SSE2Supported(void)
{
   return CPUID_ISSET(1, EDX, SSE2, CPUID_GetFeatures());
}

#endif

