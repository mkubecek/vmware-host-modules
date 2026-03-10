/*********************************************************
 * Copyright (C) 2018-2020 VMware, Inc. All rights reserved.
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
 * statVarsVmmon.c --
 *
 *     VMMon stat variables management.
 */

#ifdef __linux__
#   include "driver-config.h"
#   include <linux/string.h> /* memset() in the kernel. */
#else
#   include <string.h>
#endif
#include "vm_assert.h"
#include "hostif.h"
#include "statVarsVmmon.h"
#include "vmx86.h"


/*
 *----------------------------------------------------------------------
 *
 * StatVarsVmmon_Init --
 *
 *      Initializes the stat vars component of the VM Driver.
 *
 * Results:
 *      A pointer to the VMMon stat vars struct.
 *
 *----------------------------------------------------------------------
 */

StatVarsVmmon *
StatVarsVmmon_Init(VMDriver *vm)
{
   StatVarsVmmon *statVars;

   statVars = HostIF_AllocKernelMem(sizeof *statVars, FALSE);
   if (statVars == NULL) {
      Warning("StatVars failed to allocate handle.\n");
      return NULL;
   }
   memset(statVars, 0, sizeof *statVars);
   return statVars;
}


/*
 *----------------------------------------------------------------------
 *
 * StatVarsVmmon_Cleanup --
 *
 *      Cleans up the stat vars component of the VM Driver by freeing all
 *      previously allocated VMMon stat vars memory.
 *
 * Results:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
StatVarsVmmon_Cleanup(StatVarsVmmon *statVars)
{
   if (statVars == NULL) {
      return;
   }

   if (statVars->pages != NULL) {
      HostIF_FreeKernelMem(statVars->pages);
   }
   HostIF_FreeKernelMem(statVars);
}


/*
 *----------------------------------------------------------------------
 *
 * StatVarsVmmonValidateRegistration --
 *
 *      Validate the given VMMon stat vars registration block.
 *
 * Results:
 *      TRUE if the block is valid and can be used to register stat vars
 *      backing pages for a VCPU, FALSE otherwise.
 *
 *----------------------------------------------------------------------
 */

