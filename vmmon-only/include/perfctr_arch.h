/*********************************************************
 * Copyright (C) 2014, 2019-2020 VMware, Inc. All rights reserved.
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
 * perfctr_arch.h --
 *
 *      Performance counters (x64 specific).
 */

#ifndef _X86_PERFCTR_ARCH_H_
#define _X86_PERFCTR_ARCH_H_

#ifndef _PERFCTR_H_
#error "This file can only be included by perfctr.h"
#endif

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "vm_asm.h"
#include "x86cpuid_asm.h"

#define PERFCTR_AMD_NUM_COUNTERS                 4
#define PERFCTR_AMD_EXT_NUM_COUNTERS             6
#define PERFCTR_P6_NUM_COUNTERS                  2
#define PERFCTR_NEHALEM_NUM_GEN_COUNTERS         4
#define PERFCTR_NEHALEM_NUM_FIXED_COUNTERS       3
#define PERFCTR_SANDYBRIDGE_NUM_GEN_COUNTERS     8 /* When HT is disabled */
#define PERFCTR_CORE_NUM_ARCH_EVENTS             8
#define PERFCTR_CORE_NUM_FIXED_COUNTERS          4
#define PERFCTR_AMD_VAL_MASK                     0xffffffffffffLL
/*
 * Even though the performance counters in P6 are 40 bits,
 * we can only write to the lower 32 bits.  Bit 31 is
 * used to sign-extend the upper 8 bits.
 */
#define PERFCTR_P6_VAL_MASK                      0xffffffffffLL
#define PERFCTR_P6_WRITE_MASK                    0xffffffff

/*
 * Performance counter width is determined at runtime in CORE.
 * But the writables bits are fixed. we can only write to the
 * lower 32 bits.  Bit 31 is used to sign-extend the upper 8 bits.
 */
#define PERFCTR_CORE_WRITE_MASK                  0xffffffff

/* Common Event Selection MSR bits. */
#define PERFCTR_CPU_EVENT_MASK                   0x000000ff
#define PERFCTR_CPU_EVENT_SHIFT                  0
#define PERFCTR_CPU_UNIT_MASK                    0x0000ff00
#define PERFCTR_CPU_UNIT_SHIFT                   8
#define PERFCTR_CPU_USER_MODE                    0x00010000
#define PERFCTR_CPU_KERNEL_MODE                  0x00020000
#define PERFCTR_CPU_EDGE_DETECT                  0x00040000
#define PERFCTR_CPU_PIN_CONTROL                  0x00080000
#define PERFCTR_CPU_APIC_INTR                    0x00100000
#define PERFCTR_CPU_ENABLE                       0x00400000
#define PERFCTR_CPU_INVERT_COUNTER_MASK          0x00800000
#define PERFCTR_CPU_COUNTER_MASK                 0xff000000
#define PERFCTR_CPU_COUNTER_MASK_SHIFT           24
#define PERFCTR_CPU_EVENT_IN_USE                 0x3C /* Unhalted Core cycles */

/*
 * ----------------------------------------------------------------------
 *
 * AMD K8
 *
 * ----------------------------------------------------------------------
 */

/* AMD Event Selection MSRs */
#define PERFCTR_AMD_EVENT_MASK                   PERFCTR_CPU_EVENT_MASK
#define PERFCTR_AMD_EXT_EVENT_MASK               (0xfULL << 32)
#define PERFCTR_AMD_EVENT_SHIFT                  PERFCTR_CPU_EVENT_SHIFT
#define PERFCTR_AMD_UNIT_MASK                    PERFCTR_CPU_UNIT_MASK
#define PERFCTR_AMD_UNIT_SHIFT                   PERFCTR_CPU_UNIT_SHIFT
#define PERFCTR_AMD_USER_MODE                    PERFCTR_CPU_USER_MODE
#define PERFCTR_AMD_KERNEL_MODE                  PERFCTR_CPU_KERNEL_MODE
#define PERFCTR_AMD_EDGE_DETECT                  PERFCTR_CPU_EDGE_DETECT
#define PERFCTR_AMD_PIN_CONTROL                  PERFCTR_CPU_PIN_CONTROL
#define PERFCTR_AMD_APIC_INTR                    PERFCTR_CPU_APIC_INTR
#define PERFCTR_AMD_ENABLE                       PERFCTR_CPU_ENABLE
#define PERFCTR_AMD_INVERT_COUNTER_MASK          PERFCTR_CPU_INVERT_COUNTER_MASK
#define PERFCTR_AMD_COUNTER_MASK                 PERFCTR_CPU_COUNTER_MASK
#define PERFCTR_AMD_COUNTER_MASK_SHIFT           PERFCTR_CPU_COUNTER_MASK_SHIFT
#define PERFCTR_AMD_SHIFT_BY_UNITMASK(e)         ((e) << 8 )
#define PERFCTR_AMD_EVTSEL_HOST                  (CONST64U(1) << 41)
#define PERFCTR_AMD_EVTSEL_GUEST                 (CONST64U(1) << 40)

