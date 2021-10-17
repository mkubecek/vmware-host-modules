/*********************************************************
 * Copyright (C) 2008-2021 VMware, Inc. All rights reserved.
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

#ifndef _VIRTUAL_VT_H_
#define _VIRTUAL_VT_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMX

#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "x86_basic_defs.h"
#include "x86vt.h"

/*
 * Virtualized VT support
 */

#define VVT_NUM_MSRS                 (MSR_VMX_VMFUNC - MSR_VMX_BASIC + 1)

#define VVT_VMCS_ID                  CONST64U(1)
#define VVT_VMCS_SIZE                ((uint64)(PAGE_SIZE))
#define VVT_MEMTYPE                  ((uint64)(MTRR_TYPE_WB))

#define VVT_PINBASED_CTLS0           (VT_PINBASED_CTLS_DEFAULT1 |           \
                                      VVT_TRUE_PINBASED_CTLS0)
#define VVT_PINBASED_CTLS1           VVT_TRUE_PINBASED_CTLS1
#define VVT_PINBASED_CTLS            QWORD(VVT_PINBASED_CTLS1,              \
                                           VVT_PINBASED_CTLS0)

#define VVT_PROCBASED_CTLS0          (VT_PROCBASED_CTLS_DEFAULT1 |          \
                                      VVT_TRUE_PROCBASED_CTLS0)
#define VVT_PROCBASED_CTLS1          VVT_TRUE_PROCBASED_CTLS1
#define VVT_PROCBASED_CTLS           QWORD(VVT_PROCBASED_CTLS1,             \
                                           VVT_PROCBASED_CTLS0)

#define VVT_2ND_CTLS0                0
#define VVT_2ND_CTLS1                (VT_VMCS_2ND_VMEXEC_CTL_VMFUNC |       \
                                      VT_VMCS_2ND_VMEXEC_CTL_RDRAND |       \
                                      VT_VMCS_2ND_VMEXEC_CTL_RDSEED |       \
                                      VT_VMCS_2ND_VMEXEC_CTL_UNRESTRICTED | \
                                      VT_VMCS_2ND_VMEXEC_CTL_WBINVD |       \
                                      VT_VMCS_2ND_VMEXEC_CTL_RDTSCP |       \
                                      VT_VMCS_2ND_VMEXEC_CTL_X2APIC |       \
                                      VT_VMCS_2ND_VMEXEC_CTL_DT  |          \
                                      VT_VMCS_2ND_VMEXEC_CTL_EPT |          \
                                      VT_VMCS_2ND_VMEXEC_CTL_EPT_VIOL_VE |  \
                                      VT_VMCS_2ND_VMEXEC_CTL_VPID |         \
                                      VT_VMCS_2ND_VMEXEC_CTL_INVPCID |      \
                                      VT_VMCS_2ND_VMEXEC_CTL_XSAVES  |      \
                                      VT_VMCS_2ND_VMEXEC_CTL_PAUSE_LOOP |   \
                                      VT_VMCS_2ND_VMEXEC_CTL_EPT_MBX |      \
                                      VT_VMCS_2ND_VMEXEC_CTL_APIC |         \
                                      VT_VMCS_2ND_VMEXEC_CTL_PML |          \
                                      VT_VMCS_2ND_VMEXEC_CTL_ENCLS |        \
                                      VT_VMCS_2ND_VMEXEC_CTL_ENCLV |        \
                                      VT_VMCS_2ND_VMEXEC_CTL_EPC_VIRT_EXT)

#define VVT_2ND_CTLS                 QWORD(VVT_2ND_CTLS1,                   \
                                           VVT_2ND_CTLS0)

#define VVT_EXIT_CTLS0               (VT_EXIT_CTLS_DEFAULT1 |               \
                                      VVT_TRUE_EXIT_CTLS0)
#define VVT_EXIT_CTLS1               VVT_TRUE_EXIT_CTLS1
#define VVT_EXIT_CTLS                QWORD(VVT_EXIT_CTLS1,                  \
                                           VVT_EXIT_CTLS0)

#define VVT_ENTRY_CTLS0              (VT_ENTRY_CTLS_DEFAULT1 |              \
                                      VVT_TRUE_ENTRY_CTLS0)
#define VVT_ENTRY_CTLS1              VVT_TRUE_ENTRY_CTLS1
#define VVT_ENTRY_CTLS               QWORD(VVT_ENTRY_CTLS1,                 \
                                           VVT_ENTRY_CTLS0)