static Bool
StatVarsVmmonValidateRegistration(VMDriver *vm,
                                   VMStatVarsRegistrationBlock *block)
{
   Vcpuid vcpu      = block->vcpu;
   PageCnt numPages = block->numPages;
   PageCnt pagesPerVcpu;
   MPN *pages;

   if (vm == NULL           ||
       vm->statVars == NULL ||
       numPages == 0        ||
       vcpu >= vm->numVCPUs) {
      return FALSE;
   }

   pagesPerVcpu = vm->statVars->pagesPerVcpu;
   pages = vm->statVars->pages;
   if (pagesPerVcpu == 0) {
      ASSERT(pages == NULL);
      return TRUE;
   } else {
      return pagesPerVcpu == numPages &&
             pages[vcpu * pagesPerVcpu] == INVALID_MPN;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * StatVarsVmmon_RegisterVCPU --
 *
 *      Register stat vars backing pages for the given VCPU with VMMon.
 *      A successful return value indicates that stat vars backing pages
 *      for the given VCPU are locked and tracked by the VM Driver.
 *
 * Results:
 *      TRUE if the backing pages were successfully registered for the
 *      given VCPU, FALSE otherwise.
 *
 *----------------------------------------------------------------------
 */

Bool
StatVarsVmmon_RegisterVCPU(VMDriver *vm,
                           VMStatVarsRegistrationBlock *block)
{
   Vcpuid vcpu = block->vcpu;
   PageCnt pagesPerVcpu = block->numPages;
   StatVarsVmmon *statVars = vm->statVars;
   MPN *pages;
   PageCnt page;

   HostIF_VMLock(vm, 45);
   if (!StatVarsVmmonValidateRegistration(vm, block)) {
      HostIF_VMUnlock(vm, 45);
      return FALSE;
   }
   if (statVars->pagesPerVcpu == 0) {
      PageCnt totalPages = pagesPerVcpu * vm->numVCPUs;
      statVars->pages = HostIF_AllocKernelMem(totalPages * sizeof *pages,
                                              FALSE);
      if (statVars->pages == NULL) {
         HostIF_VMUnlock(vm, 45);
         return FALSE;
      }
      for (page = 0; page < totalPages; page++) {
         statVars->pages[page] = INVALID_MPN;
      }
      statVars->pagesPerVcpu = pagesPerVcpu;
   }

   /*
    * Validate that all pages in this VCPU's region are still invalid. Before
    * unlocking, temporarily set the first page to 0 to prevent a malicious user
    * from triggering an ASSERT by firing parallel ioctls for the same VCPU.
    */
   pages = statVars->pages + vcpu * pagesPerVcpu;
   for (page = 0; page < pagesPerVcpu; page++) {
      ASSERT(pages[page] == INVALID_MPN);
   }
   ASSERT_ON_COMPILE(INVALID_MPN != 0);
   pages[0] = 0;
   HostIF_VMUnlock(vm, 45);

   /*
    * Verify that all stat vars backing pages are locked.  The pages are
    * assumed to remain locked either until they are unlocked by the VMX
    * following a NUMA migration, or by HostIF during VM Driver tear-down.
    */
   for (page = 0; page < pagesPerVcpu; page++) {
      VPN vmxVPN = block->baseVpn + page;
      VA64 uAddr = VPN_2_VA(vmxVPN);
      PageCnt resetPage;
      MPN *currPage = pages + page;
      Bool verifiedLocked = FALSE;
      int status = Vmx86_LockPage(vm, uAddr, FALSE, currPage);

      /*
       * Backing pages are expected to be locked by the VMX before being
       * registered.
       */
      if (status == PAGE_LOCK_ALREADY_LOCKED) {
         verifiedLocked = TRUE;
         status = Vmx86_LookupUserMPN(vm, uAddr, currPage);
         /*
          * PR 2260615: Some platforms return PAGE_LOCK_SUCCESS for a successful
          * user MPN lookup, others return PAGE_LOOKUP_SUCCESS, but they both
          * happen to equal the same value.
          */
         ASSERT_ON_COMPILE(PAGE_LOOKUP_SUCCESS == PAGE_LOCK_SUCCESS);
         if (status == PAGE_LOOKUP_SUCCESS) {
            /*
             * The backing page was verified as previously locked, and was
             * successfully recorded.
             */
            continue;
         }
      }
      /*
       * The backing page was either not previously locked, or was not recorded.
       * In either case, reset all previously recorded pages to INVALID_MPN.
       */
      if (!verifiedLocked && status == PAGE_LOCK_SUCCESS) {
         status = Vmx86_UnlockPage(vm, uAddr);
         ASSERT(status == PAGE_UNLOCK_SUCCESS);
      }
      for (resetPage = 0; resetPage <= page; resetPage++) {
         pages[resetPage] = INVALID_MPN;
      }
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * StatVarsVmmon_GetRegionMPN --
 *
 *      Obtain the backing MPN for stat vars for the given VCPU at
 *      the given offset.
 *
 * Results:
 *      The backing stat vars MPN for the given VCPU at the given offset
 *      if it has been registered, or INVALID_MPN otherwise.
 *
 *----------------------------------------------------------------------
 */

MPN
StatVarsVmmon_GetRegionMPN(struct VMDriver *vm, Vcpuid vcpu, PageCnt offset)
{
   MPN *pages;
   MPN backingPage;
   StatVarsVmmon *statVars = vm->statVars;

   ASSERT(vcpu < vm->numVCPUs);

   HostIF_VMLock(vm, 46);
   if (offset >= statVars->pagesPerVcpu) {
      HostIF_VMUnlock(vm, 46);
      Warning("(%s) Requested MPN at invalid offset %"FMT64"u for VCPU %u\n",
              __FUNCTION__, offset, vcpu);
      return INVALID_MPN;
   }

   pages = statVars->pages + vcpu * statVars->pagesPerVcpu;
   backingPage = pages[offset];
   HostIF_VMUnlock(vm, 46);
   return backingPage;
}