/* AMD Performance Counter MSR Definitions */
#define PERFCTR_AMD_PERFEVTSEL0_ADDR             0xC0010000
#define PERFCTR_AMD_PERFCTR0_ADDR                0xC0010004
/* AMD with PerfCtrExtCore (PERFCORE) support MSR Definitions */
#define PERFCTR_AMD_EXT_BASE_ADDR                0xC0010200
#define PERFCTR_AMD_EXT_EVENTSEL                 0
#define PERFCTR_AMD_EXT_CTR                      1
#define PERFCTR_AMD_EXT_MSR_STRIDE               2
#define PERFCTR_AMD_AMD_EXT_CNTR_BASE_ADDR \
        (PERFCTR_AMD_EXT_BASE_ADDR + PERFCTR_AMD_EXT_CTR)
#define PERFCTR_AMD_AMD_EXT_EVSL_BASE_ADDR \
        (PERFCTR_AMD_EXT_BASE_ADDR + PERFCTR_AMD_EXT_EVENTSEL)

/* AMD Clocks */
#define PERFCTR_AMD_CPU_CLK_UNHALTED                           0x76

/* AMD Load/Store unit events */
#define PERFCTR_AMD_SEGMENT_REGISTER_LOADS                     0x20
#define PERFCTR_AMD_LS_BUFFER2_FULL                            0x23

/*
 * Event 0x2b counts SMIs on Opteron Rev.G (with upatch) and on
 * GH >= Rev.B0 without patches. GH Rev.A has no such capability.
 */
#define PERFCTR_AMD_SMI_COUNT                                  0x2b

/*
 * AMD Data Cache Events
 * Family <  17H: 0x45 - Unified TLB hit
 *                0x46 - Unified TLB miss
 * Family >= 17H: 0x45 - L1 DTLB miss (L2 DTLB hit or miss)
 *                0x46 - Tablewalker
 */
#define PERFCTR_AMD_DATA_CACHE_ACCESSES                        0x40
#define PERFCTR_AMD_DATA_CACHE_MISSES                          0x41
#define PERFCTR_AMD_DATA_CACHE_REFILLS_FROM_L2_OR_SYSTEM       0x42
#define PERFCTR_AMD_DATA_CACHE_REFILLS_FROM_SYSTEM             0x43
#define PERFCTR_AMD_DATA_CACHE_LINES_EVICTED                   0x44
#define PERFCTR_AMD_L1_DTLB_MISS_AND_L2_DTLB_HIT_OR_MISS       0x45
#define PERFCTR_AMD_L1_DTLB_AND_L2_DTLB_MISS                   0x46
#define PERFCTR_AMD_MISALIGNED_ACCESSES                        0x47
#define PERFCTR_AMD_PREFETCH_INSTRS_DISPATCHED                 0x4b
#define PERFCTR_AMD_DCACHE_MISSES_BY_LOCKED_INSTR              0x4c

/* AMD L2 Cache */
#define PERFCTR_AMD_REQUESTS_TO_L2                             0x7d
#define PERFCTR_AMD_L2_MISS                                    0x7e
#define PERFCTR_AMD_L2_FILL_WRITEBACK                          0x7f

/* AMD Instruction Cache Events */

#define PERFCTR_AMD_INSTR_FETCHES                              0x80
#define PERFCTR_AMD_INSTR_MISSES                               0x81
#define PERFCTR_AMD_INSTR_REFILLS_FROM_L2                      0x82
#define PERFCTR_AMD_INSTR_REFILLS_FROM_SYSTEM                  0x83
#define PERFCTR_AMD_L1_ITLB_MISS_L2_ITLB_HIT                   0x84
#define PERFCTR_AMD_L1_ITLB_MISS_L2_ITLB_MISS                  0x85
#define PERFCTR_AMD_INSTR_FETCH_STALL                          0x87

/* AMD Execution Unit Events */
#define PERFCTR_AMD_RET_INSTR                                  0xc0
#define PERFCTR_AMD_RET_UOPS                                   0xc1
#define PERFCTR_AMD_RET_BRANCH_INSTR                           0xc2
#define PERFCTR_AMD_RET_MISPRED_BRANCH_INSTR                   0xc3
#define PERFCTR_AMD_RET_TAKEN_BRANCH_INSTR                     0xc4
#define PERFCTR_AMD_RET_TAKEN_BRANCH_INSTR_MISPRED             0xc5
#define PERFCTR_AMD_RET_FAR_CONTROL_TRANSFERS                  0xc6
#define PERFCTR_AMD_RET_BRANCH_RESYNCS                         0xc7
#define PERFCTR_AMD_RET_NEAR_RETURNS                           0xc8
#define PERFCTR_AMD_RET_NEAR_RETURNS_MISPRED                   0xc9
#define PERFCTR_AMD_RET_INDIRECT_BRANCHES_MISPRED              0xca
#define PERFCTR_AMD_RET_MMX_FP_INSTR                           0xcb
#define PERFCTR_AMD_INT_MASKED_CYCLES                          0xcd
#define PERFCTR_AMD_INT_MASKED_CYCLES_WITH_INT_PEND            0xce

#define PERFCTR_AMD_INT_MASKED_COUNT \
  ((0xcd) | PERFCTR_AMD_EDGE_DETECT)

#define PERFCTR_AMD_INT_MASKED_COUNT_WITH_INT_PEND \
  ((0xce) | PERFCTR_AMD_EDGE_DETECT)

