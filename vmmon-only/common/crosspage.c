/*********************************************************
 * Copyright (C) 2016-2020 VMware, Inc. All rights reserved.
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
 * crosspage.c --
 *
 *    Cross page code and data.
 *
 *    The cross code page contains world switch code and interrupt/exception
 *    handlers in support of world switch.  A separate data page is also mapped
 *    in both the vmm and vmmon address spaces to describe the host and monitor
 *    state.
 *
 *    Both the monitor and the vmmon driver call into the cross page to switch
 *    worlds.  The world switch from host->vmm is able to refer directly to the
 *    various symbols defined in CrossPage_CodePage.  On the vmm->host path
 *    the VmmToHost entry point is stored in the data page.  Data references
 *    must be handled as offsets from the base of the data page.
 *
 *    Use of inline assembly in C code is delicate as the compiler has many
 *    possible reasons to emit unexpected instructions surrounding any assembly
 *    in a C function.  Frame pointers are inconvenient to disable on a per-file
 *    basis given structure of the Linux Makefile so they are being emitted
 *    in the C wrapper function.  NORETURN and NOT_REACHED_MINIMAL are used to
 *    omit a function epilogue.  The output/clobber lists of the assembly blocks
 *    are not used as the compiler needs not compensate for the register use of
 *    these hand-written assembly functions.
 *    Each assembly function begins with a label denoting the proper entry point
 *    for that function.  Only a handful of the functions need to be exported
 *    out of this file, see crosspage.h.
 */

#ifdef __linux__
#   include "driver-config.h"
/*
 * linux/frame.h dates back to 4.6-rc1, we need the STACK_FRAME_NON_STANDARD
 * definition from it which is there from the start.
 */
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#      include <linux/objtool.h>
#   elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
#      include <linux/frame.h>
#   endif
#endif

#include "modulecall.h"
#include "iocontrols.h"
#include "segs.h"
#include "x86_basic_defs.h"
#include "vm_basic_defs.h"
#include "vm_idt_x86.h"
#include "crosspage.h"

#ifdef __APPLE__
/*
 * The Mac compiler expects symbols to have a _ prefix.
 * This arranges for any symbol being exported out of the assembly code
 * to be accessible.
 */
#define ASM_PREFIX "_"
/*
 * OSX uses a segment,section style syntax for specifying output sections.
 * https://developer.apple.com/library/archive/documentation/Performance/
 * Conceptual/CodeFootprint/Articles/MachOOverview.html
 */
#define ASM_SECTION "__TEXT,cross"
#else
/*
 * This is a COFF grouped section identifier.  This ensures the crosspage code
 * is merged with the normal .text area but as a separate unit so it can have
 * its own alignment properties.
 * https://docs.microsoft.com/en-us/windows/win32/debug/pe-format
 *
 * The Linux build will handle the section based on its contents (code).
 */
#define ASM_SECTION ".text$cross"
#define ASM_PREFIX
#endif

#define EXPORTED_ASM_SYMBOL(fn) ".global " ASM_PREFIX #fn "\n"   \
                                ASM_PREFIX #fn ":\n"

/*
 * Tag the crosspage code C wrapper with the crosspage section and page
 * alignment.
 */
#define CPCODE __attribute__((section(ASM_SECTION)))   \
               __attribute__((aligned(PAGE_SIZE)))

/* No special handling is needed for the data. */
#define CPDATA

#define VMMDATALA(off) (LPN_2_LA(CROSS_PAGE_DATA_START) + (off))

#define NOT_REACHED_MINIMAL __builtin_unreachable

#ifdef ANNOTATE_INTRA_FUNCTION_CALL
#define ANNOTATE_ASM_CALL_STR2(x) #x
#define ANNOTATE_ASM_CALL_STR(x) ANNOTATE_ASM_CALL_STR2(x) "\n"
#define ANNOTATE_ASM_CALL ANNOTATE_ASM_CALL_STR(ANNOTATE_INTRA_FUNCTION_CALL)
#else
#define ANNOTATE_ASM_CALL
#endif

