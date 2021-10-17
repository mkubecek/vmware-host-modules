/*********************************************************
 * Copyright (C) 2020 VMware, Inc. All rights reserved.
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
 * cpu_types.h --
 *
 *      Type definitions for the target architecture.
 */

#if !defined(_CPU_TYPES_H_)
#define _CPU_TYPES_H_

#include "vm_basic_types.h"
#include "vm_basic_defs.h"

typedef uint8 Instruction;

/*
 * Page
 */
typedef char PageArray[PAGE_SIZE];


#include "cpu_types_arch.h"

#endif
