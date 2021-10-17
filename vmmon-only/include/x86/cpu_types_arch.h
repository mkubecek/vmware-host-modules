/*********************************************************
 * Copyright (C) 2020 VMware, Inc. All rights reserved.
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
 * cpu_types_arch.h --
 *
 *     Low-level CPU type definitions for the x86.
 */

#if !defined(_X86_CPU_TYPES_ARCH_H_)
#define _X86_CPU_TYPES_ARCH_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "address_defs.h"
#include "x86segdescrs.h"
#include "vm_pagetable.h"

/*
 * Page table
 */

typedef uint32 VM_PDE;
typedef uint32 VM_PTE;
typedef uint64 VM_PAE_PDE;
typedef uint64 VM_PAE_PTE;
typedef uint64 VM_PDPTE;


/*
 * Extended page table
 */

typedef uint64 VM_EPTE;


typedef uint16 Selector;

/*
 *   tasks
 */


#define RSP_NUM_ENTRIES 3
#define IST_NUM_ENTRIES 8
#pragma pack(push, 1)
typedef struct Task64 {
   uint32     reserved0;
   uint64     rsp[RSP_NUM_ENTRIES];   // Stacks for CPL 0-2.
   uint64     ist[IST_NUM_ENTRIES];   // ist[0] is reserved.
   uint64     reserved1;
   uint16     reserved2;
   uint16     IOMapBase;
} Task64;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct Task32 {
   uint32     prevTask;
   uint32     esp0;
   uint32     ss0;
   uint32     esp1;
   uint32     ss1;
   uint32     esp2;
   uint32     ss2;
   uint32     cr3;
   uint32     eip;
   uint32     eflags;
   uint32     eax;
   uint32     ecx;
   uint32     edx;
   uint32     ebx;
   uint32     esp;
   uint32     ebp;
   uint32     esi;
   uint32     edi;
   uint32     es;
   uint32     cs;
   uint32     ss;
   uint32     ds;
   uint32     fs;
   uint32     gs;
   uint32     ldt;
   uint16     trap;
   uint16     IOMapBase;
   uint32     ssp;  // shadow stack pointer
} Task32;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
   uint16     prevTask;
   uint16     sp0;  // static.  Unmarked fields are dynamic
   uint16     ss0;  // static
   uint16     sp1;  // static
   uint16     ss1;  // static
   uint16     sp2;  // static
   uint16     ss2;  // static
   uint16     ip;
   uint16     flags;
   uint16     ax;
   uint16     cx;
   uint16     dx;
   uint16     bx;
   uint16     sp;
   uint16     bp;
   uint16     si;
   uint16     di;
   uint16     es;
   uint16     cs;
   uint16     ss;
   uint16     ds;
   uint16     ldt;  // static
} Task16;
#pragma pack(pop)

// Task defaults to Task32 for everyone except vmkernel. Task64 is used where
// needed by these products.
#if defined VMX86_SERVER && defined VMKERNEL
#ifdef VM_X86_64
typedef Task64 Task;
#else
typedef Task32 Task;
#endif
#else /* VMX86_SERVER && defined VMKERNEL */
typedef Task32 Task;
#endif


/*
 *   far pointers
 */

#pragma pack(push, 1)
typedef struct {
   uint64   va;
   Selector seg;
} FarPtr;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct FarPtr16 {
   uint16   offset;
   uint16   selector;
} FarPtr16;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct FarPtr32 {
   uint32   offset;
   uint16   selector;
} FarPtr32;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct FarPtr64 {
   uint64   offset;
   uint16   selector;
} FarPtr64;
#pragma pack(pop)

/*
 * X86-defined stack layouts for interrupts, exceptions, irets, calls, etc.
 */

/*
 * Layout of the 64-bit stack frame on exception.
 */
#pragma pack(push, 1)
typedef struct x86ExcFrame64 {
   uint64       rip;
   uint16       cs, __sel[3];
   uint64       rflags;
   uint64       rsp;
   uint16       ss, __ssel[3];
} x86ExcFrame64;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct x86ExcFrame64WithErrorCode {
   uint32       errorCode, __errorCode;
   uint64       rip;
   uint16       cs, __sel[3];
   uint64       rflags;
   uint64       rsp;
   uint16       ss, __ssel[3];
} x86ExcFrame64WithErrorCode;
#pragma pack(pop)

/*
 * Layout of the 32-bit stack frame on exception.
 */
#pragma pack(push, 1)
typedef struct x86ExcFrame32 {
   uint32         eip;
   union {
      struct {
         uint16   sel, __sel;
      }           cs16;
      uint32      cs32;
   } u;
   uint32         eflags;
} x86ExcFrame32;
#pragma pack(pop)

/*
 * Layout of the 32-bit stack frame with ss:esp and no error code.
 */
#pragma pack(push, 1)
typedef struct x86ExcFrame32WithStack {
   uint32      eip;
   uint16      cs, __csu;
   uint32      eflags;
   uint32      esp;
   uint16      ss, __ssu;
} x86ExcFrame32WithStack;
#pragma pack(pop)

/*
 * Layout of the 32-bit stack frame on inter-level transfer.
 */
#pragma pack(push, 1)
typedef struct x86ExcFrame32IL {
   uint32      errorCode;
   uint32      eip;
   uint16      cs, __csu;
   uint32      eflags;
   uint32      esp;
   uint16      ss, __ssu;
} x86ExcFrame32IL;
#pragma pack(pop)


/*
 * Layout of the 16-bit stack frame on exception.
 */
#pragma pack(push, 1)
typedef struct x86ExcFrame16 {
   uint16   eip;
   uint16   cs;
   uint16   eflags;
} x86ExcFrame16;
#pragma pack(pop)