void VmmToHost(void);

CPDATA const VMCrossPageData cpDataTemplate = {
   .version        = CROSSPAGE_VERSION,
   .vmmonVersion   = VMMON_VERSION,

   .monRSP         = VMM_LRET_STACK_TOP,
   .monSS          = SYSTEM_DATA_SELECTOR,
   .monDS          = SYSTEM_DATA_SELECTOR,
   .monES          = SYSTEM_DATA_SELECTOR,

   .vmmToHostLA    = (VA)VmmToHost,
   .wsCR0          = CR0_PE | CR0_MP | CR0_EM | CR0_NE | CR0_WP | CR0_PG,
   .wsCR4          = CR4_PAE | CR4_OSFXSR,

   .monTask.rsp[0] = VMM_STACK_TOP,                   /* Monitor stack. */
   .monTask.rsp[1] = VPN_2_VA(VMM_STACK_GUARD_START), /* CPL1 is not used. */
   .monTask.rsp[2] = VPN_2_VA(VMM_STACK_GUARD_START), /* CPL2 is not used. */

   .monTask.ist[IST_NONE]                 = 0, /* No stack switch. */
   .monTask.ist[IST_VMM_DF]               = DF_STACK_TOP,
   .monTask.ist[IST_VMM_NMI]              = NMI_STACK_TOP,
   .monTask.ist[IST_VMM_MCE]              = MC_STACK_TOP,

   .monTask.IOMapBase = sizeof(Task64),

   .monGDTR.limit  = VMMON_GDT_LIMIT,
   .monGDTR.offset = GDT_START_VA,

   .shadowDR[6].ureg64 = DR6_DEFAULT,
   .shadowDR[7].ureg64 = DR7_ONES,

   /*
    * switchHostIDT and switchHostIDTR are initialized in vmmon.
    * switchMonIDTR is static and can be fully initialized at compile-time.
    * switchMonIDT is mostly static with all non-present entries except for
    * the 4 handled exceptions which are populated by the crosspage setup
    * code in task.c
    */
   .switchMonIDTR  = { sizeof(Gate64) * NUM_EXCEPTIONS - 1,
                       VMMDATALA(offsetof(VMCrossPageData, switchMonIDT)) },
};


/*
 *-----------------------------------------------------------------------------
 *
 * CrossPage_CodePage --
 *
 *      This function only serves as an anchor and wrapper for the code that
 *      runs in the world switch's cross page.  The function must not be called
 *      directly.  Its address may be used to locate the cross page code.
 *
 *-----------------------------------------------------------------------------
 */

