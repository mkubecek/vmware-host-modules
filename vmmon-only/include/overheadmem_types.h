/*********************************************************
 * Copyright (C) 2001-2013 VMware, Inc. All rights reserved.
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
 * overheadmem_types.h
 *
 *	Types for tracking memory overheads.
 */

#ifndef _OVERHEADMEM_TYPES_H
#define _OVERHEADMEM_TYPES_H

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "vm_basic_types.h"

/*
 * There are 4 types of memory we lock on the host.  Memory can be Mem_Mapped in
 * the vmx, anonymous memory for use by monitor is not mapped permanently in any
 * address space, guest memory regions other than main memory (can be
 * locked/unlocked on hosted but not on ESX), and main memory which can be
 * locked/unlocked in hosted and esx.
 *
 * In addition, the vmx may malloc memory or declare (large) static structures.
 * Neither of these is locked on hosted platforms and the hostOS may swap it.
 * Therefore, on hosted platforms we do not track this memory and instead
 * include a working set component (sched.mem.hosted.perVMOverheadMBs).
 * On ESX, this memory must be accounted for so we account it to user
 * (nonpaged) overhead.  At present, the accounting is extremely coarse,
 * and only aggregate sizes are hard-coded (see PR363997).
 */
typedef enum OvhdMemType {
   OvhdMem_memmap,
   OvhdMem_anon,
   OvhdMem_guest,
   OvhdMem_mainmem,
   OvhdMem_malloc,
   OvhdMem_static,
   OvhdMem_text,
   NumOvhdMemTypes
} OvhdMemType;

#define OvhdMemMask(type)    (1 << type)

#define OVHDMEM_NONE      0x0
#define OVHDMEM_MEMMAP    0x1 // OvhdMemMask(OvhdMem_memmap)
#define OVHDMEM_ANON      0x2 // OvhdMemMask(OvhdMem_anon)
#define OVHDMEM_GUEST     0x4 // OvhdMemMask(OvhdMem_guest)
#define OVHDMEM_MAINMEM   0x8 // OvhdMemMask(OvhdMem_mainmem)
#define OVHDMEM_MALLOC   0x10 // OvhdMemMask(OvhdMem_malloc)
#define OVHDMEM_STATIC   0x20 // OvhdMemMask(OvhdMem_static)
#define OVHDMEM_TEXT     0x40 // OvhdMemMask(OvhdMem_text)
#define OVHDMEM_ALL_USER (OVHDMEM_MEMMAP | OVHDMEM_GUEST | OVHDMEM_MAINMEM | \
                          OVHDMEM_MALLOC | OVHDMEM_STATIC | OVHDMEM_TEXT)
#define OVHDMEM_ALL      (OVHDMEM_ALL_USER | OVHDMEM_ANON)

/* ... and four categories of memory sources. */
typedef enum OvhdMemCategory {
   OvhdMemCat_paged,
   OvhdMemCat_nonpaged,
   OvhdMemCat_excluded,
   OvhdMemCat_anonymous,
   NumOvhdMemCategories
} OvhdMemCategory;

#define OVHDMEM_PAGED     (OVHDMEM_MALLOC  | OVHDMEM_STATIC)
#define OVHDMEM_NONPAGED  (OVHDMEM_GUEST   | OVHDMEM_MEMMAP)
#define OVHDMEM_EXCLUDED  (OVHDMEM_MAINMEM | OVHDMEM_TEXT)

#if ((OVHDMEM_PAGED & OVHDMEM_NONPAGED) != 0)    ||                          \
    ((OVHDMEM_NONPAGED & OVHDMEM_EXCLUDED) != 0) ||                          \
    ((OVHDMEM_PAGED & OVHDMEM_EXCLUDED) != 0)    ||                          \
    ((OVHDMEM_PAGED | OVHDMEM_NONPAGED | OVHDMEM_EXCLUDED | OVHDMEM_ANON) != \
              OVHDMEM_ALL)
#error Overheadmem categories do not form a partition of the overheads
#endif

/* Categories of overhead for 32-bit and 64-bit mode. */
typedef struct OvhdMem_Overheads {
   uint32 paged;
   uint32 nonpaged;
   uint32 anonymous;
   uint32 text;
} OvhdMem_Overheads;

typedef struct OvhdMem_Deltas {
   int32 paged;
   int32 nonpaged;
   int32 anonymous;
} OvhdMem_Deltas;


/* Types for tracking vmx (user) overheads. */

#define OVHDMEM_MAX_NAME_LEN 36

/* Types for tracking vmm overheads. */

typedef struct OvhdMemUsage {
   uint32 reserved; // pages
   uint32 used;     // pages
} OvhdMemUsage;

typedef struct OvhdMemNode {
   OvhdMemUsage usage;               // allocated and rsvd bytes for source
   OvhdMemUsage maxUsage;            // max allocated and rsvd bytes for source
   char name[OVHDMEM_MAX_NAME_LEN];  // name of overhead source
   OvhdMemType type;                 // how/where memory for source is managed
} OvhdMemNode;

#endif