/*
 * Layout of the 16-bit stack frame which incudes ss:sp.
 */
#pragma pack(push, 1)
typedef struct x86ExcFrame16WithStack {
   uint16   ip;
   uint16   cs;
   uint16   flags;
   uint16   sp;
   uint16   ss;
} x86ExcFrame16WithStack;
#pragma pack(pop)

/*
 * Layout of the 32-bit stack frame on exception
 * from V8086 mode. It is also a superset
 * of inter-level exception stack frame, which
 * in turn is superset of intra-level exception
 * stack frame.
 */
#pragma pack(push, 1)
typedef struct x86ExcFrameV8086 {
   uint32         eip;
   union {
      struct {
         uint16   sel, __sel;
      }           cs16;
      uint32      cs32;
   } u;
   uint32         eflags;
   uint32         esp;
   uint16         ss, __ss;
   uint16         es, __es;
   uint16         ds, __ds;
   uint16         fs, __fs;
   uint16         gs, __gs;
} x86ExcFrameV8086;
#pragma pack(pop)

/*
 * Layout of the 32-bit stack frame on exception
 * from V8086 mode with errorCode. It is
 * superset of SegmentExcFrameV8086.
 */
#pragma pack(push, 1)
typedef struct x86ExcFrameV8086WithErrorCode {
   uint32         errorCode;
   uint32         eip;
   union {
      struct {
         uint16   sel, __sel;
      }           cs16;
      uint32      cs32;
   } u;
   uint32         eflags;
   uint32         esp;
   uint16         ss, __ss;
   uint16         es, __es;
   uint16         ds, __ds;
   uint16         fs, __fs;
   uint16         gs, __gs;
} x86ExcFrameV8086WithErrorCode;
#pragma pack(pop)

/*
 * Layout of the stack on a 32 bit far call.
 */
#pragma pack(push, 1)
typedef struct x86CallStack32 {
   uint32   eip;
   uint16   cs, __cs;
} x86CallStack32;
#pragma pack(pop)

/*
 * Layout of the stack on a 16 bit far call.
 */
#pragma pack(push, 1)
typedef struct x86CallStack16 {
   uint16   ip;
   uint16   cs;
} x86CallStack16;
#pragma pack(pop)

/*
 * Layout of the stack on a 32 bit far call.
 */
#pragma pack(push, 1)
typedef struct x86CallGateStack32 {
   uint32   eip;
   uint16   cs, __cs;
   uint32   esp;
   uint16   ss, __ss;
} x86CallGateStack32;
#pragma pack(pop)

/*
 * Layout of the stack on a 16 bit far call.
 */
#pragma pack(push, 1)
typedef struct x86CallGateStack16 {
   uint16   ip;
   uint16   cs;
   uint16   sp;
   uint16   ss;
} x86CallGateStack16;
#pragma pack(pop)

typedef struct DebugControlRegister {

   int l0:1;
   int g0:1;
   int l1:1;
   int g1:1;
   int l2:1;
   int g2:1;
   int l3:1;
   int g3:1;

   int le:1;
   int ge:1;
   int oo1:3;

   int gd:1;
   int oo:2;

   int rw0:2;
   int len0:2;
   int rw1:2;
   int len1:2;
   int rw2:2;
   int len2:2;
   int rw3:2;
   int len3:2;

} DebugControlRegister;

/*
 * When an interrupt descriptor has an IST entry programmed, for any raised
 * interrupt (or fault, or exception), the stack pointer is switched to the top
 * of a specified stack and the exception frame is pushed.  If another
 * interrupt is raised with the same IST entry programmed before the first is
 * handled, the first's context is corrupted and system integrity is
 * compromised.
 *
 * When the monitor binary translates code for a guest, any interrupt must be
 * taken on a host stack, and thus IST entries are used for all vectors.  The
 * vmkernel uses a separate stack for #MCE handling, programmed via an IST
 * entry.  In both cases, the danger of context corruption by successive faults
 * is real and must be dealt with.
 *
 * In handlers for these exceptions, the initial exception frame is copied
 * further down the stack and the stack pointer updated to point to the copy.
 * The distance from the top of the stack for the copy is situation-dependent.
 *
 * ExcFrame64ForCopy extends x86ExcFrame64 and maintains congruence with the
 * full x86 exception frame types (VMKFullExcFrame and the monitor's ExcFrame).
 * Enough space is afforded for a couple of temporary software-pushed registers
 * to accommodate the stack copy.
 */
#pragma pack(push, 1)
typedef struct ExcFrame64ForCopy {
   UReg64      r13;                  // Pushed by SW. Used as temp reg.
   UReg64      r14;                  // Pushed by SW. Used as temp reg.
   UReg64      r15;                  // Pushed by SW. Pushed by gate.

   UReg64      errorCode;            // Pushed by SW or HW.

   UReg64      rip;                  // Pushed by HW.
   uint16      cs, __csu[3];         // Pushed by HW.
   uint64      rflags;               // Pushed by HW.

   UReg64      rsp;                  // Pushed by HW.
   uint16      ss, __ssu[3];         // Pushed by HW.
} ExcFrame64ForCopy;
#pragma pack(pop)

/*
 * Layout of the stack for performing a 64 bit lret instruction.
 */
typedef struct LretFrame64 {
   uint64 rip;
   uint64 cs;
} LretFrame64;

typedef union SharedUReg64 {
   UReg8  ureg8[2];
   UReg16 ureg16;
   UReg32 ureg32;
   UReg32 ureg32Pair[2];
   UReg64 ureg64;
} SharedUReg64;

#endif /* _X86_CPU_TYPES_ARCH_H_ */

