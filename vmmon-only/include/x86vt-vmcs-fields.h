/*********************************************************
 * Copyright (C) 2015 VMware, Inc. All rights reserved.
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
 * x86vt-vmcs-fields.h --
 *
 * VMCS encodings; SDM volume 3 Appendix B.
 * These are the values passed to VMWRITE and VMREAD.
 *
 * VMCS_FIELD(name, encoding)
 */

#include "community_source.h"

/*
 * VMCS_FIELD(name,                    encoding, vVT support)
 */

/* 16-bit control fields. */
VMCS_FIELD(VPID,                    0x0000, TRUE)
VMCS_FIELD(PI_NOTIFY,               0x0002, FALSE)
VMCS_FIELD(EPTP_INDEX,              0x0004, FALSE)

/* 16-bit guest state. */
VMCS_FIELD(ES,                      0x0800, TRUE)
VMCS_FIELD(CS,                      0x0802, TRUE)
VMCS_FIELD(SS,                      0x0804, TRUE)
VMCS_FIELD(DS,                      0x0806, TRUE)
VMCS_FIELD(FS,                      0x0808, TRUE)
VMCS_FIELD(GS,                      0x080A, TRUE)
VMCS_FIELD(LDTR,                    0x080C, TRUE)
VMCS_FIELD(TR,                      0x080E, TRUE)
VMCS_FIELD(INTR_STATUS,             0x0810, FALSE)

/* 16-bit host state. */
VMCS_FIELD(HOST_ES,                 0x0C00, TRUE)
VMCS_FIELD(HOST_CS,                 0x0C02, TRUE)
VMCS_FIELD(HOST_SS,                 0x0C04, TRUE)
VMCS_FIELD(HOST_DS,                 0x0C06, TRUE)
VMCS_FIELD(HOST_FS,                 0x0C08, TRUE)
VMCS_FIELD(HOST_GS,                 0x0C0A, TRUE)
VMCS_FIELD(HOST_TR,                 0x0C0C, TRUE)

/* 64-bit control fields. */
VMCS_FIELD(IOBITMAPA,               0x2000, TRUE)
VMCS_FIELD(IOBITMAPB,               0x2002, TRUE)
VMCS_FIELD(MSRBITMAP,               0x2004, TRUE)
VMCS_FIELD(VMEXIT_MSR_STORE_ADDR,   0x2006, TRUE)
VMCS_FIELD(VMEXIT_MSR_LOAD_ADDR,    0x2008, TRUE)
VMCS_FIELD(VMENTRY_MSR_LOAD_ADDR,   0x200A, TRUE)
VMCS_FIELD(EXECUTIVE_VMCS_PTR,      0x200C, TRUE)
VMCS_FIELD(TSC_OFF,                 0x2010, TRUE)
VMCS_FIELD(VIRT_APIC_ADDR,          0x2012, TRUE)
VMCS_FIELD(APIC_ACCESS_ADDR,        0x2014, FALSE)
VMCS_FIELD(PI_DESC_ADDR,            0x2016, FALSE)
VMCS_FIELD(VMFUNC_CTLS,             0x2018, FALSE)
VMCS_FIELD(EPTP,                    0x201A, TRUE)
VMCS_FIELD(EOI_EXIT0,               0x201C, FALSE)
VMCS_FIELD(EOI_EXIT1,               0x201E, FALSE)
VMCS_FIELD(EOI_EXIT2,               0x2020, FALSE)
VMCS_FIELD(EOI_EXIT3,               0x2022, FALSE)
VMCS_FIELD(EPTP_LIST_ADDR,          0x2024, FALSE)
VMCS_FIELD(VMREAD_BITMAP,           0x2026, FALSE)
VMCS_FIELD(VMWRITE_BITMAP,          0x2028, FALSE)
VMCS_FIELD(VE_INFO_ADDR,            0x202A, FALSE)
VMCS_FIELD(XSS_EXITING_BITMAP,      0x202C, FALSE)
VMCS_FIELD(ENCLS_EXITING_BITMAP,    0x202E, FALSE)

/* 64-bit read-only data field. */
VMCS_FIELD(PHYSADDR,                0x2400, TRUE)