#define VVT_EPT_VPID                 (VVT_EPT_SUPPORT | VVT_VPID_SUPPORT)

#define VVT_TRUE_PINBASED_CTLS0      VT_PINBASED_CTLS_DEFAULT1
#define VVT_TRUE_PINBASED_CTLS1      (VT_REQUIRED_PINBASED_CTLS |           \
                                      VT_VMCS_PIN_VMEXEC_CTL_VNMI)
#define VVT_TRUE_PINBASED_CTLS       QWORD(VVT_TRUE_PINBASED_CTLS1,         \
                                           VVT_TRUE_PINBASED_CTLS0)

#define VVT_TRUE_PROCBASED_CTLS0     (VT_PROCBASED_CTLS_DEFAULT1 &          \
                                      ~(VT_VMCS_CPU_VMEXEC_CTL_LDCR3 |      \
                                        VT_VMCS_CPU_VMEXEC_CTL_STCR3))
#define VVT_TRUE_PROCBASED_CTLS1     (VT_REQUIRED_PROCBASED_CTLS |          \
                                      VT_VMCS_CPU_VMEXEC_CTL_MWAIT |        \
                                      VT_VMCS_CPU_VMEXEC_CTL_VNMI_WINDOW |  \
                                      VT_VMCS_CPU_VMEXEC_CTL_IOBITMAP |     \
                                      VT_VMCS_CPU_VMEXEC_CTL_MTF |          \
                                      VT_VMCS_CPU_VMEXEC_CTL_MSRBITMAP |    \
                                      VT_VMCS_CPU_VMEXEC_CTL_MONITOR |      \
                                      VT_VMCS_CPU_VMEXEC_CTL_PAUSE |        \
                                      VT_VMCS_CPU_VMEXEC_CTL_USE_2ND)
#define VVT_TRUE_PROCBASED_CTLS      QWORD(VVT_TRUE_PROCBASED_CTLS1,        \
                                           VVT_TRUE_PROCBASED_CTLS0)

#define VVT_TRUE_EXIT_CTLS0          (VT_EXIT_CTLS_DEFAULT1 &               \
                                      ~VT_VMCS_VMEXIT_CTL_SAVE_DEBUGCTL)
#define VVT_TRUE_EXIT_CTLS1          (VT_REQUIRED_EXIT_CTLS |               \
                                      VT_VMCS_VMEXIT_CTL_SAVE_EFER |        \
                                      VT_VMCS_VMEXIT_CTL_LOAD_EFER |        \
                                      VT_VMCS_VMEXIT_CTL_CLEAR_BNDCFGS |    \
                                      VT_VMCS_VMEXIT_CTL_LOAD_PKRS |        \
                                      VT_VMCS_VMEXIT_CTL_LOAD_PGC)
#define VVT_TRUE_EXIT_CTLS           QWORD(VVT_TRUE_EXIT_CTLS1,             \
                                           VVT_TRUE_EXIT_CTLS0)

#define VVT_TRUE_ENTRY_CTLS0         (VT_ENTRY_CTLS_DEFAULT1 &              \
                                      ~VT_VMCS_VMENTRY_CTL_LOAD_DEBUGCTL)
#define VVT_TRUE_ENTRY_CTLS1         (VT_REQUIRED_ENTRY_CTLS |              \
                                      VT_VMCS_VMENTRY_CTL_LOAD_EFER |       \
                                      VT_VMCS_VMENTRY_CTL_LOAD_BNDCFGS |    \
                                      VT_VMCS_VMENTRY_CTL_LOAD_PKRS    |    \
                                      VT_VMCS_VMENTRY_CTL_LOAD_PGC)
#define VVT_TRUE_ENTRY_CTLS          QWORD(VVT_TRUE_ENTRY_CTLS1,            \
                                           VVT_TRUE_ENTRY_CTLS0)

/*
 * If bit X is 1 in VVT_CR0_FIXED0, then that bit of CR0 is fixed to 1
 * in nested VMX operation.  Similarly, if bit X is 0 in VVT_CR0_FIXED1,
 * then that bit of CR0 is fixed to 0 in nested VMX operation.  Reserved
 * bits are not fixed to 0; they are simply ignored.
 */
#define VVT_CR0_FIXED0               (CR0_PG | CR0_NE | CR0_PE)
#define VVT_CR0_FIXED1               ~0