#define PERFCTR_AMD_INT_TAKEN                                       0xcf
#define PERFCTR_AMD_DECODER_EMPTY_CYCLES                            0xd0
#define PERFCTR_AMD_DISPATCH_STALLS                                 0xd1
#define PERFCTR_AMD_DISPATCH_MISPRED_BRANCH_STALL_CYCLES            0xd2
#define PERFCTR_AMD_DISPATCH_SERIALIZATION_STALL_CYCLES             0xd3
#define PERFCTR_AMD_DISPATCH_SEGMENTLOAD_STALL_CYCLES               0xd4
#define PERFCTR_AMD_DISPATCH_REORDER_BUFFER_FULL_STALL_CYCLES       0xd5
#define PERFCTR_AMD_DISPATCH_RESERVATION_STATION_FULL_STALL_CYCLES  0xd6
#define PERFCTR_AMD_DISPATCH_LS_FULL_STALL_CYCLES                   0xd8
#define PERFCTR_AMD_DISPATCH_WAIT_ALLQUIET_STALL_CYCLES             0xd9
#define PERFCTR_AMD_DISPATCH_FAR_XFER_OR_RESYNC_RETIRE_STALL_CYCLES 0xda

/* AMD Memory Controller Events */

#define PERFCTR_AMD_MEM_CTRL_PAGE_TABLE_OVERFLOWS                   0xe1
#define PERFCTR_AMD_CPU_IO_REQUESTS_TO_MEMORY_IO                    0xe9
#define PERFCTR_AMD_PROBE_RESPONSE_AND_UPSTREAM_REQ                 0xec

/* AMD HyperTransport Interface Events */

#define PERFCTR_AMD_HT_L0_TX_BW                                     0xf6
#define PERFCTR_AMD_HT_L1_TX_BW                                     0xf7
#define PERFCTR_AMD_HT_L2_TX_BW                                     0xf8

/*
 * ----------------------------------------------------------------------
 *
 * Intel P6 family (excluding newer Core architecture)
 *
 * ----------------------------------------------------------------------
 */

/*
 * Event numbers for P6 Family Processors.
 */

/* P6 Data Cache Unit (DCU) */
#define PERFCTR_P6_DATA_MEM_REFS                 0x00000043
#define PERFCTR_P6_DCU_LINES_IN                  0x00000045
#define PERFCTR_P6_DCU_M_LINES_IN                0x00000046
#define PERFCTR_P6_DCU_MISS_OUTSTANDING          0x00000048

/* P6 Instruction Fetch Unit (IFU) */
#define PERFCTR_P6_IFU_IFETCH                    0x00000080
#define PERFCTR_P6_IFU_IFETCH_MISS               0x00000081
#define PERFCTR_P6_ITLB_MISS                     0x00000085
#define PERFCTR_P6_IFU_MEM_STALL                 0x00000086
#define PERFCTR_P6_ILD_STALL                     0x00000087

/* P6 L2 Cache */
#define PERFCTR_P6_L2_IFETCH                     0x00000f28
#define PERFCTR_P6_L2_LD                         0x00000f29
#define PERFCTR_P6_L2_ST                         0x00000f2a
#define PERFCTR_P6_L2_LINES_IN                   0x00000024
#define PERFCTR_P6_L2_LINES_OUT                  0x00000026
#define PERFCTR_P6_L2_LINES_INM                  0x00000025
#define PERFCTR_P6_L2_LINES_OUTM                 0x00000027
#define PERFCTR_P6_L2_RQSTS                      0x00000f2e
#define PERFCTR_P6_L2_ADS                        0x00000021
#define PERFCTR_P6_L2_DBUS_BUSY_RD               0x00000023

/* P6 External Bus Logic */
#define PERFCTR_P6_BUSDRDY_CLOCKS_SELF           0x00000062
#define PERFCTR_P6_BUSDRDY_CLOCKS_ANY            0x00002062
#define PERFCTR_P6_BUS_LOCK_CLOCKS_SELF          0x00000063
#define PERFCTR_P6_BUS_LOCK_CLOCKS_ANY           0x00002063
#define PERFCTR_P6_BUS_REQ_OUTSTANDING           0x00000060
#define PERFCTR_P6_BUS_TRAN_BRD_SELF             0x00000065
#define PERFCTR_P6_BUS_TRAN_BRD_ANY              0x00002065
#define PERFCTR_P6_BUS_TRAN_RFO_SELF             0x00000066
#define PERFCTR_P6_BUS_TRAN_RFO_ANY              0x00002066
#define PERFCTR_P6_BUS_TRAN_WB_SELF              0x00000067
#define PERFCTR_P6_BUS_TRAN_WB_ANY               0x00002067
#define PERFCTR_P6_BUS_TRAN_IFETCH_SELF          0x00000068
#define PERFCTR_P6_BUS_TRAN_IFETCH_ANY           0x00002068
#define PERFCTR_P6_BUS_TRAN_INVAL_SELF           0x00000069
#define PERFCTR_P6_BUS_TRAN_INVAL_ANY            0x00002069
#define PERFCTR_P6_BUS_TRAN_PWR_SELF             0x0000006a
#define PERFCTR_P6_BUS_TRAN_PWR_ANY              0x0000206a
#define PERFCTR_P6_BUS_TRAN_P_SELF               0x0000006b
#define PERFCTR_P6_BUS_TRAN_P_ANY                0x0000206b
#define PERFCTR_P6_BUS_TRAN_IO_SELF              0x0000006c
#define PERFCTR_P6_BUS_TRAN_IO_ANY               0x0000206c
#define PERFCTR_P6_BUS_TRAN_DEF_SELF             0x0000006d
#define PERFCTR_P6_BUS_TRAN_DEF_ANY              0x0000206d
#define PERFCTR_P6_BUS_TRAN_DEF_SELF             0x0000006d
#define PERFCTR_P6_BUS_TRAN_DEF_ANY              0x0000206d
#define PERFCTR_P6_BUS_TRAN_BURST_SELF           0x0000006e
#define PERFCTR_P6_BUS_TRAN_BURST_ANY            0x0000206e
#define PERFCTR_P6_BUS_TRAN_ANY_SELF             0x00000070
#define PERFCTR_P6_BUS_TRAN_ANY_ANY              0x00002070
#define PERFCTR_P6_BUS_TRAN_MEM_SELF             0x0000006f
#define PERFCTR_P6_BUS_TRAN_MEM_ANY              0x0000206f
#define PERFCTR_P6_BUS_TRAN_RCV                  0x00000064
#define PERFCTR_P6_BUS_BNR_DRV                   0x00000061
#define PERFCTR_P6_BUS_HIT_DRV                   0x0000007a
#define PERFCTR_P6_BUS_HITM_DRV                  0x0000007b
#define PERFCTR_P6_BUS_SNOOP_STALL               0x0000007E

