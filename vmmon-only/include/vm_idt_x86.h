/*********************************************************
 * Copyright (C) 2012,2017-2018,2020 VMware, Inc. All rights reserved.
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
 * vm_idt_x86.h --
 *
 *	Definitions for IDT use in x86/x86-64 ESX and hosted VMware Products.
 */

#ifndef _VM_IDT_X86_H_
#define _VM_IDT_X86_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#if defined __cplusplus
extern "C" {
#endif

/*
 * User worlds are the only worlds that expect to receive
 * interrupts on a stack specified in the %rsp entries of
 * the TSS.
 */
#define TSS_RSP0 0
#define TSS_RSP1 1
#define TSS_RSP2 2

#define IDT_NUM_GATES              0x100
/*
 * The monitor and the vmkernel use gate stub handlers of a single size in
 * their IDTs.  The only exception is the #PF gate (#14) which is twice as
 * long in the monitor IDT, to copy CR2.
 */
#define IDT_STUB_SIZE              16

#define IST_NONE                    0
/* IST entries for the monitor. */
#define IST_VMM_DF                  1
#define IST_VMM_NMI                 2
#define IST_VMM_MCE                 3
#define MAX_VMM_IST                 3
/* IST entries for the vmkernel. */
#define IST_VMK_MCE                 5
#define IST_VMK_DF                  6
#define IST_VMK_NMI                 7


static INLINE int
IDT_MonitorISTForVector(int v)
{
   /*
    * For non-SVM execution, the monitor uses the IST for #DF, #NMI, and #MC.
    */
   if (v == EXC_DF) {
      return IST_VMM_DF;
   } else if (v == EXC_NMI) {
      return IST_VMM_NMI;
   } else if (v == EXC_MC) {
      return IST_VMM_MCE;
   }
   return IST_NONE;
}

#if defined __cplusplus
} // extern "C"
#endif

#endif /* _VM_IDT_X86_H_ */