/*
 * If bit X is 1 in VVT_CR4_FIXED0, then that bit of CR4 is fixed to 1
 * in nested VMX operation.  Similarly, if bit X is 0 in VVT_CR4_FIXED1,
 * then that bit of CR4 is fixed to 0 in nested VMX operation.
 */
#define VVT_CR4_FIXED0               CR4_VMXE
#define VVT_CR4_FIXED1               (~CR4_RESERVED)

/*
 * Define VVT_MAX_INDEX as large as possible, given our VVMCS layout.
 * We have one page total.  2 dwords are used for the revision and abort
 * fields, and one boolean is used for the launched flag.  The remaining
 * space is for four two-dimensional arrays--two of uint64, one of uint32
 * and one of uint16.  The first dimension is VT_ENCODING_NUM_TYPES, and
 * the other dimension is VVT_MAX_INDEX + 1.
 *
 * (The numerical value of VVT_MAX_INDEX is actually 45.)
 */
#define VVT_MAX_INDEX                (((PAGE_SIZE - 2 * sizeof(uint32)         \
                                        - sizeof(Bool))  /                     \
                                       (VT_ENCODING_NUM_TYPES *                \
                                        (2 * sizeof(uint64) + sizeof(uint32) + \
                                         sizeof(uint16)))) - 1)

#define VVT_CR0_MASK                 ~(CR0_ET | CR0_NW | CR0_CD)

#define VVT_CR3_TARGETS              4

#define VVT_MISC                     (MSR_VMX_MISC_ACTSTATE_HLT | \
                                      MSR_VMX_MISC_ACTSTATE_SHUTDOWN | \
                                      MSR_VMX_MISC_ACTSTATE_SIPI | \
                                      MSR_VMX_MISC_VMEXIT_SAVES_LMA | \
                                      MSR_VMX_MISC_ZERO_VMENTRY_INSTLEN)

#define VVT_EPT_SUPPORT              (VT_REQUIRED_EPT_SUPPORT | \
                                      MSR_VMX_EPT_VPID_EPTE_X | \
                                      MSR_VMX_EPT_VPID_ETMT_UC | \
                                      MSR_VMX_EPT_VPID_INVEPT_EPT_CTX | \
                                      MSR_VMX_EPT_VPID_INVEPT_GLOBAL | \
                                      MSR_VMX_EPT_VPID_ACCESS_DIRTY | \
                                      MSR_VMX_EPT_VPID_ADV_EXIT_INFO)

#define VVT_VPID_SUPPORT             (VT_REQUIRED_VPID_SUPPORT | \
                                      MSR_VMX_EPT_VPID_INVVPID_VPID_CTX | \
                                      MSR_VMX_EPT_VPID_INVVPID_VPID_CTX_LOCAL)


#define VVT_VMFUNC_SUPPORT           VT_VMFUNC_MASK(SWITCH_EPTP)

#define VVT_SMI_HVSTATE_SHIFT        0
#define VVT_SMI_HVSTATE_MASK         0x3
#define VVT_SMI_CPL_SHIFT            2
#define VVT_SMI_CPL_MASK             (0x3 << VVT_SMI_CPL_SHIFT)
#define VVT_SMI_VM_SHIFT             4
#define VVT_SMI_VM_MASK              (1 << VVT_SMI_VM_SHIFT)
#define VVT_SMI_VMXE_SHIFT           5
#define VVT_SMI_VMXE_MASK            (1 << VVT_SMI_VMXE_SHIFT)


/*
 * VVT requires certain VMX features from the host.  In the following
 * requirements, CLEAR refers to VMX feature bits that must be clear,
 * and SET refers to VMX feature bits that must be set.
 */

#define VVT_REQUIRED_BASIC_CLEAR      MSR_VMX_BASIC_32BITPA
#define VVT_REQUIRED_BASIC_SET        (MSR_VMX_BASIC_ADVANCED_IOINFO | \
                                       MSR_VMX_BASIC_TRUE_CTLS)

#define VVT_REQUIRED_CR0_FIXED0_CLEAR ~(CR0_PG | CR0_NE | CR0_PE)
#define VVT_REQUIRED_CR0_FIXED1_SET   (~0U)

