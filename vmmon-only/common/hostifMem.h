/*********************************************************
 * Copyright (C) 1998, 2008, 2018 VMware, Inc. All rights reserved.
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
 *  hostifMem.h - Platform dependent interface. This module defines
 *                functions for allocating and releasing memory
 *                from the kernel.
 */


#ifndef _HOSTIFMEM_H_
#define _HOSTIFMEM_H_

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"
#include "vm_basic_types.h"


void *HostIF_AllocKernelMem(size_t size, Bool nonPageable);
void  HostIF_FreeKernelMem(void *ptr);

#endif // ifdef _HOSTIFMEM_H_
