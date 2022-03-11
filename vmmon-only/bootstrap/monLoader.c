/*********************************************************
 * Copyright (C) 2015-2020 VMware, Inc. All rights reserved.
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
 * monLoader.c --
 *
 *      Processes the monitor loader header.
 *
 *      See monLoader.h for a full description.
 */

#if defined VMKERNEL

#include "libc.h"
#include "vm_libc.h"

#define UINT_MAX (~0U)

#elif defined VMMON

#include "hostKernel.h"

#ifdef __linux__
#   include "driver-config.h"
#   include <linux/string.h>
#   include <linux/kernel.h>
#elif defined __APPLE__
#   include <machine/limits.h>
#   include <string.h>
#else
#   include <limits.h>
#   include <string.h>
#endif

#include "vm_assert.h"

#else /* !defined VMKERNEL && !defined VMMON */
#error MonLoader cannot be built as part of this environment
#endif

#include "vm_basic_types.h"
#include "monLoader.h"
#include "vcpuid.h"
#include "vm_pagetable.h"
#include "address_defs.h"
#include "monLoaderLog.h"

#define CANONICAL_MASK MASK64(36)
/* The index of VPN v within an array of LxEs where index 0 maps VPN b. */
#define L4EArrayIdx(b,v) ((unsigned)((((v) - (b)) & CANONICAL_MASK) >> 36))
#define L3EArrayIdx(b,v) ((unsigned)((((v) - (b)) & CANONICAL_MASK) >> 27))
#define L2EArrayIdx(b,v) ((unsigned)((((v) - (b)) & CANONICAL_MASK) >> 18))
#define L1EArrayIdx(b,v) ((unsigned)((((v) - (b)) & CANONICAL_MASK) >> 9))

/* The maximum (canonical-address) VPN */
#define VPN_MAX MASK64(52)

/* Sufficient MPN counts to accommodate the monitor's top 64MB. */
#define L4MPNCOUNTMAX  1
#define L3MPNCOUNTMAX  1
#define L2MPNCOUNTMAX  1
#define L1MPNCOUNTMAX 32

#define LINE_INVALID  ((unsigned)-1)

typedef struct MonPTMPNs {
   MPN L4MPNs[L4MPNCOUNTMAX]; /* The page root.  Definitely only one page. */
   MPN L3MPNs[L3MPNCOUNTMAX];
   MPN L2MPNs[L2MPNCOUNTMAX];
   MPN L1MPNs[L1MPNCOUNTMAX];
   unsigned L4MPNCount;
   unsigned L3MPNCount;
   unsigned L2MPNCount;
   unsigned L1MPNCount;
} MonPTMPNs;
 
typedef struct MonLoaderContext {
   struct MonLoaderEnvContext *envCtx; /* Environment-specific context */
   struct {
      MonPTMPNs    ptMPNs;      /* Mappings into the AS for the current VCPU. */
      VPN          ASFirstVPN;  /* first VPN in the address space */
      VPN          ASLastVPN;   /* last VPN in the address space (inclusive) */
      uint64       ASPTEFlags;  /* PTE flags for L4->L1 connection. */
      Vcpuid       currentVCPU;
      Bool         hasAddrSpace;
   } vcpu;
} MonLoaderContext;


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderTranslateMonVPNToL1E --
 *
 *      Translates a monitor VPN to a level 1 page table entry, for the current
 *      VCPU.  Assumes preallocated and connected page tables, so only the L1
 *      table is examined.
 *
 * Result:
 *      ML_OK and a PTE if successful, some other value on error.  Fails if a
 *      page table page is not already allocated or the VPN is not in the
 *      context's address space's defined range.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static MonLoaderError
