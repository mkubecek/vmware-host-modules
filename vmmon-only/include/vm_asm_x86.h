/*********************************************************
 * Copyright (C) 1998-2014,2016-2020 VMware, Inc. All rights reserved.
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
 * vm_asm_x86.h
 *
 *      IA32 asm macros
 */

#ifndef _VM_ASM_X86_H_
#define _VM_ASM_X86_H_


#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "cpu_types.h"
#include "x86desc.h"
#include "x86sel.h"
#include "x86_basic_defs.h"
#include "x86msr.h"

#ifdef VM_X86_64
#define _GETSET_DTR_TYPE DTR64
#else
#define _GETSET_DTR_TYPE DTR32
#endif

#ifdef __GNUC__

/* ASSERT_ON_COMPILE_SELECTOR_SIZE:
 *
 *  - Selector must be 16-bits.
 *  - If a constant is used, it better be only 16-bits.
 *  - If it's not a constant, it must be Selector-sized. or less.
 *
 * Although aesthetically the following looks nicer, gcc is unable
 * to produce a constant expression for it:
 *
 *  ASSERT_ON_COMPILE(sizeof(Selector) == 2 &&                            \
 *                    ((__builtin_constant_p(expr) ? ((expr) >> 16) == 0) \
 *                                                 : sizeof(expr) <= 2)
 */
#ifndef USE_UBSAN
#define ASSERT_ON_COMPILE_SELECTOR_SIZE(expr)                                \
   ASSERT_ON_COMPILE(sizeof(Selector) == 2 &&                                \
		     __builtin_choose_expr(__builtin_constant_p(expr),       \
					   ((expr) >> 16) == 0,              \
					   sizeof(expr) <= 2))
#else
#define ASSERT_ON_COMPILE_SELECTOR_SIZE(expr)
#endif


/* Checked against the Intel manual and GCC --hpreg */
static INLINE void
_Set_GDT(_GETSET_DTR_TYPE *dtr)
{
   __asm__(
      "lgdt %0"
      :
      : "m" (*dtr)
   );
}

/* Checked against the Intel manual and GCC --hpreg */
static INLINE void
_Set_IDT(_GETSET_DTR_TYPE *dtr)
{
   __asm__(
      "lidt %0"
      :
      : "m" (*dtr)
   );
}

/*
 * Checked against the Intel manual and GCC --hpreg
 * volatile because there's a hidden input (the [IG]DTR) that can change
 * without the compiler knowing it.
 */
static INLINE void
_Get_GDT(_GETSET_DTR_TYPE *dtr)
{
   __asm__ __volatile__(
      "sgdt %0"
      : "=m" (*dtr)
   );
}

/*
 * Checked against the Intel manual and GCC --hpreg
 * volatile because the [IG]DT can change without the compiler knowing it
 * (when we use l[ig]dt).
 */
static INLINE void
_Get_IDT(_GETSET_DTR_TYPE *dtr)
{
   __asm__ __volatile__(
      "sidt %0"
      : "=m" (*dtr)
   );
}


#define SET_LDT(expr)                                                   \
   do {                                                                 \
      const Selector _set_ldt_sel = (Selector)(expr);                   \
      ASSERT_ON_COMPILE_SELECTOR_SIZE(expr);                            \
      /* lldt reads from the GDT; don't sink any writes. */             \
      COMPILER_MEM_BARRIER();                                           \
      /* Checked against the Intel manual and GCC --hpreg */            \
      __asm__("lldt %0"                                                 \
              :                                                         \
              : "rm" (_set_ldt_sel));                                   \
   } while (0)


/* Checked against the Intel manual and GCC --hpreg
 *
 * volatile because the LDT can change without the compiler knowing it
 * (when we use lldt).
 */
static INLINE void
_GET_LDT(Selector * const result)
{
   __asm__ __volatile__("sldt %0"
                        : "=rm" (*result));
}


#define GET_LDT(var)    \
   do {                 \
      _GET_LDT(&(var)); \
   } while (0)


#define _BUILD_SET_R(func, reg)        \
   static INLINE void                  \
   func(uintptr_t r)                   \
   {                                   \
      __asm__("mov %0, %%" #reg        \
              : /* no outputs */       \
              :  "r" (r)               \
              : "memory");             \
   }

