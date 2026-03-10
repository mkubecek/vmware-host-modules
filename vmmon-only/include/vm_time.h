/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * vm_time.h  --
 *
 *    Time management functions.
 *    Part of driver-only distribution
 *
 *    see comment in poll.c
 */


#ifndef VM_TIME_H
#define VM_TIME_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"


struct VmTimeVirtualRealClock;
typedef struct VmTimeVirtualRealClock VmTimeVirtualRealClock;

#ifdef USERLEVEL

VmTimeType VmTime_ReadVirtualTime(void);
VmTimeVirtualRealClock *VmTime_NewVirtualRealClock(void);
void VmTime_StartVirtualRealClock(VmTimeVirtualRealClock *, double);
void VmTime_ResetVirtualRealClock(VmTimeVirtualRealClock *);
VmTimeType VmTime_ReadVirtualRealTime(VmTimeVirtualRealClock *);
VmTimeType VmTime_RemainingVirtualRealTime(VmTimeVirtualRealClock *,
                                           VmTimeType realTime);
void VmTime_UpdateVirtualRealTime(VmTimeVirtualRealClock *clock,
                                  VmTimeType realTime,
                                  VmTimeType virtualTime);
#endif
#endif /* VM_TIME_H */