#define VVT_REQUIRED_CR4_FIXED0_CLEAR (~CR4_VMXE)
#define VVT_REQUIRED_CR4_FIXED1_SET   (CR4_VMXE | CR4_OSXMMEXCPT | \
                                       CR4_OSFXSR | CR4_PCE | CR4_PGE | \
                                       CR4_MCE | CR4_PAE | CR4_PSE | \
                                       CR4_DE | CR4_TSD | CR4_PVI | CR4_VME)

#define VVT_REQUIRED_EPT_VPID_SET     (MSR_VMX_EPT_VPID_EPTE_X | \
                                       MSR_VMX_EPT_VPID_GAW_48 | \
                                       MSR_VMX_EPT_VPID_ETMT_WB | \
                                       MSR_VMX_EPT_VPID_SP_2MB | \
                                       MSR_VMX_EPT_VPID_INVEPT | \
                                       MSR_VMX_EPT_VPID_INVEPT_EPT_CTX | \
                                       MSR_VMX_EPT_VPID_INVEPT_GLOBAL | \
                                       MSR_VMX_EPT_VPID_INVVPID | \
                                       MSR_VMX_EPT_VPID_INVVPID_ADDR | \
                                       MSR_VMX_EPT_VPID_INVVPID_VPID_CTX | \
                                       MSR_VMX_EPT_VPID_INVVPID_ALL_CTX | \
                                       MSR_VMX_EPT_VPID_INVVPID_VPID_CTX_LOCAL)

#define VVT_REQUIRED_MIN_CR3_TARGETS  4

/*
 * For the remainder of the requirements, CLEAR refers to the low dword
 * of the MSR (bits that are allowed to be zero), and SET refers to the
 * high dword of the MSR (bits that are allowed to be one).
 */

#define VVT_REQUIRED_2ND_CLEAR       ~(VT_VMCS_2ND_VMEXEC_CTL_EPT | \
                                       VT_VMCS_2ND_VMEXEC_CTL_VPID | \
                                       VT_VMCS_2ND_VMEXEC_CTL_UNRESTRICTED)
#define VVT_REQUIRED_2ND_SET          (VT_VMCS_2ND_VMEXEC_CTL_EPT | \
                                       VT_VMCS_2ND_VMEXEC_CTL_DT | \
                                       VT_VMCS_2ND_VMEXEC_CTL_RDTSCP | \
                                       VT_VMCS_2ND_VMEXEC_CTL_VPID | \
                                       VT_VMCS_2ND_VMEXEC_CTL_WBINVD)

#define VVT_REQUIRED_TRUE_PIN_CLEAR   (~VT_PINBASED_CTLS_DEFAULT1)
#define VVT_REQUIRED_TRUE_PIN_SET     (VT_PINBASED_CTLS_DEFAULT1 | \
                                       VT_VMCS_PIN_VMEXEC_CTL_EXTINT_EXIT | \
                                       VT_VMCS_PIN_VMEXEC_CTL_NMI_EXIT | \
                                       VT_VMCS_PIN_VMEXEC_CTL_VNMI)

#define VVT_REQUIRED_TRUE_CPU_CLEAR   (~VT_PROCBASED_CTLS_DEFAULT1 | \
                                       VT_VMCS_CPU_VMEXEC_CTL_LDCR3 | \
                                       VT_VMCS_CPU_VMEXEC_CTL_STCR3)
#define VVT_REQUIRED_TRUE_CPU_SET     (VT_PROCBASED_CTLS_DEFAULT1 | \
                                       VT_VMCS_CPU_VMEXEC_CTL_VINTR_WINDOW | \
                                       VT_VMCS_CPU_VMEXEC_CTL_TSCOFF | \
                                       VT_VMCS_CPU_VMEXEC_CTL_HLT | \
                                       VT_VMCS_CPU_VMEXEC_CTL_INVLPG | \
                                       VT_VMCS_CPU_VMEXEC_CTL_MWAIT | \
                                       VT_VMCS_CPU_VMEXEC_CTL_RDPMC | \
                                       VT_VMCS_CPU_VMEXEC_CTL_RDTSC | \
                                       VT_VMCS_CPU_VMEXEC_CTL_LDCR3 | \
                                       VT_VMCS_CPU_VMEXEC_CTL_STCR3 | \
                                       VT_VMCS_CPU_VMEXEC_CTL_LDCR8 | \
                                       VT_VMCS_CPU_VMEXEC_CTL_STCR8 | \
                                       VT_VMCS_CPU_VMEXEC_CTL_TPR_SHADOW | \
                                       VT_VMCS_CPU_VMEXEC_CTL_VNMI_WINDOW | \
                                       VT_VMCS_CPU_VMEXEC_CTL_MOVDR | \
                                       VT_VMCS_CPU_VMEXEC_CTL_IO | \
                                       VT_VMCS_CPU_VMEXEC_CTL_IOBITMAP | \
                                       VT_VMCS_CPU_VMEXEC_CTL_MTF | \
                                       VT_VMCS_CPU_VMEXEC_CTL_MSRBITMAP | \
                                       VT_VMCS_CPU_VMEXEC_CTL_MONITOR | \
                                       VT_VMCS_CPU_VMEXEC_CTL_USE_2ND)


