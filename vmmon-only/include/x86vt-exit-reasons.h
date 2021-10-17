/*********************************************************
 * Copyright (C) 2016-2018,2021 VMware, Inc. All rights reserved.
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
 * x86vt-exit-reasons.h --
 *
 * VT exit reasons.
 *
 */

#include "community_source.h"

#define VT_EXITREASON_VMENTRYFAIL           (1U << 31)

VT_EXIT(EXC_OR_NMI,            0)
VT_EXIT(EXTINT,                1)
VT_EXIT(TRIPLEFAULT,           2)
VT_EXIT(INIT,                  3)
VT_EXIT(SIPI,                  4)
VT_EXIT(IOSMI,                 5)
VT_EXIT(OTHERSMI,              6)
VT_EXIT(VINTR_WINDOW,          7)
VT_EXIT(VNMI_WINDOW,           8)
VT_EXIT(TS,                    9)
VT_EXIT(CPUID,                10)
VT_EXIT(GETSEC,               11)
VT_EXIT(HLT,                  12)
VT_EXIT(INVD,                 13)
VT_EXIT(INVLPG,               14)
VT_EXIT(RDPMC,                15)
VT_EXIT(RDTSC,                16)
VT_EXIT(RSM,                  17)
VT_EXIT(VMCALL,               18)
VT_EXIT(VMCLEAR,              19)
VT_EXIT(VMLAUNCH,             20)
VT_EXIT(VMPTRLD,              21)
VT_EXIT(VMPTRST,              22)
VT_EXIT(VMREAD,               23)
VT_EXIT(VMRESUME,             24)
VT_EXIT(VMWRITE,              25)
VT_EXIT(VMXOFF,               26)
VT_EXIT(VMXON,                27)
VT_EXIT(CR,                   28)
VT_EXIT(DR,                   29)
VT_EXIT(IO,                   30)
VT_EXIT(RDMSR,                31)
VT_EXIT(WRMSR,                32)
VT_EXIT(VMENTRYFAIL_GUEST,   (33 | VT_EXITREASON_VMENTRYFAIL))
VT_EXIT(VMENTRYFAIL_MSR,     (34 | VT_EXITREASON_VMENTRYFAIL))
VT_EXIT(VMEXIT35,             35)
VT_EXIT(MWAIT,                36)
VT_EXIT(MTF,                  37)
VT_EXIT(VMEXIT38,             38)
VT_EXIT(MONITOR,              39)
VT_EXIT(PAUSE,                40)
VT_EXIT(VMENTRYFAIL_MC,      (41 | VT_EXITREASON_VMENTRYFAIL))
VT_EXIT(VMEXIT42,             42)
VT_EXIT(TPR,                  43)
VT_EXIT(APIC,                 44)
VT_EXIT(EOI,                  45)
VT_EXIT(GDTR_IDTR,            46)
VT_EXIT(LDTR_TR,              47)
VT_EXIT(EPT_VIOLATION,        48)
VT_EXIT(EPT_MISCONFIG,        49)
VT_EXIT(INVEPT,               50)
VT_EXIT(RDTSCP,               51)
VT_EXIT(TIMER,                52)
VT_EXIT(INVVPID,              53)
VT_EXIT(WBINVD,               54)
VT_EXIT(XSETBV,               55)
VT_EXIT(APIC_WRITE,           56)
VT_EXIT(RDRAND,               57)
VT_EXIT(INVPCID,              58)
VT_EXIT(VMFUNC,               59)
VT_EXIT(ENCLS,                60)
VT_EXIT(RDSEED,               61)
VT_EXIT(PML_LOGFULL,          62)
VT_EXIT(XSAVES,               63)
VT_EXIT(XRSTORS,              64)
VT_EXIT(VMEXIT65,             65)
VT_EXIT(VMEXIT66,             66)
VT_EXIT(UMWAIT,               67)
VT_EXIT(TPAUSE,               68)
VT_EXIT(VMEXIT69,             69)
VT_EXIT(ENCLV,                70)
VT_EXIT(SGX_CONFLICT,         71)
/* Bump this up if you add an exit reason. */
#define VT_NUM_EXIT_REASONS   72
