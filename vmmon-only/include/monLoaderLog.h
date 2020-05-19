/*********************************************************
 * Copyright (C) 2016-2019 VMware, Inc. All rights reserved.
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
 * monLoaderLog.h --
 *
 *      Logging definitions for MonLoader.
 */

#ifndef _MONLOADER_LOG_H_
#define _MONLOADER_LOG_H_

#if defined VMKERNEL

#define LOGLEVEL_MODULE MonLoader
#include "log.h"

#elif defined VMMON

#include "vm_assert.h"
#define ML_LOGLEVEL_VMMON 0 /* MonLoader/vmmon loglevel. */

#undef LOG
#define LOG(_min, _fmt, ...)                         \
do {                                                 \
   if (vmx86_log && ML_LOGLEVEL_VMMON >= (_min)) {   \
      Log(_fmt "\n", ## __VA_ARGS__);                \
   }                                                 \
} while (0)

#else /* !defined VMKERNEL && !defined VMMON */
#error MonLoader cannot be built as part of this environment
#endif

#endif /* _MONLOADER_LOG_H_ */
