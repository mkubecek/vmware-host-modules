/*********************************************************
 * Copyright (C) 1998-2012,2014 VMware, Inc. All rights reserved.
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
 * perfctr_generic.h --
 *
 */

#ifndef _PERFCTR_GENERIC_H_
#define _PERFCTR_GENERIC_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON

#include "includeCheck.h"
#include "vm_basic_types.h"

/*
 * nmiNo      -- vmm peer is not attempting to do nmi profiling this run
 * nmiYes     -- vmm peer is doing nmi profiling and nmis are currently enabled
 * nmiStopped -- vmm peer is doing nmi profiling, but nmis are temporarily
 *               disabled for safety reasons.
 */
typedef enum {nmiNo = 0, nmiYes, nmiStopped} NMIStatus;
typedef struct NMIShared { /* shared with vmx and vmkernel */
   NMIStatus vmmStatus;
   int32     nmiErrorCode;
   int64     nmiErrorData;
} NMIShared;

/*
 * CrossProf: structures for unified profiling of vmm, vmx, and
 * vmkernel.  Per-vcpu.
 */

#define CALLSTACK_CROSSPROF_PAGES 1

typedef struct {
   /*
    * This structure is per-vcpu.  The raw data is a packed vector
    * of MonitorCallStackSample, a variable-length structure.
    */

   /* raw data - packed vec of MonitorCallStackSample, variable length */
   uint8  crossProfSampleBuffer[PAGES_2_BYTES(CALLSTACK_CROSSPROF_PAGES)];

   uint32 crossProfSampleBytes;
   uint32 crossProfNumDroppedSamples; /* For when buffer fills up */
   Bool   enabled; /* Can be false in stats build if monitor.callstack=FALSE */
   uint8  _pad[3];
} CrossProfShared;

#endif // ifndef _PERFCTR_GENERIC_H_