CPCODE void
CrossPage_CodePage(void)
{
   __asm__ __volatile__ (


   /*
    *---------------------------------------------------------------------------
    *
    * SwitchDBHandler --
    *
    *      DB handler that operates during worldswitch (in both directions).
    *
    *      This handler is expected to be used in two cases:
    *      1) If the host kernel has set the DR7_GD (guard) bit, when switch
    *         code attempts to save debug registers, a #DB will be thrown.  In
    *         this case, the handler sets wsException[EXC_DB] and returns.
    *      2) When stress-testing the worldswitch code, RFLAGS.TF is set across
    *         most switch paths.  This causes a #DB to be thrown before each
    *         instruction is executed.  In this case, the handler must set
    *         RFLAGS.RF in the return frame to guarantee progress while
    *         RFLAGS.TF is set.  For added stress, the handler induces a
    *         simulated #NMI.
    *
    *      In case (1), returning is accomplished via a simulated iret in order
    *      to preserve any NMI-blocking.  In case (2), the inherent
    *      incompatibility of RFLAGS.RF for single-stepping and simulating an
    *      iret via popf and lretq necessitates the use of an actual iret.  As
    *      a result, there is no strong NMI-blocking guarantee when
    *      stress-testing the switch path.
    *
    * Input:
    *
    *       0(%rsp) = previous RIP
    *       8(%rsp) = previous CS
    *      16(%rsp) = previous RFLAGS
    *      24(%rsp) = previous RSP
    *      32(%rsp) = previous SS
    *
    * Output:
    *
    *      wsException[EXC_DB] = TRUE
    *
    *---------------------------------------------------------------------------
    */

   ".p2align 4\n"
   EXPORTED_ASM_SYMBOL(SwitchDBHandler)
   "pushq        %%rax\n"
   ANNOTATE_ASM_CALL
   "call         SwitchExcGetCrossPageData\n"
   "addq         %[wsExceptionDB], %%rax\n"
   "movb         $1,               (%%rax)\n" /* log EXC_DB */
   "popq         %%rax\n"
   "testl        %[EFLAGS_TF],     16(%%rsp)\n" /* check trap flag */
   "jz           return_without_enabling_nmi\n"
   "orl          %[EFLAGS_RF],     16(%%rsp)\n"
   "int          $2\n"
   "iretq\n"


   /*
    *---------------------------------------------------------------------------
    *
    * SwitchUDHandler --
    *
    *      UD handler that operates during worldswitch (in both directions).
    *
    *      It sets the wsException[EXC_UD] flag then returns.
    *
    * Input:
    *
    *       0(%rsp) = previous RIP
    *       8(%rsp) = previous CS
    *      16(%rsp) = previous RFLAGS
    *      24(%rsp) = previous RSP
    *      32(%rsp) = previous SS
    *
    * Output:
    *
    *      wsException[EXC_UD] = TRUE
    *
    *---------------------------------------------------------------------------
    */

   ".p2align 4\n"
   EXPORTED_ASM_SYMBOL(SwitchUDHandler)
   "pushq        %%rax\n"
   "pushq        %%rbx\n"
   "pushq        %%rcx\n"
   ANNOTATE_ASM_CALL
   "call         SwitchExcGetCrossPageData\n"
   "movl         %[wsExceptionUD],      %%ecx\n"    /* log EXC_UD */
   "movb         $1,                    (%%rax, %%rcx)\n"
   /* Check if the exception came from a monitor RIP. */
   "cmpq         %[MONITOR_MINIMUM_VA], 24(%%rsp)\n"
   "jae          monitor_context\n"
   /* Check if the exception came from the crosspage code. */
   "movq         24(%%rsp),              %%rbx\n"
   "andq         %[PageAlignMask],       %%rbx\n"
   "movl         %[crosspageCodeLA],     %%ecx\n"
   "cmpq         (%%rax, %%rcx),         %%rbx\n"
   "je           monitor_context\n"
   /*
    * Linux halts the processor on host #UD2.  Act similarly.  PR 1281662.
    * Note that this can escape due to an #NMI, #MC, or #DB.
    */
   "cli\n"
   "hlt\n"
   /* Advance 2 bytes to skip past the #UD2 instruction. */
   "monitor_context:\n"
   "movq         24(%%rsp), %%rbx\n"
   "movl         %[wsUD2],  %%ecx\n"
   "movq         %%rbx,     (%%rax, %%rcx)\n"
   "addq         $2,        24(%%rsp)\n"
   "popq         %%rcx\n"
   "popq         %%rbx\n"
   "popq         %%rax\n"
   "jmp          return_without_enabling_nmi\n"


   /*
    *---------------------------------------------------------------------------
    *
    * SwitchNMIHandler --
    *
    *      NMI handler that operates during worldswitch (in both
    *      directions).
    *
    *      It 'simply' sets the wsException[EXC_NMI] flag then returns,
    *      leaving further NMI delivery inhibited.
    *
    *      As long as we don't execute 'iret', the CPU will not allow further
    *      NMIs to be delivered; this is how the CPU protects itself from
    *      runaway NMIs eating up the stack and triple-faulting.
    *
    *      See Vol 3, 5.5.1, Handling multiple NMIs.
    *
    * Input:
    *
    *       0(%rsp) = previous RIP
    *       8(%rsp) = previous CS
    *      16(%rsp) = previous RFLAGS
    *      24(%rsp) = previous RSP
    *      32(%rsp) = previous SS
    *
    * Output:
    *
    *      wsException[EXC_NMI] = TRUE
    *      further NMI delivery inhibited
    *
    *---------------------------------------------------------------------------
    */

   ".p2align 4\n"
   EXPORTED_ASM_SYMBOL(SwitchNMIHandler)
   "pushq        %%rax\n"
   ANNOTATE_ASM_CALL
   "call         SwitchExcGetCrossPageData\n"
   "addq         %[wsExceptionNMI], %%rax\n"
   "movb         $1,                (%%rax)\n" /* log EXC_NMI */
   "popq         %%rax\n"
   "jmp          return_without_enabling_nmi\n"


   /*
    *---------------------------------------------------------------------------
    *
    * SwitchMCEHandler --
    *
    *      MCE handler that operates during worldswitch (in both
    *      directions).
    *
    *      It simply sets the wsException[EXC_MC] flag then returns.
    *
    * Input:
    *
    *       0(%rsp) = previous RIP
    *       8(%rsp) = previous CS
    *      16(%rsp) = previous RFLAGS
    *      24(%rsp) = previous RSP
    *      32(%rsp) = previous SS
    *
    * Output:
    *
    *      wsException[EXC_MC] = TRUE
    *
    *---------------------------------------------------------------------------
    */

   ".p2align 4\n"
   EXPORTED_ASM_SYMBOL(SwitchMCEHandler)
   "pushq        %%rax\n"
   ANNOTATE_ASM_CALL
   "call         SwitchExcGetCrossPageData\n"
   "addq         %[wsExceptionMC], %%rax\n"
   "movb         $1,              (%%rax)\n" /* log EXC_MC */
   "popq         %%rax\n"
   "jmp          return_without_enabling_nmi\n"


   /*
    *---------------------------------------------------------------------------
    *
    * SwitchHandlerReturns --
    *
    *      Shared code to return without enabling NMIs.  Switch handlers call
    *      directly to these labels as needed.
    *
    *---------------------------------------------------------------------------
    */

   ".p2align 4\n"
   "return_without_enabling_nmi:\n"
   "pushq        %%rbp\n"
   "pushq        %%rax\n"
   "movq         40(%%rsp), %%rbp\n"
   "subq         $32, %%rbp\n"
   "movq         24(%%rsp), %%rax\n"
   "movq         %%rax, 24(%%rbp)\n"
   "movq         16(%%rsp), %%rax\n"
   "movq         %%rax, 16(%%rbp)\n"
   "movq         32(%%rsp), %%rax\n"
   "movq         %%rax, 8(%%rbp)\n"
   "movq         8(%%rsp),  %%rax\n"
   "movq         %%rax, (%%rbp)\n"
   "popq         %%rax\n"
   "movq         %%rbp, %%rsp\n"
   "popq         %%rbp\n"
   "popfq\n"
   "lretq\n"


   /*
    *---------------------------------------------------------------------------
    *
    * HostToVmm --
    *
    *      Switch from host to monitor.
    *
    * Entered with:
    *      RCX = crosspage host address
    *      (RSP) = return to host address
    *
    * Must preserve:
    *      RBX, RSI, RDI, RBP, RSP, R12..R15
    *
    *---------------------------------------------------------------------------
    */

   ".p2align 4\n"
   EXPORTED_ASM_SYMBOL(HostToVmm)
   /* Create an lret frame on the host stack. */
   "pushq           (%%rsp)\n"
   "mov             %%cs,  8(%%rsp)\n"
   "movq            %%rsp, %%rax\n" /* Temporarily hold host %rsp */

   /* Start from the "empty" host context and save. */
   "leaq            %c[hostContextEmpty](%%rcx), %%rsp\n"
   /* Note that only %ss is stored, not %ds/%es. */
   "movw            %%ss,  %%dx\n"
   "pushw           %%dx\n"
   "pushq           %%rax\n" /* host %rsp */
   "pushq           %%r15\n"
   "pushq           %%r14\n"
   "pushq           %%r13\n"
   "pushq           %%r12\n"
   "pushq           %%rdi\n"
   "pushq           %%rsi\n"
   "pushq           %%rbp\n"
   "pushq           %%rbx\n"
   "movq            %%cr3, %%rax\n"
   "pushq           %%rax\n"

   /* Start from the "full" monitor context and load. */
   "leaq            %c[monContextFull](%%rcx), %%rsp\n"
   "popq            %%rsi\n" /* monitor %cr3 */
   "popq            %%rbx\n"
   "popq            %%rbp\n"
   "popq            %%r12\n"
   "popq            %%r13\n"
   "popq            %%r14\n"
   "popq            %%r15\n"
   "popq            %%rax\n" /* monitor %rsp */
   "popw            %%dx\n"  /* monitor %ss */
   /* Load the cross GDT before reloading segments, %cr3 */
   "lgdtq           %c[crossGDTHKLADesc](%%rcx)\n"
   /* Load %cr3 last to flush the TLB after all stack ops. */
   "movq            %%rsi, %%cr3\n"
   /* Reload the monitor's %ss into %ds/%es. */
   "movw            %%dx,  %%ds\n"
   "movw            %%dx,  %%es\n"
   "movw            %%dx,  %%ss\n"
   "movq            %%rax, %%rsp\n"

   "lretq\n"


   /*
    *---------------------------------------------------------------------------
    *
    * VmmToHost --
    *
    *      Switch from monitor to host.
    *
    * Must preserve:
    *      RBX, RBP, RSP, R12..R15
    *
    *---------------------------------------------------------------------------
    */

   ".p2align 4\n"
   EXPORTED_ASM_SYMBOL(VmmToHost)
   "movq            %c[VMMCROSSPAGE] + %c[crosspageDataLA], %%rcx\n"
   /* Create an lret frame on the monitor stack. */
   "pushq           (%%rsp)\n"
   "mov             %%cs,  8(%%rsp)\n"
   "movq            %%rsp, %%rax\n" /* Temporarily hold monitor %rsp */

   /* Start from the "empty" monitor context and save. */
   "leaq            %c[VMMCROSSPAGE] + %c[monContextEmpty], %%rsp\n"
   /* Note that only %ss is stored, not %ds/%es. */
   "movw            %%ss,  %%dx\n"
   "pushw           %%dx\n"
   "pushq           %%rax\n" /* monitor %rsp */
   "pushq           %%r15\n"
   "pushq           %%r14\n"
   "pushq           %%r13\n"
   "pushq           %%r12\n"
   "pushq           %%rbp\n"
   "pushq           %%rbx\n"
   "movq            %%cr3, %%rax\n"
   "pushq           %%rax\n"

   /* Start from the "full" host context and load. */
   "leaq            %c[hostContextFull](%%rcx), %%rsp\n"
   "popq            %%r9\n" /* host %cr3 */
   "popq            %%rbx\n"
   "popq            %%rbp\n"
   "popq            %%rsi\n" /* Not ABI-required. */
   "popq            %%rdi\n" /* Not ABI-required. */
   "popq            %%r12\n"
   "popq            %%r13\n"
   "popq            %%r14\n"
   "popq            %%r15\n"

   "popq            %%rax\n" /* host %rsp */
   "popw            %%dx\n"  /* host %ss */
   /* Load the cross GDT and IDT before reloading segments, %cr3 */
   "lgdtq           %c[crossGDTHKLADesc](%%rcx)\n"
   "lidtq           %c[switchHostIDTR](%%rcx)\n"
   /* Load %cr3 last to flush the TLB after all stack ops. */
   "movq            %%r9,  %%cr3\n"
   /* Reload the host's %ss into %ds/%es.  Technically wrong. */
   "movw            %%dx,  %%ds\n"
   "movw            %%dx,  %%es\n"
   "movw            %%dx,  %%ss\n"
   "movq            %%rax, %%rsp\n"

   /* Microsoft RTL/codegen assumes EFLAGS<DF> = 0. */
   "cld\n"
   "lretq\n"

   /*
    *---------------------------------------------------------------------------
    *
    * SwitchExcGetCrossPageData --
    *
    *      Common function for the exception handlers to locate the data
    *      crosspage so they can record their respective events.  In order to
    *      reach this code an exception had to vector through the IDT.  The IDT
    *      is known to be in the data page.  Therefore, the data page can be
    *      found by accessing IDTR and rounding down to page alignment.
    *
    * Input:
    *     None
    *
    * Output:
    *     %rax = Page aligned address of the current crosspage data area.
    *
    * Note:
    *     %rax (return value) and %rflags are destroyed, all other registers
    *     are preserved.  Since this is only called by exception handlers, the
    *     CPU has already saved %rflags so no additional handling is required.
    *
    *---------------------------------------------------------------------------
    */

   ".p2align 4\n"
   "SwitchExcGetCrossPageData:\n"
   "subq            $0x10,            %%rsp\n"
   "sidt            0(%%rsp)\n"
   "movq            2(%%rsp),         %%rax\n" /* DTR.offset */
   "addq            $0x10,            %%rsp\n"
   "andq            %[PageAlignMask], %%rax\n"
   "ret\n"

   EXPORTED_ASM_SYMBOL(CrossPage_CodeEnd)

   : /* No output list, this is not really C code. */
   : [MONITOR_MINIMUM_VA] "i" (MONITOR_LINEAR_START),
     [PageAlignMask]      "i" (~PAGE_MASK),
     [EFLAGS_TF]          "i" (EFLAGS_TF),
     [EFLAGS_RF]          "i" (EFLAGS_RF),
     [VMMCROSSPAGE]       "i" (CROSS_PAGE_DATA_START * PAGE_SIZE),
     [wsUD2]              "i" (offsetof(VMCrossPageData, wsUD2)),

     [wsExceptionDB]      "i" (offsetof(VMCrossPageData, wsException[EXC_DB])),
     [wsExceptionUD]      "i" (offsetof(VMCrossPageData, wsException[EXC_UD])),
     [wsExceptionNMI]     "i" (offsetof(VMCrossPageData, wsException[EXC_NMI])),
     [wsExceptionMC]      "i" (offsetof(VMCrossPageData, wsException[EXC_MC])),

     [hostContextEmpty]   "i" (offsetof(VMCrossPageData, hostDS)),
     [monContextEmpty]    "i" (offsetof(VMCrossPageData, monDS)),

     [hostContextFull]    "i" (offsetof(VMCrossPageData, hostCR3)),
     [monContextFull]     "i" (offsetof(VMCrossPageData, monCR3)),

     [crosspageDataLA]    "i" (offsetof(VMCrossPageData, crosspageDataLA)),
     [crosspageCodeLA]    "i" (offsetof(VMCrossPageData, crosspageCodeLA)),
     [crossGDTHKLADesc]   "i" (offsetof(VMCrossPageData, crossGDTHKLADesc)),
     [switchHostIDTR]     "i" (offsetof(VMCrossPageData, switchHostIDTR))
   );
   NOT_REACHED_MINIMAL();
}

#ifdef STACK_FRAME_NON_STANDARD
STACK_FRAME_NON_STANDARD(CrossPage_CodePage);
#endif

/*
 * The crosspage data must not exceed a single page.
 * The code area also must fit in a single page but that size isn't available
 * until link time.
 */
MY_ASSERTS(CROSSPAGE_DATA,
   ASSERT_ON_COMPILE(sizeof(VMCrossPageData) <= PAGE_SIZE);
)
