/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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
 * cpu_defs.h --
 *
 *	CPU-related definitions shared between vmkernel and user-space.
 */

#ifndef CPU_DEFS_H
#define CPU_DEFS_H

#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMKDRIVERS
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON

#define INCLUDE_ALLOW_VMX
#include "includeCheck.h"

#include "vm_basic_types.h"

typedef uint32 PCPU;
#define INVALID_PCPU		((PCPU) -1)

#define MAX_PCPUS       1024
#define MAX_PCPUS_BITS  10  // MAX_PCPUS <= (1 << MAX_PCPUS_BITS)
#define MAX_PCPUS_MASK  ((1 << MAX_PCPUS_BITS) - 1)

#endif
