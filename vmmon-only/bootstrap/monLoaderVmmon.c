/*********************************************************
 * Copyright (C) 2016-2017 VMware, Inc. All rights reserved.
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
 * monLoaderVmmon.c --
 *
 *      vmmon implementation of the MonLoader callouts.
 *
 */

#ifdef __linux__
#   include "driver-config.h"
#   include <linux/string.h>
#else
#   include <string.h>
#endif

#include "vm_assert.h"
#include "vcpuid.h"
#include "x86types.h"
#include "hostif.h"
#include "x86paging_common.h"
#include "x86paging_64.h"
#include "vmx86.h"
#include "monLoaderLog.h"

typedef struct MonLoaderEnvContext {
   VMDriver *vm;
   VMSharedRegion *shRegions;
} MonLoaderEnvContext;


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderCallout_Init --
 *
 *      Initializes platform-specific MonLoader context.
 *
 * Returns:
 *      TRUE if successful, FALSE if not.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
MonLoaderCallout_Init(void *args, MonLoaderEnvContext **ctx)
{
   MonLoaderEnvContext *c;
   MonLoaderArgs *mlArgs = (MonLoaderArgs *)args;

   c = HostIF_AllocKernelMem(sizeof *c, FALSE);
   if (c == NULL) {
      Log("Failed to allocate memory for MonLoader context\n");
      return FALSE;
   }
   memset(c, 0, sizeof *c);
   c->vm = mlArgs->vm;
   c->shRegions = mlArgs->shRegions;

   *ctx = c;
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderCallout_Cleanup --
 *
 *      Cleans up platform-specific MonLoader context.
 *
 * Returns:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
MonLoaderCallout_CleanUp(MonLoaderEnvContext *ctx)
{
   ASSERT(ctx != NULL);
   HostIF_FreeKernelMem(ctx);
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderCallout_GetPageRoot --
 *
 *      Gets the page root MPN for a VCPU.
 *
 * Returns:
 *      An MPN, or INVALID_MPN in case of error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */


MPN
MonLoaderCallout_GetPageRoot(MonLoaderEnvContext *ctx,  // IN
                             Vcpuid               vcpu) // IN
{
   VMDriver *vm = ctx->vm;

   if (vcpu >= vm->numVCPUs) {
      return INVALID_MPN;
   }
   return vm->ptRootMpns[vcpu];
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderCallout_GetPTE --
 *
 *      Gets the page table entry at index idx in a page table MPN.
 *
 * Returns:
 *      TRUE and sets *pte on success.  FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
MonLoaderCallout_GetPTE(MonLoaderEnvContext *ctx,   // IN
                        MPN                  ptMPN, // IN
                        unsigned             idx,   // IN
                        Vcpuid               vcpu,  // IN
                        PT_L1E              *pte)   // OUT
{
   VMDriver *vm = ctx->vm;
   MA pteMA = MPN_2_MA(ptMPN) + idx * sizeof *pte;

   ASSERT(idx < (PAGE_SIZE / sizeof *pte));
   if (HostIF_ReadPhysical(vm, pteMA, PtrToVA64(pte), TRUE, sizeof *pte) != 0) {
      Log("Failed to read PTE %d from MPN %"FMT64"x\n", idx, ptMPN);
      return FALSE;
   }
   return TRUE;
}


Bool
MonLoaderCallout_ImportPage(MonLoaderEnvContext *ctx, // IN
                            MPN mpn,                  // IN
                            Vcpuid vcpu)              // IN
{
   return TRUE;
}


MPN
MonLoaderCallout_AllocMPN(MonLoaderEnvContext *ctx,  // IN
                          Vcpuid               vcpu) // IN
{
   NOT_IMPLEMENTED();
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderCallout_MapMPNInPTE --
 *
 *      Maps an MPN into a page table at the specified index.
 *
 * Returns:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      A mapping is created.
 *
 *----------------------------------------------------------------------
 */

Bool
MonLoaderCallout_MapMPNInPTE(MonLoaderEnvContext *ctx, // IN
                             MPN ptMPN,                // IN
                             unsigned idx,             // IN
                             uint64 flags,             // IN
                             MPN mpn,                  // IN
                             Vcpuid vcpu)              // IN
{
   VMDriver *vm = ctx->vm;
   PT_L1E pte = LM_MAKE_PTE(mpn, 0, flags);
   MA pteMA = MPN_2_MA(ptMPN) + idx * sizeof pte;

   ASSERT(idx < (PAGE_SIZE / sizeof pte));
   if (HostIF_WritePhysical(vm, pteMA, PtrToVA64(&pte), TRUE, sizeof pte) !=
       0) {
      Warning("Failed to map MPN %"FMT64"x\n", mpn);
      return FALSE;
   }
   LOG(5, "MonLoader mapped MPN %"FMT64"x at PT index %u for VCPU %u\n",
       mpn, idx, vcpu);
   return TRUE;
}


Bool
MonLoaderCallout_FillPage(MonLoaderEnvContext *ctx,     // IN
                          uint8                pattern, // IN
                          MPN                  mpn,     // IN
                          Vcpuid               vcpu)    // IN
{
   NOT_IMPLEMENTED();
}


Bool
MonLoaderCallout_CopyFromBlob(MonLoaderEnvContext *ctx,        // IN
                              uint64               blobOffset, // IN
                              size_t               copySize,   // IN
                              MPN                  mpn,        // IN
                              Vcpuid               vcpu)       // IN
{
   NOT_IMPLEMENTED();
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderFindSharedRegion --
 *
 *      Searches the shared region table for the given index and returns
 *      the corresponding descriptor.
 *
 * Returns:
 *      A pointer to the shared region descriptor or NULL if not found.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static VMSharedRegion *
MonLoaderFindSharedRegion(MonLoaderEnvContext *ctx,
                          uint64 index)
{
   int i;

   for (i = 0; i < ML_SHARED_REGIONS_MAX; i++) {
      if (ctx->shRegions[i].baseVpn == INVALID_VPN) {
         /* Not found. */
         return NULL;
      } else if (ctx->shRegions[i].index == index) {
         return &ctx->shRegions[i];
      }
   }
   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderGetSharedRegionMPN --
 *
 *      Gets shared page's MPN. Assumes vcpuid == 0 for the bootstrap.
 *
 * Returns:
 *      An MPN or INVALID_MPN.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static MPN
MonLoaderGetSharedRegionMPN(MonLoaderEnvContext *ctx,
                            uint64 index,
                            Vcpuid vcpuid,
                            unsigned pgOffset)
{
   VMSharedRegion *s;
   VA addr;
   MPN mpn;

   ASSERT(IS_BOOT_VCPUID(vcpuid));

   if (index >= ML_SHARED_REGIONS_MAX) {
      Log("Invalid shared region %"FMT64"x", index);
      return INVALID_MPN;
   }
   s = MonLoaderFindSharedRegion(ctx, index);
   if (s == NULL) {
      return INVALID_MPN;
   }
   if (pgOffset >= s->numPages) {
      return INVALID_MPN;
   }

   addr = VPN_2_VA(s->baseVpn) + pgOffset * PAGE_SIZE;
   HostIF_VMLock(ctx->vm, 38);
   if (HostIF_LookupUserMPN(ctx->vm, addr, &mpn) != PAGE_LOOKUP_SUCCESS) {
      HostIF_VMUnlock(ctx->vm, 38);
      Log("Failed to lookup MPN for shared region VA %"FMTVA"x\n", addr);
      return INVALID_MPN;
   }
   HostIF_VMUnlock(ctx->vm, 38);
   return mpn;
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderCallout_GetSharedUserPage --
 *
 *      Gets a user-provided shared page's MPN for a VCPU.
 *
 * Returns:
 *      An MPN or INVALID_MPN.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

MPN
MonLoaderCallout_GetSharedUserPage(MonLoaderEnvContext *ctx,
                                   uint64 subIndex, // IN
                                   unsigned page,   // IN
                                   Vcpuid vcpu)     // IN
{
   return MonLoaderGetSharedRegionMPN(ctx, subIndex, vcpu, page);
}


MPN
MonLoaderCallout_GetSharedHostPage(MonLoaderEnvContext *ctx,      // IN
                                   uint64               subIndex, // IN
                                   unsigned             page,     // IN
                                   Vcpuid               vcpu)     // IN
{
   NOT_IMPLEMENTED();
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderCallout_IsPrivileged --
 *
 *      Returns whether or not this is a privileged environment.
 *
 * Returns:
 *      TRUE if privileged, FALSE if not.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
MonLoaderCallout_IsPrivileged(MonLoaderEnvContext *ctx) // IN
{
   return TRUE;
}

MPN
MonLoaderCallout_GetBlobMpn(MonLoaderEnvContext *ctx,    // IN
                            uint64               offset) // IN
{
   return INVALID_MPN;
}

Bool
MonLoaderCallout_SetEntrypoint(MonLoaderEnvContext *ctx,           // IN
                               uint16               codeSelector,  // IN
                               VA64                 code,          // IN
                               uint16               stackSelector, // IN
                               VA64                 stack)         // IN
{
   NOT_IMPLEMENTED();
}