/*
 * The inline asm is marked 'volatile' because CRs and DRs can change
 * without the compiler knowing it (when there is a page fault, when a
 * breakpoint occurs, and moreover it seems there is no way to teach
 * gcc that smsw clobbers cr0 for example).
 */
#define _BUILD_GET_R(func, reg)                         \
   static INLINE uintptr_t                              \
   func(void)                                           \
   {                                                    \
      uintptr_t result;                                 \
      __asm__ __volatile__("mov %%" #reg ", %0"         \
                           : "=r" (result));            \
      return result;                                    \
   }

_BUILD_SET_R(_SET_CR0, cr0)
_BUILD_SET_R(_SET_CR2, cr2)
_BUILD_SET_R(_SET_CR3, cr3)
_BUILD_SET_R(_SET_CR4, cr4)
_BUILD_SET_R(_SET_CR8, cr8)

_BUILD_GET_R(_GET_CR0, cr0)
_BUILD_GET_R(_GET_CR2, cr2)
_BUILD_GET_R(_GET_CR3, cr3)
_BUILD_GET_R(_GET_CR4, cr4)
_BUILD_GET_R(_GET_CR8, cr8)

_BUILD_SET_R(_SET_DR0, dr0)
_BUILD_SET_R(_SET_DR1, dr1)
_BUILD_SET_R(_SET_DR2, dr2)
_BUILD_SET_R(_SET_DR3, dr3)
_BUILD_SET_R(_SET_DR6, dr6)
_BUILD_SET_R(_SET_DR7, dr7)

_BUILD_GET_R(_GET_DR0, dr0)
_BUILD_GET_R(_GET_DR1, dr1)
_BUILD_GET_R(_GET_DR2, dr2)
_BUILD_GET_R(_GET_DR3, dr3)
_BUILD_GET_R(_GET_DR6, dr6)
_BUILD_GET_R(_GET_DR7, dr7)

#define SET_CR2(expr) SET_CR_DR(CR, 2, expr)

#define SET_SEGREG(reg, expr)                                          \
   do {                                                                \
      const Selector _set_segreg_sel = (Selector)(expr);               \
      ASSERT_ON_COMPILE_SELECTOR_SIZE(expr);                           \
      /* mov to Sreg reads from the [GL]DT; don't sink any writes. */  \
      COMPILER_MEM_BARRIER();                                          \
      /* Checked against the Intel manual and GCC --hpreg */           \
      __asm__("movw %0, %%" #reg                                       \
              :                                                        \
              : "rm" (_set_segreg_sel));                               \
   } while (0)

#define SET_DS(expr) SET_SEGREG(ds, expr)
#define SET_ES(expr) SET_SEGREG(es, expr)
#define SET_FS(expr) SET_SEGREG(fs, expr)
#define SET_GS(expr) SET_SEGREG(gs, expr)
#define SET_SS(expr) SET_SEGREG(ss, expr)

/* Checked against the Intel manual and GCC --hpreg
 *
 * volatile because the content of CS can change without the compiler
 * knowing it (when we use call gates).
 *
 * XXX: The segment register getter functions have not been updated to
 *      have stricter type checking like many other functions in this
 *      file because they return a value, rather than taking an
 *      argument.  Perhaps sometime in the future, a willing soul will
 *      change these accessors to take an argument and at the same
 *      time install better type checking.
 */
#define _BUILD_GET_SEG(func, reg)                \
   static INLINE Selector                        \
   func(void)                                    \
   {                                             \
      Selector result;                           \
      __asm__ __volatile__("movw %%" #reg ", %0" \
                           : "=rm" (result));    \
      return result;                             \
   }

_BUILD_GET_SEG(GET_CS, cs)
_BUILD_GET_SEG(GET_DS, ds)
_BUILD_GET_SEG(GET_ES, es)
_BUILD_GET_SEG(GET_FS, fs)
_BUILD_GET_SEG(GET_GS, gs)
_BUILD_GET_SEG(GET_SS, ss)


