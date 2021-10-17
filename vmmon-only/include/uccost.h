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
 * uccost.h
 *
 *        Definitons for VMX86_UCCOST builds.
 */

#ifndef _UCCOST_H
#define _UCCOST_H

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "vm_basic_defs.h"

/*
 * Define VMX86_UCCOST in the makefiles (Local.mk,
 * typically) if you want a special build whose only purpose
 * is to measure the overhead of a user call and its
 * breakdown.
 *
 * WINDOWS NOTE: I don't know how to pass VMX86_UCCOST to
 * the driver build on Windows.  It must be defined by hand.
 *
 * ESX Note: we don't have a crosspage in which to store these
 * timestamps.  Such a feature would perhaps be nice (if we
 * ever tire of the argument that esx does so few usercalls
 * that speed doesn't matter).
 */

#if defined(VMX86_UCCOST) && !defined(VMX86_SERVER)
#define UCTIMESTAMP(ptr, stamp) \
             do { (ptr)[UCCOST_ ## stamp] = RDTSC(); } while (0)
#else
#define UCTIMESTAMP(cp, stamp)
#endif

#ifdef VMX86_SERVER
typedef struct UCCostResults {
   uint32 vmksti;
   uint32 vmkcli;
   uint32 ucnop;
} UCCostResults;
#else

typedef struct UCCostResults {
   uint32 htom;
   uint32 mtoh;
   uint32 ucnop;
} UCCostResults;

typedef enum UCCostStamp {
#define UC(x, y) UCCOST_ ## x,
#include "uccostTable.h"
   UCCOST_MAX
} UCCostStamp;
#endif // VMX86_SERVER

#endif
