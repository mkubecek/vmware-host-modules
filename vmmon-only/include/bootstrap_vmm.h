/*********************************************************
 * Copyright (C) 2015,2017 VMware, Inc. All rights reserved.
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
 * bootstrap_vmm.h --
 *
 *    Public VMM bootstrap declarations.
 */

#ifndef _BOOTSTRAP_VMM_H
#define _BOOTSTRAP_VMM_H

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_USERLEVEL

#include "includeCheck.h"
#include "vm_basic_types.h"
#include "monAddrLayout.h"

#define BOOTSTRAP_MAGIC 42
#define BOOTSTRAP_MAX_GDT_DESCS 2

typedef struct BSVMM_GDTInitEntry {
   uint16 index;
   LA32   base;
   VA32   limit;
   uint32 type;
   uint32 S;
   uint32 DPL;
   uint32 present;
   uint32 longmode;
   uint32 DB;
   uint32 gran;
} BSVMM_GDTInitEntry;

typedef struct BSVMM_GDTInit {
   BSVMM_GDTInitEntry entries[BOOTSTRAP_MAX_GDT_DESCS];
} BSVMM_GDTInit;

typedef struct BSVMM_HostParams {
   uint32           magic;
   VMM64_AddrLayout addrLayout;
   BSVMM_GDTInit    gdtInit;
} BSVMM_HostParams;

BSVMM_HostParams* BSVMM_Validate(void *buf, uint32 nbytes);

#endif
