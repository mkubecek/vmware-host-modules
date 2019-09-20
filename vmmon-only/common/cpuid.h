/*********************************************************
 * Copyright (C) 1998, 2016-2019 VMware, Inc. All rights reserved.
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

extern uint32      cpuidFeatures;
extern uint32      cpuidVersion;
extern CpuidVendor cpuidVendor;
extern Bool        hostSupportsVT;
extern Bool        hostSupportsSVM;
extern Bool        hostHasSpecCtrl;
extern Bool        hostSupportsXSave;

void CPUID_Init(void);
Bool CPUID_AddressSizeSupported(void);

static INLINE uint32
CPUID_GetFeatures(void)
{
   return cpuidFeatures;
}

static INLINE CpuidVendor
CPUID_GetVendor(void)
{
   ASSERT(cpuidVendor != CPUID_NUM_VENDORS);
   return cpuidVendor;
}

static INLINE uint32
CPUID_GetVersion(void)
{
   return cpuidVersion;
}

static INLINE Bool
CPUID_HostSupportsVT(void)
{
   return hostSupportsVT;
}

static INLINE Bool
CPUID_HostSupportsSVM(void)
{
   return hostSupportsSVM;
}

static INLINE Bool
CPUID_HostSupportsHV(void)
{
   return hostSupportsVT || hostSupportsSVM;
}

static INLINE Bool
CPUID_HostSupportsSpecCtrl(void)
{
   return hostHasSpecCtrl;
}

static INLINE Bool
CPUID_HostSupportsXSave(void)
{
   return hostSupportsXSave;
}

static INLINE Bool
CPUID_SSE2Supported(void)
{
   return CPUID_ISSET(1, EDX, SSE2, CPUID_GetFeatures());
}

#endif