/* P6 Floating-Point Unit */
#define PERFCTR_P6_FLOPS                         0x000000c1
#define PERFCTR_P6_FP_COMP_OPS_EXE               0x00000010
#define PERFCTR_P6_FP_ASSIST                     0x00000011
#define PERFCTR_P6_MUL                           0x00000012
#define PERFCTR_P6_DIV                           0x00000013
#define PERFCTR_P6_CYCLES_DIV_BUSY               0x00000014

/* P6 Memory Ordering */
#define PERFCTR_P6_LD_BLOCKS                     0x00000003
#define PERFCTR_P6_SB_DRAINS                     0x00000004
#define PERFCTR_P6_MISALIGN_MEM_REF              0x00000005
#define PERFCTR_P6_EMON_KNI_PREF_DISPATCHED_NTA  0x00000007
#define PERFCTR_P6_EMON_KNI_PREF_DISPATCHED_T1   0x00000107
#define PERFCTR_P6_EMON_KNI_PREF_DISPATCHED_T2   0x00000207
#define PERFCTR_P6_EMON_KNI_PREF_DISPATCHED_WOS  0x00000307
#define PERFCTR_P6_EMON_KNI_PREF_MISS_NTA        0x0000004b
#define PERFCTR_P6_EMON_KNI_PREF_MISS_T1         0x0000014b
#define PERFCTR_P6_EMON_KNI_PREF_MISS_T2         0x0000024b
#define PERFCTR_P6_EMON_KNI_PREF_MISS_WOS        0x0000034b

/* P6 Instruction Decoding and Retirement */
#define PERFCTR_P6_INST_RETIRED                  0x000000c0
#define PERFCTR_P6_UOPS_RETIRED                  0x000000c2
#define PERFCTR_P6_INST_DECODED                  0x000000d0
#define PERFCTR_P6_EMON_KNI_INST_RETIRED         0x000000d8
#define PERFCTR_P6_EMON_KNI_INST_RETIRED_SC      0x000001d8
#define PERFCTR_P6_EMON_KNI_COMP_INST_RETIRED    0x000000d9
#define PERFCTR_P6_EMON_KNI_COMP_INST_RETIRED_SC 0x000001d9

/* P6 Interrupts */
#define PERFCTR_P6_HW_INT_RX                     0x000000c8
#define PERFCTR_P6_CYCLES_INT_MASKED             0x000000c6
#define PERFCTR_P6_CYCLES_INT_PENDING_AND_MASKED 0x000000c7

/* P6 Branches */
#define PERFCTR_P6_BR_INST_RETIRED               0x000000c4
#define PERFCTR_P6_BR_MISS_PRED_RETIRED          0x000000c5
#define PERFCTR_P6_BR_TAKEN_RETIRED              0x000000c9
#define PERFCTR_P6_BR_MISS_PRED_TAKEN_RET        0x000000ca
#define PERFCTR_P6_BR_INST_DECODED               0x000000e0
#define PERFCTR_P6_BTB_MISSES                    0x000000e2
#define PERFCTR_P6_BR_BOGUS                      0x000000e4
#define PERFCTR_P6_BACLEARS                      0x000000e6

/* P6 Stalls */
#define PERFCTR_P6_RESOURCE_STALLS               0x000000a2
#define PERFCTR_P6_PARTIAL_RAT_CLEARS            0x000000d2

/* P6 Segment Register Loads */
#define PERFCTR_P6_SEGMENT_REG_LOADS             0x00000006

/* P6 Clocks */
#define PERFCTR_P6_CPU_CLK_UNHALTED              0x00000079

