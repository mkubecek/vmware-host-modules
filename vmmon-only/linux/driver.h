/*********************************************************
 * Copyright (C) 1998-2011,2014-2020 VMware, Inc. All rights reserved.
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

#ifndef __DRIVER_H__
#define __DRIVER_H__

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/rwsem.h>
#include <linux/wait.h>

#include "vmx86.h"


/* Per-instance driver state */
struct VMDriver;

typedef struct Device {
   struct Device   *next;
   struct VMDriver *vm;
   /*
    * This RW semaphore protects accesses to the VMDriver to
    * avoid racing between various ioctls, and the creation
    * and removal of the VM in question. The lock is read-acquired
    * by ioctls that reference the VMDriver, and write-acquired by
    * ioctls or device callbacks that allocate or destroy the
    * VMDriver.
    */
   struct rw_semaphore vmDriverRWSema;
} Device;


/*
 * Static driver state.
 */

#define LINUXLOG_BUFFER_SIZE  1024

typedef struct VMXLinuxState {
   char buf[LINUXLOG_BUFFER_SIZE];
   Device *head;

   struct task_struct *fastClockThread;
   unsigned fastClockRate;
} VMXLinuxState;

extern VMXLinuxState linuxState;

#endif
