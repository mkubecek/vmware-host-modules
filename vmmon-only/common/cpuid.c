/*********************************************************
 * Copyright (C) 1998, 2016-2021 VMware, Inc. All rights reserved.
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

#ifdef __linux__
/* Must come before any kernel header file --hpreg */
#   include "driver-config.h"

#   include <linux/string.h>
#endif
#ifdef __APPLE__
#   include <string.h> // For strcmp().
#endif

#include "vm_assert.h"
#include "hostif.h"
#include "cpuid.h"
#include "x86cpuid_asm.h"
#include "x86svm.h"
#include "x86vt.h"

uint32      cpuidFeatures;
CpuidVendor cpuidVendor = CPUID_NUM_VENDORS;
uint32      cpuidVersion;
Bool        hostSupportsVT;
Bool        hostSupportsSVM;
Bool        hostHasSpecCtrl;
Bool        hostSupportsXSave;


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
   CPUIDRegs regs, regs88;
   uint32 *ptr;
   char name[16];

   __GET_CPUID(1, &regs);
   cpuidVersion = regs.eax;
   cpuidFeatures = regs.edx;
   hostSupportsXSave = CPUID_ISSET(1, ECX, XSAVE, regs.ecx);

   __GET_CPUID(0, &regs);
   ptr = (uint32 *)name;
   ptr[0] = regs.ebx;
   ptr[1] = regs.edx;
   ptr[2] = regs.ecx;
   ptr[3] = 0;

   if (strcmp(name, CPUID_INTEL_VENDOR_STRING_FIXED) == 0) {
      cpuidVendor = CPUID_VENDOR_INTEL;
   } else if (strcmp(name, CPUID_AMD_VENDOR_STRING_FIXED) == 0) {
      cpuidVendor = CPUID_VENDOR_AMD;
   } else if (strcmp(name, CPUID_CYRIX_VENDOR_STRING_FIXED) == 0) {
      cpuidVendor = CPUID_VENDOR_CYRIX;
   } else if (strcmp(name, CPUID_HYGON_VENDOR_STRING_FIXED) == 0) {
      cpuidVendor = CPUID_VENDOR_HYGON;
   } else {
      Warning("VMMON CPUID: Unrecognized CPU\n");
      cpuidVendor = CPUID_VENDOR_UNKNOWN;
   }

   __GET_CPUID2(7, 0, &regs);
   __GET_CPUID2(0x80000008, 0, &regs88);
   hostHasSpecCtrl =  CPUID_ISSET(7, EDX, IBRSIBPB, regs.edx) ||
                      CPUID_ISSET(7, EDX, STIBP, regs.edx)    ||
                      CPUID_ISSET(7, EDX, SSBD,  regs.edx)    ||
                      CPUID_ISSET(0x80000008, EBX, LEAF88_SSBD_SPEC_CTRL,
                                  regs88.ebx)                 ||
                      CPUID_ISSET(0x80000008, EBX, LEAF88_PSFD, regs88.ebx);

   hostSupportsVT = VT_CapableCPU();
   hostSupportsSVM = SVM_CapableCPU();
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