/* 64-bit guest state. */
VMCS_FIELD(LINK_PTR,                0x2800, TRUE)
VMCS_FIELD(DEBUGCTL,                0x2802, TRUE)
VMCS_FIELD(PAT,                     0x2804, FALSE)
VMCS_FIELD(EFER,                    0x2806, TRUE)
VMCS_FIELD(PGC,                     0x2808, TRUE)
VMCS_FIELD(PDPTE0,                  0x280A, TRUE)
VMCS_FIELD(PDPTE1,                  0x280C, TRUE)
VMCS_FIELD(PDPTE2,                  0x280E, TRUE)
VMCS_FIELD(PDPTE3,                  0x2810, TRUE)

/* 64-bit host state. */
VMCS_FIELD(HOST_PAT,                0x2C00, FALSE)
VMCS_FIELD(HOST_EFER,               0x2C02, TRUE)
VMCS_FIELD(HOST_PGC,                0x2C04, TRUE)

/* 32-bit control fields. */
VMCS_FIELD(PIN_VMEXEC_CTL,          0x4000, TRUE)
VMCS_FIELD(CPU_VMEXEC_CTL,          0x4002, TRUE)
VMCS_FIELD(XCP_BITMAP,              0x4004, TRUE)
VMCS_FIELD(PF_ERR_MASK,             0x4006, TRUE)
VMCS_FIELD(PF_ERR_MATCH,            0x4008, TRUE)
VMCS_FIELD(CR3_TARG_COUNT,          0x400A, TRUE)
VMCS_FIELD(VMEXIT_CTL,              0x400C, TRUE)
VMCS_FIELD(VMEXIT_MSR_STORE_COUNT,  0x400E, TRUE)
VMCS_FIELD(VMEXIT_MSR_LOAD_COUNT,   0x4010, TRUE)
VMCS_FIELD(VMENTRY_CTL,             0x4012, TRUE)
VMCS_FIELD(VMENTRY_MSR_LOAD_COUNT,  0x4014, TRUE)
VMCS_FIELD(VMENTRY_INTR_INFO,       0x4016, TRUE)
VMCS_FIELD(VMENTRY_XCP_ERR,         0x4018, TRUE)
VMCS_FIELD(VMENTRY_INSTR_LEN,       0x401A, TRUE)
VMCS_FIELD(TPR_THRESHOLD,           0x401C, TRUE)
VMCS_FIELD(2ND_VMEXEC_CTL,          0x401E, TRUE)
VMCS_FIELD(PAUSE_LOOP_GAP,          0x4020, FALSE)
VMCS_FIELD(PAUSE_LOOP_WINDOW,       0x4022, FALSE)

/* 32-bit read-only data fields. */
VMCS_FIELD(VMINSTR_ERR,             0x4400, TRUE)
VMCS_FIELD(EXIT_REASON,             0x4402, TRUE)
VMCS_FIELD(EXIT_INTR_INFO,          0x4404, TRUE)
VMCS_FIELD(EXIT_INTR_ERR,           0x4406, TRUE)
VMCS_FIELD(IDTVEC_INFO,             0x4408, TRUE)
VMCS_FIELD(IDTVEC_ERR,              0x440A, TRUE)
VMCS_FIELD(INSTRLEN,                0x440C, TRUE)
VMCS_FIELD(INSTR_INFO,              0x440E, TRUE)

/* 32-bit guest state. */
VMCS_FIELD(ES_LIMIT,                0x4800, TRUE)
VMCS_FIELD(CS_LIMIT,                0x4802, TRUE)
VMCS_FIELD(SS_LIMIT,                0x4804, TRUE)
VMCS_FIELD(DS_LIMIT,                0x4806, TRUE)
VMCS_FIELD(FS_LIMIT,                0x4808, TRUE)
VMCS_FIELD(GS_LIMIT,                0x480A, TRUE)
VMCS_FIELD(LDTR_LIMIT,              0x480C, TRUE)
VMCS_FIELD(TR_LIMIT,                0x480E, TRUE)
VMCS_FIELD(GDTR_LIMIT,              0x4810, TRUE)
VMCS_FIELD(IDTR_LIMIT,              0x4812, TRUE)
VMCS_FIELD(ES_AR,                   0x4814, TRUE)
VMCS_FIELD(CS_AR,                   0x4816, TRUE)
VMCS_FIELD(SS_AR,                   0x4818, TRUE)
VMCS_FIELD(DS_AR,                   0x481A, TRUE)
VMCS_FIELD(FS_AR,                   0x481C, TRUE)
VMCS_FIELD(GS_AR,                   0x481E, TRUE)
VMCS_FIELD(LDTR_AR,                 0x4820, TRUE)
VMCS_FIELD(TR_AR,                   0x4822, TRUE)
VMCS_FIELD(HOLDOFF,                 0x4824, TRUE)
VMCS_FIELD(ACTSTATE,                0x4826, TRUE)
VMCS_FIELD(SMBASE,                  0x4828, TRUE)
VMCS_FIELD(SYSENTER_CS,             0x482A, TRUE)
VMCS_FIELD(TIMER,                   0x482E, FALSE)