/* P6 MMX Unit */
#define PERFCTR_P6_MMX_INSTR_EXEC                0x000000b0
#define PERFCTR_P6_MMX_SAT_INSTR_EXEC            0x000000b1
#define PERFCTR_P6_MMX_UOPS_EXEC                 0x000000b2
#define PERFCTR_P6_MMX_INSTR_TYPE_EXEC_PK_MUL    0x000001b3
#define PERFCTR_P6_MMX_INSTR_TYPE_EXEC_PK_SHIFT  0x000002b3
#define PERFCTR_P6_MMX_INSTR_TYPE_EXEC_PK_OP     0x000004b3
#define PERFCTR_P6_MMX_INSTR_TYPE_EXEC_UNPK_OP   0x000008b3
#define PERFCTR_P6_MMX_INSTR_TYPE_EXEC_PK_LOG    0x000010b3
#define PERFCTR_P6_MMX_INSTR_TYPE_EXEC_PK_ARITH  0x000020b3
#define PERFCTR_P6_FP_MMX_TRANS_TO               0x000000cc
#define PERFCTR_P6_FP_MMX_TRANS_FROM             0x000001cc
#define PERFCTR_P6_FP_MMX_ASSIST                 0x000000cd
#define PERFCTR_P6_FP_MMX_INSTR_RET              0x000000ce

/* P6 Segment Register Renaming */
#define PERFCTR_P6_SEG_RENAME_STALLS_ES          0x000001d4
#define PERFCTR_P6_SEG_RENAME_STALLS_DS          0x000002d4
#define PERFCTR_P6_SEG_RENAME_STALLS_FS          0x000004d4
#define PERFCTR_P6_SEG_RENAME_STALLS_GS          0x000008d4
#define PERFCTR_P6_SEG_RENAME_STALLS_ANY         0x00000fd4
#define PERFCTR_P6_SEG_RENAMES_ES                0x000001d5
#define PERFCTR_P6_SEG_RENAMES_DS                0x000002d5
#define PERFCTR_P6_SEG_RENAMES_FS                0x000004d5
#define PERFCTR_P6_SEG_RENAMES_GS                0x000008d5
#define PERFCTR_P6_SEG_RENAMES_ANY               0x00000fd5
#define PERFCTR_P6_RET_SEG_RENAMES               0x000000d6

/*
 * P6 Event Selection MSRs
 */

#define PERFCTR_P6_EVENT_MASK                    0x000000ff
#define PERFCTR_P6_EVENT_SHIFT                   0
#define PERFCTR_P6_UNIT_MASK                     0x0000ff00
#define PERFCTR_P6_UNIT_SHIFT                    8
#define PERFCTR_P6_USER_MODE                     0x00010000
#define PERFCTR_P6_KERNEL_MODE                   0x00020000
#define PERFCTR_P6_EDGE_DETECT                   0x00040000
#define PERFCTR_P6_PIN_CONTROL                   0x00080000
#define PERFCTR_P6_APIC_INTR                     0x00100000
#define PERFCTR_P6_ENABLE                        0x00400000
#define PERFCTR_P6_INVERT_COUNTER_MASK           0x00800000
#define PERFCTR_P6_COUNTER_MASK                  0xff000000
#define PERFCTR_P6_COUNTER_MASK_SHIFT            24
#define PERFCTR_P6_SHIFT_BY_UNITMASK(e)          (e << 8)

/*
 * P6 Performance Counter MSR Addresses
 */
#define PERFCTR_P6_PERFEVTSEL0_ADDR              0x00000186
#define PERFCTR_P6_PERFCTR0_ADDR                 0x000000c1

/*
 * ----------------------------------------------------------------------
 *
 * Intel Core architecture
 *
 *    Use CPUID 0xa to get perf capabilities.  Some (7) events are
 *    architectural; most are version-specific.
 *
 *    V1: Yonah, V2: Merom, V2+: Penryn, V3: Nehalem, V4: Skylake
 *
 *    V1 is similar to P6, with some additions:
 *       - Global control MSR
 *    V2 introduces:
 *       - 3 fixed counters
 *       - Ability to freeze a counter before PMI delivery.
 *       - Freeze during SMI (Penryn+ only)
 *       - VMCS global enable (Penryn+ only)
 *    V3 introduces:
 *       - (nothing we virtualize)
 *    V4 introduces:
 *       - Global unavailable (in-use) MSR
 *       - Freeze on PMI bit
 *       - ASCI bit
 *       - Global status set MSR
 *       - Global status reset MSR (previously called Ovf ctrl)
 *
 * ----------------------------------------------------------------------
 */
