/*********************************************************
 * Copyright (C) 2002-2013, 2016-2017 VMware, Inc. All rights reserved.
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
 * vcpuset_types.h --
 *
 *    ADT for a set of VCPUs.  Implemented as an array of bitmasks.
 *
 */

#ifndef _VCPUSET_TYPES_H_
#define _VCPUSET_TYPES_H_


#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vcpuid.h"
#include "vm_assert.h"

#define VCS_SUBSET_WIDTH                                                   64
#define VCS_SUBSET_SHIFT                                                    6
#define VCS_SUBSET_MASK               ((CONST64U(1) << VCS_SUBSET_SHIFT) - 1)
#define VCS_SUBSET_COUNT                                                   32


#define VCS_VCPUID_SUBSET_IDX(v)                    ((v) >> VCS_SUBSET_SHIFT)
#define VCS_VCPUID_SUBSET_BIT(v)     (CONST64U(1) << ((v) & VCS_SUBSET_MASK))

/*
 * If you update this type, you also need to update the SEND_IPI line in
 * bora/public/iocontrolsMacosTable.h.
 */
typedef struct VCPUSet {
   uint64 subset[VCS_SUBSET_COUNT];
} VCPUSet;

MY_ASSERTS(VCPUSET_ASSERTS,
           ASSERT_ON_COMPILE(VCS_SUBSET_WIDTH * VCS_SUBSET_COUNT >= MAX_VCPUS);
           /*
            * Catch changes in VCPUSet which need to be reflected in
            * bora/public/iocontrolsMacosTable.h.
            */
           ASSERT_ON_COMPILE(VCS_SUBSET_COUNT == 32);
           /*
            * There is code that depends on sizeof(VCPUSet) being a power of
            * 2 in at least vcpuHotPlug.c and possible other places.
            */
            ASSERT_ON_COMPILE((sizeof(VCPUSet) & (sizeof(VCPUSet) - 1)) == 0);
)
#endif