typedef struct {
   uint32 revision;
   uint32 abort;
   uint64 field64[VT_ENCODING_NUM_TYPES][VVT_MAX_INDEX + 1];
   uint64 fieldNat[VT_ENCODING_NUM_TYPES][VVT_MAX_INDEX + 1];
   uint32 field32[VT_ENCODING_NUM_TYPES][VVT_MAX_INDEX + 1];
   uint16 field16[VT_ENCODING_NUM_TYPES][VVT_MAX_INDEX + 1];
   Bool   launched;
} VVMCS;


/*
 *----------------------------------------------------------------------
 *
 * VVT_SupportedFromFeatures --
 *
 *   Returns TRUE if the given VMX features are compatible with our VVT
 *   implementation.
 *
 *   We assume that the un-TRUE VMX capabilities match the TRUE VMX
 *   capabilities, except that all default1 bits are set.  Since we
 *   require TRUE VMX capabilities, we only check those.
 *
 *   The minimum supported hardware has the intersection of the
 *   Nehalem feature set with the VMX capabilities of HWv9.
 *
 *----------------------------------------------------------------------
 */
static INLINE Bool
VVT_SupportedFromFeatures(uint64 basic, uint64 misc, uint64 cr0Fixed0,
                          uint64 cr0Fixed1, uint64 cr4Fixed0,
                          uint64 cr4Fixed1, uint64 secondary, uint64 eptVPID,
                          uint64 truePin, uint64 trueCPU)
{
   if ((basic & VVT_REQUIRED_BASIC_CLEAR) != 0 ||
       (basic & VVT_REQUIRED_BASIC_SET) != VVT_REQUIRED_BASIC_SET) {
      return FALSE;
   }

   if ((cr0Fixed0 & VVT_REQUIRED_CR0_FIXED0_CLEAR) != 0 ||
       (cr0Fixed1 & VVT_REQUIRED_CR0_FIXED1_SET) !=
       VVT_REQUIRED_CR0_FIXED1_SET) {
      return FALSE;
   }

   if ((cr4Fixed0 & VVT_REQUIRED_CR4_FIXED0_CLEAR) != 0 ||
       (cr4Fixed1 & VVT_REQUIRED_CR4_FIXED1_SET) !=
       VVT_REQUIRED_CR4_FIXED1_SET) {
      return FALSE;
   }

   if ((eptVPID & VVT_REQUIRED_EPT_VPID_SET) != VVT_REQUIRED_EPT_VPID_SET) {
      return FALSE;
   }

   if (((misc >> MSR_VMX_MISC_CR3_TARGETS_SHIFT) &
        MSR_VMX_MISC_CR3_TARGETS_MASK) < VVT_REQUIRED_MIN_CR3_TARGETS) {
      return FALSE;
   }

   if ((LODWORD(secondary) & VVT_REQUIRED_2ND_CLEAR) != 0 ||
       (HIDWORD(secondary) & VVT_REQUIRED_2ND_SET) != VVT_REQUIRED_2ND_SET) {
      return FALSE;
   }

   if ((LODWORD(truePin) & VVT_REQUIRED_TRUE_PIN_CLEAR) != 0 ||
       (HIDWORD(truePin) & VVT_REQUIRED_TRUE_PIN_SET) !=
       VVT_REQUIRED_TRUE_PIN_SET) {
      return FALSE;
   }

   if ((LODWORD(trueCPU) & VVT_REQUIRED_TRUE_CPU_CLEAR) != 0 ||
       (HIDWORD(trueCPU) & VVT_REQUIRED_TRUE_CPU_SET) !=
       VVT_REQUIRED_TRUE_CPU_SET) {
      return FALSE;
   }

   return TRUE;
}

#endif /* _VIRTUAL_VT_H_ */
