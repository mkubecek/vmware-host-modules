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
 *  hostifGlobalLock.h - Platform dependent interface. This module
 *                       defines functions for manipulating/checking
 *                       the Global lock used by some drivers.
 */


#ifndef _HOSTIFGLOBALLOCK_H_
#define _HOSTIFGLOBALLOCK_H_

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"


#ifdef __APPLE__
Bool HostIFGlobalLock_Start(void);
void HostIFGlobalLock_Stop(void);
#endif
void HostIF_GlobalLock(int callerID);
void HostIF_GlobalUnlock(int callerID);
#ifdef VMX86_DEBUG
Bool HostIF_GlobalLockIsHeld(void);
#endif


#endif // ifdef _HOSTIFGLOBALLOCK_H_
