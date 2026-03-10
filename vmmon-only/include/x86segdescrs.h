/*********************************************************
 * Copyright (C) 2006-2020 VMware, Inc. All rights reserved.
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
 * x86segdescrs.h --
 *
 *	Type definitions for the x86 segment descriptors.
 */

#ifndef _X86SEGDESCRS_H_
#define _X86SEGDESCRS_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vm_basic_defs.h"

/*
 * Segment Descriptors.
 */

typedef struct Descriptor {
   unsigned   limit_lo  : 16;
   unsigned   base_lo   : 16;
   unsigned   base_mid  : 8;
   unsigned   type      : 4;
   unsigned   S         : 1;
   unsigned   DPL       : 2;
   unsigned   present   : 1;
   unsigned   limit_hi  : 4;
   unsigned   AVL       : 1;
   unsigned   longmode  : 1;
   unsigned   DB        : 1;
   unsigned   gran      : 1;
   unsigned   base_hi   : 8;
} Descriptor;

/*
 * 16-byte system descriptors for 64-bit mode.
 */

#pragma pack(push, 1)
typedef struct Descriptor64 {
   uint64   limit_lo  : 16;   // Limit bits 15-0.
   uint64   base_lo   : 24;   // Base bits  23-0.
   uint64   type      : 4;
   uint64   S         : 1;
   uint64   DPL       : 2;
   uint64   present   : 1;
   uint64   limit_hi  : 4;    // Limit bits 19-16.
   uint64   AVL       : 1;
   uint64   reserved0 : 2;
   uint64   gran      : 1;
   uint64   base_mid  : 8;    // Base bits 31-24.
   uint64   base_hi   : 32;   // Base bits 63-32.
   uint64   reserved1 : 8;
   uint64   ext_attrs : 5;
   uint64   reserved2 : 19;
} Descriptor64;
#pragma pack(pop)

typedef union {
   Descriptor desc;
   uint32     word[2];
   uint64     qword;
} DescriptorUnion;

typedef union {
   Descriptor64 desc;
   Descriptor   part[2];
   uint32       word[4];
   uint64       qword[2];
} Descriptor64Union;


#endif // ifndef _X86SEGDESCRS_H_
