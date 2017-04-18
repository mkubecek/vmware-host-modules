/*********************************************************
 * Copyright (C) 1998,2015 VMware, Inc. All rights reserved.
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

#ifndef __VMMONINT_H__
#define __VMMONINT_H__

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"


/*
 * Hide all kernel compatibility stuff in these macros and functions.
 */

#ifdef VMW_HAVE_SMP_CALL_3ARG
#define compat_smp_call_function(fn, info, wait) smp_call_function(fn, info, wait)
#else
#define compat_smp_call_function(fn, info, wait) smp_call_function(fn, info, 1, wait)
#endif


/*
 *----------------------------------------------------------------------
 *
 * compat_tsc_khz --
 *
 *    Compatibility wrapper for tsc_khz.
 *
 * Returns:
 *
 *    Returns 0 if symbol is not exported by the kernel, else
 *    value of tsc_khz.
 *
 *----------------------------------------------------------------------
 */

static inline unsigned int
compat_tsc_khz(void)
{
#if defined(VMW_HAVE_TSC_KHZ)
   return tsc_khz;
#else
   return 0;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * compat_smp_call_function_single --
 *
 *    Compatibility wrapper for calling smp_call_function_single.
 *    Versions prior to 2.6.20 did not export the symbol for both
 *    i386 and x86_64 kernels.
 *
 * Returns:
 *
 *    Returns -ENOSYS if the host kernel does not implement or export
 *    the function, else returns the error status of
 *    smp_call_function_single.
 *
 *----------------------------------------------------------------------
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
#   ifdef VMW_HAVE_SMP_CALL_3ARG
#      define compat_smp_call_function_single(cpu, fn, info, wait) \
              smp_call_function_single(cpu, fn, info, wait)
#   else
#      define compat_smp_call_function_single(cpu, fn, info, wait) \
              smp_call_function_single(cpu, fn, info, 1, wait)
#   endif // VMW_HAVE_SMP_CALL_3ARG
#else // VERSION >= 2.6.20
#      define compat_smp_call_function_single(cpu, fn, info, wait) (-ENOSYS)
#endif


/*
 * Although this is not really related to kernel-compatibility, I put this
 * helper macro here for now for a lack of better place --hpreg
 *
 * The exit(2) path does, in this order:
 * . set current->files to NULL
 * . close all fds, which potentially calls LinuxDriver_Close()
 *
 * fget() requires current->files != NULL, so we must explicitely check --hpreg
 */
#define vmware_fget(_fd) (current->files ? fget(_fd) : NULL)

extern void LinuxDriverWakeUp(Bool selective);

#endif /* __VMMONINT_H__ */
