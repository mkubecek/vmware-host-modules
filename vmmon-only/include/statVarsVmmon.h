/*********************************************************
 * Copyright (C) 2018-2019 VMware, Inc. All rights reserved.
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
 * statVarsVmmon.h --
 *
 *     VMMon stat vars management.
 */

#ifndef STAT_VARS_VMMON_H
#define STAT_VARS_VMMON_H

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "iocontrols.h"

struct VMDriver;

/* The VMMon Driver stat vars area. */
typedef struct StatVarsVmmon {
   PageCnt pagesPerVcpu;
   MPN *pages;
} StatVarsVmmon;

StatVarsVmmon *StatVarsVmmon_Init(struct VMDriver *vm);
void StatVarsVmmon_Cleanup(StatVarsVmmon *statVars);
Bool StatVarsVmmon_RegisterVCPU(struct VMDriver *driver,
                                VMStatVarsRegistrationBlock *block);
MPN StatVarsVmmon_GetRegionMPN(struct VMDriver *vm, Vcpuid vcpuid,
                               PageCnt offset);
#endif /* STAT_VARS_VMMON_H */
