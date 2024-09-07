/*********************************************************
 * Copyright (C) 2008-2020 VMware, Inc. All rights reserved.
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

#ifndef _MSRCACHE_H_
#define _MSRCACHE_H_

/*
 * msrCache.h -
 *
 *      Module to handle MSR data from multiple CPUs.
 *
 */

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "x86msr.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define FORALL_MSRS(_msr, _cache)                                  \
   do {                                                            \
      unsigned _ix;                                                \
      unsigned _count = MSRCache_NumMSRs(_cache);                  \
      const uint32 *_list = MSRCache_MSRList(_cache);              \
      for (_ix = 0; _ix < _count; _ix++) {                         \
         uint32 _msr = _list[_ix];

#define MSRS_DONE                                                  \
      }                                                            \
   } while(0)

typedef struct MSRCache MSRCache;
typedef Bool (*MSRQueryFunction)(MSRQuery *query);

unsigned MSRCache_NumCPUs(const MSRCache *cache);
unsigned MSRCache_NumMSRs(const MSRCache *cache);
const uint32 *MSRCache_MSRList(const MSRCache *cache);
MSRCache *MSRCache_Alloc(unsigned nCPUs, unsigned nMSRs, const uint32 *msrNum);
void MSRCache_Free(MSRCache *cache);
uint64 MSRCache_Get(const MSRCache *cache, uint32 msrNum, unsigned cpu);
void MSRCache_Set(MSRCache *cache, uint32 msrNum, unsigned cpu, uint64 val);
void MSRCache_Populate(MSRCache *cache, unsigned numCPUs,
                       MSRQueryFunction queryFn);
MSRCache *MSRCache_Clone(const MSRCache *cache);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