MonLoaderTranslateMonVPNToL1E(MonLoaderContext *ctx,    // IN/OUT
                              VPN               monVPN, // IN
                              PT_L1E           *pte)    // OUT
{
   Vcpuid vcpu = ctx->vcpu.currentVCPU;
   /* Only modify the L1E as L4->L1 are guaranteed connected already. */
   unsigned L1Off = PT_LPN_2_L1OFF(monVPN);
   unsigned L1Page = L1EArrayIdx(ctx->vcpu.ASFirstVPN, monVPN);

   if (monVPN < ctx->vcpu.ASFirstVPN || monVPN > ctx->vcpu.ASLastVPN) {
      return ML_ERROR_INVALID_VPN; /* Address is outside address space. */
   }
   if (ctx->vcpu.ptMPNs.L1MPNs[L1Page] == INVALID_MPN) {
      return ML_ERROR_MAP; /* Page table not sufficiently preallocated. */
   }
   if (!MonLoaderCallout_GetPTE(ctx->envCtx, ctx->vcpu.ptMPNs.L1MPNs[L1Page],
                                L1Off, vcpu, pte)) {
      return ML_ERROR_CALLOUT_GETPTE;
   }
   return ML_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderIsMapped --
 *
 *      Checks whether a VPN has an existing mapping on the current VCPU.
 *      Assumes preallocated and connected page tables.
 *
 * Result:
 *      ML_OK if successful, some other value on error.  Fails if a page table
 *      page is not already allocated or the VPN is not in the context's
 *      address space's defined range.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static MonLoaderError
MonLoaderIsMapped(MonLoaderContext *ctx,    // IN/OUT
                  VPN               monVPN, // IN
                  Bool             *mapped) // OUT
{
   PT_L1E pte;
   MonLoaderError ret = MonLoaderTranslateMonVPNToL1E(ctx, monVPN, &pte);

   if (ret != ML_OK) {
      return ret;
   }
   *mapped = ML_PERM_PRESENT(pte);
   return ML_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderAllocMPN --
 *
 *      Allocates a new MPN.
 *
 * Result:
 *      ML_OK and an MPN, on success.  An error and no MPN on failure.
 *
 * Side effects:
 *      An MPN may be allocated.
 *
 *----------------------------------------------------------------------
 */
static MonLoaderError
MonLoaderAllocMPN(MonLoaderContext *ctx, // IN
                  MPN *mpn)              // OUT
{
   *mpn = MonLoaderCallout_AllocMPN(ctx->envCtx, ctx->vcpu.currentVCPU);
   if (*mpn == INVALID_MPN) {
      return ML_ERROR_ALLOC;
   }
   return ML_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderMapMPN --
 *
 *      Maps an MPN into the current VCPU's address space at a VPN.
 *
 * Result:
 *      ML_OK if successful, some other value on error.  Fails if a page table
 *      page is not already allocated or the VPN is not in the context's
 *      address space's defined range.
 *
 * Side effects:
 *      An MPN is mapped on success.  Nothing on failure.
 *
 *----------------------------------------------------------------------
 */
static MonLoaderError
MonLoaderMapMPN(MonLoaderContext *ctx,    // IN/OUT
                MPN               mpn,    // IN
                uint64            flags,  // IN
                VPN               monVPN) // IN
{
   MonPTMPNs *ptMPNs = &ctx->vcpu.ptMPNs;
   /* Only modify the L1E, as L4->L1 are guaranteed connected already. */
   unsigned L1Off = PT_LPN_2_L1OFF(monVPN);
   unsigned L1Page = L1EArrayIdx(ctx->vcpu.ASFirstVPN, monVPN);

   if (vmx86_debug) {
      MonLoaderError ret;
      Bool mapped;
      if (monVPN < ctx->vcpu.ASFirstVPN || monVPN > ctx->vcpu.ASLastVPN) {
         LOG(5, "%s: monVPN=0x%"FMTVPN"x, AS=0x%"FMTVPN"x-0x%"FMTVPN"x\n",
                 __FUNCTION__, monVPN, ctx->vcpu.ASFirstVPN,
                 ctx->vcpu.ASLastVPN);
         return ML_ERROR_INVALID_VPN; /* Address is outside address space. */
      }
      if (ptMPNs->L1MPNs[L1Page] == INVALID_MPN) {
         return ML_ERROR_MAP; /* Page table not sufficiently preallocated. */
      }
      ret = MonLoaderIsMapped(ctx, monVPN, &mapped);
      if (ret != ML_OK) {
         return ret;
      }
      if (mapped) {
         return ML_ERROR_ALREADY_MAPPED;
      }
   }
   if (!MonLoaderCallout_MapMPNInPTE(ctx->envCtx, ptMPNs->L1MPNs[L1Page], L1Off,
                                     flags, mpn, ctx->vcpu.currentVCPU)) {
      return ML_ERROR_CALLOUT_MAPINPTE;
   }
   return ML_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderBuildsPtLevel --
 *
 *      Determines whether MonLoader allocates and maps the page table(s) for
 *      the monitor at the given level.
 *
 * Result:
 *      TRUE if MonLoader creates the page table at the given level, FALSE
 *      otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static Bool
MonLoaderCreatesPtLevel(PT_Level level)
{
   ASSERT(level >= PT_LEVEL_STOP && level <= PT_MAX_LEVELS);
   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderCreateAddressSpace --
 *
 *      Creates or verifies an address space.  The VPN range specified by
 *      firstVPN and size is used to determine page counts at each page table
 *      level to map every page.  Sufficient pages are then allocated or
 *      verified.
 *
 * Result:
 *      ML_OK if successful, some other value on error.
 *
 * Side effects:
 *      MPNs are allocated if not importing.  Sets ctx->vcpu.hasAddrSpace.
 *
 *----------------------------------------------------------------------
 */
static MonLoaderError
MonLoaderCreateAddressSpace(MonLoaderContext *ctx,      // IN/OUT
                            VPN               firstVPN, // IN
                            uint64            flags,    // IN
                            uint64            monPages) // IN
{
   Vcpuid vcpu = ctx->vcpu.currentVCPU;
   VPN lastVPN = firstVPN + monPages - 1;
   unsigned L3MPNsNeeded = L3EArrayIdx(0, lastVPN) -
                           L3EArrayIdx(0, firstVPN) + 1;
   unsigned L2MPNsNeeded = L2EArrayIdx(0, lastVPN) -
                           L2EArrayIdx(0, firstVPN) + 1;
   unsigned L1MPNsNeeded = L1EArrayIdx(0, lastVPN) -
                           L1EArrayIdx(0, firstVPN) + 1;
   unsigned i;
   MPN mpn;
   MonPTMPNs *ptMPNs;

   if (vcpu == VCPUID_INVALID) {
      return ML_ERROR_ARGS;
   }

   /* Verify MonPTMPNs is large enough for this address space. */
   if (L3MPNsNeeded > L3MPNCOUNTMAX || L2MPNsNeeded > L2MPNCOUNTMAX ||
       L1MPNsNeeded > L1MPNCOUNTMAX) {
      return ML_ERROR_ADDRSPACE_TOO_LARGE;
   }
   ctx->vcpu.ASFirstVPN = firstVPN;
   ctx->vcpu.ASLastVPN = lastVPN;
   ctx->vcpu.ASPTEFlags = flags;

   ptMPNs = &ctx->vcpu.ptMPNs;

   if (MonLoaderCreatesPtLevel(PT_LEVEL_4)) {
      NOT_IMPLEMENTED();
   } else {
      /* Verify the VMX's allocation. */
      mpn = MonLoaderCallout_GetPageRoot(ctx->envCtx, vcpu);
      if (mpn == INVALID_MPN) {
         return ML_ERROR_CALLOUT_PAGEROOT_GET;
      }
      LOG(5, "%s: vcpu %u page root=0x%"FMT64"x\n", __FUNCTION__, vcpu, mpn);
      ptMPNs->L4MPNs[0] = mpn;
      MonLoaderCallout_ImportPage(ctx->envCtx, mpn, vcpu);
   }
   ptMPNs->L4MPNCount = 1;

   for (i = 0; i < L3MPNsNeeded; i++) {
      if (MonLoaderCreatesPtLevel(PT_LEVEL_3)) {
         NOT_IMPLEMENTED();
      } else {
         VPN monVPN = firstVPN + i * PT_PAGES_PER_L4E;
         unsigned L4Off = PT_LPN_2_L4OFF(monVPN);
         PT_L1E pte;
         if (!MonLoaderCallout_GetPTE(ctx->envCtx, ptMPNs->L4MPNs[0], L4Off,
                                      vcpu, &pte)) {
            return ML_ERROR_CALLOUT_GETPTE;
         }
         LOG(5, "%s: monVPN=0x%"FMTVPN"x: L4E=0x%"FMT64"x\n", __FUNCTION__,
                 monVPN, pte);
         mpn = ML_PTE_2_PFN(pte);
         if (mpn == INVALID_MPN || !ML_PERMS_MATCH(pte, flags)) {
            return ML_ERROR_PAGE_TABLE_IMPORT;
         }
         MonLoaderCallout_ImportPage(ctx->envCtx, mpn, vcpu);
      }
      ptMPNs->L3MPNs[i] = mpn;
   }
   ptMPNs->L3MPNCount = L3MPNsNeeded;
      
   for (i = 0; i < L2MPNsNeeded; i++) {
      if (MonLoaderCreatesPtLevel(PT_LEVEL_2)) {
         NOT_IMPLEMENTED();
      } else {
         VPN monVPN = firstVPN + i * PT_PAGES_PER_L3E;
         unsigned L3Off = PT_LPN_2_L3OFF(monVPN);
         unsigned L3Page = L3EArrayIdx(ctx->vcpu.ASFirstVPN, monVPN);
         PT_L1E pte;
         if (!MonLoaderCallout_GetPTE(ctx->envCtx, ptMPNs->L3MPNs[L3Page],
                                      L3Off, vcpu, &pte)) {
            return ML_ERROR_CALLOUT_GETPTE;
         }
         LOG(5, "%s: monVPN=0x%"FMTVPN"x: L3E=0x%"FMT64"x\n", __FUNCTION__,
                 monVPN, pte);
         mpn = ML_PTE_2_PFN(pte);
         if (mpn == INVALID_MPN || !ML_PERMS_MATCH(pte, flags)) {
            return ML_ERROR_PAGE_TABLE_IMPORT;
         }
         MonLoaderCallout_ImportPage(ctx->envCtx, mpn, vcpu);
      }
      ptMPNs->L2MPNs[i] = mpn;
   }
   ptMPNs->L2MPNCount = L2MPNsNeeded;

   for (i = 0; i < L1MPNsNeeded; i++) {
      if (MonLoaderCreatesPtLevel(PT_LEVEL_1)) {
         NOT_IMPLEMENTED();
      } else {
         VPN monVPN = firstVPN + i * PT_PAGES_PER_L2E;
         unsigned L2Off = PT_LPN_2_L2OFF(monVPN);
         unsigned L2Page = L2EArrayIdx(ctx->vcpu.ASFirstVPN, monVPN);
         PT_L1E pte;
         if (!MonLoaderCallout_GetPTE(ctx->envCtx, ptMPNs->L2MPNs[L2Page],
                                      L2Off, vcpu, &pte)) {
            return ML_ERROR_CALLOUT_GETPTE;
         }
         LOG(5, "%s: monVPN=0x%"FMTVPN"x: L2E=0x%"FMT64"x\n", __FUNCTION__,
                 monVPN, pte);
         if (!ML_PERM_PRESENT(pte)) {
            ptMPNs->L1MPNs[i] = INVALID_MPN;
            continue;
         }
         mpn = ML_PTE_2_PFN(pte);
         if (mpn == INVALID_MPN || !ML_PERMS_MATCH(pte, flags)) {
            return ML_ERROR_PAGE_TABLE_IMPORT;
         }
         MonLoaderCallout_ImportPage(ctx->envCtx, mpn, vcpu);
      }
      ptMPNs->L1MPNs[i] = mpn;
   }
   ptMPNs->L1MPNCount = L1MPNsNeeded;

   ctx->vcpu.hasAddrSpace = TRUE;
   return ML_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderMapPageTables --
 *
 *      Maps in the page tables for the specified level, or verifies that the
 *      existing mappings match internal state and flags if the address space
 *      was imported.  Assumes pre-allocated memory at all levels and
 *      pre-connected L4->L1 page tables.
 *
 * Result:
 *      ML_OK if successful, some other value on error.
 *
 * Side effects:
 *      Page tables are updated if mappings are created.
 *
 *----------------------------------------------------------------------
 */
static MonLoaderError
MonLoaderMapPageTables(MonLoaderContext *ctx,      // IN/OUT
                       PT_Level          level,    // IN
                       uint64            flags,    // IN
                       VPN               monVPN,   // IN
                       uint64            monPages) // IN
{
   Bool verify = !MonLoaderCreatesPtLevel(level);
   uint64    i;
   unsigned  count;
   MPN      *ptMPNs;

   if (vmx86_debug) {
      if (!ctx->vcpu.hasAddrSpace) {
         return ML_ERROR_NO_ADDRSPACE;
      }
      if (level < PT_LEVEL_STOP || level > PT_MAX_LEVELS) {
         return ML_ERROR_ARGS;
      }
   }

   switch (level) {
      case PT_LEVEL_4:
         count = ctx->vcpu.ptMPNs.L4MPNCount;
         ptMPNs = ctx->vcpu.ptMPNs.L4MPNs;
         break;
      case PT_LEVEL_3:
         count = ctx->vcpu.ptMPNs.L3MPNCount;
         ptMPNs = ctx->vcpu.ptMPNs.L3MPNs;
         break;
      case PT_LEVEL_2:
         count = ctx->vcpu.ptMPNs.L2MPNCount;
         ptMPNs = ctx->vcpu.ptMPNs.L2MPNs;
         break;
      case PT_LEVEL_1:
         count = ctx->vcpu.ptMPNs.L1MPNCount;
         ptMPNs = ctx->vcpu.ptMPNs.L1MPNs;
         break;
      default:
         return ML_ERROR_ARGS;
         break;

      if (monPages != count) {
         return ML_ERROR_PAGE_TABLE_MAP_SIZE;
      }
      
      for (i = 0; i < monPages; i++) {
         VPN vpn = monVPN + i;
         if (verify) {
            PT_L1E l1e;
            if (ptMPNs[i] == INVALID_MPN && level == PT_LEVEL_1) {
               continue;
            }
            if (MonLoaderTranslateMonVPNToL1E(ctx, vpn, &l1e) != ML_OK ||
                ML_PTE_2_PFN(l1e) != ptMPNs[i] || !ML_PERMS_MATCH(l1e, flags)) {
               return ML_ERROR_PAGE_TABLE_VERIFY;
            }
         } else {
            NOT_IMPLEMENTED();
         }
      }
   }
   return ML_OK;
} 


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderZero --
 *
 *      Allocates new pages, zeroes them and maps them on the current VCPU.
 *
 * Result:
 *      ML_OK if successful, some other value on error.  The number of
 *      MPNs allocated in *allocs is incremented.
 *
 * Side effects:
 *      Allocations, zeroing, mapping and accounting are performed.
 *
 *----------------------------------------------------------------------
 */
static MonLoaderError
MonLoaderZero(MonLoaderContext *ctx,      // IN/OUT
              uint64            flags,    // IN
              VPN               monVPN,   // IN
              uint64            numPages, // IN
              uint64            monPages, // IN
              unsigned         *allocs)   // IN/OUT
{
   uint64 i;

   if (numPages == 0 || numPages > monPages) {
      return ML_ERROR_SIZE;
   }
   for (i = 0; i < numPages; i++) {
      MPN mpn;
      MonLoaderError ret = MonLoaderAllocMPN(ctx, &mpn);
      if (ret != ML_OK) {
         return ret;
      }
      if (!MonLoaderCallout_FillPage(ctx->envCtx, 0, mpn,
                                     ctx->vcpu.currentVCPU)) {
         return ML_ERROR_CALLOUT_ZERO;
      }
      ret = MonLoaderMapMPN(ctx, mpn, flags, monVPN + i);
      if (ret != ML_OK) {
         return ret;
      }
      *allocs += 1;
   }
   return ML_OK;
}
   

/*
 *----------------------------------------------------------------------
 *
 * MonLoaderCopyFromBlob --
 *
 *      Allocates new pages, copies blob contents and maps the pages.
 *
 * Result:
 *      ML_OK if successful, some other value on error.  The number of
 *      MPNs allocated in *allocs is incremented.
 *
 * Side effects:
 *      Allocations, copying, mapping and accounting are performed.
 *
 *----------------------------------------------------------------------
 */
static MonLoaderError
MonLoaderCopyFromBlob(MonLoaderContext *ctx,        // IN/OUT
                      uint64            flags,      // IN
                      VPN               monVPN,     // IN
                      uint64            monBytes,   // IN
                      uint64            blobOffset, // IN
                      uint64            blobSize,   // IN
                      unsigned         *allocs)     // IN/OUT
{
   uint64 bytesLeft = blobSize;

   if (blobSize > monBytes || blobSize == 0) {
      return ML_ERROR_SIZE; /* Entry corrupt: size mismatch. */
   }
   while (bytesLeft) {
      MPN mpn;
      uint64 toCopy = MIN(bytesLeft, PAGE_SIZE);

      MonLoaderError ret = MonLoaderAllocMPN(ctx, &mpn);
      if (ret != ML_OK) {
         return ret;
      }

      ret = MonLoaderMapMPN(ctx, mpn, flags, monVPN);
      if (ret != ML_OK) {
         return ret;
      }

      if (!MonLoaderCallout_CopyFromBlob(ctx->envCtx, blobOffset, toCopy, mpn,
                                         ctx->vcpu.currentVCPU)) {
         return ML_ERROR_CALLOUT_COPY;
      }

      monVPN++;
      blobOffset += toCopy;
      bytesLeft -= toCopy;
      *allocs += 1;
   }
   return ML_OK;
}
 

/*
 *----------------------------------------------------------------------
 *
 * MonLoaderShareWork --
 *
 *      Maps user or host pages in.  monPages is a maximum, so partial sharing
 *      is considered successful and remaining monitor pages will not be mapped.
 *
 * Result:
 *      ML_OK if successful, some other value on error.
 *
 * Side effects:
 *      User or host pages may be mapped in.
 *
 *----------------------------------------------------------------------
 */
static MonLoaderError
MonLoaderShareWork(MonLoaderContext *ctx,        // IN/OUT
                   uint64            flags,      // IN
                   VPN               monVPN,     // IN
                   uint64            monPages,   // IN
                   uint64            subIndex,   // IN
                   Bool              user)       // IN
{
   Vcpuid vcpu = ctx->vcpu.currentVCPU;
   uint64 i;

   if (monPages == 0 || monPages > UINT_MAX) {
      return ML_ERROR_SIZE; /* Entry corrupt: size mismatch. */
   }
   for (i = 0; i < monPages; i++) {
      MPN mpn;
      MonLoaderError ret;
      unsigned pgNum = (unsigned)i;

      if (user) {
         mpn = MonLoaderCallout_GetSharedUserPage(ctx->envCtx, subIndex, pgNum,
                                                  vcpu);
      } else {
         mpn = MonLoaderCallout_GetSharedHostPage(ctx->envCtx, subIndex, pgNum,
                                                  vcpu);
      }
      if (mpn == INVALID_MPN) {
         if (subIndex == MONLOADER_HT_MAP_IDX) {
            /*
             * This item is tied to a vmkernel feature.
             * When the feature is disabled, there is nothing to share.
             */
            ASSERT(vmx86_server);
            return ML_OK;
         }
         /* Partial sharing is allowed.  Return success if any occurred. */
         return i != 0 ? ML_OK : ML_ERROR_SHARE;
      }
      ret = MonLoaderMapMPN(ctx, mpn, flags, monVPN + i);
      if (ret != ML_OK) {
         return ret;
      }
   }
   return ML_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderShareFromUser --
 *
 *      Maps user pages in.  monPages is a maximum, so partial sharing is
 *      considered successful and remaining monitor pages will not be mapped.
 *
 * Result:
 *      ML_OK if successful, some other value on error.
 *
 * Side effects:
 *      User pages may be mapped in.
 *
 *----------------------------------------------------------------------
 */
static MonLoaderError
MonLoaderShareFromUser(MonLoaderContext *ctx,        // IN/OUT
                       uint64            flags,      // IN
                       VPN               monVPN,     // IN
                       uint64            monPages,   // IN
                       uint64            subIndex)   // IN
{
   return MonLoaderShareWork(ctx, flags, monVPN, monPages, subIndex, TRUE);
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderShareFromHost --
 *
 *      Maps host pages in.  monPages is a maximum, so partial sharing is
 *      considered successful and remaining monitor pages will not be mapped.
 *
 * Result:
 *      ML_OK if successful, some other value on error.
 *
 * Side effects:
 *      Host pages may be mapped in.
 *
 *----------------------------------------------------------------------
 */
static MonLoaderError
MonLoaderShareFromHost(MonLoaderContext *ctx,        // IN/OUT
                       uint64            flags,      // IN
                       VPN               monVPN,     // IN
                       uint64            monPages,   // IN
                       uint64            subIndex)   // IN
{
   return MonLoaderShareWork(ctx, flags, monVPN, monPages, subIndex, FALSE);
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoaderShareFromBlob --
 *
 *      Maps blob pages in. monPages is a maximum, so partial sharing is
 *      considered successful and remaining monitor pages will not be
 *      mapped. blobOffset must be page-aligned and blobSize must be a
 *      multiple of PAGE_SIZE. Writeable mappings are not allowed.
 *
 *     Result:
 *          ML_OK if successful, some other value on error.
 *
 *     Side effects:
 *          Blob pages may be mapped in.
 *
 *----------------------------------------------------------------------
 */
static MonLoaderError
MonLoaderShareFromBlob(MonLoaderContext *ctx,        // IN/OUT
                       uint64            flags,      // IN
                       VPN               monVPN,     // IN
                       uint64            monBytes,   // IN
                       uint64            blobOffset, // IN
                       uint64            blobSize)   // IN
{
   uint64 endOff = blobOffset + blobSize;

   if ((blobOffset & (PAGE_SIZE - 1)) != 0 ||
       (blobSize & (PAGE_SIZE - 1)) != 0 ||
       ML_PERM_WRITEABLE(flags)) {
      return ML_ERROR_SHARE;
   }
   if (blobSize > monBytes || blobSize == 0) {
      return ML_ERROR_SIZE;
   }
   while (blobOffset < endOff) {
      MonLoaderError ret;
      MPN mpn = MonLoaderCallout_GetBlobMpn(ctx->envCtx, blobOffset);
      if (mpn == INVALID_MPN) {
         return ML_ERROR_INVALID_VPN;
      }
      ret = MonLoaderMapMPN(ctx, mpn, flags, monVPN);
      if (ret != ML_OK) {
         return ret;
      }
      blobOffset += PAGE_SIZE;
      monVPN++;
   }
   ASSERT(blobOffset == endOff);
   return ML_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * MonLoader_Process --
 *
 *      Builds and populates a monitor address space per the given a header and
 *      VCPU count.  Calls out to support functions.
 *
 * Result:
 *      ML_OK if successful, some other value on error.  If an error occurs,
 *      *line contains the line in the table at which the failure occurred and
 *      *vcpu contains the VCPU for which processing was running.
 *
 * Side effects:
 *      An address space and page table mappings are created or verified, for
 *      all VCPUs.  Callouts are called.
 *
 *----------------------------------------------------------------------
 */
MonLoaderError
MonLoader_Process(MonLoaderHeader  *header,   // IN/OUT
                  unsigned          numVCPUs, // IN
                  void             *args,     // IN
                  unsigned         *line,     // OUT
                  Vcpuid           *vcpu)     // OUT
{
   unsigned i;
   MonLoaderError ret;
   MonLoaderContext ctx;

   if (numVCPUs > MAX_VCPUS) {
      return ML_ERROR_ARGS;
   }

   if (header->magic != LOADER_HEADER_MAGIC) {
      return ML_ERROR_MAGIC;
   }

   if (vmx86_debug && !line) {
      return ML_ERROR_ARGS;
   }
   *line = LINE_INVALID;

   if (header->count == 0) {
      return ML_ERROR_TABLE_MISSING;
   }

   if (!MonLoaderCallout_Init(args, &ctx.envCtx, numVCPUs)) {
      return ML_ERROR_CALLOUT_INIT;
   }
   ret = ML_OK;
   for (*vcpu = 0; *vcpu < numVCPUs && ret == ML_OK; (*vcpu) += 1) {
      /* Reset VCPU-specific part of context. */
      memset(&ctx.vcpu, 0, sizeof ctx.vcpu);
      ctx.vcpu.currentVCPU = *vcpu;
      ctx.vcpu.hasAddrSpace = FALSE;
      for (i = 0; i < header->count && ret == ML_OK; i++) {
         MonLoaderEntry *entry = &header->entries[i];
         MonLoaderContentType content    = entry->content;
         MonLoaderSourceType  source     = entry->source;
         VPN                  monVPN     = entry->monVPN;
         uint64               blobOffset = entry->blobSrc.offset;
         uint64               blobSize   = entry->blobSrc.size;
         uint64               flags      = entry->flags;
         uint64               monPages   = entry->monPages;
         uint64               monBytes   = PAGES_2_BYTES(monPages);
         uint64               subIndex   = entry->subIndex;
         uint64               bspOnly    = entry->bspOnly;
         *line = i;

         /*
          * The entry is specific to the bootstrap, which only runs in VCPU 0.
          */
         if (bspOnly && !IS_BOOT_VCPUID(*vcpu)) {
            continue;
         }
         if (monPages == 0 || monVPN + monPages - 1 > VPN_MAX) {
            ret = ML_ERROR_SIZE;
            continue;
         }

         switch (content) {
            case ML_CONTENT_ADDRSPACE:
               if (ctx.vcpu.hasAddrSpace) {
                  ret = ML_ERROR_DUPLICATE;
                  continue;
               }
               /* Create or verify the address space and page table MPNs. */
               ret = MonLoaderCreateAddressSpace(&ctx, monVPN, flags, monPages);
               break;
            case ML_CONTENT_PAGETABLE_L4:
            case ML_CONTENT_PAGETABLE_L3:
            case ML_CONTENT_PAGETABLE_L2:
            case ML_CONTENT_PAGETABLE_L1:
               /* Create or verify page table mappings. */
               ret = MonLoaderMapPageTables(&ctx, CONTENT_TO_PTLEVEL(content),
                                            flags, monVPN, monPages);
               break;
            case ML_CONTENT_ALLOCZERO: {
               uint64 numPages;
               if (source == ML_SOURCE_NONE) {
                  numPages = monPages;
               } else if (source == ML_SOURCE_BLOB) {
                  /*
                   * Used for loading the uninitialized data section
                   * (.bss), which has a blob size but no blob image
                   * associated with it.
                   */
                  numPages = CEILING(blobSize, PAGE_SIZE);
               } else {
                  ret = ML_ERROR_SOURCE_INVALID;
                  break;
               }
               ret = MonLoaderZero(&ctx, flags, monVPN, numPages, monPages,
                                   &entry->allocs);
               break;
            }
            case ML_CONTENT_COPY:
               switch (source) {
                  case ML_SOURCE_BLOB:
                     ret = MonLoaderCopyFromBlob(&ctx, flags, monVPN, monBytes,
                                                 blobOffset, blobSize,
                                                 &entry->allocs);
                     break;
                  default:
                     ret = ML_ERROR_SOURCE_INVALID;
                     break;
               }
               break;
            case ML_CONTENT_SHARE:
               switch (source) {
                  case ML_SOURCE_USER:
                     ret = MonLoaderShareFromUser(&ctx, flags, monVPN,
                                                  monPages, subIndex);
                     break;
                  case ML_SOURCE_HOST:
                     ret = MonLoaderShareFromHost(&ctx, flags, monVPN,
                                                  monPages, subIndex);
                     break;
                  case ML_SOURCE_BLOB:
                     ret = MonLoaderShareFromBlob(&ctx, flags, monVPN,
                                                  monBytes, blobOffset,
                                                  blobSize);
                     break;
                  default:
                     ret = ML_ERROR_SOURCE_INVALID;
                     break;
               }
               break;
            case ML_CONTENT_INVALID:
            default:
               ret = ML_ERROR_CONTENT_INVALID;
               break;
         }
      }
   }
#ifdef VMKERNEL
   if (ret == ML_OK) {
      if (ctx.vcpu.ASLastVPN == 0) {
         ret = ML_ERROR_NO_ADDRSPACE;
      }
   }
#endif
   MonLoaderCallout_CleanUp(ctx.envCtx);
   return ret;
}
