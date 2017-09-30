/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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


#ifndef _MEMDEFAULTS_H_
#define _MEMDEFAULTS_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "vm_basic_math.h"
#include "vm_basic_defs.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define MEMDEFAULTS_MIN_HOST_PAGES   MBYTES_2_PAGES(128)


/*
 *-----------------------------------------------------------------------------
 *
 * MemDefaults_CalcMaxLockedPages --
 *
 *      Calculate the rough estimate of the maximum amount of memory
 *      that can be locked (total for the kernel, all VMs, and other apps),
 *      based on the size of host memory as supplied in pages.
 *
 * Results:
 *      The estimated maximum memory that can be locked in pages.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE unsigned
MemDefaults_CalcMaxLockedPages(unsigned hostPages)  // IN:
{
   unsigned reservedPages;

#if defined(__APPLE__)
   /*
    * Reserve (25% of the host memory + 512 MB) or 4 GB, whichever is lower.
    * 4 GB hosts perform poorly with less than 1.5 GB reserved, and large
    * memory hosts (>= 16 GB) may want to use more than 75% for VMs.
    */
   reservedPages = MIN((hostPages / 4) + MBYTES_2_PAGES(512),
                       GBYTES_2_PAGES(4));
#elif defined(_WIN32)
   {
      unsigned int hostGig = PAGES_2_GBYTES(hostPages);

      if (hostGig <= 4) {
         reservedPages = hostPages / 4;
      } else if (hostGig >= 16) {
         reservedPages = hostPages / 8;
      } else {
         /*
          * Increment by 1/32 for each 4GB of host mem between 4 and 16.
          * See PR779556.
          */
         reservedPages = hostPages / 32 * (8 - hostGig / 4);
      }
   }
#else  // Linux
   reservedPages = hostPages / 8;
#endif

   reservedPages = MAX(reservedPages, MEMDEFAULTS_MIN_HOST_PAGES);

   return hostPages > reservedPages ? hostPages - reservedPages : 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MemDefaults_CalcMaxLockedMBs --
 *
 *      Calculate the rough estimate of the maximum amount of memory
 *      that can be locked based on the size of host memory as supplied
 *      in MBytes.
 *
 * Results:
 *      The estimated maximum memory that can be locked in MBytes.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
MemDefaults_CalcMaxLockedMBs(uint32 hostMem)  // IN:
{
   return PAGES_2_MBYTES(
             MemDefaults_CalcMaxLockedPages(MBYTES_2_PAGES(hostMem)));
}


/*
 *-----------------------------------------------------------------------------
 *
 * MemDefaults_CalcMinReservedMBs --
 *
 *      Provide a lower bound on the user as to the minimum amount
 *      of memory to lock based on the size of host memory. This
 *      threshold might be crossed as a result of the user limiting
 *      the amount of memory consumed by all VMs.
 *
 * Results:
 *      The minimum locked memory requirement in MBytes.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
MemDefaults_CalcMinReservedMBs(uint32 hostMem)  // IN:
{
   if (hostMem < 512) {
      return 32;
   } else if (hostMem < 1024) {
      return 64;
   } else {
      return 128;
   }
}


void MemDefaults_GetReservedMemory(uint32 *host, uint32 *min,
                                   uint32 *max, uint32 *recommended);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
