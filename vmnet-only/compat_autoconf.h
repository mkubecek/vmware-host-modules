/*********************************************************
 * Copyright (c) 2009-2025 Broadcom. All Rights Reserved.
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

#ifndef __COMPAT_AUTOCONF_H__
#   define __COMPAT_AUTOCONF_H__

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMKDRIVERS
#include "includeCheck.h"


#ifndef LINUX_VERSION_CODE
#   error "Include compat_version.h before compat_autoconf.h"
#endif

#include <generated/autoconf.h>

#if defined(CONFIG_SUSE_VERSION) && defined(CONFIG_SUSE_PATCHLEVEL)
#   if CONFIG_SUSE_VERSION == 15 && CONFIG_SUSE_PATCHLEVEL >= 5
#      define SLE15_SP5_BACKPORTS 1
#   endif
#endif

#endif /* __COMPAT_AUTOCONF_H__ */
