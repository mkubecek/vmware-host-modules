/*********************************************************
 * Copyright (C) 1998, 2016-2017 VMware, Inc. All rights reserved.
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

#ifdef linux
/* Must come before any kernel header file --hpreg */
#   include "driver-config.h"

#   include <linux/string.h>
#endif
#ifdef __APPLE__
#   include <string.h> // For strcmp().
#endif

#include "vmware.h"
#include "vm_assert.h"
#include "hostif.h"
#include "cpuid.h"
#include "x86cpuid_asm.h"
#include "x86svm.h"
#include "x86vt.h"

uint32 cpuidFeatures;
static CpuidVendor vendor = CPUID_NUM_VENDORS;
static uint32 version;


/*
 *-----------------------------------------------------------------------------
 *
 * CPUIDExtendedSupported --
 *
 *     Determine whether processor supports extended CPUID (0x8000xxxx)
 *     and how many of them.
 *
 * Results:
 *     0         if extended CPUID is not supported
 *     otherwise maximum extended CPUID supported (bit 31 set)
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

static uint32
CPUIDExtendedSupported(void)
{
   uint32 eax;

   eax = __GET_EAX_FROM_CPUID(0x80000000);
   if ((eax & 0x80000000) != 0x80000000) {
      return 0;
   }

   return eax;
}


void
CPUID_Init(void)
{
   CPUIDRegs regs;
   uint32 *ptr;
   char name[16];

   __GET_CPUID(1, &regs);
   version = regs.eax;
   cpuidFeatures = regs.edx;

   __GET_CPUID(0, &regs);
   ptr = (uint32 *)name;
   ptr[0] = regs.ebx;
   ptr[1] = regs.edx;
   ptr[2] = regs.ecx;
   ptr[3] = 0;

   if (strcmp(name, CPUID_INTEL_VENDOR_STRING_FIXED) == 0) {
      vendor = CPUID_VENDOR_INTEL;
   } else if (strcmp(name, CPUID_AMD_VENDOR_STRING_FIXED) == 0) {
      vendor = CPUID_VENDOR_AMD;
   } else if (strcmp(name, CPUID_CYRIX_VENDOR_STRING_FIXED) == 0) {
      vendor = CPUID_VENDOR_CYRIX;
   } else {
      Warning("VMMON CPUID: Unrecognized CPU\n");
      vendor = CPUID_VENDOR_UNKNOWN;
   }
}


CpuidVendor
CPUID_GetVendor(void)
{
   ASSERT(vendor != CPUID_NUM_VENDORS);
   return vendor;
}


uint32
CPUID_GetVersion(void)
{
   return version;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CPUID_AddressSizeSupported --
 *
 *     Determine whether processor supports the address size cpuid
 *     extended leaf.
 *
 * Results:
 *     True iff the processor supports CPUID 0x80000008.
 *
 * Side effects:
 *     It determines value only on first call, caching it for future.
 *
 *-----------------------------------------------------------------------------
 */

Bool
CPUID_AddressSizeSupported(void)
{
   /*
    * It is OK to use local static variables here as 'result' does not depend
    * on any work done in CPUID_Init(). It purely depends on the CPU.
    */
   static Bool initialized = FALSE;
   static Bool result;

   if (UNLIKELY(!initialized)) {
      result = CPUIDExtendedSupported() >= 0x80000008;
      initialized = TRUE;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CPUID_HostSupportsHV --
 *
 *     Determine whether processor supports hardware virtualization.  Two
 *     possibilities are valid: VMX on Intel or SVM on AMD.
 *
 * Results:
 *     True iff the processor supports the required features.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
CPUID_HostSupportsHV(void)
{
   return (vendor == CPUID_VENDOR_AMD   && SVM_CapableCPU()) ||
          (vendor == CPUID_VENDOR_INTEL && VT_CapableCPU());
}
