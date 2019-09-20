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
 * sharedAreaType.h --
 *
 *      This file contains shared area type definitions.
 */

#ifndef _SHAREDAREATYPE_H_
#define _SHAREDAREATYPE_H_

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE

#include "includeCheck.h"

typedef enum {
   SHARED_AREA_PER_VM_VMX = 0,
   SHARED_AREA_INTER_VCPU_VMX,
   SHARED_AREA_PER_VCPU_VMX,
   SHARED_AREA_PER_VM,
   SHARED_AREA_INTER_VCPU,
   SHARED_AREA_PER_VCPU,
   NUM_SHARED_AREAS
} SharedAreaType;

#endif // _SHAREDAREATYPE_H_