#define PERFCTR_CORE_PERFCTR0_ADDR                  0x0c1
#define PERFCTR_CORE_PERFEVTSEL0_ADDR               0x186
#define PERFCTR_CORE_FIXED_CTR0_ADDR                0x309
#define PERFCTR_CORE_FIXED_CTR_CTRL_ADDR            0x38d
#define PERFCTR_CORE_FIXED_CTR_CTRL_PMI_MASK        0x888
#define PERFCTR_CORE_GLOBAL_STATUS_ADDR             0x38e
#define PERFCTR_CORE_GLOBAL_CTRL_ADDR               0x38f
#define PERFCTR_CORE_GLOBAL_OVF_CTRL_ADDR           0x390
#define PERFCTR_CORE_GLOBAL_STATUS_RESET_ADDR       0x390
#define PERFCTR_CORE_GLOBAL_STATUS_SET_ADDR         0x391
#define PERFCTR_CORE_GLOBAL_UNAVAILABLE_STATUS_ADDR 0x392
#define PERFCTR_CORE_PERFCTR0_FULL_WIDTH_ADDR       0x4c1
#define PERFCTR_CORE_GLOBAL_PMC0_ENABLE             0x1
#define PERFCTR_CORE_GLOBAL_PMC1_ENABLE             0x2
#define PERFCTR_CORE_GLOBAL_FIXED_ENABLE            0x700000000ULL
#define PERFCTR_CORE_USER_MODE                      PERFCTR_CPU_USER_MODE
#define PERFCTR_CORE_KERNEL_MODE                    PERFCTR_CPU_KERNEL_MODE
#define PERFCTR_CORE_APIC_INTR                      PERFCTR_CPU_APIC_INTR
#define PERFCTR_CORE_ENABLE                         PERFCTR_CPU_ENABLE
/* AnyThread Counting deprecated since PMU v5. */
#define PERFCTR_CORE_ANYTHREAD                      0x00200000
#define PERFCTR_CORE_IN_TX                          (CONST64U(1) << 32)
#define PERFCTR_CORE_IN_TXCP                        (CONST64U(1) << 33)
#define PERFCTR_CORE_SHIFT_BY_UNITMASK(e)           ((e) << 8)
#define PERFCTR_CORE_FIXED_CTR0_PMC                 0x40000000
#define PERFCTR_CORE_FIXED_CTR1_PMC                 0x40000001
#define PERFCTR_CORE_FIXED_PMI_MASKn(n)             (CONST64U(0x8) << ((n) * 4))
#define PERFCTR_CORE_FIXED_ANY_MASKn(n)             (CONST64U(0x4) << ((n) * 4))
#define PERFCTR_CORE_FIXED_KERNEL_MASKn(n)          (CONST64U(0x1) << ((n) * 4))
#define PERFCTR_CORE_FIXED_USER_MASKn(n)            (CONST64U(0x2) << ((n) * 4))
#define PERFCTR_CORE_FIXED_ENABLE_MASKn(n)          (CONST64U(0x3) << ((n) * 4))
#define PERFCTR_CORE_FIXED_MASKn(n)                 (CONST64U(0xf) << ((n) * 4))
#define PERFCTR_CORE_FIXED_SHIFTBYn(n)              ((n) * 4)
#define PERFCTR_CORE_FIXED_ANYTHREAD                CONST64U(0x00000444)
#define PERFCTR_CORE_PMI_UNAVAILABLE_IN_USE         (CONST64U(1) << 63)
// XXX serebrin/dhecht: 1-10-11: Make ANYTHREAD depend on number of fixed PMCs

#define PERFCTR_CORE_GLOBAL_STATUS_TOPA_PMI      (1ULL << 55)
#define PERFCTR_CORE_GLOBAL_STATUS_CTR_FRZ       (1ULL << 59)
#define PERFCTR_CORE_GLOBAL_STATUS_ASCI          (1ULL << 60)
#define PERFCTR_CORE_GLOBAL_STATUS_OVFBUFFER     (1ULL << 62)

/* Architectural event counters */
#define PERFCTR_CORE_UNHALTED_CORE_CYCLES        0x3c
#define PERFCTR_CORE_INST_RETIRED                0xc0
/* bus cycles */
#define PERFCTR_CORE_UNHALTED_REF_CYCLES         (0x3c | (0x01 << 8))
#define PERFCTR_CORE_TOPDOWN_SLOTS               (0xa4 | (0x01 << 8))

/*
 * See Tables 30-2, 30-4 of the
 * "Intel 64 and IA-32 Architecture's Software Developer Manual,
 * Volume 3B, System Programming Guide, Part 2
 */
#define PERFCTR_CORE_LLC_REF                     (0x2e | (0x4f << 8))
#define PERFCTR_CORE_LLC_MISSES                  (0x2e | (0x41 << 8))
#define PERFCTR_CORE_LLC_MISSES_PREFETCH         (0x2e | (0x71 << 8))
#define PERFCTR_CORE_LLC_MISSES_ALL              (0x2e | (0xc1 << 8))
#define PERFCTR_CORE_LLC_MISSES_ALL_PREFETCH     (0x2e | (0xf1 << 8))
#define PERFCTR_CORE_BRANCH_RETIRED              0xc4
#define PERFCTR_CORE_BRANCH_MISPRED_RETIRED      0xc5

