/*********************************************************
 * Copyright (C) 2006-2017, 2020 VMware, Inc. All rights reserved.
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
 *
 *      The crossGDT size is exactly one page.
 *
 *      The hosted world switch code is based on the assumption that by
 *      placing VMM descriptors at the end of the page, they will not
 *      overlap with host kernel descriptors in use when "crossing over".
 *
 *      All necessary host segments must be from first page of GDT.
 *      In Nov 2006, host GDT limits easily met this constraint:
 *
 *          Linux 64 bit:  80 (yes 80, not 7F)
 *          MacOS 64 bit:  8F
 *        Windows 64 bit:  6F
 */

#ifndef _CROSSGDT_H_
#define _CROSSGDT_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_defs.h"  // PAGE_SIZE
#include "x86/cpu_types_arch.h"       // Descriptor


typedef struct CrossGDT {
   Descriptor gdtes[PAGE_SIZE / sizeof(Descriptor)];
} CrossGDT;

#endif
