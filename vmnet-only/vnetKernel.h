/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * vnetKernel.h --
 *    This file defines platform-independent functions for accessing basic
 *    kernel functions. This is the Linux implementation.
 */

#ifndef _VNETKERNEL_H_
#define _VNETKERNEL_H_

#include "driver-config.h" /* must be first */
#include <linux/errno.h>
#include <linux/slab.h>
#include "vm_basic_types.h"

#define VNetKernel_EBUSY  (-EBUSY)
#define VNetKernel_EINVAL (-EINVAL)
#define VNetKernel_ENOMEM (-ENOMEM)

typedef struct VNetKernel_SpinLock {
   spinlock_t lock;
} VNetKernel_SpinLock;

static INLINE void *
VNetKernel_MemoryAllocate(size_t size)
{
   return kmalloc(size, GFP_ATOMIC);
}

static INLINE void
VNetKernel_MemoryFree(void *ptr)
{
   kfree(ptr);
}

static INLINE void
VNetKernel_SpinLockInit(VNetKernel_SpinLock *lock)
{
   spin_lock_init(&lock->lock);
}

static INLINE void
VNetKernel_SpinLockFree(VNetKernel_SpinLock *lock)
{
   /* nothing to do */
}

static INLINE void
VNetKernel_SpinLockAcquire(VNetKernel_SpinLock *lock)
{
   spin_lock(&lock->lock);
}

static INLINE void
VNetKernel_SpinLockRelease(VNetKernel_SpinLock *lock)
{
   spin_unlock(&lock->lock);
}

static INLINE void *
VNetKernel_ThreadCurrent(void)
{
   return current;
}

#endif // _VNETKERNEL_H_
