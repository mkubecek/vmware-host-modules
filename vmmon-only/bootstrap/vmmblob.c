/*********************************************************
 * Copyright (C) 2017-2020 VMware, Inc. All rights reserved.
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
 * vmmblob.c --
 *
 *     VMM blob management.
 */

#ifdef __linux__
#   include "driver-config.h"
#endif
#include "vm_assert.h"
#include "hostif.h"
#include "vmmblob.h"
#include "monLoader.h"

#define VMMBLOB_SIZE_MAX (22 * 1024 * 1024) /* Ensure enough space for
                                             * obj build with * GCOV_VMM=1. */

/*
 *----------------------------------------------------------------------
 *
 * VmmBlob_GetPtr --
 *
 *      Returns a pointer to the buffer containing the VMM blob.
 *
 * Results:
 *      A pointer.
 *
 *----------------------------------------------------------------------
 */

uint8 *
VmmBlob_GetPtr(VMDriver *vm)
{
   return vm->blobInfo->blobPtr;
}


/*
 *----------------------------------------------------------------------
 *
 * VmmBlob_GetSize --
 *
 *      Returns the size of the VMM blob in bytes.
 *
 * Results:
 *      Number of bytes.
 *
 *----------------------------------------------------------------------
 */

uint64
VmmBlob_GetSize(VMDriver *vm)
{
   return vm->blobInfo->numBytes;
}


/*
 *----------------------------------------------------------------------
 *
 * VmmBlob_GetMpn --
 *
 *      Returns the MPN backing a VMM blob page.
 *
 * Results:
 *      An MPN, or INVALID_MPN.
 *
 *----------------------------------------------------------------------
 */

MPN
VmmBlob_GetMpn(VMDriver *vm, uint64 pgOffset)
{
   VmmBlobInfo *bi = vm->blobInfo;
   uint32 blobNumPages = CEILING(bi->numBytes, PAGE_SIZE);
   MPN mpn = INVALID_MPN;

   if (pgOffset < blobNumPages) {
      mpn = bi->mpns[pgOffset];
   }
   return mpn;
}


/*
 *----------------------------------------------------------------------
 *
 * VmmBlob_GetHeaderMpn --
 *
 *      Returns the MPN backing the MonLoader header.
 *
 * Results:
 *      An MPN, or INVALID_MPN.
 *
 *----------------------------------------------------------------------
 */

MPN
VmmBlob_GetHeaderMpn(VMDriver *vm)
{
   VmmBlobInfo *bi = vm->blobInfo;
   uint64 headerOffset = (uint8 *)bi->header - (uint8 *)bi->blobPtr;
   ASSERT((headerOffset & (PAGE_SIZE - 1)) == 0);
   return VmmBlob_GetMpn(vm, BYTES_2_PAGES(headerOffset));
}


/*
 *----------------------------------------------------------------------
 *
 * VmmBlob_Cleanup --
 *
 *      Cleans up VMM blob state by freeing its memory and associated
 *      metadata.
 *
 * Results:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VmmBlob_Cleanup(VmmBlobInfo *bi)
{
   const uint32 numPages = CEILING(bi->numBytes, PAGE_SIZE);

   HostIF_FreeKernelPages(numPages, bi->blobPtr);
   HostIF_FreeKernelMem(bi->mpns);
   HostIF_FreeKernelMem(bi);
}


/*
 *----------------------------------------------------------------------
 *
 * VmmBlob_Load --
 *
 *      Copies and instantiates a VMM bootstrap blob from userspace.
 *
 * Results:
 *      TRUE if successful, FALSE if not.
 *
 *----------------------------------------------------------------------
 */

Bool
VmmBlob_Load(UserVA64 blobAddr,
             uint32 numBytes,
             uint32 headerOffset,
             VmmBlobInfo **blobInfo)
{
   uint8 *blob = NULL;
   MPN *mpns = NULL;
   MonLoaderHeader *header;
   VmmBlobInfo *bi = NULL;
   uint32 numPages = CEILING(numBytes, PAGE_SIZE);
   size_t fixedHdrSize = MonLoader_GetFixedHeaderSize();

   if (numBytes > VMMBLOB_SIZE_MAX || headerOffset > numBytes ||
       fixedHdrSize > numBytes - headerOffset) {
      Warning("Invalid arguments for processing bootstrap. "
              "Header offset: %u, Fixed header size: %"FMTSZ"u bytes, "
              "Blob size: %u bytes\n", headerOffset, fixedHdrSize, numBytes);
      goto error;
   }
   mpns = HostIF_AllocKernelMem(numPages * sizeof(*mpns), FALSE);
   if (mpns == NULL) {
      Warning("Memory allocation for VMM bootstrap blob failed\n");
      goto error;
   }
   bi = HostIF_AllocKernelMem(sizeof *bi, FALSE);
   if (bi == NULL) {
      Warning("Memory allocation for VMM bootstrap blob failed\n");
      goto error;
   }
   blob = HostIF_AllocKernelPages(numPages, mpns);
   if (blob == NULL) {
      Warning("Memory allocation for VMM bootstrap blob failed\n");
      goto error;
   }
   if (HostIF_CopyFromUser(blob, blobAddr, numBytes) != 0) {
      Warning("Error copying VMM bootstrap blob from userspace\n");
      goto error;
   }
   header = (MonLoaderHeader *)(blob + headerOffset);
   if (header->count > (numBytes - headerOffset - fixedHdrSize) /
                        sizeof (MonLoaderEntry)) {
      Warning("Invalid arguments for processing bootstrap. "
              "Header offset: %u, Number of header entries: %u, "
              "Blob size: %u bytes\n", headerOffset, header->count, numBytes);
      goto error;
   }
   bi->mpns = mpns;
   bi->blobPtr = blob;
   bi->numBytes = numBytes;
   bi->header = header;
   *blobInfo = bi;

   return TRUE;

error:
   if (blob != NULL) {
      HostIF_FreeKernelPages(numPages, blob);
   }
   if (bi != NULL) {
      HostIF_FreeKernelMem(bi);
   }
   if (mpns != NULL) {
      HostIF_FreeKernelMem(mpns);
   }
   return FALSE;
}
