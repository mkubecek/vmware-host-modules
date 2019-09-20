/*********************************************************
 * Copyright (C) 1998,2018-2019 VMware, Inc. All rights reserved.
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

#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/rwsem.h>
#include <linux/wait.h>

#include "vmx86.h"
#include "driver_vmcore.h"


/* Per-instance driver state */
struct VMDriver;

/* 16 pages (64KB) looks as a good limit for one allocation */
#define VMMON_MAX_LOWMEM_PAGES  16

typedef struct Device {
   struct Device   *next;
   struct VMDriver *vm;
   /*
    * This RW semaphore protects accesses to the VMDriver to
    * avoid racing between various ioctls, and the creation
    * and removal of the VM in question. The lock is read-acquired
    * by ioctls that reference the VMDriver, and write-acquired by
    * ioctls or device callbacks that allocate or destory the
    * VMDriver.
    */
   struct rw_semaphore vmDriverRWSema;
   /*
    * The semaphore protect accesses to size4Gb and pages4Gb
    * in mmap(). mmap() may happen only once, and all other
    * accesses except cleanup are read-only, and may happen
    * only after successful mmap.
    */
   struct semaphore lock4Gb;
   PageCnt size4Gb;
   struct page *pages4Gb[VMMON_MAX_LOWMEM_PAGES];
} Device;


/*
 * Static driver state.
 */

#define VM_DEVICE_NAME_SIZE 32
#define LINUXLOG_BUFFER_SIZE  1024

typedef struct VMXLinuxState {
   int major;
   int minor;
   struct miscdevice misc;
   char deviceName[VM_DEVICE_NAME_SIZE];
   char buf[LINUXLOG_BUFFER_SIZE];
   Device *head;

   struct task_struct *fastClockThread;
   unsigned fastClockRate;
   uint64 swapSize;
} VMXLinuxState;

extern VMXLinuxState linuxState;
extern uint8 monitorIPIVector;
extern uint8 hvIPIVector;

#endif
