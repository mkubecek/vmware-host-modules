/*********************************************************
 * Copyright (C) 1998,2015-2017 VMware, Inc. All rights reserved.
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

#ifndef _X86APIC_H_
#define _X86APIC_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

/*
 * This file describes all the APIC/IOAPIC register locations and formats
 * that are defined by the x86 architecture.
 */


/*
 * APIC registers
 */
#define APIC_DEFAULT_ADDRESS      0xfee00000
#define APIC_ADDRESS_ALIGNMENT    4096
#define APIC_ADDRESS_MASK         (APIC_ADDRESS_ALIGNMENT-1)
#define APIC_VERSION_0x11         0x11
#define XAPIC_VERSION_0x14        0x14
#define X2APIC_VERSION_0x15       0x15
#define APIC_MAXLVT_0x4           0x4
#define XAPIC_MAXLVT_0x5          0x5
#define XAPIC_MAXLVT_0x6          0x6 // Intel only: Nehalem (1A) and onward
#define APIC_VERSION_MASK         0xff
#define APIC_MAX_LVT_MASK         0xff
#define APIC_MAX_LVT_SHIFT        16
#define APIC_EXTAPICSPACE_MASK    0x80000000
#define APIC_INVALID_ID           0xff
#define X2APIC_INVALID_ID         0xffffffff

#define APICR_ID          0x02
#define APICR_VERSION     0x03
#define APICR_TPR         0x08
#define APICR_APR         0x09
#define APICR_PPR         0x0a
#define APICR_EOI         0x0b
#define APICR_RMTREAD     0x0c
#define APICR_LDR         0x0d
#define APICR_DFR         0x0e
#define APICR_SVR         0x0f
#define APICR_ISR         0x10
#define APICR_TMR         0x18
#define APICR_IRR         0x20
#define APICR_ESR         0x28
#define APICR_CMCILVT     0x2f
#define APICR_ICRLO       0x30
#define APICR_ICRHI       0x31
#define APICR_TIMERLVT    0x32
#define APICR_THERMLVT    0x33
#define APICR_PCLVT       0x34
#define APICR_LVT0        0x35
#define APICR_LVT1        0x36
#define APICR_ERRLVT      0x37
#define APICR_INITCNT     0x38
#define APICR_CURCNT      0x39
#define APICR_DIVIDER     0x3e
#define APICR_SELFIPI     0x3f // X2 APIC only
#define APICR_EXTFEATURE  0x40
#define APICR_EXTLVT      0x50

#define X2APIC_READONLY_BITMASK \
   ((CONST64U(1) << APICR_ID)     | (CONST64U(1) << APICR_VERSION) | \
    (CONST64U(1) << APICR_APR)    | (CONST64U(1) << APICR_PPR)     | \
    (CONST64U(1) << APICR_CURCNT) | (CONST64U(1) << APICR_ICRHI)   | \
    (CONST64U(1) << APICR_LDR)    | (CONST64U(1) << APICR_DFR)     | \
    (MASK64(8) << APICR_ISR)      | (MASK64(8) << APICR_IRR)       | \
    (MASK64(8) << APICR_TMR))

#define APICR_SIZE             0x540

#define APIC_TPR_RESERVED     0xffffff00
#define APIC_PR_MASK          0x000000ff
#define APIC_PR_XMASK         0x000000f0
#define APIC_PR_YMASK         0x0000000f
#define APIC_PR_X(_r)         (((_r) & APIC_PR_XMASK) >> 4)
#define APIC_PR_Y(_r)         (((_r) & APIC_PR_YMASK) >> 0)

#define APIC_SVR_ONES         0x0000000f
#define APIC_SVR_VECTOR       0x000000ff
#define APIC_SVR_APICENABLE   0x00000100
#define APIC_SVR_FOCUSCHECK   0x00000200
#define APIC_SVR_X2_RESERVED  0xffffee00

#define APIC_LVT_MASK         0x10000
#define APIC_LVT_DELVMODE_NMI 0x400
#define APIC_LVT_DELVMODE(_lvt) (_lvt & 0x700)
#define APIC_LVT_RESET_VALUE  0x00010000

#define APIC_LVT_TIMER_RESERVED 0xfff8ef00
#define APIC_LVT_ERROR_RESERVED 0xfffeef00 
#define APIC_LVT_LINT_RESERVED  0xfffe0800
#define APIC_LVT_OTHER_RESERVED 0xfffee800 // CMCI, PC, THERM LVTs

