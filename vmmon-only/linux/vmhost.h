/*********************************************************
 * Copyright (C) 2002-2020 VMware, Inc. All rights reserved.
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

#ifndef __VMHOST_H__
#define __VMHOST_H__

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include <linux/semaphore.h>


#ifdef VMX86_DEBUG
/*
 * A MutexHolder object. In debug builds, we record information about the
 * holder of a Mutex. --hpreg
 */
typedef struct MutexHolder {
   /* Linux task ID. --hpreg */
   int pid;

   /* Location in the code. --hpreg */
   int callerID;
} MutexHolder;
#endif


/*
 * A Mutex object. In debug builds,
 * o we track contention,
 * o we check when the Mutex should be held,
 * o we check the pairing and nesting of lock/unlock operations.
 *  --hpreg
 */
typedef struct Mutex {
   /* A binary semaphore. --hpreg */
   struct semaphore sem;
#ifdef VMX86_DEBUG

   /*
    * _static_ string describing the Mutex. Set once for all when the Mutex is
    * initialized. --hpreg
    */
   char const *name;

   /* Information about the previous holder. Protected by 'sem'. --hpreg */
   MutexHolder prev;

   /* Information about the current holder. Protected by 'sem'. --hpreg */
   MutexHolder cur;
#endif
} Mutex;


/*
 * Per-vm host-specific state.
 */

typedef struct VMHost {
   /*
    * Used for shared modifications to VM's VMDriver data, mostly page locking.
    * It has higher rank than the global mutex.
    */
   Mutex vmMutex;

   struct task_struct **vcpuSemaTask;        /* ptr to numVCPUs-sized array */

   /*
    * Pages that were allocated/mapped by VMX and locked by the driver and
    * don't have a particular VA.
    */
   struct PhysTracker  *lockedPages;
   /*
    * Locked pages that were allocated by the driver and don't have
    * a particular VA. They are used as monitor anonymous pages or
    * as pages for "AWE" guest memory.
    */
   struct PhysTracker  *AWEPages;
   /*
    * Pointer to a userlevel 64-bit area containing the value 1.
    * This is used for HostIF_SemaphoreSignal.
    */
   void *__user vmmonData;
} VMHost;

#endif
