/*********************************************************
 * Copyright (C) 2006-2015 VMware, Inc. All rights reserved.
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

#define CROSSGDT_NUMPAGES CEILING(sizeof (CrossGDT), PAGE_SIZE)

/*
 * Out of the 5 pages, only the first and last are really used.
 *
 * All we need to map are the first and last pages.  This mask tells
 * the setup code which pages it can put stuff in and it tells the
 * mapping and invalidation code which pages are mapped and unmapped.
 */
#define CROSSGDT_PAGEMASK 0x11
#define CROSSGDT_GETINDEXMASK(i) (1 << ((i) * sizeof (Descriptor) / PAGE_SIZE))
#define CROSSGDT_TESTINDEXMASK(i) (CROSSGDT_GETINDEXMASK(i) & CROSSGDT_PAGEMASK)

/*
 * All necessary host segments must be below CROSSGDT_HOSTLIMIT. In Nov 2006,
 * host GDT limits for the various guest OSes were:
 *
 *     Linux 64 bit:  80 (yes 80, not 7F)
 *     MacOS 64 bit:  8F
 *   Windows 64 bit:  6F
 */
#define CROSSGDT_HOSTLIMIT (PAGE_SIZE / sizeof (Descriptor)) /* 1st page */

#endif
