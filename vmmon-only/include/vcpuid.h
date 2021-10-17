/*********************************************************
 * Copyright (C) 1998-2014, 2016-2021 VMware, Inc. All rights reserved.
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
 *
 * vcpuid.h --
 *
 *    Monitor's VCPU ID.
 */

#ifndef _VCPUID_H_
#define _VCPUID_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_types.h"

#if defined __cplusplus
extern "C" {
#endif

typedef uint32 Vcpuid;                 // VCPU number

#define VCPUID_INVALID  (~0U)

#define BOOT_VCPU_ID     0
#define IS_BOOT_VCPUID(vcpuid)  ((vcpuid) == BOOT_VCPU_ID)

#define MAX_VCPUS            2048
#define MAX_CORES_PER_SOCKET  256
#define MAX_VCPU_SOCKETS      128

/*
 * There are several properties of the VM which change at the 128 VCPU
 * boundary.  EFI firmware, x2APIC, and IOMMU are required among others.
 */
#define MAX_SMALL_VM_VCPUS 128

/* Supported limit. */
#define MAX_SUPPORTED_VCPUS   768

#if defined __cplusplus
} // extern "C"
#endif

#endif // ifndef _VCPUID_H_
