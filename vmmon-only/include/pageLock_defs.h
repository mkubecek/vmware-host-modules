/*********************************************************
 * Copyright (C) 2015 VMware, Inc. All rights reserved.
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
 * pageLock_defs.h
 *
 *        Page lock status codes, used by vmmon.
 */

#ifndef _PAGELOCK_DEFS_H_
#define _PAGELOCK_DEFS_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

/*
 * Return codes from page locking, unlocking, and MPN lookup.
 * They share an error code space because they call one another
 * internally.
 *
 *    PAGE_LOCK_FAILED              The host refused to lock a page.
 *    PAGE_LOCK_LIMIT_EXCEEDED      We have reached the limit of locked
 *                                  pages for all VMs
 *    PAGE_LOCK_TOUCH_FAILED        Failed to touch page after lock.
 *    PAGE_LOCK_IN_TRANSITION       The page is locked but marked by Windows
 *                                  as nonpresent in CPU PTE and in transition
 *                                  in Windows PFN.
 *
 *    PAGE_LOCK_SYS_ERROR           System call error.
 *    PAGE_LOCK_ALREADY_LOCKED      Page already locked.
 *    PAGE_LOCK_MEMTRACKER_ERROR    MemTracker fails.
 *    PAGE_LOCK_PHYSTRACKER_ERROR   PhysTracker fails.
 *    PAGE_LOCK_MDL_ERROR           Mdl error on Windows.
 *
 *    PAGE_UNLOCK_NO_ERROR          Unlock successful (must be 0).
 *    PAGE_UNLOCK_NOT_TRACKED       Not in memtracker.
 *    PAGE_UNLOCK_NO_MPN            Tracked but no MPN.
 *    PAGE_UNLOCK_NOT_LOCKED        Not locked.
 *    PAGE_UNLOCK_TOUCH_FAILED      Failed to touch page.
 *    PAGE_UNLOCK_MISMATCHED_TYPE   Tracked but was locked by different API
 *
 *    PAGE_LOOKUP_INVALID_ADDR      Consistency checking.
 *    PAGE_LOOKUP_BAD_HIGH_ADDR     Consistency checking.
 *    PAGE_LOOKUP_ZERO_ADDR         Consistency checking.
 *    PAGE_LOOKUP_SMALL_ADDR        Consistency checking.
 *
 * All error values must be negative values less than -4096 to avoid
 * conflicts with errno values on Linux.
 *
 * -- edward
 */

#define PAGE_LOCK_SUCCESS                   0
#define PAGE_LOCK_FAILED              (-10001)
#define PAGE_LOCK_LIMIT_EXCEEDED      (-10002)
#define PAGE_LOCK_TOUCH_FAILED        (-10003)
#define PAGE_LOCK_IN_TRANSITION       (-10004)

#define PAGE_LOCK_SYS_ERROR           (-10010)
#define PAGE_LOCK_ALREADY_LOCKED      (-10011)
#define PAGE_LOCK_MEMTRACKER_ERROR    (-10012)
#define PAGE_LOCK_PHYSTRACKER_ERROR   (-10013)
#define PAGE_LOCK_MDL_ERROR           (-10014)

#define PAGE_UNLOCK_SUCCESS                 0
#define PAGE_UNLOCK_NOT_TRACKED       (-10100)
#define PAGE_UNLOCK_NO_MPN            (-10101)
#define PAGE_UNLOCK_NOT_LOCKED        (-10102)
#define PAGE_UNLOCK_TOUCH_FAILED      (-10103)
#define PAGE_UNLOCK_MISMATCHED_TYPE   (-10104)

#define PAGE_LOOKUP_SUCCESS                 0
#define PAGE_LOOKUP_INVALID_ADDR      (-10200)
#define PAGE_LOOKUP_BAD_HIGH_ADDR     (-10201)
#define PAGE_LOOKUP_ZERO_ADDR         (-10202)
#define PAGE_LOOKUP_SMALL_ADDR        (-10203)
#define PAGE_LOOKUP_SYS_ERROR         (-10204)
#define PAGE_LOOKUP_NOT_TRACKED          (-10)	// added to another code
#define PAGE_LOOKUP_NO_MPN               (-20)	// added to another code
#define PAGE_LOOKUP_NOT_LOCKED           (-30)	// added to another code
#define PAGE_LOOKUP_NO_VM                (-40)	// added to another code

#define PAGE_LOCK_SOFT_FAILURE(status) (status <= PAGE_LOCK_FAILED && \
                                        status > PAGE_LOCK_SYS_ERROR)

#endif // ifndef _PAGELOCK_DEFS_H_
