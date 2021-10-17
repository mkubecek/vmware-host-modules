/*********************************************************
 * Copyright (C) 2016-2020 VMware, Inc. All rights reserved.
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
#include "cpu_types.h"
#include "hostif.h"
#include "x86paging_common.h"
#include "x86paging_64.h"
#include "vmx86.h"
#include "monLoader.h"
#include "monLoaderLog.h"
#include "vmmblob.h"
#include "memtrack.h"

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
MonLoaderCallout_Init(void *args, MonLoaderEnvContext **ctx, unsigned numVCPUs)
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
   Vmx86_CleanupVMMPages(ctx->vm);
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


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderCallout_AllocMPN --
 *
 *      Allocates and maps a new VMM page for the specified VCPU.
 *
 * Returns:
 *      An MPN on success, INVALID_MPN on failure.
 *
 * Side effects:
 *      The allocated page is accounted for and its mapping tracked.
 *
 *----------------------------------------------------------------------
 */

MPN
MonLoaderCallout_AllocMPN(MonLoaderEnvContext *ctx,  // IN
                          Vcpuid               vcpu) // IN
{
   VMDriver *vm = ctx->vm;
   MemTrackEntry *entry;
   MPN mpn;
   VPN vpn;

   if (Vmx86_AllocLockedPages(vm, (VA64)&mpn, 1, TRUE, FALSE) != 1) {
      Log("Failed to allocate page\n");
      return INVALID_MPN;
   }
   vpn = Vmx86_MapPage(mpn);
   if (vpn == 0) {
      Log("Failed to map MPN 0x%"FMT64"x\n", mpn);
      Vmx86_FreeLockedPages(vm, &mpn, 1);
      return INVALID_MPN;
   }
   HostIF_VMLock(vm, 41);
   entry = MemTrack_Add(vm->vmmTracker, vpn, mpn);
   HostIF_VMUnlock(vm, 41);
   if (entry == NULL) {
      Log("Failed to track mapping from VPN 0x%"FMTVPN"x to MPN 0x%"FMT64"x\n",
          vpn, mpn);
      Vmx86_UnmapPage(vpn);
      Vmx86_FreeLockedPages(vm, &mpn, 1);
      return INVALID_MPN;
   }
   return mpn;
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


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderCallout_FillPage --
 *
 *      Fills a page with a pattern, given the MPN of the page.
 *
 * Returns:
 *      TRUE on success, FALSE if a mapping of the page was not found or
 *      invalid.
 *
 * Side effects:
 *      The page is filled.
 *
 *----------------------------------------------------------------------
 */

Bool
MonLoaderCallout_FillPage(MonLoaderEnvContext *ctx,     // IN
                          uint8                pattern, // IN
                          MPN                  mpn,     // IN
                          Vcpuid               vcpu)    // IN
{
   VMDriver *vm = ctx->vm;
   MemTrackEntry *entry;

   HostIF_VMLock(vm, 42);
   entry = MemTrack_LookupMPN(vm->vmmTracker, mpn);
   HostIF_VMUnlock(vm, 42);
   if (entry == NULL || entry->mpn != mpn || entry->vpn == 0) {
      Log("Failed to look up MPN 0x%"FMT64"x\n", mpn);
      return FALSE;
   }
   memset((void *)VPN_2_VA(entry->vpn), pattern, PAGE_SIZE);
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderCallout_CopyFromBlob --
 *
 *     Copies contents of up to PAGE_SIZE length from the blob at a
 *     given offset into the page specified by the given MPN. Zero-fills
 *     the remaining space.
 *
 * Returns:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      The page is filled with blob contents.
 *
 *----------------------------------------------------------------------
 */

Bool
MonLoaderCallout_CopyFromBlob(MonLoaderEnvContext *ctx,        // IN
                              uint64               blobOffset, // IN
                              size_t               copySize,   // IN
                              MPN                  mpn,        // IN
                              Vcpuid               vcpu)       // IN
{
   MemTrackEntry *entry;
   VMDriver *vm = ctx->vm;
   uint64 blobSize = VmmBlob_GetSize(vm);
   uint8 *blob = VmmBlob_GetPtr(vm);
   uint8 *buf;

   if (copySize > PAGE_SIZE || copySize == 0 || blobSize < copySize ||
       blobOffset > blobSize - copySize) {
      Log("Invalid VMM blob copy parameters: blobOffset 0x%"FMT64"x, "
          "copySize 0x%"FMTSZ"x, blobSize 0x%"FMT64"x\n", blobOffset, copySize,
          blobSize);
      return FALSE;
   }
   HostIF_VMLock(vm, 44);
   entry = MemTrack_LookupMPN(vm->vmmTracker, mpn);
   HostIF_VMUnlock(vm, 44);
   if (entry == NULL || entry->vpn == 0) {
      Log("Failed to look up MPN 0x%"FMT64"x\n", mpn);
      return FALSE;
   }
   buf = (uint8 *)VPN_2_VA(entry->vpn);
   memcpy(buf, blob + blobOffset, copySize);
   memset(buf + copySize, 0, PAGE_SIZE - copySize);
   return TRUE;
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
      if (ctx->shRegions[i].index == index &&
          ctx->shRegions[i].baseVpn != INVALID_VPN) {
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

   s = MonLoaderFindSharedRegion(ctx, index);
   if (s == NULL) {
      return INVALID_MPN;
   }
   if (pgOffset >= s->numPages) {
      return INVALID_MPN;
   }
   addr = VPN_2_VA(s->baseVpn) + pgOffset * PAGE_SIZE;
   HostIF_VMLock(ctx->vm, 43);
   if (HostIF_LookupUserMPN(ctx->vm, addr, &mpn) != PAGE_LOOKUP_SUCCESS) {
      HostIF_VMUnlock(ctx->vm, 43);
      Log("Failed to lookup MPN for shared region VA %"FMTVA"x\n", addr);
      return INVALID_MPN;
   }
   HostIF_VMUnlock(ctx->vm, 43);
   return mpn;
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderCallout_GetSharedUserPage --
 *
 *      Gets a shared page's MPN for a VCPU.
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
MonLoaderCallout_GetSharedUserPage(MonLoaderEnvContext *ctx, // IN
                                   uint64 subIndex,          // IN
                                   unsigned page,            // IN
                                   Vcpuid vcpu)              // IN
{
   if (subIndex == MONLOADER_HEADER_IDX) {
      return VmmBlob_GetHeaderMpn(ctx->vm);
   } else {
      return MonLoaderGetSharedRegionMPN(ctx, subIndex, vcpu, page);
   }
}


MPN
MonLoaderCallout_GetSharedHostPage(MonLoaderEnvContext *ctx,      // IN
                                   uint64               subIndex, // IN
                                   unsigned             page,     // IN
                                   Vcpuid               vcpu)     // IN
{
   switch (subIndex) {
   case MONLOADER_CROSS_PAGE_DATA_IDX:
      return HostIF_GetCrossPageDataMPN(ctx->vm->crosspage[vcpu]);
   case MONLOADER_CROSS_PAGE_CODE_IDX:
      return HostIF_GetCrossPageCodeMPN();
   default:
      return MonLoaderGetSharedRegionMPN(ctx, subIndex, vcpu, page);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderCallout_GetBlobMpn --
 *
 *      Returns the MPN backing the given VMM blob offset.
 *
 * Returns:
 *      An MPN, or INVALID_MPN
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

MPN
MonLoaderCallout_GetBlobMpn(MonLoaderEnvContext *ctx,    // IN
                            uint64               offset) // IN
{
   ASSERT((offset & (PAGE_SIZE - 1)) == 0);
   return VmmBlob_GetMpn(ctx->vm, BYTES_2_PAGES(offset));
}
