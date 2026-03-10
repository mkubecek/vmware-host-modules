/*********************************************************
 * Copyright (C) 2017-2018 VMware, Inc. All rights reserved.
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
 * vmmblob.h --
 *
 *     VMM blob management.
 */

#ifndef VMMBLOB_H
#define VMMBLOB_H

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

struct VMDriver;
struct MonLoaderHeader;

typedef struct VmmBlobInfo {
   uint8                  *blobPtr;
   uint32                  numBytes;
   MPN                    *mpns;
   struct MonLoaderHeader *header;
} VmmBlobInfo;

void   VmmBlob_Cleanup(VmmBlobInfo *bi);
Bool   VmmBlob_Load(VA64 bsBlobAddr, uint32 numBytes, uint32 headerOffset,
                    VmmBlobInfo **blobInfo);
MPN    VmmBlob_GetMpn(struct VMDriver *vm, uint64 blobOffset);
MPN    VmmBlob_GetHeaderMpn(struct VMDriver *vm);
uint8 *VmmBlob_GetPtr(VMDriver *vm);
uint64 VmmBlob_GetSize(VMDriver *vm);

#endif /* VMMBLOB_H */
