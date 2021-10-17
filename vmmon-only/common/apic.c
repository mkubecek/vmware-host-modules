/*********************************************************
 * Copyright (C) 2011, 2016, 2018,2020 VMware, Inc. All rights reserved.
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

#include "vm_basic_defs.h"
#include "x86apic.h"
#include "x86msr.h"
#include "x86cpuid_asm.h"
#include "cpuid.h"
#include "apic.h"

/*
 *----------------------------------------------------------------------
 *
 * APIC_GetMA --
 *
 *      Return the MA of the host's APIC by reading the APIC_BASE
 *      MSR and applying any necessary masking.
 *
 * Side effects:
 *      None
 *
 * Return value:
 *      MA of host APIC if successful (guaranteed to be page-aligned),
 *      or the sentinel (MA)-1 if unsucessful or if X2 APIC mode is enabled
 *      since this disables the MMIO interface.
 *
 *----------------------------------------------------------------------
 */

MA
APIC_GetMA(void)
{
   uint64 result;
   CpuidVendor cpuVendor = CPUID_GetVendor();
   uint32 features = CPUID_GetFeatures();

   if (!CPUID_ISSET(1, EDX, MSR, features) ||
       !CPUID_ISSET(1, EDX, APIC, features)) {
      return (MA)-1;
   }

   if (cpuVendor != CPUID_VENDOR_INTEL &&
       cpuVendor != CPUID_VENDOR_AMD &&
       cpuVendor != CPUID_VENDOR_HYGON) {
      return (MA)-1;
   }

   /*
    * Check if X2 APIC mode is enabled.
    */

   if ((X86MSR_GetMSR(MSR_APIC_BASE) & APIC_MSR_X2APIC_ENABLED) != 0) {
      return (MA)-1;
   }

   /*
    * APIC is present and enabled.  The CPUID[0x1].edx[APIC] bit,
    * already checked, mirrors the APIC base MSR's enable bit.
    */

   // Mask out goo in the low 12 bits, which is unrelated to the address.
   result = X86MSR_GetMSR(MSR_APIC_BASE) & ~MASK64(PAGE_SHIFT);

   /*
    * On Intel, the high bits are reserved so we mask.
    * On AMD and Hygon, high bits are explicitly MBZ, so no need.
    */
   if (cpuVendor == CPUID_VENDOR_INTEL) {
      /*
       * Intel suggests using CPUID 0x80000008.eax[7-0] (physical
       * address size), with 36 (24 bit MPNs) as a fallback.
       */
      unsigned numPhysicalBits = 36;

      if (CPUID_AddressSizeSupported()) {
         numPhysicalBits = __GET_EAX_FROM_CPUID(0x80000008) & 0xff;
      }

      result &= MASK64(numPhysicalBits);
   }

   ASSERT_ON_COMPILE(sizeof(result) == sizeof(MA));
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * APIC_Read --
 *
 *      Reads the given APIC register using the proper interface.  Does not
 *      check to see if the register number is valid.
 *
 * Side effects:
 *      Yes.
 *
 * Return value:
 *      Value of the register.
 *
 *----------------------------------------------------------------------
 */

uint32
APIC_Read(const APICDescriptor *desc, // IN
          int regNum)                 // IN
{
   if (desc->isX2) {
      return (uint32)X86MSR_GetMSR(MSR_X2APIC_BASE + regNum);
   } else {
      return desc->base[regNum][0];
   }
}


/*
 *----------------------------------------------------------------------
 *
 * APIC_ReadID --
 *
 *      Reads the APIC ID using the proper interface.  The semantics of the
 *      ID are different in X2APIC mode so APIC_Read() should not be used.
 *
 * Side effects:
 *      None.
 *
 * Return value:
 *      APIC ID.
 *
 *----------------------------------------------------------------------
 */

uint32
APIC_ReadID(const APICDescriptor *desc) // IN
{
   uint32 reg = APIC_Read(desc, APICR_ID);

   if (desc->isX2) {
      return reg;
   } else {
      return (reg & XAPIC_ID_MASK) >> APIC_ID_SHIFT;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * APIC_Write --
 *
 *      Writes the given value to the given APIC register using the proper
 *      interface.  Does not check to see if the register number is valid.
 *
 * Side effects:
 *      Yes.
 *
 * Return value:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
APIC_Write(const APICDescriptor *desc, // IN
           int regNum,                 // IN
           uint32 val)                 // IN
{
   if (desc->isX2) {
      X86MSR_SetMSR(MSR_X2APIC_BASE + regNum, val);
   } else {
      desc->base[regNum][0] = val;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * APIC_ReadICR --
 *
 *      Reads the APIC ICR using the proper interface.  The semantics of the
 *      ICR are different in X2APIC mode so APIC_Read() should not be used.
 *
 * Side effects:
 *      Yes.
 *
 * Return value:
 *      The full 64-bit value of the ICR.
 *
 *----------------------------------------------------------------------
 */

uint64
APIC_ReadICR(const APICDescriptor *desc) // IN
{
   if (desc->isX2) {
      return X86MSR_GetMSR(MSR_X2APIC_BASE + APICR_ICRLO);
   } else {
      uint32 icrHi = desc->base[APICR_ICRHI][0];
      uint32 icrLo = desc->base[APICR_ICRLO][0];
      return (uint64) icrHi << 32 | icrLo;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * APIC_WriteICR --
 *
 *      Writes the given value to the APIC ICR using the proper interface.
 *      The semantics of the ICR are different in X2APIC mode so APIC_Write()
 *      should not be used.
 *
 * Side effects:
 *      Yes.
 *
 * Return value:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
APIC_WriteICR(const APICDescriptor *desc, // IN
              uint32 id,                  // IN
              uint32 icrLo)               // IN
{
   if (desc->isX2) {
      uint64 icr = (uint64) id << 32 | icrLo;
      X86MSR_SetMSR(MSR_X2APIC_BASE + APICR_ICRLO, icr);
   } else {
      ASSERT(!(id & ~(APIC_ICRHI_DEST_MASK >> APIC_ICRHI_DEST_OFFSET)));
      desc->base[APICR_ICRHI][0] = id << APIC_ICRHI_DEST_OFFSET;
      desc->base[APICR_ICRLO][0] = icrLo;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * APIC_MaxLvt --
 *
 *      Reads the maximum number of LVT entries from the APIC version register.
 *
 * Side effects:
 *      No.
 *
 * Return value:
 *      The maximum number of LVT entries.
 *
 *----------------------------------------------------------------------
 */

uint32
APIC_MaxLVT(const APICDescriptor *desc) // IN
{
   uint32 ver = APIC_Read(desc, APICR_VERSION);

   return (ver >> APIC_MAX_LVT_SHIFT) & APIC_MAX_LVT_MASK;
}
