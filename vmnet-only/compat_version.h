/*********************************************************
 * Copyright (c) 1998-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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

#ifndef __COMPAT_VERSION_H__
#   define __COMPAT_VERSION_H__

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMKDRIVERS
#include "includeCheck.h"


#ifndef __linux__
#   error "linux-version.h"
#endif


#include <linux/version.h>

#ifndef KERNEL_VERSION
#   error KERNEL_VERSION macro is not defined, environment is busted
#endif

/* 
 * Use COMPAT_LINUX_VERSION_CHECK_LT iff you need to compare running kernel to
 * versions 4.0 and above.
 *
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
   /* Straight forward comparison if kernel version is 4.0.0 and beyond */
#   define COMPAT_LINUX_VERSION_CHECK_LT(a, b, c) LINUX_VERSION_CODE < KERNEL_VERSION (a, b, c)
#endif

#if defined(RHEL_RELEASE_CODE) && defined(RHEL_RELEASE_VERSION)
#   if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 4)
#      define RHEL84_BACKPORTS 1
#   endif
#   if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 5)
#      define RHEL85_BACKPORTS 1
#   endif
#   if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(9, 0)
#      define RHEL90_BACKPORTS 1
#   endif
#   if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(9, 1)
#      define RHEL91_BACKPORTS 1
#   endif
#   if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(9, 2)
#      define RHEL92_BACKPORTS 1
#   endif
#   if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(9, 4)
#      define RHEL94_BACKPORTS 1
#   endif
#endif

#endif /* __COMPAT_VERSION_H__ */
