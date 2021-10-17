/*********************************************************
 * Copyright (C) 2018,2020 VMware, Inc. All rights reserved.
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
 * sharedAreaVmmon.c --
 *
 *     VMMon shared area management.
 */

#ifdef __linux__
#   include "driver-config.h"
#   include <linux/string.h> /* memset() in the kernel. */
#else
#   include <string.h>
#endif
#include "vm_assert.h"
#include "hostif.h"
#include "sharedAreaVmmon.h"
#include "vmx86.h"


/*
 *----------------------------------------------------------------------
 *
 * SharedAreaVmmonIsMultiVCPU --
 *
 *      Helper function for determining if a shared area type represents
 *      a region that has VCPU-specific memory.
 *
 * Results:
 *      TRUE if type has VCPU-specific memory, FALSE otherwise.
 *
 *----------------------------------------------------------------------
 */

static Bool
SharedAreaVmmonIsMultiVCPU(SharedAreaType type)
{
   switch (type) {
   case SHARED_AREA_PER_VM:
   case SHARED_AREA_PER_VM_VMX:
      return FALSE;
   default:
      return TRUE;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * SharedAreaVmmon_Init --
 *
 *      Initializes the shared area component of the VM Driver.
 *
 * Results:
 *      A pointer to the VMMon shared area.
 *
 *----------------------------------------------------------------------
 */

SharedAreaVmmon *
SharedAreaVmmon_Init(VMDriver *vm)
{
   SharedAreaVmmon *sa;

   /*
    * Allocate the shared area pointer here. The pages in each region are
    * lazily allocated when a region is registered.
    */
   sa = HostIF_AllocKernelMem(sizeof *sa, FALSE);
   if (sa == NULL) {
      Warning("SharedArea failed to allocate handle.\n");
      return NULL;
   }
   memset(sa, 0, sizeof *sa);
   return sa;
}


/*
 *----------------------------------------------------------------------
 *
 * SharedAreaVmmon_Cleanup --
 *
 *      Cleans up the shared area component of the VM Driver by freeing all
 *      previously allocated VMMon shared area memory.
 *
 * Results:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
SharedAreaVmmon_Cleanup(SharedAreaVmmon *area)
{
   SharedAreaType t;

   if (area == NULL) {
      return;
   }

   for (t = 0; t < NUM_SHARED_AREAS; t++) {
      SharedAreaVmmonRegion *region = &area->regions[t];

      if (region->pages != NULL) {
         HostIF_FreeKernelMem(region->pages);
      }
   }
   HostIF_FreeKernelMem(area);
}


/*
 *----------------------------------------------------------------------
 *
 * SharedAreaVmmon_ValidateRegionArgs --
 *
 *      Validate the given VMMon shared area region registration block.
 *
 * Results:
 *      TRUE if the block is valid and can be used to register a shared
 *      area region for a VCPU, FALSE otherwise.
 *
 *----------------------------------------------------------------------
 */

Bool
SharedAreaVmmon_ValidateRegionArgs(VMDriver *vm,
                                   VMSharedAreaRegistrationBlock *block)
{
   SharedAreaVmmonRegion *region;
   Vcpuid vcpu         = block->vcpu;
   SharedAreaType type = block->region.index;
   PageCnt numPages    = block->region.numPages;

   if (vm == NULL               ||
       vm->sharedArea == NULL   ||
       vcpu >= vm->numVCPUs     ||
       numPages == 0            ||
       type >= NUM_SHARED_AREAS ||
       (!SharedAreaVmmonIsMultiVCPU(type) && vcpu > 0)) {
      return FALSE;
   }

   region = &vm->sharedArea->regions[type];
   if (region->pagesPerVcpu != 0) {

      /*
       * A region that was previously registered for a given VCPU should have
       * the same number of pages as originally specified. Also, a region can
       * only be reserved once for a given VCPU throughout the runtime of a VM.
       */
      return region->pagesPerVcpu == numPages &&
             region->pages[vcpu * region->pagesPerVcpu] == INVALID_MPN;
   } else {
      /* First registration for this region. */
      ASSERT(region->pages == NULL);
      return TRUE;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * SharedAreaVmmon_RegisterRegion --
 *
 *      Register a VMMon shared area region for a given VCPU with the VM Driver.
 *      A successful return value indicates that the backing pages of the
 *      shared area region are locked and tracked by the VM Driver.
 *
 * Results:
 *      TRUE if the region was successfully registered for the given VCPU,
 *      FALSE otherwise.
 *
 *----------------------------------------------------------------------
 */

Bool
SharedAreaVmmon_RegisterRegion(VMDriver *vm,
                               VMSharedAreaRegistrationBlock *block)
{
   Vcpuid vcpu = block->vcpu;
   SharedAreaType type = block->region.index;
   SharedAreaVmmonRegion *region;
   MPN *pages;
   PageCnt page;

   ASSERT(SharedAreaVmmon_ValidateRegionArgs(vm, block));
   region = &vm->sharedArea->regions[type];
   if (region->pagesPerVcpu == 0) {
      PageCnt pagesInRegion = block->region.numPages;

      if (SharedAreaVmmonIsMultiVCPU(type)) {
         pagesInRegion *= vm->numVCPUs;
      }
      region->pages = HostIF_AllocKernelMem(sizeof *pages * pagesInRegion,
                                            FALSE);
      if (region->pages == NULL) {
         Warning("Failed to allocate pages array for region %u\n", type);
         return FALSE;
      }
      for (page = 0; page < pagesInRegion; page++) {
         region->pages[page] = INVALID_MPN;
      }
      region->pagesPerVcpu = block->region.numPages;
   }

   pages = region->pages + vcpu * region->pagesPerVcpu;
   for (page = 0; page < region->pagesPerVcpu; page++) {
      ASSERT(pages[page] == INVALID_MPN);
   }

   /*
    * Lock all shared area backing pages throughout the runtime of the monitor.
    * The pages remain locked until they are freed by HostIF during VM Driver
    * tear-down.
    */
   for (page = 0; page < region->pagesPerVcpu; page++) {
      VPN vmxVPN = block->region.baseVpn + page;
      VA64 uAddr = VPN_2_VA(vmxVPN);
      int status = Vmx86_LockPage(vm, uAddr, FALSE, pages + page);

      if (status != PAGE_LOCK_SUCCESS) {
         PageCnt resetPage;

         /* Reset the region pages for this VCPU to their original state. */
         for (resetPage = 0; resetPage < page; resetPage++) {
            status = Vmx86_UnlockPage(vm, uAddr);
            ASSERT(status == PAGE_UNLOCK_SUCCESS);
            pages[resetPage] = INVALID_MPN;
         }
         return FALSE;
      }
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * SharedAreaVmmon_GetRegionMPN --
 *
 *      For the shared area region corresponding to the given type and VCPU,
 *      get the MPN backing the region at the given offset.
 *
 * Results:
 *      The MPN backing the region, or INVALID_MPN if no MPN is found.
 *
 *----------------------------------------------------------------------
 */

MPN
SharedAreaVmmon_GetRegionMPN(VMDriver *vm, SharedAreaVmmonRequest *request)
{
   SharedAreaType type = request->type;
   Vcpuid vcpu = request->vcpu;
   SharedAreaVmmonRegion *region;
   PageCnt pgOffset = request->offset;
   MPN *pages;

   ASSERT(vcpu < vm->numVCPUs);
   ASSERT(type < NUM_SHARED_AREAS);
   ASSERT(IMPLIES(!SharedAreaVmmonIsMultiVCPU(type), vcpu == 0));
   region = &vm->sharedArea->regions[type];
   pages = region->pages + vcpu * region->pagesPerVcpu;
   if (region->pages == NULL || region->pagesPerVcpu == 0) {
      Warning("(%s) Requested unregistered region %u, VCPU %u\n", __FUNCTION__,
              type, vcpu);
      return INVALID_MPN;
   }
   if (pgOffset >= region->pagesPerVcpu) {
      Warning("(%s) Offset %"FMT64"u (type %u, per-VCPU size %"FMT64"u)\n",
              __FUNCTION__, pgOffset, type, region->pagesPerVcpu);
      return INVALID_MPN;
   }

   return pages[pgOffset];
}
