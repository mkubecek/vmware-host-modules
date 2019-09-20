/*********************************************************
 * Copyright (C) 1998-2018 VMware, Inc. All rights reserved.
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

/* Do not include inline functions, typedefs or other such
 * nonsense here.   This file is for _constants_.
 */

#ifndef _VMM_CONSTANTS_H_
#define _VMM_CONSTANTS_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"


#define VMM_PANIC_NONE         0
#define VMM_PANIC_MSG_COPYING  1
#define VMM_PANIC_MSG_COPIED   2
#define VMM_PANIC_COREDUMPING  3
#define VMM_PANIC_VCPU         4

#define VMM_PANIC_MSG_SIZE     256

#endif