#define APIC_EXTFEATURE_DEFAULT  0x00040004
#define APIC_EXTLVT_DEFAULT      0x00010000

/*
 * Use APIC_MSR_BASEMASK as mask only for reading the MSR_APIC_BASE.
 * Use guestReservedAPIC as mask when writing MSR_APIC_BASE.
 */
#define APIC_MSR_BASEMASK     CONST64U(0x0000fffffffff000)
#define APIC_MSR_X2APIC_ENABLED 0x00000400
#define APIC_MSR_ENABLED      0x00000800
#define APIC_MSR_BSP          0x00000100

#define APIC_VTE_VECTOR_MASK      0x000000ff
#define APIC_VTE_MODE_FIXED       0x00000000
#define APIC_VTE_MODE_SMI         0x00000200
#define APIC_VTE_MODE_NMI         0x00000400
#define APIC_VTE_MODE_EXTINT      0x00000700
#define APIC_VTE_MODE_MASK        0x00000700
#define APIC_VTE_DELVSTATUS       0x00001000
#define APIC_VTE_PINPOL           0x00002000
#define APIC_VTE_REMIRR           0x00004000
#define APIC_VTE_TRIGMODE         0x00008000
#define APIC_VTE_MASK             0x00010000
#define APIC_VTE_TMR_ONESHOT      0x00000000
#define APIC_VTE_TMR_PERIODIC     0x00020000
#define APIC_VTE_TMR_TSC_DEADLINE 0x00040000
#define APIC_VTE_TMR_MODE_MASK    0x00070000

/* 
 * Kernels 2.4.0-test8 and higher have the definition 
 * APIC_ID_MASK, so we use APIC_ID_BITS to avoid naming conflicts.
 */
#define APIC_ID_BITS          0x0f000000
#define XAPIC_ID_BITS         0xff000000
#define XAPIC_ID_MASK         0xff000000
#define X2APIC_ID_BITS        0xffffffff
#define APIC_ID_SHIFT         24
#define APIC_LDR_BITS         0xff000000
#define APIC_LDR_SHIFT        24

#define APIC_DIVIDER_BY_1     0x0000000b
#define APIC_DIVIDER_RESERVED 0xfffffff4

/* APIC illegal vectors */
#define APIC_MIN_LEGAL_VECTOR 16

/* APIC delivery modes */
#define APIC_DELMODE_FIXED    0
#define APIC_DELMODE_LOWEST   1
#define APIC_DELMODE_SMI      2
#define APIC_DELMODE_RSVD     3
#define APIC_DELMODE_NMI      4
#define APIC_DELMODE_INIT     5
#define APIC_DELMODE_STARTUP  6
#define APIC_DELMODE_EXTINT   7

/* APIC destination modes */
#define APIC_DESTMODE_PHYS    0
#define APIC_DESTMODE_LOGICAL 1

/* APIC polarities (active high/low) */
#define APIC_POLARITY_HIGH    0
#define APIC_POLARITY_LOW     1

/* APIC trigger types (edge/level) */
#define APIC_TRIGGER_EDGE     0
#define APIC_TRIGGER_LEVEL    1

/* APIC destination shorthands */
#define APIC_DEST_DEST        0
#define APIC_DEST_LOCAL       1
#define APIC_DEST_ALL_INC     2
#define APIC_DEST_ALL_EXC     3

/* APIC physical mode broadcasts */
#define APIC_DEST_BROADCAST   0x0f
#define XAPIC_DEST_BROADCAST  0xff
#define X2APIC_DEST_BROADCAST 0xffffffff

/* APIC IPI Command Register format */
#define APIC_ICRHI_RESERVED         0x00ffffff
#define APIC_ICRHI_DEST_MASK        0xff000000
#define APIC_ICRHI_DEST_OFFSET      24
#define X2APIC_ICR_DEST_OFFSET      32

