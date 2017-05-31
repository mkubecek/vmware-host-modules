/*********************************************************
 * Copyright (C) 2006-2014 VMware, Inc. All rights reserved.
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
 * crossgdt.h --
 *
 *      This GDT is used for switching between monitor and host context.
 *      It contains the host and monitor basic segment descriptors.
 *      There is only one in the whole host system, shared by all VMs.
 *      It is allocated when the first VCPU is started and freed when the
 *      driver is unloaded.
 */

#ifndef _CROSSGDT_H_
#define _CROSSGDT_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_defs.h"  // PAGE_SIZE
#include "x86types.h"       // Descriptor

typedef struct CrossGDT {
   Descriptor gdtes[0x5000 / sizeof (Descriptor)];  // 0x5000 > GDT_LIMIT
} CrossGDT;

#define CROSSGDT_NUMPAGES ((sizeof (CrossGDT) + PAGE_SIZE - 1) / PAGE_SIZE)

/*
 * Out of the 5 pages, only the first and last are really used.
 *
 * All we need to map are the first and last pages.  This mask tells
 * the setup code which pages it can put stuff in and it tells the
 * mapping and invalidation code which pages are mapped and unmapped.
 */
#define CROSSGDT_PAGEMASK 0x11

/*
 * These segments are placed in the first crossGDT page.  We assume
 * they do not overlap any host segments (checked by
 * Task_InitCrossGDT).  The only real requirement is that they (and
 * the host CS/SS) be physically contiguous with the start of the
 * crossGDT so they will remain valid when paging is turned off.
 *
 * As of this writing (Nov 2006), host GDT limits:
 *     Linux 64 bit:  80 (yes 80, not 7F)
 *           32 bit:  FF
 *     MacOS 64 bit:  8F
 *           32 bit:  8F
 *   Windows 64 bit:  6F
 *           32 bit: 3FF
 */
#define CROSSGDT_LOWSEG   (0x0FD0 / 8)  // all host segs must be below this
#define CROSSGDT_64BITCS  (0x0FD0 / 8)  // 64-bit code segment
#define CROSSGDT_64BITSS  (0x0FD8 / 8)  // 64-bit data segment
#define CROSSGDT_FLAT32CS (0x0FE0 / 8)  // 32-bit flat code seg
#define CROSSGDT_FLAT32SS (0x0FE8 / 8)  // 32-bit flat data seg
#define CROSSGDT_MON32CS  (0x0FF0 / 8)  // 32-bit FFC00000 base code seg
#define CROSSGDT_MON32SS  (0x0FF8 / 8)  // 32-bit FFC00000 base data seg

#endif