/* Non-Architectural event counters in Intel Core and Core 2 */
#define PERFCTR_CORE_L2_LINES_IN                 0x24
#define PERFCTR_CORE_L2_M_LINES_IN               0x25
#define PERFCTR_CORE_L2_LINES_OUT                0x26
#define PERFCTR_CORE_L2_M_LINES_OUT              0x27
#define PERFCTR_CORE_DATA_MEM_REF                0x43
#define PERFCTR_CORE_DATA_MEM_CACHE_REF          0x44
#define PERFCTR_CORE_DCACHE_REPL                 0x45
#define PERFCTR_CORE_DCACHE_M_REPL               0x46
#define PERFCTR_CORE_DCACHE_M_EVICT              0x47
#define PERFCTR_CORE_DCACHE_PEND_MISS            0x48
#define PERFCTR_CORE_DTLB_MISS                   0x49
#define PERFCTR_CORE_BUS_TRANS                   0x70
#define PERFCTR_CORE_ICACHE_READS                0x80
#define PERFCTR_CORE_ICACHE_MISSES               0x81
#define PERFCTR_CORE_ITLB_MISSES                 0x85
#define PERFCTR_CORE_UOPS_RETIRED                0xC2
#define PERFCTR_CORE_RESOURCE_STALLS             0xDC
#define PERFCTR_NEHALEM_OFFCORE_RESP0_EVENT      (0xB7 | (0x01 << 8))
#define PERFCTR_NEHALEM_OFFCORE_RESP1_EVENT      (0xBB | (0x01 << 8))

/* Intel TSX performance events introduced on Haswell */
#define PERFCTR_HASWELL_HLE_RETIRED_START        (0xc8 | (0x01 << 8))
#define PERFCTR_HASWELL_HLE_RETIRED_COMMIT       (0xc8 | (0x02 << 8))
#define PERFCTR_HASWELL_HLE_RETIRED_ABORT        (0xc8 | (0x04 << 8))
#define PERFCTR_HASWELL_RTM_RETIRED_START        (0xc9 | (0x01 << 8))
#define PERFCTR_HASWELL_RTM_RETIRED_COMMIT       (0xc9 | (0x02 << 8))
#define PERFCTR_HASWELL_RTM_RETIRED_ABORT        (0xc9 | (0x04 << 8))

/*
 * Nehalem off-core response events. See Section 30.6.1.2 of the
 * "Intel 64 and IA-32 Architecture's Software Developer Manual,
 * Volume 3B, System Programming Guide, Part 2.
 * One can specify
 * (request from the core, response from the uncore) pairs.
 */
#define PERFCTR_NEHALEM_OFFCORE_RESP0_ADDR             0x1A6
// requests
#define PERFCTR_NEHALEM_OFFCORE_RQST_DMND_DATA_RD       0x1
#define PERFCTR_NEHALEM_OFFCORE_RQST_DMND_RFO           0x2
#define PERFCTR_NEHALEM_OFFCORE_RQST_DMND_IFETCH        0x4
#define PERFCTR_NEHALEM_OFFCORE_RQST_WB                 0x8
#define PERFCTR_NEHALEM_OFFCORE_RQST_PF_DATA_RD         0x10
#define PERFCTR_NEHALEM_OFFCORE_RQST_PF_RFO             0x20
#define PERFCTR_NEHALEM_OFFCORE_RQST_PF_IFETCH          0x40
#define PERFCTR_NEHALEM_OFFCORE_RQST_OTHER              0x80

// responses
#define PERFCTR_NEHALEM_OFFCORE_RESP_UNCORE_HIT         0x100
#define PERFCTR_NEHALEM_OFFCORE_RESP_OTHER_CORE_HIT_SNP 0x200
#define PERFCTR_NEHALEM_OFFCORE_RESP_OTHER_CORE_HITM    0x400
#define PERFCTR_NEHALEM_OFFCORE_RESP_REMOTE_CACHE_FWD   0x1000
#define PERFCTR_NEHALEM_OFFCORE_RESP_REMOTE_DRAM        0x2000
#define PERFCTR_NEHALEM_OFFCORE_RESP_LOCAL_DRAM         0x4000
#define PERFCTR_NEHALEM_OFFCORE_RESP_NON_DRAM           0x8000

// Nehalem Uncore events.

/*
 * Uncore event MSRs. See table B-5 "MSRs in processors based on the
 * Intel micro-architecture (Nehalem), in "Intel 64 and IA-32 Architecture's
 * Software Developer Manual, Volume 3B, System Programming Guide, Part 2".
 * Though note that the addresses listed for PMCs and event select MSRs are
 * incorrect (swapped) in the manual.
 */
#define PERFCTR_NEHALEM_UNCORE_GLOBALCTRL_ADDR   0x391
#define PERFCTR_NEHALEM_UNCORE_PERFEVTSEL0_ADDR  0x3c0
#define PERFCTR_NEHALEM_UNCORE_PERFCTR0_ADDR     0x3b0

/*
 * The uncore event masks. See section 30.6.2 of the
 * "Intel 64 and IA-32 Architecture's Software Developer Manual,
 * Volume 3B, System Programming Guide, Part 2"
 */

// Enable the use of the programmable uncore counter "x".
#define PERFCTR_NEHALEM_UNCORE_ENABLE_CTR(x)     (1 << (x))
#define PERFCTR_NEHALEM_UNCORE_EDGE_DETECT       0x40000
#define PERFCTR_NEHALEM_UNCORE_ENABLE            0x400000
#define PERFCTR_NEHALEM_UNCORE_L3_LINES_IN       (0x0a | (0x0f << 8))

#define PERFCTR_CONFIG_ARCH_FIELDS \
   uint32 addr;      \
   uint32 escrAddr;  \
   uint32 resetHi;   \
   Bool pebsEnabled; \

/*
 * Program/reprogram event reg(s) associated w/perfctrs & start or stop perfctrs
 */