#define APIC_ICRLO_RESERVED         0xfff32000
#define APIC_ICRLO_DEST_MASK        0x000c0000
#define APIC_ICRLO_DEST_OFFSET      18
#define APIC_ICRLO_TRIGGER_MASK     0x00008000
#define APIC_ICRLO_TRIGGER_OFFSET   15
#define APIC_ICRLO_LEVEL_MASK       0x00004000
#define APIC_ICRLO_LEVEL_OFFSET     14
#define APIC_ICRLO_STATUS_MASK      0x00001000
#define APIC_ICRLO_STATUS_OFFSET    12
#define APIC_ICRLO_DESTMODE_MASK    0x00000800
#define APIC_ICRLO_DESTMODE_OFFSET  11
#define APIC_ICRLO_DELMODE_MASK     0x00000700
#define APIC_ICRLO_DELMODE_OFFSET   8
#define APIC_ICRLO_VECTOR_MASK      0x000000ff
#define APIC_ICRLO_VECTOR_OFFSET    0

/* x2APIC Logical ID fields */ 
#define X2APIC_LDR_BITVEC_MASK      0x0000ffff
#define X2APIC_LDR_CLUSTER_MASK     0xffff0000
#define X2APIC_LDR_CLUSTER_SHIFT    16

/* APIC error register bits */ 
#define APIC_ERR_ILL_REG         (1<<7)
#define APIC_REC_ILL_VEC         (1<<6)
#define APIC_SENT_ILL_VEC        (1<<5)
#define APIC_ERR_REDIR_IPI       (1<<4) // X2APIC

#define X2APIC_SELFIPI_RESERVED     0xffffff00

/*
 * APIC register accessors
 */
#define APIC_VERSION_REG(_apic)  (_apic[APICR_VERSION][0])
#define APIC_MAX_LVT(_apic)      ((APIC_VERSION_REG(_apic) >> \
                                   APIC_MAX_LVT_SHIFT) & APIC_MAX_LVT_MASK)

#define APIC_LVT_ISMASKED(_lvt)  (_lvt & APIC_LVT_MASK)
#define APIC_LVT_VECTOR(_lvt)    (_lvt & 0xff)

#define APIC_SPINT_REG(_apic)    (_apic[APICR_SVR][0])
#define APIC_TIMER_REG(_apic)    (_apic[APICR_TIMERLVT][0])
#define APIC_THERM_REG(_apic)    (_apic[APICR_THERMLVT][0])
#define APIC_PC_REG(_apic)       (_apic[APICR_PCLVT][0])
#define APIC_LINT0_REG(_apic)    (_apic[APICR_LVT0][0])
#define APIC_LINT1_REG(_apic)    (_apic[APICR_LVT1][0])
#define APIC_ERR_REG(_apic)      (_apic[APICR_ERRLVT][0])
#define APIC_INITCNT_REG(_apic)  (_apic[APICR_INITCNT][0])
#define APIC_CURCNT_REG(_apic)   (_apic[APICR_CURCNT][0])

#define APIC_SPINT_VECTOR(_apic) (APIC_SPINT_REG(_apic) & 0xff)
#define APIC_TIMER_VECTOR(_apic) (APIC_TIMER_REG(_apic) & 0xff)
#define APIC_PC_VECTOR(_apic)    (APIC_PC_REG(_apic) & 0xff)
#define APIC_LINT0_VECTOR(_apic) (APIC_LINT0_REG(_apic) & 0xff)
#define APIC_LINT1_VECTOR(_apic) (APIC_LINT1_REG(_apic) & 0xff)
#define APIC_ERR_VECTOR(_apic)   (APIC_ERR_REG(_apic) & 0xff)


/*
 * I/O APIC registers
 */
#define IOAPIC_DEFAULT_ADDRESS    0xfec00000
#define IOAPIC_ADDRESS_ALIGNMENT  1024
#define IOAPIC_ADDRESS_MASK       (IOAPIC_ADDRESS_ALIGNMENT-1)
#define IOAPIC_UVERSIONMASK       0xff

#define IOAPICID              0x00
#define IOAPICVER             0x01
#define IOAPICARB             0x02
#define IOREDTBL_FIRST        0x10
#define IOREDTBL_LAST         0x3f

#define IO_APIC_TIMER_PIN               2
#define IO_APIC_RTC_PIN                 8

#define IO_APIC_REG0_RES2_MASK          0x00FFFFFF
#define IO_APIC_REG0_RES2_OFFSET        0
#define IO_APIC_REG0_RES1_MASK          0xF0000000
#define IO_APIC_REG0_RES1_OFFSET        28

