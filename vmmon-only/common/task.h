/*********************************************************
 * Copyright (C) 1998-2013,2015,2019-2020 VMware, Inc. All rights reserved.
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

#ifndef TASK_H
#define TASK_H

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"
#include "bootstrap_vmm.h"

struct MonLoaderHeader;

extern Bool Task_CreateCrossGDT(BSVMM_GDTInit *gdt);
extern Bool Task_InitCrosspage(VMDriver *vm, LPN monStartLPN, LPN monEndLPN,
                               PerVcpuPages *perVcpuPages);
extern Bool Task_Switch(VMDriver *vm, Vcpuid vcpuid);
extern Bool Task_Initialize(void);
extern void Task_Terminate(void);
extern MPN Task_GetHVRootPageForPCPU(uint32 pCPU);
extern Descriptor *Task_GetTmpGDT(uint32 pCPU);
extern void Task_SwitchPTPPageCleanup(VMDriver *vm);

#endif