#define SET_TR(expr)                                                    \
   do {                                                                 \
      const Selector _set_tr_sel = (Selector)(expr);                    \
      ASSERT_ON_COMPILE_SELECTOR_SIZE(expr);                            \
      /* ltr reads from the GDT; don't sink any writes. */              \
      COMPILER_MEM_BARRIER();                                           \
      /* Checked against the Intel manual and GCC --hpreg */            \
      __asm__ __volatile__("ltr %0"                                     \
                           :                                            \
                           : "rm" (_set_tr_sel) : "memory");            \
   } while (0)

/* Checked against the Intel manual and GCC --hpreg

   volatile because the content of TR can change without the compiler knowing
   it (when we use task gates). */
static INLINE void
_GET_TR(Selector * const result)
{
   __asm__ __volatile__("str %0"
                        : "=rm" (*result));
}

#define GET_TR(var)    \
   do {                \
      _GET_TR(&(var)); \
   } while (0)


/* Checked against the Intel manual and GCC --hpreg

   We use this to restore interrupts, so this cannot be reordered around
   by gcc */
static INLINE void
_Set_flags(uintptr_t f)
{
   __asm__ __volatile__(
      "push %0" "\n\t"
      "popf"
      :
      : "g" (f)
      : "memory", "cc"
   );
}



/* Checked against the Intel manual and GCC --hpreg

   volatile because gcc 2.7.2.3 doesn't know when eflags are modified (it
   seems to ignore the "cc" clobber). gcc 2.95.2 is ok: it optimize 2
   successive calls only if there is no instructions in between. */
static INLINE uintptr_t
_Get_flags(void)
{
   uintptr_t result;

   __asm__ __volatile__(
      "pushf"  "\n\t"
      "pop %0"
      : "=rm" (result)
      :
      : "memory"
   );

   return result;
}

#define SAVE_FLAGS(var) do { \
   var = _Get_flags();       \
} while (0)

static INLINE Bool
HwInterruptsEnabled(uint32 eflags)
{
   return (eflags & EFLAGS_IF) != 0;
}

static INLINE void
HwInterruptsDisable(uint64 *rflags)
{
   *rflags &= ~EFLAGS_IF;
}

/* Checked against the Intel manual and GCC --hpreg */
static INLINE void
CLTS(void)
{
   __asm__ __volatile__ ("clts");
}


/* Beginning of the section whose correctness has NOT been checked */
#define FNCLEX()         __asm__("fnclex" ::);

/* TLB_INVALIDATE_PAGE is not checked yet */
#define TLB_INVALIDATE_PAGE(_addr) do { \
     __asm__ __volatile__("invlpg %0": :"m" (*(char *) (_addr)):"memory"); \
} while (0)

#define RESTORE_FLAGS _Set_flags
#define ENABLE_INTERRUPTS() __asm__ __volatile__ ("sti": : :"memory")
#define CLEAR_INTERRUPTS()  __asm__ __volatile__ ("cli": : :"memory")
#define RAISE_INTERRUPT(_x)  __asm__ __volatile__("int %0" :: "g" (_x))
#define RETURN_FROM_INT()   __asm__ __volatile__("iret" :: )

/* End of the section whose correctness has NOT been checked */

#elif defined _MSC_VER  /* !__GNUC__ */

#define _BUILD_SET_DR(func, reg)                      \
   static INLINE void                                 \
   func(uintptr_t r)                                  \
   {                                                  \
      __writedr(reg, r);                              \
   }

#define _BUILD_GET_DR(func, reg)                      \
   static INLINE uintptr_t                            \
   func(void)                                         \
   {                                                  \
      return __readdr(reg);                           \
   }

#define _BUILD_SET_CR(func, reg)                      \
   static INLINE void                                 \
   func(uintptr_t r)                                  \
   {                                                  \
      __writecr##reg(r);                              \
   }

#define _BUILD_GET_CR(func, reg)                      \
   static INLINE uintptr_t                            \
   func(void)                                         \
   {                                                  \
      return __readcr##reg();                         \
   }

_BUILD_SET_DR(_SET_DR0, 0)
_BUILD_SET_DR(_SET_DR1, 1)
_BUILD_SET_DR(_SET_DR2, 2)
_BUILD_SET_DR(_SET_DR3, 3)
_BUILD_SET_DR(_SET_DR6, 6)
_BUILD_SET_DR(_SET_DR7, 7)

