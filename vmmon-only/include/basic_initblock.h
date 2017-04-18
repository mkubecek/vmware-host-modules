/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * basic_initblock.h --
 *
 *    VM initialization block.
 */

#ifndef _BASIC_INITBLOCK_H_
#define _BASIC_INITBLOCK_H_


#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"


#include "vcpuid.h"


#define MAX_INITBLOCK_CPUS     128


typedef
#include "vmware_pack_begin.h"
struct InitBlock {
   uint32 magicNumber;     /* Magic number (INIT_BLOCK_MAGIC) */
   Vcpuid numVCPUs;
   VA64   crosspage[MAX_INITBLOCK_CPUS];
   uint32 vmInitFailurePeriod;
   LA64   crossGDTHKLA;
   MPN    crossGDTMPNs[5];  // CROSSGDT_NUMPAGES
}
#include "vmware_pack_end.h"
InitBlock;


#endif // _BASIC_INITBLOCK_H_
