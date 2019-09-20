/*********************************************************
 * Copyright (C) 2018 VMware, Inc. All rights reserved.
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
 * sharedAreaVmmon.h --
 *
 *     VMMon shared area management.
 */

#ifndef SHARED_AREA_VMMON_H
#define SHARED_AREA_VMMON_H

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "iocontrols.h"

struct VMDriver;

/* A collection of pages backing a shared area region. */
typedef struct SharedAreaVmmonRegion {
   PageCnt pagesPerVcpu;
   MPN *pages;
} SharedAreaVmmonRegion;

/* The VMMon Driver shared area. */
typedef struct SharedAreaVmmon {
   SharedAreaVmmonRegion regions[NUM_SHARED_AREAS];
} SharedAreaVmmon;

/* Request for a backing MPN for a shared area region at a given VCPU/offset. */
typedef struct SharedAreaVmmonRequest {
   SharedAreaType type;
   Vcpuid vcpu;
   PageCnt offset;
} SharedAreaVmmonRequest;

SharedAreaVmmon *SharedAreaVmmon_Init(struct VMDriver *vm);
void SharedAreaVmmon_Cleanup(SharedAreaVmmon *area);
Bool SharedAreaVmmon_RegisterRegion(struct VMDriver *driver,
                                    VMSharedAreaRegistrationBlock *block);
Bool SharedAreaVmmon_ValidateRegionArgs(struct VMDriver *driver,
                                        VMSharedAreaRegistrationBlock *block);
MPN SharedAreaVmmon_GetRegionMPN(struct VMDriver *vm,
                                 SharedAreaVmmonRequest *request);
#endif /* SHARED_AREA_VMMON_H */