_BUILD_GET_DR(_GET_DR0, 0)
_BUILD_GET_DR(_GET_DR1, 1)
_BUILD_GET_DR(_GET_DR2, 2)
_BUILD_GET_DR(_GET_DR3, 3)
_BUILD_GET_DR(_GET_DR6, 6)
_BUILD_GET_DR(_GET_DR7, 7)

_BUILD_SET_CR(_SET_CR0, 0);
_BUILD_SET_CR(_SET_CR3, 3);
_BUILD_SET_CR(_SET_CR4, 4);
_BUILD_SET_CR(_SET_CR8, 8);

_BUILD_GET_CR(_GET_CR0, 0);
_BUILD_GET_CR(_GET_CR2, 2);
_BUILD_GET_CR(_GET_CR3, 3);
_BUILD_GET_CR(_GET_CR4, 4);
_BUILD_GET_CR(_GET_CR8, 8);

static INLINE void
_Set_GDT(_GETSET_DTR_TYPE *dtr)
{
   _lgdt(dtr);
}

static INLINE void
_Get_GDT(_GETSET_DTR_TYPE *dtr)
{
   _sgdt(dtr);
}

static INLINE void
_Set_IDT(_GETSET_DTR_TYPE *dtr)
{
   __lidt(dtr);
}

static INLINE void
_Get_IDT(_GETSET_DTR_TYPE *dtr)
{
   __sidt(dtr);
}


#define ENABLE_INTERRUPTS() _enable()
#define CLEAR_INTERRUPTS()  _disable()

#define SAVE_FLAGS(x) do { \
   (x) = __readeflags();   \
} while (0)

#define RESTORE_FLAGS(x) __writeeflags(x)

#endif /* !__GNUC__ && !_MSC_VER */


#define SET_CR_DR(regType, regNum, expr)                     \
   do {                                                      \
      /* Ensure no implicit truncation of 'expr' */          \
      ASSERT_ON_COMPILE(sizeof(expr) <= sizeof(uintptr_t));  \
      _SET_##regType##regNum(expr);                          \
   } while (0)

#define GET_CR_DR(regType, regNum, var) \
   do {                                 \
      var = _GET_##regType##regNum();   \
   } while (0)

#define SET_DR0(expr) SET_CR_DR(DR, 0, expr)
#define SET_DR1(expr) SET_CR_DR(DR, 1, expr)
#define SET_DR2(expr) SET_CR_DR(DR, 2, expr)
#define SET_DR3(expr) SET_CR_DR(DR, 3, expr)
#define SET_DR6(expr) SET_CR_DR(DR, 6, expr)
#define SET_DR7(expr) SET_CR_DR(DR, 7, expr)

#define GET_DR0(var)  GET_CR_DR(DR, 0, var)
#define GET_DR1(var)  GET_CR_DR(DR, 1, var)
#define GET_DR2(var)  GET_CR_DR(DR, 2, var)
#define GET_DR3(var)  GET_CR_DR(DR, 3, var)
#define GET_DR6(var)  GET_CR_DR(DR, 6, var)
#define GET_DR7(var)  GET_CR_DR(DR, 7, var)

/* Undefine GET_CR0; it is defined in mach_asm.h for SLES cross-compile */
#undef GET_CR0
#define GET_CR0(var)  GET_CR_DR(CR, 0, var)
#define GET_CR2(var)  GET_CR_DR(CR, 2, var)
#define GET_CR3(var)  GET_CR_DR(CR, 3, var)
#define GET_CR4(var)  GET_CR_DR(CR, 4, var)
#define GET_CR8(var)  GET_CR_DR(CR, 8, var)

#define SET_CR0(expr) SET_CR_DR(CR, 0, expr)
#define SET_CR3(expr) SET_CR_DR(CR, 3, expr)
#define SET_CR4(expr) SET_CR_DR(CR, 4, expr)
#define SET_CR8(expr) SET_CR_DR(CR, 8, expr)

static INLINE Bool
INTERRUPTS_ENABLED(void)
{
#if !defined(USERLEVEL)
   uintptr_t flags;
   SAVE_FLAGS(flags);
   return ((flags & EFLAGS_IF) != 0);
#else
   /* At userlevel interrupts are always enabled. */
   return TRUE;
#endif
}


