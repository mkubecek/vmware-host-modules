/*********************************************************
 * Copyright (C) 1998-2012,2014-2019 VMware, Inc. All rights reserved.
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
 * perfctr.h --
 *
 */

#ifndef _PERFCTR_H_
#define _PERFCTR_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON

#include "includeCheck.h"
#include "vm_basic_types.h"
#include "perfctr_arch.h"

#define PERF_EVENT_NAME_LEN                      64
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

/*
 * PerfCtr_Config --
 *      Describes configuration for a single hardware performance counter
 *
 *      Since this is only used to record general performance counters, we
 *      made the assumption in nmiProfiler.c that the type is GENERAL and
 *      index is counter number of type GENERAL.
 *
 *      **** x86 ****
 *      On AMD K8 and GH:
 *      index:        Which perf ctr, 0 to 3.  RDPMC argument
 *      addr:         MSR of raw perf ctr              (0xc0010004 + index).
 *      escrAddr:     MSR # of the Perf Event Selector (0xc0010000 + index).
 *      escrVal:      Value placed in PerfEvtSel MSR; what to measure.
 *
 *      On AMD with PerfCtrExtCore support:
 *      index:        Which perf ctr, 0 to 5.  RDPMC argument
 *      addr:         MSR of raw perf ctr              (0xc0010201 + 2 * index).
 *                                  aliased PMCs 0 - 3 (0xc0010004 + index).
 *      escrAddr:     MSR # of the Perf Event Selector (0xc0010200 + 2 * index).
 *                                  aliased PMCs 0 - 3 (0xc0010000 + index).
 *      escrVal:      Value placed in PerfEvtSel MSR; what to measure.
 *
 *      On Intel Core architecture:
 *      <to be documented>
 *
 *      **** ARM ****
 *      escrVal:     Value placed in PMEVTYPER<n>_EL0 to configure event counter
 *      index:       Index of the event counter.
 */
typedef struct PerfCtr_Config {
   uint64 escrVal;
   uint32 index;
   uint32 periodMean;

   /*
    * Random number (whose absolute value is capped at
    * periodJitterMask) is used to randomize sampling interval.
    */
   uint32  periodJitterMask;
   uint32  seed;    // seed is used to compute next random number
   uint16  config;
   Bool    valid;
   PERFCTR_CONFIG_ARCH_FIELDS
} PerfCtr_Config;


#endif // ifndef _PERFCTR_H_
