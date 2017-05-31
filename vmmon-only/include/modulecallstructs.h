/*********************************************************
 * Copyright (C) 2006,2009-2011,2013-2014 VMware, Inc. All rights reserved.
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
 * modulecallstructs.h --
 *
 *
 *      Data structures that need to be included in modulecall.h
 *      as well as the vmkernel.
 *
 */

#ifndef _MODULECALLSTRUCTS_H_
#define _MODULECALLSTRUCTS_H_

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMCORE

#include "includeCheck.h"

#include "vm_basic_types.h"

/*
 *      Flags indicating switched MSR status.
 *
 * UNUSED    - Not used by the monitor (yet). [This is a value, not a flag.]
 * USED      - Hardware MSR is used by the monitor.
 * RESTORED  - Monitor value is restored on world switch into the VMM.
 * SHADOWED  - Live monitor value is always shadowed in the SwitchedMSRState.
 *
 */

#define SWITCHED_MSR_FLAG_UNUSED           0
#define SWITCHED_MSR_FLAG_USED             1
#define SWITCHED_MSR_FLAG_RESTORED         2
#define SWITCHED_MSR_FLAG_SHADOWED         4

/*
 * Note: If you add an msr to this list, please also ensure that
 *       hardware support for the msr is properly indicated in
 *       both the monitor (MonMSRIsSupported) and in the vmkernel
 *       (world switch msrSupported array).
 */
#define SWITCHED_MSRS       \
   SWMSR(MSR_SYSENTER_CS)   \
   SWMSR(MSR_SYSENTER_EIP)  \
   SWMSR(MSR_SYSENTER_ESP)  \
   SWMSR(MSR_STAR)          \
   SWMSR(MSR_LSTAR)         \
   SWMSR(MSR_CSTAR)         \
   SWMSR(MSR_SFMASK)        \
   SWMSR(MSR_TSC_AUX)       \
   SWMSR(MSR_BD_TSC_RATIO)

/*
 *      Data structures for dealing with the context-switched MSRs that need
 *      to be specially handled.  While the MSR definitions themselves
 *      are part of the x86 architecture, our handling of them (and hence
 *      these data structures) is an implementation detail.
 */


typedef enum SwitchedMSR {
#define SWMSR(msr) SWITCHED_##msr,
   SWITCHED_MSRS
#undef SWMSR
   NUM_SWITCHED_MSRS
} SwitchedMSR;

/*
 * Switched MSR values for each [vp]CPU.
 */
typedef struct SwitchedMSRValues {
   uint64 a[NUM_SWITCHED_MSRS];
} SwitchedMSRValues;

typedef struct SwitchedMSRState {
   SwitchedMSRValues smv;
   uint8             flags[NUM_SWITCHED_MSRS];
   uint32            _pad;
} SwitchedMSRState;

#endif
