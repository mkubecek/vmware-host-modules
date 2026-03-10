/*********************************************************
 * Copyright (C) 2008,2019-2020 VMware, Inc. All rights reserved.
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

#ifndef USERCALLDEFS_H
#define USERCALLDEFS_H

#ifdef __linux__
#include <linux/errno.h>
#define USERCALL_RESTART (-ERESTARTNOINTR)
#else
#include <usercall.h>
#define USERCALL_RESTART (USERCALL_NOP)
#endif
/*
 * -1 to -4096 are reserved for syscall errors on Linux.  -1 is reserved for
 * failing DeviceIoControl on Windows.
 */
#define USERCALL_VMX86ALLOCERR (-8192)
#define USERCALL_SWITCHERR     (-8193)

#endif