/* 32-bit host state. */
VMCS_FIELD(HOST_SYSENTER_CS,        0x4C00, TRUE)

/* natural-width control fields. */
VMCS_FIELD(CR0_GHMASK,              0x6000, TRUE)
VMCS_FIELD(CR4_GHMASK,              0x6002, TRUE)
VMCS_FIELD(CR0_SHADOW,              0x6004, TRUE)
VMCS_FIELD(CR4_SHADOW,              0x6006, TRUE)
VMCS_FIELD(CR3_TARGVAL0,            0x6008, TRUE)
VMCS_FIELD(CR3_TARGVAL1,            0x600A, TRUE)
VMCS_FIELD(CR3_TARGVAL2,            0x600C, TRUE)
VMCS_FIELD(CR3_TARGVAL3,            0x600E, TRUE)

/* natural-width read-only data fields. */
VMCS_FIELD(EXIT_QUAL,               0x6400, TRUE)
VMCS_FIELD(IO_ECX,                  0x6402, TRUE)
VMCS_FIELD(IO_ESI,                  0x6404, TRUE)
VMCS_FIELD(IO_EDI,                  0x6406, TRUE)
VMCS_FIELD(IO_EIP,                  0x6408, TRUE)
VMCS_FIELD(LINEAR_ADDR,             0x640A, TRUE)

/* natural-width guest state. */
VMCS_FIELD(CR0,                     0x6800, TRUE)
VMCS_FIELD(CR3,                     0x6802, TRUE)
VMCS_FIELD(CR4,                     0x6804, TRUE)
VMCS_FIELD(ES_BASE,                 0x6806, TRUE)
VMCS_FIELD(CS_BASE,                 0x6808, TRUE)
VMCS_FIELD(SS_BASE,                 0x680A, TRUE)
VMCS_FIELD(DS_BASE,                 0x680C, TRUE)
VMCS_FIELD(FS_BASE,                 0x680E, TRUE)
VMCS_FIELD(GS_BASE,                 0x6810, TRUE)
VMCS_FIELD(LDTR_BASE,               0x6812, TRUE)
VMCS_FIELD(TR_BASE,                 0x6814, TRUE)
VMCS_FIELD(GDTR_BASE,               0x6816, TRUE)
VMCS_FIELD(IDTR_BASE,               0x6818, TRUE)
VMCS_FIELD(DR7,                     0x681A, TRUE)
VMCS_FIELD(ESP,                     0x681C, TRUE)
VMCS_FIELD(EIP,                     0x681E, TRUE)
VMCS_FIELD(EFLAGS,                  0x6820, TRUE)
VMCS_FIELD(PENDDBG,                 0x6822, TRUE)
VMCS_FIELD(SYSENTER_ESP,            0x6824, TRUE)
VMCS_FIELD(SYSENTER_EIP,            0x6826, TRUE)

/* natural-width host state. */
VMCS_FIELD(HOST_CR0,                0x6C00, TRUE)
VMCS_FIELD(HOST_CR3,                0x6C02, TRUE)
VMCS_FIELD(HOST_CR4,                0x6C04, TRUE)
VMCS_FIELD(HOST_FSBASE,             0x6C06, TRUE)
VMCS_FIELD(HOST_GSBASE,             0x6C08, TRUE)
VMCS_FIELD(HOST_TRBASE,             0x6C0A, TRUE)
VMCS_FIELD(HOST_GDTRBASE,           0x6C0C, TRUE)
VMCS_FIELD(HOST_IDTRBASE,           0x6C0E, TRUE)
VMCS_FIELD(HOST_SYSENTER_ESP,       0x6C10, TRUE)
VMCS_FIELD(HOST_SYSENTER_EIP,       0x6C12, TRUE)
VMCS_FIELD(HOST_ESP,                0x6C14, TRUE)
VMCS_FIELD(HOST_EIP,                0x6C16, TRUE)
