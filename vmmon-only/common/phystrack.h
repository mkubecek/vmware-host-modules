/*********************************************************
 * Copyright (C) 1998,2019 VMware, Inc. All rights reserved.
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
 * phystrack.h --
 *
 *    track down the utilization of the physical pages
 */

#ifndef PHYSTRACK_H
#define PHYSTRACK_H

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#if !defined (__linux__)
/* XXX: can be more efficient based on server vs. desktop and version of Windows */
#define PHYSTRACK_MAX_SUPPORTED_GB (2048 + 4) /* 2 TB 64-bit W2k8 + 4 GB PCI */
#endif

struct PhysTracker;
struct VMDriver;

EXTERN struct PhysTracker *PhysTrack_Alloc(struct VMDriver *vm);
EXTERN void PhysTrack_Free(struct PhysTracker *);

EXTERN void PhysTrack_Add(struct PhysTracker *, MPN);
EXTERN void PhysTrack_Remove(struct PhysTracker *, MPN);
EXTERN Bool PhysTrack_Test(const struct PhysTracker *, MPN);
EXTERN MPN PhysTrack_GetNext(const struct PhysTracker *, MPN);
EXTERN PageCnt PhysTrack_GetNumTrackedPages(const struct PhysTracker *);

#endif