/*
 * [GS]ET_[GI]DT() are defined as macros wrapping a function
 * so we can pass the argument implicitly by reference (requires
 * a macro) and get type checking too (requires a function).
 */

#define SET_GDT(_gdt)    _Set_GDT(&(_gdt))
#define GET_GDT(_gdt)    _Get_GDT(&(_gdt))

#define SET_IDT(_idt)    _Set_IDT(&(_idt))
#define GET_IDT(_idt)    _Get_IDT(&(_idt))

#if !defined(VMKERNEL)
#define NO_INTERRUPTS_BEGIN() do { \
                                   uintptr_t _flags; \
                                   SAVE_FLAGS(_flags); \
                                   CLEAR_INTERRUPTS();

#define NO_INTERRUPTS_END()        RESTORE_FLAGS(_flags); \
                                 } while(0)
#endif


#if defined (__GNUC__)
static INLINE unsigned
CURRENT_CPL(void)
{
   return SELECTOR_RPL(GET_CS());
}
#endif


static INLINE uint64
RDPMC(int counter)
{
#ifdef __GNUC__
#ifdef VM_X86_64
   uint64 pmcLow;
   uint64 pmcHigh;

   __asm__ __volatile__(
      "rdpmc"
      : "=a" (pmcLow), "=d" (pmcHigh)
      : "c" (counter)
   );

   return pmcHigh << 32 | pmcLow;
#else
   uint64 pmc;

   __asm__ __volatile__(
      "rdpmc"
      : "=A" (pmc)
      : "c" (counter)
   );

   return pmc;
#endif
#elif defined _MSC_VER
   return __readpmc(counter);
#endif
}


#if defined(VMM) || defined(VMKERNEL) || defined(FROBOS) || defined (ULM)
static INLINE uint64
__XGETBV(int cx)
{
#ifdef __GNUC__
#ifdef VM_X86_64
   uint64 lowval, hival;
   __asm__ __volatile__(
      "xgetbv"
      : "=a" (lowval), "=d" (hival)
      : "c" (cx)
   );
   return hival << 32 | lowval;
#else
   uint64 val;
   __asm__ __volatile__(
      "xgetbv"
      : "=A" (val)
      : "c" (cx)
   );
   return val;
#endif
#elif defined _MSC_VER
   return _xgetbv((unsigned)cx);
#endif
}

static INLINE void
__XSETBV(int cx, uint64 val)
{
#ifdef __GNUC__
   __asm__ __volatile__(
      "xsetbv"
      : /* no outputs */
      : "a" ((uint32)val), "d" ((uint32)(val >> 32)), "c" (cx)
    );
#elif defined _MSC_VER
   _xsetbv((unsigned)cx, val);
#endif
}

static INLINE uint64
GET_XCR0(void)
{
   return __XGETBV(0);
}

#define SET_XCR0(val) __XSETBV(0, val)

static INLINE void
SET_XCR0_IF_NEEDED(uint64 newVal, uint64 oldVal)
{
#ifndef VMM_BOOTSTRAP
   ASSERT(oldVal == GET_XCR0());
   if (newVal != oldVal) {
      SET_XCR0(newVal);
   }
#endif
}
#endif


static INLINE uint32
RDTSCP_AuxOnly(void)
{
#ifdef __GNUC__
   uint32 tscLow, tscHigh, tscAux;

   __asm__ __volatile__(
      "rdtscp"
      : "=a" (tscLow), "=d" (tscHigh), "=c" (tscAux)
   );

   return tscAux;
#elif defined _MSC_VER
   uint32 tscAux;

   __rdtscp(&tscAux);

   return tscAux;
#endif
}


static INLINE uint64
RDTSCP(void)
{
#ifdef __GNUC__
   uint32 tscLow, tscHigh, tscAux;

   __asm__ __volatile__(
      "rdtscp"
      :"=a" (tscLow), "=d" (tscHigh), "=c" (tscAux)
   );

   return QWORD(tscHigh, tscLow);
#elif defined _MSC_VER
   uint32 tscAux;

   return __rdtscp(&tscAux);
#endif
}

#endif