static INLINE void
PerfCtr_WriteEvtSel(uint32 addr,       // IN: Event register to write
                    uint32 escrVal)    // IN: event register value
{
   X86MSR_SetMSR(addr, escrVal);
}


/*
 * Set/reset performance counters to engender desired period before overflow
 */
static INLINE void
PerfCtr_WriteCounter(uint32 addr,   // IN: counter to write
                     uint64 value)  // IN: value to write
{
   X86MSR_SetMSR(addr, value);
}


static INLINE uint64
PerfCtr_SelValidBits(Bool amd)
{
   /*
    * Intel enforces PIN_CONTROL as MBZ; AMD does not.  Always mask on AMD
    * to avoid toggling the physical pin.
    */
   uint64 bits = PERFCTR_CPU_EVENT_MASK  | PERFCTR_CPU_UNIT_MASK |
                 PERFCTR_CPU_USER_MODE   | PERFCTR_CPU_KERNEL_MODE |
                 PERFCTR_CPU_EDGE_DETECT | PERFCTR_CPU_APIC_INTR |
                 PERFCTR_CPU_ENABLE      | PERFCTR_CPU_INVERT_COUNTER_MASK |
                 PERFCTR_CPU_COUNTER_MASK;
   if (amd) {
      bits |= PERFCTR_AMD_EXT_EVENT_MASK | PERFCTR_AMD_EVTSEL_HOST |
              PERFCTR_AMD_EVTSEL_GUEST;
   } else {
      bits |= PERFCTR_CORE_ANYTHREAD | PERFCTR_CORE_IN_TX |
              PERFCTR_CORE_IN_TXCP;
   }
   return bits;
}

static INLINE uint64
PerfCtr_PgcValidBits(unsigned numGenCtrs, unsigned numFixCtrs)
{
   return MASK64(numGenCtrs) | (MASK64(numFixCtrs) << 32);
}

static INLINE uint64
PerfCtr_FccValidBits(unsigned numFixCtrs)
{
   return MASK64(numFixCtrs * 4);
}

static INLINE uint64
PerfCtr_PgcToOvfValidBits(uint64 pgcValBits)
{
   return pgcValBits | MASKRANGE64(63, 61);
}

static INLINE uint64
PerfCtr_PgcToStsRstValidBits(uint64 pgcValBits)
{
   return pgcValBits |
          PERFCTR_CORE_GLOBAL_STATUS_TOPA_PMI |
          MASKRANGE64(63, 58);
}

static INLINE uint64
PerfCtr_PgcToGssValidBits(uint64 pgcValBits)
{
   return pgcValBits |
          PERFCTR_CORE_GLOBAL_STATUS_TOPA_PMI |
          MASKRANGE64(62, 58);
}

/*
 *----------------------------------------------------------------------
 *
 *  PerfCtr_HypervisorCPUIDSig --
 *
 *      Get the hypervisor signature string from CPUID.
 *
 * Results:
 *      TRUE on success FALSE otherwise.
 *      Unqualified 16 byte nul-terminated hypervisor string which may contain
 *      garbage.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
PerfCtr_HypervisorCPUIDSig(CPUIDRegs *name)
{
   CPUIDRegs regs;

   __GET_CPUID(1, &regs);
   if (!CPUID_ISSET(1, ECX, HYPERVISOR, regs.ecx)) {
      return FALSE;
   }

   __GET_CPUID(0x40000000, name);

   if (name->eax < 0x40000000) {
      return FALSE;
   }

   return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 *  PerfCtr_PEBSAvailable --
 *
 *      Checks if this CPU is capable of PEBS.
 *
 * Results:
 *      TRUE if PEBS is supported, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
PerfCtr_PEBSAvailable(void)
{
   CPUIDRegs regs;
   __GET_CPUID(0, &regs);
   if (CPUID_IsVendorIntel(&regs) &&
       (X86MSR_GetMSR(MSR_MISC_ENABLE) &
        (MSR_MISC_ENABLE_EMON_AVAILABLE | MSR_MISC_ENABLE_PEBS_UNAVAILABLE))
       == MSR_MISC_ENABLE_EMON_AVAILABLE) {
      CPUIDRegs hvendor;

      /*
       * Hyper-V doesn't support PEBS and may #GP if we try to write the
       * PEBS enable MSR so always consider PEBS un-available on Hyper-V -
       * PR 1039970.
       */
      return !PerfCtr_HypervisorCPUIDSig(&hvendor) ||
             !CPUID_IsRawVendor(&hvendor,
                                CPUID_HYPERV_HYPERVISOR_VENDOR_STRING);
   }
   return FALSE;
}

/*
 *----------------------------------------------------------------------
 *
 *  PerfCtr_PTAvailable --
 *
 *      Checks if this CPU is capable of PT(Intel processor trace).
 *
 * Results:
 *      TRUE if PT is supported, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
PerfCtr_PTAvailable(void)
{
   CPUIDRegs regs;
   __GET_CPUID(0, &regs);
   if (CPUID_IsVendorIntel(&regs)) {
      __GET_CPUID2(7, 0, &regs);
      return (regs.ebx & CPUID_INTERNAL_MASK_PT) != 0;
   }
   return FALSE;
}

#endif // _X86_PERFCTR_ARCH_H_
