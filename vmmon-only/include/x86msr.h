/*********************************************************
 * Copyright (C) 1998-2020 VMware, Inc. All rights reserved.
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
 * x86msr.h --
 *
 *      MSR number definitions.
 */

#ifndef _X86MSR_H_
#define _X86MSR_H_
#include <asm/msr-index.h>
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMX

#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "community_source.h"

#if defined __cplusplus
extern "C" {
#endif

#define SIZEOF_X86_MSR     8

/*
 * Results of calling rdmsr(msrNum) on all logical processors.
 */
#ifdef _MSC_VER
#pragma warning (disable :4200) // non-std extension: zero-sized array in struct
#endif

typedef
#include "vmware_pack_begin.h"
struct MSRReply {
   /*
    * Unique host logical CPU identifier. It does not change across queries, so
    * we use it to correlate the replies of multiple queries.
    */
   uint64 tag;              // OUT
   uint64 msrVal;           // OUT
   uint8  implemented;      // OUT
   uint8  _pad[7];
}
#include "vmware_pack_end.h"
MSRReply;

typedef
#include "vmware_pack_begin.h"
struct MSRQuery {
   uint32 msrNum;           // IN
   uint32 numLogicalCPUs;   // IN/OUT
   MSRReply logicalCPUs[0]; // OUT
}
#include "vmware_pack_end.h"
MSRQuery;

#define MSR_TSC               0x00000010
#define MSR_PLATFORM_ID       0x00000017
#define MSR_APIC_BASE         0x0000001b
#define MSR_SMI_COUNT         0x00000034 // Intel Nehalem Family
#define MSR_CORE_THREAD_COUNT 0x00000035 // Intel Nehalem Family +
#define MSR_FEATCTL           0x0000003a
#define MSR_TSC_ADJUST        0x0000003b
#define MSR_SPEC_CTRL         0x00000048
#define MSR_PRED_CMD          0x00000049
#define MSR_BIOS_UPDT_TRIG    0x00000079
#define MSR_BIOS_SIGN_ID      0x0000008b
#define MSR_PERFCTR0          0x000000c1
#define MSR_PERFCTR1          0x000000c2
#define MSR_PLATFORM_INFO     0x000000ce // Intel Nehalem Family
#define MSR_MTRR_CAP          0x000000fe
#define MSR_L2CFG             0x0000011e
#define MSR_SYSENTER_CS       0x00000174
#define MSR_SYSENTER_ESP      0x00000175
#define MSR_SYSENTER_EIP      0x00000176
#define MSR_MCG_CAP           0x00000179
#define MSR_MCG_STATUS        0x0000017a
#define MSR_MCG_CTL           0x0000017b
#define MSR_MCG_EXT_CTL       0x000004d0
#define MSR_EVNTSEL0          0x00000186
#define MSR_EVNTSEL1          0x00000187
#define MSR_FLEX_RATIO        0x00000194 // Intel Nehalem Family
#define MSR_CLOCK_MODULATION  0x0000019a
#define MSR_MISC_ENABLE       0x000001a0
#define MSR_DEBUGCTL          0x000001d9
#define MSR_TSC_DEADLINE      0x000006e0
#define MSR_EFER              0xc0000080
#define MSR_FSBASE            0xc0000100
#define MSR_GSBASE            0xc0000101
#define MSR_KERNELGSBASE      0xc0000102
#define MSR_TSC_AUX           0xc0000103
#define MSR_BD_TSC_RATIO      0xc0000104

/* CET MSRs */
#define MSR_U_CET                            0x6a0
#define MSR_S_CET                            0x6a2
#define MSR_CET_SH_STK_EN                         (1ULL << 0)
#define MSR_CET_WR_SHSTK_EN                       (1ULL << 1)
#define MSR_PL0_SSP                          0x6a4
#define MSR_PL1_SSP                          0x6a5
#define MSR_PL2_SSP                          0x6a6
#define MSR_PL3_SSP                          0x6a7
#define MSR_ISST_ADDR                        0x6a8

#ifndef MSR_TEST_CTRL
#define MSR_TEST_CTRL                        0x33
#endif
#ifndef MSR_TEST_CTRL_SPLIT_LOCK_DETECT
#define MSR_TEST_CTRL_SPLIT_LOCK_DETECT           (1ULL << 29)
#endif

#define IA32_MSR_ARCH_CAPABILITIES           0x10a
#define MSR_ARCH_CAPABILITIES_RDCL_NO             (1ULL << 0)
#define MSR_ARCH_CAPABILITIES_IBRS_ALL            (1ULL << 1)
#define MSR_ARCH_CAPABILITIES_RSBA                (1ULL << 2)
#define MSR_ARCH_CAPABILITIES_NOL1F_VMENTRY       (1ULL << 3)
#define MSR_ARCH_CAPABILITIES_SSB_NO              (1ULL << 4)
#define MSR_ARCH_CAPABILITIES_MDS_NO              (1ULL << 5)
#define MSR_ARCH_CAPABILITIES_IF_PSCHANGE_MC_NO   (1ULL << 6)
#define MSR_ARCH_CAPABILITIES_TSX_CTRL            (1ULL << 7)
#define MSR_ARCH_CAPABILITIES_TAA_NO              (1ULL << 8)

#define MSR_FLUSH_CMD                        0x10b
#define MSR_FLUSH_CMD_FLUSH_L1D                   (1ULL << 0)

#define MSR_SPEC_CTRL_IBRS                        (1UL << 0)
#define MSR_SPEC_CTRL_STIBP                       (1UL << 1)
#define MSR_SPEC_CTRL_SSBD                        (1UL << 2)

#define MSR_PRED_CMD_IBPB                         (1UL << 0)

#define MSR_TSX_CTRL                         0x122
#define MSR_TSX_CTRL_RTM_DISABLE                  (1ULL << 0)
#define MSR_TSX_CTRL_CPUID_CLEAR                  (1ULL << 1)

#ifndef MSR_MISC_FEATURES_ENABLES
#define MSR_MISC_FEATURES_ENABLES            0x140
#endif

/* Intel Core Architecture and later: use only architected counters. */
#define IA32_MSR_PERF_CAPABILITIES                   0x345
#define MSR_PERF_CAPABILITIES_LBRFMT_SHIFT           0
#define MSR_PERF_CAPABILITIES_LBRFMT_MASK            0x3f
#define MSR_PERF_CAPABILITIES_PEBSTRAP               (1u << 6)
#define MSR_PERF_CAPABILITIES_PEBSSAVEARCHREGS       (1u << 7)
#define MSR_PERF_CAPABILITIES_PEBSRECORDFMT_SHIFT    8
#define MSR_PERF_CAPABILITIES_PEBSRECORDFMT_MASK     0xf
#define MSR_PERF_CAPABILITIES_FREEZE_WHILE_SMM       (1u << 12)
#define MSR_PERF_CAPABILITIES_FULL_WIDTH_WRITES      (1u << 13)
#define MSR_PERF_CAPABILITIES_PEBS_ADAPTIVE_DATA     (1u << 14)
#define MSR_PERF_CAPABILITIES_PERF_METRICS_AVAILABLE (1u << 15)

#define IA32_MSR_PEBS_ENABLE                      0x3f1

#define MSR_MTRR_BASE0        0x00000200
#define MSR_MTRR_MASK0        0x00000201
#define MSR_MTRR_BASE1        0x00000202
#define MSR_MTRR_MASK1        0x00000203
#define MSR_MTRR_BASE2        0x00000204
#define MSR_MTRR_MASK2        0x00000205
#define MSR_MTRR_BASE3        0x00000206
#define MSR_MTRR_MASK3        0x00000207
#define MSR_MTRR_BASE4        0x00000208
#define MSR_MTRR_MASK4        0x00000209
#define MSR_MTRR_BASE5        0x0000020a
#define MSR_MTRR_MASK5        0x0000020b
#define MSR_MTRR_BASE6        0x0000020c
#define MSR_MTRR_MASK6        0x0000020d
#define MSR_MTRR_BASE7        0x0000020e
#define MSR_MTRR_MASK7        0x0000020f
#define MSR_MTRR_FIX64K_00000 0x00000250
#define MSR_MTRR_FIX16K_80000 0x00000258
#define MSR_MTRR_FIX16K_A0000 0x00000259
#define MSR_MTRR_FIX4K_C0000  0x00000268
#define MSR_MTRR_FIX4K_C8000  0x00000269
#define MSR_MTRR_FIX4K_D0000  0x0000026a
#define MSR_MTRR_FIX4K_D8000  0x0000026b
#define MSR_MTRR_FIX4K_E0000  0x0000026c
#define MSR_MTRR_FIX4K_E8000  0x0000026d
#define MSR_MTRR_FIX4K_F0000  0x0000026e
#define MSR_MTRR_FIX4K_F8000  0x0000026f
#define MSR_MTRR_DEF_TYPE     0x000002ff

#define MSR_CR_PAT           0x00000277

#define MSR_MC0_CTL          0x00000400
#define MSR_MC0_STATUS       0x00000401
#define MSR_MC0_ADDR         0x00000402
#define MSR_MC0_MISC         0x00000403
#define MSR_MC0_CTL2         0x00000280

#define MSR_DS_AREA          0x00000600

#define MSR_LASTBRANCHFROMIP 0x000001db // Intel P6 Family
#define MSR_LASTBRANCHTOIP   0x000001dc // Intel P6 Family
#define MSR_LASTINTFROMIP    0x000001dd // Intel P6 Family
#define MSR_LASTINTTOIP      0x000001de // Intel P6 Family

#define MSR_LER_FROM_LIP     0x000001d7 // Intel Pentium4 Family
#define MSR_LER_TO_LIP       0x000001d8 // Intel Pentium4 Family
#define MSR_LASTBRANCH_TOS   0x000001da // Intel Pentium4 Family
#define MSR_LASTBRANCH_0     0x000001db // Intel Pentium4 Family
#define MSR_LASTBRANCH_1     0x000001dc // Intel Pentium4 Family
#define MSR_LASTBRANCH_2     0x000001dd // Intel Pentium4 Family
#define MSR_LASTBRANCH_3     0x000001de // Intel Pentium4 Family

#define CORE_LBR_SIZE        8
#define CORE2_LBR_SIZE       4

/* Power Management MSRs */
#define MSR_PERF_STATUS      0x00000198 // Current Performance State (ro)
#define MSR_PERF_CTL         0x00000199 // Target Performance State (rw)
#define MSR_POWER_CTL        0x000001fc // Power Control Register
#define MSR_CST_CONFIG_CTL   0x000000e2 // C-state Configuration (CORE)
#define MSR_MISC_PWR_MGMT    0x000001aa // Misc Power Management (NHM)
#define MSR_ENERGY_PERF_BIAS 0x000001b0 // Performance Energy Bias Hint
#define MSR_PKG_C2_RESIDENCY 0x0000060d
#define MSR_PKG_C3_RESIDENCY 0x000003f8
#define MSR_PKG_C6_RESIDENCY 0x000003f9
#define MSR_PKG_C7_RESIDENCY 0x000003fa
#define MSR_CORE_C3_RESIDENCY 0x000003fc
#define MSR_CORE_C6_RESIDENCY 0x000003fd
#define MSR_CORE_C7_RESIDENCY 0x000003fe

/* MSR_POWER_CTL bits (Intel) */
#define MSR_POWER_CTL_C1E    0x00000002 // C1E enable (NHM)

/* P-State Hardware Coordination Feedback Capability (Intel) */
#define MSR_MPERF            0x000000e7 // Maximum Performance (rw)
#define MSR_APERF            0x000000e8 // Actual Performance (rw)

/* Software Controlled Clock Modulation and Thermal Monitors (Intel) */
#define MSR_CLOCK_MODULATION 0x0000019a // Thermal Monitor Control (rw)
#define MSR_THERM_INTERRUPT  0x0000019b // Thermal Interrupt Control (rw)
#define MSR_THERM_STATUS     0x0000019c // Thermal Monitor Status (rw)
#define MSR_THERM2_CTL       0x0000019d // Thermal Monitor 2 Control (ro)

/* x2APIC MSRs */
#define MSR_X2APIC_BASE      0x00000800
#define MSR_X2APIC_MAX       0x0000083f
#define MSR_X2APIC_LIMIT     0x00000bff
#define MSR_X2APIC_ID        0x00000802
#define MSR_X2APIC_VERSION   0x00000803
#define MSR_X2APIC_TPR       0x00000808
#define MSR_X2APIC_PPR       0x0000080a
#define MSR_X2APIC_EOI       0x0000080b
#define MSR_X2APIC_LDR       0x0000080d
#define MSR_X2APIC_SVR       0x0000080f
#define MSR_X2APIC_ISR       0x00000810
#define MSR_X2APIC_TMR       0x00000818
#define MSR_X2APIC_IRR       0x00000820
#define MSR_X2APIC_ESR       0x00000828
#define MSR_X2APIC_CMCILVT   0x0000082f
#define MSR_X2APIC_ICRLO     0x00000830
#define MSR_X2APIC_TIMERLVT  0x00000832
#define MSR_X2APIC_THERMLVT  0x00000833
#define MSR_X2APIC_PCLVT     0x00000834
#define MSR_X2APIC_LVT0      0x00000835
#define MSR_X2APIC_LVT1      0x00000836
#define MSR_X2APIC_ERRLVT    0x00000837
#define MSR_X2APIC_INITCNT   0x00000838
#define MSR_X2APIC_CURCNT    0x00000839
#define MSR_X2APIC_DIVIDER   0x0000083e
#define MSR_X2APIC_SELFIPI   0x0000083f

#define MSR_BNDCFGS          0x00000d90  // Sup. mode bounds configuration

#define MSR_XSS              0x00000da0  // Extended Supervisor State Mask

#define MSR_MPX_LAX          0x00001000  // MPX Linear Address Extension

/* RTIT MSRs */
#define MSR_RTIT_CTL              0x00000570
#define MSR_RTIT_STATUS           0x00000571
#define MSR_RTIT_OUTPUT_BASE      0x00000560
#define MSR_RTIT_OUTPUT_MASK_PTRS 0x00000561
#define MSR_RTIT_CR3_MATCH        0x00000572
#define MSR_RTIT_ADDR0_A          0x00000580
#define MSR_RTIT_ADDR0_B          0x00000581
#define MSR_RTIT_ADDR1_A          0x00000582
#define MSR_RTIT_ADDR1_B          0x00000583
#define MSR_RTIT_ADDR2_A          0x00000584
#define MSR_RTIT_ADDR2_B          0x00000585
#define MSR_RTIT_ADDR3_A          0x00000586
#define MSR_RTIT_ADDR3_B          0x00000587

#define MSR_RTIT_NUM_OF_ADDR_REG  8

/* RTIT control MSR bits */
#define MSR_RTIT_CTL_TRACE_EN      (1LL<<0)  // Enable tracing
#define MSR_RTIT_CTL_CYC_EN        (1LL<<1)  // Enable CYC Packet
#define MSR_RTIT_CTL_OS            (1LL<<2)  // CPL0 filter
#define MSR_RTIT_CTL_USER          (1LL<<3)  // CPL > 0 filter
#define MSR_RTIT_CTL_FABRIC_EN     (1LL<<6)  // Trace output direction
#define MSR_RTIT_CTL_CR3_FILTER_EN (1LL<<7)  // Enable CR3 filter
#define MSR_RTIT_CTL_TOPA          (1LL<<8)  // Enable ToPA output scheme
#define MSR_RTIT_CTL_MTC_EN        (1LL<<9)  // Enable MTC packet
#define MSR_RTIT_CTL_TSC_EN        (1LL<<10) // Enable TSC packet
#define MSR_RTIT_CTL_DIS_RETC      (1LL<<11) // Disable RET compression
#define MSR_RTIT_CTL_BRANCH_EN     (1LL<<13) // Disable COFI-based packets
#define MSR_RTIT_CTL_RSVD          CONST64U(0xffff0000f0841030) // Reserved bits

#define MSR_RTIT_CTL_MTCFREQ_MASK CONST64U(0xf)
#define MSR_RTIT_CTL_MTCFREQ_SHIFT 14
#define MSR_RTIT_CTL_MTCFREQ(_msr)  \
       (((_msr) >> MSR_RTIT_CTL_MTCFREQ_SHIFT) & MSR_RTIT_CTL_MTCFREQ_MASK)

#define MSR_RTIT_CTL_CYCTHRESH_MASK CONST64U(0xf)
#define MSR_RTIT_CTL_CYCTHRESH_SHIFT 19
#define MSR_RTIT_CTL_CYCTHRESH(_msr)  \
       (((_msr) >> MSR_RTIT_CTL_CYCTHRESH_SHIFT) & MSR_RTIT_CTL_CYCTHRESH_MASK)

#define MSR_RTIT_CTL_PSBFREQ_MASK CONST64U(0xf)
#define MSR_RTIT_CTL_PSBFREQ_SHIFT 24
#define MSR_RTIT_CTL_PSBFREQ(_msr)  \
       (((_msr) >> MSR_RTIT_CTL_PSBFREQ_SHIFT) & MSR_RTIT_CTL_PSBFREQ_MASK)

#define MSR_RTIT_CTL_ADDR0_CFG_MASK  CONST64U(0xf)
#define MSR_RTIT_CTL_ADDR0_CFG_SHIFT 32
#define MSR_RTIT_CTL_ADDR0_CFG(_msr)  \
       (((_msr) >> MSR_RTIT_CTL_ADDR0_CFG_SHIFT) & MSR_RTIT_CTL_ADDR0_CFG_MASK)

#define MSR_RTIT_CTL_ADDR1_CFG_MASK  CONST64U(0xf)
#define MSR_RTIT_CTL_ADDR1_CFG_SHIFT 36
#define MSR_RTIT_CTL_ADDR1_CFG(_msr)  \
       (((_msr) >> MSR_RTIT_CTL_ADDR1_CFG_SHIFT) & MSR_RTIT_CTL_ADDR1_CFG_MASK)

#define MSR_RTIT_CTL_ADDR2_CFG_MASK  CONST64U(0xf)
#define MSR_RTIT_CTL_ADDR2_CFG_SHIFT 40
#define MSR_RTIT_CTL_ADDR2_CFG(_msr)  \
       (((_msr) >> MSR_RTIT_CTL_ADDR2_CFG_SHIFT) & MSR_RTIT_CTL_ADDR2_CFG_MASK)

#define MSR_RTIT_CTL_ADDR3_CFG_MASK  CONST64U(0xf)
#define MSR_RTIT_CTL_ADDR3_CFG_SHIFT 44
#define MSR_RTIT_CTL_ADDR3_CFG(_msr)  \
       (((_msr) >> MSR_RTIT_CTL_ADDR3_CFG_SHIFT) & MSR_RTIT_CTL_ADDR3_CFG_MASK)

/* RTIT status MSR bits */
#define MSR_RTIT_STATUS_FILTER_EN    (1LL<<0)
#define MSR_RTIT_STATUS_CONTEXT_EN   (1LL<<1)
#define MSR_RTIT_STATUS_TRIGGER_EN   (1LL<<2)
#define MSR_RTIT_STATUS_ERROR        (1LL<<4)
#define MSR_RTIT_STATUS_STOPPED      (1LL<<5)
#define MSR_RTIT_STATUS_RSVD         CONST64U(0xfffe0000ffffffc8)

#define MSR_RTIT_STATUS_PKT_BYTES_CNT_MASK  CONST64U(0x1ffff)
#define MSR_RTIT_STATUS_PKT_BYTES_CNT_SHIFT 32
#define MSR_RTIT_STATUS_PKT_BYTES_CNT(_msr)  \
        (((_msr) >> MSR_RTIT_STATUS_PKT_BYTES_CNT_SHIFT) & \
             MSR_RTIT_STATUS_PKT_BYTES_CNT_MASK)

/* RTIT CR3 MSR bits */
#define MSR_RTIT_CR3_MATCH_MASK CONST64U(~0x1f)

/* RTIT output base MSR bits */
#define MSR_RTIT_OUTPUT_BASE_MASK CONST64U(~0x7f)
#define MSR_RTIT_OUTPUT_BASE_RSVD CONST64U(0x7f)

/* RTIT output mask ptrs MSR bits */
#define MSR_RTIT_OUTPUT_MASK_LOWERMASK CONST64U(0x7f)

/*
 * Get the mask value for the single contiguous output region
 * when MSR_RTIT_CTL.ToPA is masked
 */
#define MSR_RTIT_OUTPUT_MASK_MASK  CONST64U(0xffffffff)
#define MSR_RTIT_OUTPUT_MASK(_msr) \
        ((_msr) & MSR_RTIT_OUTPUT_MASK_MASK)

/* Get the offset pointer of the ToPA table when MSR_RTIT_CTL.ToPA is set */
#define MSR_RTIT_OUTPUT_TABLE_OFFSET_MASK  CONST64U(0x1ffffff)
#define MSR_RTIT_OUTPUT_TABLE_OFFSET_SHIFT 7
#define MSR_RTIT_OUTPUT_TABLE_OFFSET(_msr) \
        ((((_msr) >> MSR_RTIT_OUTPUT_TABLE_OFFSET_SHIFT) & \
              MSR_RTIT_OUTPUT_TABLE_OFFSET_MASK) << 3)
#define MSR_RTIT_OUTPUT_TABLE_ENTRY(_msr) \
        (((_msr) >> MSR_RTIT_OUTPUT_TABLE_OFFSET_SHIFT) & \
              MSR_RTIT_OUTPUT_TABLE_OFFSET_MASK)

#define MSR_RTIT_OUTPUT_OFFSET_MASK  CONST64U(0xffffffff)
#define MSR_RTIT_OUTPUT_OFFSET_SHIFT 32
#define MSR_RTIT_OUTPUT_OFFSET(_msr) \
        (((_msr) >> MSR_RTIT_OUTPUT_OFFSET_SHIFT) & \
             MSR_RTIT_OUTPUT_OFFSET_MASK)

#define MSR_RTIT_CR3_MATCH_RSVD CONST64U(0x1f)

/* SGX MSRs */
#define MSR_SGX_SVN_STATUS               0x00000500

/* SGX SVN status MSR fields */
#define MSR_SGX_SVN_STATUS_LOCK          0x1
#define MSR_SGX_SVN_STATUS_SINIT_SVN     CONST64U(0xff0000)
#define MSR_SGX_SVN_STATUS_RSVD          CONST64U(0xffffffffff00fffe)

/*
 * SGX Flexible Launch Control MSRs.
 * These MSRs store hash of Launch Enclave's public key.
 */
#define MSR_SGXLEPUBKEYHASH0  0x0000008c
#define MSR_SGXLEPUBKEYHASH1  0x0000008d
#define MSR_SGXLEPUBKEYHASH2  0x0000008e
#define MSR_SGXLEPUBKEYHASH3  0x0000008f

#define NUM_SGXLEPUBKEYHASH_MSRs (4)

/* MSR_CR_PAT power-on value */
#define MSR_CR_PAT_DEFAULT   0x0007040600070406ULL

/* MSR_MISC_ENABLE bits (Intel) */
#define MSR_MISC_ENABLE_FAST_STRINGS     (1LL<<0)  // Enable Fast string ops
#define MSR_MISC_ENABLE_FOPCODE_COMPAT   (1LL<<2)
#define MSR_MISC_ENABLE_TM1              (1LL<<3)  // Enable Thermal Monitor 1
#define MSR_MISC_ENABLE_EMON_AVAILABLE   (1LL<<7)  // Perf Monitoring Available
#define MSR_MISC_ENABLE_BTS_UNAVAILABLE  (1LL<<11)
#define MSR_MISC_ENABLE_PEBS_UNAVAILABLE (1LL<<12)
#define MSR_MISC_ENABLE_TM2              (1LL<<13) // Enable Thermal Monitor 2
#define MSR_MISC_ENABLE_ESS              (1LL<<16) // Enable Enhanced SpeedStep
#define MSR_MISC_ENABLE_LIMIT_CPUID      (1LL<<22) // Enable CPUID maxval
#define MSR_MISC_ENABLE_C1E              (1LL<<25) // C1E enable (Merom/Penryn)
#define MSR_MISC_ENABLE_ACNT2            (1LL<<27) // ACNT2 (Nehalem only, deprecated)
#define MSR_MISC_ENABLE_TURBO_DISABLE    (1LL<<38) // Turbo Mode Disabled

/* DebugCtlMSR bits */
#define MSR_DEBUGCTL_LBR                   0x00000001
#define MSR_DEBUGCTL_BTF                   0x00000002
#define MSR_DEBUGCTL_TR                    0x00000040
#define MSR_DEBUGCTL_BTS                   0x00000080
#define MSR_DEBUGCTL_BTINT                 0x00000100
#define MSR_DEBUGCTL_BTS_OFF_OS            0x00000200
#define MSR_DEBUGCTL_BTS_OFF_USR           0x00000400
#define MSR_DEBUGCTL_FREEZE_LBRS_ON_PMI    0x00000800
#define MSR_DEBUGCTL_FREEZE_PERFMON_ON_PMI 0x00001000
#define MSR_DEBUGCTL_ENABLE_UNCORE_PMI     0x00002000
#define MSR_DEBUGCTL_FREEZE_WHILE_SMM      0x00004000
#define MSR_DEBUGCTL_RTM                   0x00008000
#define MSR_DEBUGCTL_RSVD          0xffffffffffff003cULL

/* Feature control bits */
#define MSR_FEATCTL_LOCK     0x00000001
#define MSR_FEATCTL_SMXE     0x00000002
#define MSR_FEATCTL_VMXE     0x00000004
#define MSR_FEATCTL_SENTERP  0x00007F00
#define MSR_FEATCTL_SENTERE  0x00008000
#define MSR_FEATCTL_FLCE     0x00020000
#define MSR_FEATCTL_SGXE     0x00040000
#define MSR_FEATCTL_LMCE     0x00100000

/* MSR_EFER bits. */
#define MSR_EFER_SCE         0x0000000000000001ULL  /* Sys call ext'ns:  r/w */
#define MSR_EFER_LME         0x0000000000000100ULL  /* Long mode enable: r/w */
#define MSR_EFER_LMA         0x0000000000000400ULL  /* Long mode active: r/o */
#define MSR_EFER_NXE         0x0000000000000800ULL  /* No-exec enable:   r/w */
#define MSR_EFER_SVME        0x0000000000001000ULL  /* SVM(AMD) enabled? r/w */
#define MSR_EFER_LMSLE       0x0000000000002000ULL  /* LM seg lim enable:r/w */
#define MSR_EFER_FFXSR       0x0000000000004000ULL  /* Fast FXSAVE:      r/w */
#define MSR_EFER_TCE         0x0000000000008000ULL  /* Trans. cache ext. r/w */
/* Vendor specific EFER bits */
#define MSR_EFER_INTEL_MBZ   0xffffffffffff02feULL  /* Must be zero (resrvd) */
#define MSR_EFER_INTEL_RAZ   0x0000000000000000ULL  /* Read as zero          */
#define MSR_EFER_AMD_MBZ     0xffffffffffff0200ULL  /* Must be zero (resrvd) */
#define MSR_EFER_AMD_RAZ     0x00000000000000feULL  /* Read as zero          */

#define MSR_AMD_PATCH_LOADER 0xc0010020

/* This ifndef is necessary because this is defined by some kernel headers. */
#ifndef MSR_K7_HWCR
#define MSR_K7_HWCR                0xc0010015    // Available on AMD processors
#endif
#define MSR_K7_HWCR_SSEDIS         0x00008000ULL // Disable SSE bit
#define MSR_K7_HWCR_MONMWAITUSEREN 0x00000400ULL // Enable MONITOR/MWAIT CPL>0
#define MSR_K7_HWCR_TLBFFDIS       0x00000040ULL // Disable TLB Flush Filter
#ifndef MSR_K7_HWCR_SMMLOCK
#define MSR_K7_HWCR_SMMLOCK        0x00000001ULL // Lock SMM environment
#endif

#ifndef MSR_K8_SYSCFG
#define MSR_K8_SYSCFG        0xc0010010
#endif
#define MSR_K8_SYSCFG_MTRRTOM2EN         (1ULL<<21)
#define MSR_K8_SYSCFG_TOM2FORCEMEMTYPEWB (1ULL<<22)
#define MSR_K8_SYSCFG_SMEE               (1ULL<<23)
#define MSR_K8_SYSCFG_SNPE               (1ULL<<24)
#define MSR_K8_SYSCFG_VMPLE              (1ULL<<25)

#define MSR_K8_TOPMEM2       0xc001001d

/* AMD "Greyhound" MSRs */
#define MSR_GH_CMPHLT            0xc0010055  // Interrupt Pending & CMP Halt
#define MSR_GH_CMPHLT_C1E        (1ULL<<27)  // C1E on CMP Halt is enabled
#define MSR_GH_CMPHLT_SMI        (1ULL<<28)  // SMI on CMP Halt is enabled

#define MSR_GH_OSVW_LENGTH       0xc0010140  // OS visible workaround length
#define MSR_GH_OSVW_STATUS       0xc0010141  // OS visible workaround bits
#define MSR_GH_OSVW_C1E          (1ULL<<1)   // Workaround for C1E not needed

#define MSR_GH_PSTATE_LIMIT      0xc0010061  // P-state Limit Register
#define MSR_GH_PSTATE_CONTROL    0xc0010062  // P-state Control Register [2:0]
#define MSR_GH_PSTATE_STATUS     0xc0010063  // P-state Status Register [2:0]
#define MSR_GH_PSTATE0           0xc0010064  // P-state 0
#define MSR_GH_PSTATE1           0xc0010065  // P-state 1
#define MSR_GH_PSTATE2           0xc0010066  // P-state 2
#define MSR_GH_PSTATE3           0xc0010067  // P-state 3
#define MSR_GH_PSTATE4           0xc0010068  // P-state 4
#define MSR_GH_COFVID_CONTROL    0xc0010070  // COFVID Control Register
#define MSR_GH_COFVID_STATUS     0xc0010071  // COFVID Status Register

#define MSR_AMD_MCA_INTR_CFG     0xc0000410  // MCA Interrupt Configuration

/* SVM related MSRs */
#define MSR_VM_CR                  0xc0010114
#define MSR_IGNNE                  0xc0010115
#define MSR_SMM_CTL                0xc0010116
#define MSR_VM_HSAVE_PA            0xc0010117

#define MSR_VM_CR_DPD              0x0000000000000001ULL // Disable HDT
#define MSR_VM_CR_R_INIT           0x0000000000000002ULL
#define MSR_VM_CR_DIS_A20M         0x0000000000000004ULL
#define MSR_VM_CR_SVM_LOCK         0x0000000000000008ULL
#define MSR_VM_CR_SVME_DISABLE     0x0000000000000010ULL
#define MSR_VM_CR_RESERVED         0xffffffffffffffe0ULL

/* SEV related MSRs. */
#define MSR_VMPAGE_FLUSH           0xc001011e
#define MSR_GHCB_PA                0xc0010130
#define MSR_GHCB_PA_FUNCTION_MASK       0xfff
#define MSR_GHCB_PA_SEVINFO_HV              1
#define MSR_GHCB_PA_SEVINFO_REQ             2
#define MSR_GHCB_PA_AP_JUMP_TABLE           3
#define MSR_GHCB_PA_CPUID_REQ               4
#define MSR_GHCB_PA_CPUID_RESP              5
#define MSR_GHCB_PA_TERMINATE             256
#define MSR_SEV_STATUS             0xc0010131

#define MSR_SEV_STATUS_SEV_EN      0x0000000000000001ULL // SEV is enabled
#define MSR_SEV_STATUS_SEV_ES_EN   0x0000000000000002ULL // SEV-ES is enabled
#define MSR_SEV_STATUS_SEV_SNP_EN  0x0000000000000004ULL // SEV-SNP is enabled

/* SEV-SNP (Secure Nested Paging) MSRs. */
#define MSR_RMP_BASE              0xc0010132 // Address of first byte of RMP
#define MSR_RMP_END               0xc0010133 // Address of last byte of RMP

#define MSR_AMD_DE_CFG           0xc0011029  // Decode configuration
#define MSR_AMD_DE_CFG_BIT1      (1ULL<<1)

#define MSR_AMD_LS_CFG           0xc0011020  // load-store configuration
#define MSR_AMD_LS_CFG_SSBD_BULLDOZER (1ULL<<54)  // BD family non-arch SSBD
#define MSR_AMD_LS_CFG_SSBD_KYOTO     (1ULL<<33)  // Kyoto family non-arch SSBD
#define MSR_AMD_LS_CFG_SSBD_ZEN       (1ULL<<10)  // Zen family non-arch SSBD

#define MSR_AMD_VIRT_SPEC_CTRL   0xc001011f  // Virtual speculation control
#define MSR_AMD_VIRT_SPEC_CTRL_SSBD   (1ULL<<2)   // Virtual SSBD

/* Syscall/Sysret related MSRs (x86_64) */
#define MSR_STAR             0xc0000081 // Also present on Athlons.
#define MSR_LSTAR            0xc0000082
#define MSR_CSTAR            0xc0000083
#define MSR_SFMASK           0xc0000084


/*
 * Hyper-V synthetic MSRs.
 */

#define MSR_HYPERV_GUEST_OS_ID                   0x40000000
#define MSR_HYPERV_HYPERCALL                     0x40000001
#define MSR_HYPERV_VP_INDEX                      0x40000002
#define MSR_HYPERV_RESET                         0x40000003
#define MSR_HYPERV_VP_RUNTIME                    0x40000010
#define MSR_HYPERV_TIME_REF_COUNT                0x40000020
#define MSR_HYPERV_REFERENCE_TSC                 0x40000021
#define MSR_HYPERV_TIME_TSC_FREQUENCY            0x40000022
#define MSR_HYPERV_TIME_APIC_FREQUENCY           0x40000023
#define MSR_HYPERV_EOI                           0x40000070
#define MSR_HYPERV_ICR                           0x40000071
#define MSR_HYPERV_TPR                           0x40000072
#define MSR_HYPERV_APIC_ASSIST_PAGE              0x40000073
#define MSR_HYPERV_SCONTROL                      0x40000080
#define MSR_HYPERV_SVERSION                      0x40000081
#define MSR_HYPERV_SIEFP                         0x40000082
#define MSR_HYPERV_SIMP                          0x40000083
#define MSR_HYPERV_EOM                           0x40000084
#define MSR_HYPERV_SINT0                         0x40000090
#define MSR_HYPERV_SINT1                         0x40000091
#define MSR_HYPERV_SINT2                         0x40000092
#define MSR_HYPERV_SINT3                         0x40000093
#define MSR_HYPERV_SINT4                         0x40000094
#define MSR_HYPERV_SINT5                         0x40000095
#define MSR_HYPERV_SINT6                         0x40000096
#define MSR_HYPERV_SINT7                         0x40000097
#define MSR_HYPERV_SINT8                         0x40000098
#define MSR_HYPERV_SINT9                         0x40000099
#define MSR_HYPERV_SINT10                        0x4000009A
#define MSR_HYPERV_SINT11                        0x4000009B
#define MSR_HYPERV_SINT12                        0x4000009C
#define MSR_HYPERV_SINT13                        0x4000009D
#define MSR_HYPERV_SINT14                        0x4000009E
#define MSR_HYPERV_SINT15                        0x4000009F
#define MSR_HYPERV_STIMER0_CONFIG                0x400000B0
#define MSR_HYPERV_STIMER0_COUNT                 0x400000B1
#define MSR_HYPERV_STIMER1_CONFIG                0x400000B2
#define MSR_HYPERV_STIMER1_COUNT                 0x400000B3
#define MSR_HYPERV_STIMER2_CONFIG                0x400000B4
#define MSR_HYPERV_STIMER2_COUNT                 0x400000B5
#define MSR_HYPERV_STIMER3_CONFIG                0x400000B6
#define MSR_HYPERV_STIMER3_COUNT                 0x400000B7
#define MSR_HYPERV_POWER_STATE_TRIGGER_C1        0x400000C1
#define MSR_HYPERV_POWER_STATE_TRIGGER_C2        0x400000C2
#define MSR_HYPERV_POWER_STATE_TRIGGER_C3        0x400000C3
#define MSR_HYPERV_POWER_STATE_CONFIG_C1         0x400000D1
#define MSR_HYPERV_POWER_STATE_CONFIG_C2         0x400000D2
#define MSR_HYPERV_POWER_STATE_CONFIG_C3         0x400000D3
#define MSR_HYPERV_STATS_PARTITION_RETAIL_PAGE   0x400000E0
#define MSR_HYPERV_STATS_PARTITION_INTERNAL_PAGE 0x400000E1
#define MSR_HYPERV_STATS_VP_RETAIL_PAGE          0x400000E2
#define MSR_HYPERV_STATS_VP_INTERNAL_PAGE        0x400000E3
#define MSR_HYPERV_GUEST_IDLE                    0x400000F0
#define MSR_HYPERV_SYNTH_DEBUG_CONTROL           0x400000F1
#define MSR_HYPERV_SYNTH_DEBUG_STATUS            0x400000F2
#define MSR_HYPERV_SYNTH_DEBUG_SEND_BUFFER       0x400000F3
#define MSR_HYPERV_SYNTH_DEBUG_RECEIVE_BUFFER    0x400000F4
#define MSR_HYPERV_SYNTH_DEBUG_PENDING_BUFFER    0x400000F5

/* Guest Crash Enlightenment MSRs */
#define MSR_HYPERV_CRASH_P0                      0x40000100
#define MSR_HYPERV_CRASH_P1                      0x40000101
#define MSR_HYPERV_CRASH_P2                      0x40000102
#define MSR_HYPERV_CRASH_P3                      0x40000103
#define MSR_HYPERV_CRASH_P4                      0x40000104
#define MSR_HYPERV_CRASH_CTL                     0x40000105

#define MSR_HYPERV_HYPERCALL_EN                  1ULL
#define MSR_HYPERV_REFERENCE_TSC_EN              1ULL
#define MSR_HYPERV_VP_ASSIST_EN                  1ULL
#define MSR_HYPERV_SIEF_EN                       1ULL
#define MSR_HYPERV_SIM_EN                        1ULL

#define MSR_HYPERV_GUESTOSID_VENDOR_SHIFT        48
#define MSR_HYPERV_GUESTOSID_VENDOR_MASK         0xfULL
#define MSR_HYPERV_GUESTOSID_VENDOR_MICROSOFT    1ULL

#define MSR_HYPERV_GUESTOSID_OS_SHIFT            40
#define MSR_HYPERV_GUESTOSID_OS_MASK             0xfULL
#define MSR_HYPERV_GUESTOSID_OS_WINNT_DERIVATIVE 4ULL

/* MSR for forcing RTM abort to recover PMC3 (see PR 2333817) */
#ifndef MSR_TSX_FORCE_ABORT
#define MSR_TSX_FORCE_ABORT                      0x0000010f
#endif
#define MSR_TSX_FORCE_ABORT_RTM_BIT_INDEX        0

/*
 * MTRR bit description
 */
#define MTRR_CAP_WC           0x400
#define MTRR_CAP_FIX          0x100
#define MTRR_CAP_VCNT_MASK    0xff

#define MTRR_DEF_ENABLE       0x800
#define MTRR_DEF_FIXED_ENABLE 0x400
#define MTRR_DEF_TYPE_MASK    0xff

#define MTRR_BASE_TYPE_MASK   0xff

#define MTRR_MASK_VALID       0x800

typedef unsigned char MTRRType;

#define MTRR_TYPE_UC          0
#define MTRR_TYPE_WC          1
#define MTRR_TYPE_WT          4
#define MTRR_TYPE_WP          5
#define MTRR_TYPE_WB          6
/* PAT Memory Type Only */
/* UC- is equivalent to UC, except that the MTRR values take precedence */
#define MTRR_TYPE_UCM         7

/*
 * This value is marked as reserved in the Intel manual. We use it to
 * specify that type is unknown as it is very unlikely that Intel will
 * use this value. Note that linux is taking the same liberty.
 */
#define MTRR_TYPE_UNKNOW     0xff

/*
 * PERF_STATUS bits
 */
#define MSR_PERF_STATUS_MAX_BUS_RATIO_SHIFT 40
#define MSR_PERF_STATUS_MAX_BUS_RATIO_MASK  0x1f

/*
 * PLATFORM_INFO bits
 */
#define MSR_PLATFORM_INFO_CPUID_FAULTING (1UL << 31)  // Faulting is supported
#define MSR_PLATFORM_INFO_MIN_RATIO_SHIFT 40
#define MSR_PLATFORM_INFO_MIN_RATIO_MASK 0xff
#define MSR_PLATFORM_INFO_MAX_NONTURBO_RATIO_SHIFT 8
#define MSR_PLATFORM_INFO_MAX_NONTURBO_RATIO_MASK 0xff

/*
 * MISC_FEATURES_ENABLES bits
 */
#ifdef MSR_MISC_FEATURES_ENABLES_CPUID_FAULT
#define MSR_MISC_FEATURES_ENABLES_CPUID_FAULTING MSR_MISC_FEATURES_ENABLES_CPUID_FAULT
#else
#define MSR_MISC_FEATURES_ENABLES_CPUID_FAULTING 1
#endif




// Platform Quality of Service (PQM) MSRs
#define MSR_INTEL_PQM_EVTSEL    0xc8d
#define MSR_INTEL_PQM_CTR       0xc8e
#define MSR_INTEL_PQM_ASSOC     0xc8f

 // Platform Quality Enforcement (PQE) MSRs
#define MSR_INTEL_PQE_CLOS_MASK_BASE     0xc90
#define MSR_INTEL_PQE_CLOS_MASK_MAX      0xd8f

#define MSR_INTEL_PQE_CLOS_L3_MASK_BASE     0xc90
#define MSR_INTEL_PQE_CLOS_L3_MASK_MAX      0xd0f
#define MSR_INTEL_PQE_CLOS_L2_MASK_BASE     0xd10
#define MSR_INTEL_PQE_CLOS_L2_MASK_MAX      0xd4f

static INLINE uint32
X86MSR_SysCallEIP(uint64 star)
{
   return (uint32)star;
}


static INLINE uint16
X86MSR_SysCallCS(uint64 star)
{
   return (uint16)(star >> 32);
}


static INLINE uint16
X86MSR_SysRetCS(uint64 star)
{
   return (uint16)(star >> 48);
}


/*
 *----------------------------------------------------------------------
 *
 * X86MSR_{G,S}etMSR --
 *
 *      Get or set an MSR in hardware. The interface should remain
 *      agnostic to the underlying compiler and architecture.
 *
 *----------------------------------------------------------------------
 */

#ifdef __GNUC__
/*
 * Checked against the Intel manual and GCC --hpreg
 * volatile because the msr can change without the compiler knowing it
 * (when we use wrmsr).
 */
#ifdef VM_X86_64
static INLINE uint64
X86MSR_GetMSR(uint32 cx)
{
   uint64 msr;
   __asm__ __volatile__(
      "rdmsr; shlq $32, %%rdx; orq %%rdx, %%rax"
      : "=a" (msr)
      : "c" (cx)
      : "%rdx"
   );

   return msr;
}

static INLINE void
X86MSR_SetMSR(uint32 cx, uint64 value)
{
   __asm__ __volatile__(
      "wrmsr"
      : /* no outputs */
      : "a" ((uint32) value),
        "d" ((uint32)(value >> 32)),
        "c" (cx)
    );
}
#else // __GNUC__ && !VM_X86_64
static INLINE uint64
X86MSR_GetMSR(uint32 cx)
{
   uint64 msr;
   __asm__ __volatile__(
      "rdmsr"
      : "=A" (msr)
      : "c"  (cx)
   );

   return msr;
}

static INLINE void
X86MSR_SetMSR(uint32 cx, uint64 value)
{
   __asm__ __volatile__(
      "wrmsr"
      : /* no outputs */
      : "A" (value),
        "c" (cx)
    );
}
#endif
#elif defined _MSC_VER // !__GNUC__ && _MSC_VER
unsigned __int64  __readmsr(unsigned long);
void              __writemsr(unsigned long, unsigned __int64);
#pragma intrinsic(__readmsr, __writemsr)
static INLINE uint64
X86MSR_GetMSR(uint32 cx)
{
   return __readmsr((unsigned long)cx);
}

static INLINE void
X86MSR_SetMSR(uint32 cx, uint64 value)
{
   __writemsr((unsigned long)(cx), (unsigned __int64)(value));
}
#else // !__GNUC__ && !_MSC_VER
#error No compiler defined for RDMSR/WRMSR.
#endif


#if defined __cplusplus
}
#endif

#endif /* _X86MSR_H_ */
