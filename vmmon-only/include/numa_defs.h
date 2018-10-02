/*********************************************************
 * Copyright (C) 2006,2009-2017 VMware, Inc. All rights reserved.
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
 * numa_defs.h --
 *	This is the internal header file for the NUMA module.
 */

#ifndef _NUMA_DEFS_H
#define _NUMA_DEFS_H

#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMKERNEL

#include "includeCheck.h"
#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "cpu_defs.h"

/* Machine NUMA nodes */
typedef uint32 NUMA_Node;
typedef uint32 NUMA_PxmID;
typedef uint64 NUMA_NodeMask;
typedef uint8  NUMA_MemRangeID;

/*
 * Constants
 */
#define NUMA_MAX_NODES              64
#define NUMA_MAX_CPUS_PER_NODE      (vmx86_server ? MAX_PCPUS : 32)
#ifdef VMKBOOT
#define NUMA_MAX_MEM_RANGES         64
#else
#define NUMA_MAX_MEM_RANGES         256
#endif
#define INVALID_NUMANODE            ((NUMA_Node)-1)
#define NUMA_NODE_MASK_ANY          ((NUMA_NodeMask)-1)
#define NUMA_NODE_MASK_NONE         ((NUMA_NodeMask)0)


/*
 * Structures
 */
typedef struct {
   MPN          startMPN;
   MPN          endMPN;
   NUMA_PxmID   id;
   Bool         isReliable;
   Bool         isVolatile;
} NUMA_MemRange;

typedef struct NUMA_MemRangesList {
   uint64        numMemRanges;
   NUMA_MemRange memRange[NUMA_MAX_MEM_RANGES];
} NUMA_MemRangesList;


typedef struct NUMA_MemRangesListRef {
   uint64        numMemRanges;
   const NUMA_MemRange *memRange;
} NUMA_MemRangesListRef;

#endif // _NUMA_DEFS_H