#define IO_APIC_REG1_VERSION_MASK       0x000000FF
#define IO_APIC_REG1_VERSION_OFFSET     0
#define IO_APIC_REG1_RES2_MASK          0x0000FF00
#define IO_APIC_REG1_RES2_OFFSET        8
#define IO_APIC_REG1_ENTRIES_MASK       0x00FF0000
#define IO_APIC_REG1_ENTRIES_OFFSET     16
#define IO_APIC_REG1_RES1_MASK          0xFF000000
#define IO_APIC_REG1_RES1_OFFSET        24

#define IO_APIC_REG2_RES1_MASK          0x00FFFFFF
#define IO_APIC_REG2_RES1_OFFSET        0
#define IO_APIC_REG2_RES2_MASK          0xF0000000
#define IO_APIC_REG2_RES2_OFFSET        28

#define IO_APIC_ROUTE_VECTOR_MASK       0x000000FF
#define IO_APIC_ROUTE_VECTOR_OFFSET     0

#define IO_APIC_INTMASK_MASK            0x00010000
#define IO_APIC_INTMASK_OFFSET          16

#define IO_APIC_DELMODE_MASK            0x00000700
#define IO_APIC_DELMODE_OFFSET          8

#define IO_APIC_DESTMODE_MASK           0x00000800
#define IO_APIC_DESTMODE_OFFSET         11

#define IO_APIC_POLARITY_MASK           0x00002000
#define IO_APIC_POLARITY_OFFSET         13

#define IO_APIC_TRIGGER_MASK            0x00008000
#define IO_APIC_TRIGGER_OFFSET          15

#define IO_APIC_DEST_MASK               0xff000000
#define IO_APIC_DEST_OFFSET             24

#define IO_APIC_REG0_RES1(_reg) \
   ((_reg & IO_APIC_REG0_RES1_MASK) >> IO_APIC_REG0_RES1_OFFSET)
#define IO_APIC_REG0_RES2(_reg) \
   ((_reg & IO_APIC_REG0_RES2_MASK) >> IO_APIC_REG0_RES2_OFFSET)
#define IO_APIC_REG1_RES1(_reg) \
   ((_reg & IO_APIC_REG1_RES1_MASK) >> IO_APIC_REG1_RES1_OFFSET)
#define IO_APIC_REG1_RES2(_reg) \
   ((_reg & IO_APIC_REG1_RES2_MASK) >> IO_APIC_REG1_RES2_OFFSET)
#define IO_APIC_REG2_RES1(_reg) \
   ((_reg & IO_APIC_REG2_RES1_MASK) >> IO_APIC_REG2_RES1_OFFSET)
#define IO_APIC_REG2_RES2(_reg) \
   ((_reg & IO_APIC_REG2_RES2_MASK) >> IO_APIC_REG2_RES2_OFFSET)
#define IO_APIC_VERSION(_reg) \
   ((_reg & IO_APIC_REG1_VERSION_MASK) >> IO_APIC_REG1_VERSION_OFFSET)
#define IO_APIC_ENTRIES(_reg) \
   ((_reg & IO_APIC_REG1_ENTRIES_MASK) >> IO_APIC_REG1_ENTRIES_OFFSET)
#define IO_APIC_ENTRY_VECTOR(_entry) \
   ((_entry & IO_APIC_ROUTE_VECTOR_MASK) >> IO_APIC_ROUTE_VECTOR_OFFSET)
#define IO_APIC_ISMASKED(_entry) \
   ((_entry & IO_APIC_INTMASK_MASK) >> IO_APIC_INTMASK_OFFSET)
#define IO_APIC_ENTRY_DELMODE(_entry) \
   ((_entry & IO_APIC_DELMODE_MASK) >> IO_APIC_DELMODE_OFFSET)
#define IO_APIC_ENTRY_DESTMODE(_entry) \
   ((_entry & IO_APIC_DESTMODE_MASK) >> IO_APIC_DESTMODE_OFFSET)
#define IO_APIC_ENTRY_POLARITY(_entry) \
   ((_entry & IO_APIC_POLARITY_MASK) >> IO_APIC_POLARITY_OFFSET)
#define IO_APIC_ENTRY_TRIGGER(_entry) \
   ((_entry & IO_APIC_TRIGGER_MASK) >> IO_APIC_TRIGGER_OFFSET)
#define IO_APIC_ENTRY_DEST(_entry) \
   ((_entry & IO_APIC_DEST_MASK) >> IO_APIC_DEST_OFFSET)

//
// We emulate an IOAPIC with 24 redirection registers
//
#define IOAPIC_NUM_REDIR_REGS 24

#endif // _X86APIC_H_
