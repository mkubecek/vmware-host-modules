/*********************************************************
 * Copyright (C) 1998-2014,2016-2017 VMware, Inc. All rights reserved.
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
 *	IA32 asm macros
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

#include "x86types.h"
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

#if defined __APPLE__
/* PR 352418: GCC produces error if the non-Apple version is used */
#define ASSERT_ON_COMPILE_SELECTOR_SIZE(expr)
#else
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
#if (__GNUC__ >= 4) && (__GNUC_MINOR__ >= 1) && !defined(USE_UBSAN)
#define ASSERT_ON_COMPILE_SELECTOR_SIZE(expr)                                \
   ASSERT_ON_COMPILE(sizeof(Selector) == 2 &&                                \
                     ((__builtin_constant_p(expr) && ((expr) >> 16) == 0) || \
                      sizeof(expr) <= 2))
#else
/* gcc 3.3.3 is not able to produce a constant expression (PR 356383) */
#define ASSERT_ON_COMPILE_SELECTOR_SIZE(expr)
#endif
#endif


/*
 * [GS]ET_[GI]DT() are defined as macros wrapping a function
 * so we can pass the argument implicitly by reference (requires
 * a macro) and get type checking too (requires a function).
 */

#define SET_GDT(var) _Set_GDT(&(var))

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

#define SET_IDT(var) _Set_IDT(&(var))

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

#define GET_GDT(var) _Get_GDT(&(var))

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

#define GET_IDT(var) _Get_IDT(&(var))

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


/* Checked against the Intel manual and GCC --thutt */
#define _BUILD_SET_R(func, reg)        \
   static INLINE void                  \
   func(uintptr_t r)                   \
   {                                   \
      __asm__("mov %0, %%" #reg        \
              : /* no outputs */       \
              :  "r" (r)               \
              : "memory");             \
   }

/* Not yet checked against the Intel manual and GCC --slava
 *
 * 'volatile' because CRs and DRs can change without the compiler
 * knowing it (when there is a page fault, when a breakpoint occurs,
 * and moreover it seems there is no way to teach gcc that smsw
 * clobbers cr0 for example).
 *
 * The parameter is a 'uintptr_t *' so that the size of the actual
 * parameter must exactly match the size of the hardware register.
 * This prevents the use of 32-bit variables when building 64-bit
 * code.
 */
#define _BUILD_GET_R(func, reg)                         \
   static INLINE void                                   \
   func(uintptr_t *result)                              \
   {                                                    \
      __asm__ __volatile__("mov %%" #reg ", %0"         \
                           : "=r" (*result));           \
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

#if defined __APPLE__
/* Mac OS gcc 4 uses DBx instead of DRx register names. */
_BUILD_SET_R(_SET_DR0, db0)
_BUILD_SET_R(_SET_DR1, db1)
_BUILD_SET_R(_SET_DR2, db2)
_BUILD_SET_R(_SET_DR3, db3)
_BUILD_SET_R(_SET_DR6, db6)
_BUILD_SET_R(_SET_DR7, db7)

_BUILD_GET_R(_GET_DR0, db0)
_BUILD_GET_R(_GET_DR1, db1)
_BUILD_GET_R(_GET_DR2, db2)
_BUILD_GET_R(_GET_DR3, db3)
_BUILD_GET_R(_GET_DR6, db6)
_BUILD_GET_R(_GET_DR7, db7)
#else
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
#endif

#define SET_CR_DR(regType, regNum, expr)                     \
   do {                                                      \
      /* Ensure no implicit truncation of 'expr' */          \
      ASSERT_ON_COMPILE(sizeof(expr) <= sizeof(uintptr_t));  \
      _SET_##regType##regNum(expr);                          \
   } while (0)

#define GET_CR_DR(regType, regNum, var) \
   do {                                 \
      _GET_##regType##regNum(&(var));   \
   } while (0)

#define SET_CR0(expr) SET_CR_DR(CR, 0, expr)
#define SET_CR2(expr) SET_CR_DR(CR, 2, expr)
#define SET_CR3(expr) SET_CR_DR(CR, 3, expr)
#define SET_CR4(expr) SET_CR_DR(CR, 4, expr)
#define SET_CR8(expr) SET_CR_DR(CR, 8, expr)

/* Undefine GET_CR0; it is defined in mach_asm.h for SLES cross-compile */
#undef GET_CR0
#define GET_CR0(var)  GET_CR_DR(CR, 0, var)
#define GET_CR2(var)  GET_CR_DR(CR, 2, var)
#define GET_CR3(var)  GET_CR_DR(CR, 3, var)
#define GET_CR4(var)  GET_CR_DR(CR, 4, var)
#define GET_CR8(var)  GET_CR_DR(CR, 8, var)

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

/* Checked against the Intel manual and GCC --hpreg */
static INLINE void
CLTS(void)
{
   __asm__ __volatile__ ("clts");
}


/* Beginning of the section whose correctness has NOT been checked */
#define FNCLEX()         __asm__("fnclex" ::);

/* TLB_INVALIDATE_xxx are not checked yet */
#define TLB_INVALIDATE_PAGE(_addr) do { \
     __asm__ __volatile__("invlpg %0": :"m" (*(char *) (_addr)):"memory"); \
} while (0)

#define TLB_INVALIDATE_PAGE_OFF_FS(_addr) do { \
     __asm__ __volatile__("fs; invlpg %0": :"m" (*(char *) (_addr)):"memory"); \
} while (0)

#define RESTORE_FLAGS _Set_flags
#define ENABLE_INTERRUPTS() __asm__ __volatile__ ("sti": : :"memory")
#define CLEAR_INTERRUPTS()  __asm__ __volatile__ ("cli": : :"memory")
#define RAISE_INTERRUPT(_x)  __asm__ __volatile__("int %0" :: "g" (_x))
#define RETURN_FROM_INT()   __asm__ __volatile__("iret" :: )

#if ! defined(VMKERNEL)
#define NO_INTERRUPTS_BEGIN()	do { \
                                   uintptr_t _flags; \
                                   SAVE_FLAGS(_flags); \
                                   CLEAR_INTERRUPTS();

#define NO_INTERRUPTS_END()	   RESTORE_FLAGS(_flags); \
                                } while(0)
#endif

/* End of the section whose correctness has NOT been checked */

#elif defined _MSC_VER  /* !__GNUC__ */

#define SET_IDT(_idt)    _Set_IDT(&(_idt))
#define SET_GDT(_gdt)    _Set_GDT(&(_gdt))
#define SET_TR(_tr)      _Set_TR(_tr)
#define SET_LDT(_tr)     _Set_LDT(_tr)

#define GET_IDT(_idt)    _Get_IDT(&(_idt))
#define GET_GDT(_gdt)    _Get_GDT(&(_gdt))
#define GET_TR(_tr)      do { _tr = _Get_TR();  } while (0)
#define GET_LDT(_tr)     do { _tr = _Get_LDT(); } while (0)

#define GET_CR0(_reg)    __asm mov eax, cr0 __asm mov _reg, eax
#define SET_CR0(_reg)    __asm mov eax, _reg __asm mov cr0, eax
#define GET_CR2(_reg)    __asm mov eax, cr2 __asm mov _reg, eax
#define SET_CR2(_reg)    __asm mov eax, _reg __asm mov cr2, eax
#define GET_CR3(_reg)    __asm mov eax, cr3 __asm mov _reg, eax
#define SET_CR3(_reg)    __asm mov eax, _reg __asm mov cr3, eax
/*
 * MSC doesn't seem to like CR4 in __asm statements. We emit
 * the opcode for MOV EAX,CR4 = 0xf020e0 and MOV CR4,EAX = 0xf022e0
 */
#define GET_CR4(_reg) { \
 __asm _emit 0x0f __asm _emit 0x20 __asm _emit 0xe0 \
 __asm mov _reg, eax \
}
#define SET_CR4(_reg)    { \
  __asm mov eax, _reg \
  __asm _emit 0x0f __asm _emit 0x22 __asm _emit 0xe0 \
}


#define GET_DR0(_reg) do { __asm mov eax,dr0 __asm mov _reg,eax } while (0)
#define SET_DR0(_reg) do { __asm mov eax,_reg __asm mov dr0,eax } while (0)
#define GET_DR1(_reg) do { __asm mov eax,dr1 __asm mov _reg,eax } while (0)
#define SET_DR1(_reg) do { __asm mov eax,_reg __asm mov dr1,eax } while (0)
#define GET_DR2(_reg) do { __asm mov eax,dr2 __asm mov _reg,eax } while (0)
#define SET_DR2(_reg) do { __asm mov eax,_reg __asm mov dr2,eax } while (0)
#define GET_DR3(_reg) do { __asm mov eax,dr3 __asm mov _reg,eax } while (0)
#define SET_DR3(_reg) do { __asm mov eax,_reg __asm mov dr3,eax } while (0)
#define GET_DR6(_reg) do { __asm mov eax,dr6 __asm mov _reg,eax } while (0)
#define SET_DR6(_reg) do { __asm mov eax,_reg __asm mov dr6,eax } while (0)
#define GET_DR7(_reg) do { __asm mov eax,dr7 __asm mov _reg,eax } while (0)
#define SET_DR7(_reg) do { __asm mov eax,_reg __asm mov dr7,eax } while (0)


#define CLTS()           __asm clts

#define FNCLEX()         __asm fnclex

#define TLB_INVALIDATE_PAGE(_addr) {  \
	 void *_a = (_addr); \
     __asm mov eax, _a __asm invlpg [eax] \
}

#define TLB_INVALIDATE_PAGE_OFF_FS(_addr) { \
	uint32 __a = (uint32) (_addr); \
	__asm mov eax, __a _asm invlpg fs:[eax] \
}


#define ENABLE_INTERRUPTS() { __asm sti }
#define CLEAR_INTERRUPTS()  { __asm cli }

#define RAISE_INTERRUPT(_x)  {__asm int _x }
#define RETURN_FROM_INT()   {__asm iretd }


#define SAVE_FLAGS(x) { \
     __asm pushfd __asm pop eax __asm mov x, eax \
}

#define RESTORE_FLAGS(x) { \
     __asm push x __asm popfd\
}



static INLINE void SET_DS(Selector val)
{
   __asm mov ax, val
   __asm mov ds, ax
}

static INLINE void SET_ES(Selector val)
{
   __asm mov ax, val
   __asm mov es, ax
}

static INLINE void SET_FS(Selector val)
{
   __asm mov ax, val
   __asm mov fs, ax
}

static INLINE void SET_GS(Selector val)
{
   __asm mov ax, val
   __asm mov gs, ax
}

static INLINE void SET_SS(Selector val)
{
   __asm mov ax, val
   __asm mov ss, ax
}

static INLINE Selector GET_FS(void)
{
	Selector _v;
	__asm mov _v,fs
	return _v;
}

static INLINE Selector GET_GS(void)
{
	Selector _v;
	__asm mov _v,gs
	return _v;
}


static INLINE Selector GET_DS(void)
{
	Selector _v;
	__asm mov _v,ds
	return _v;
}

static INLINE Selector GET_ES(void)
{
	Selector _v;
	__asm mov _v,es
	return _v;
}

static INLINE Selector GET_SS(void)
{
	Selector _v;
	__asm mov _v,ss
	return _v;
}

static INLINE Selector GET_CS(void)
{
	Selector _v;
	__asm mov _v,cs
	return _v;
}

#pragma warning( disable : 4035)

static INLINE uint32  GET_WORD_FROM_FS(uint32 *_addr) {
	__asm mov eax, _addr
    __asm mov eax, fs:[eax]
}

static INLINE uint16  GET_SHORT_FROM_FS(uint16 *_addr) {
	__asm mov eax, _addr
    __asm mov ax, fs:[eax]
}

static INLINE uint8  GET_BYTE_FROM_FS(uint8 *_addr) {
	__asm mov eax, _addr
     __asm mov al, fs:[eax]
}

#pragma warning (default: 4035)

static INLINE void  SET_WORD_FS(uint32 *_addr, uint32 _val) {
    __asm mov eax, _addr
    __asm mov ebx, _val
    __asm mov fs:[eax], ebx
}

static INLINE void  SET_SHORT_FS(uint32 *_addr, uint16 _val) {
    __asm mov eax, _addr
    __asm mov bx, _val
    __asm mov fs:[eax], bx
}

static INLINE void  SET_BYTE_FS(uint32 *_addr, uint8 _val) {
    __asm mov eax, _addr
    __asm mov bl, _val
    __asm mov fs:[eax], bl
}

static INLINE void _Set_GDT(_GETSET_DTR_TYPE *dtr) {
   __asm mov eax, dtr
   __asm lgdt [eax]
}

static INLINE void _Set_IDT(_GETSET_DTR_TYPE *dtr) {
   __asm mov eax, dtr
   __asm lidt [eax]
}

static INLINE void _Set_LDT(Selector val)
{
   __asm lldt val
}

static INLINE void _Set_TR(Selector val)
{
   __asm ltr val
}

static INLINE void _Get_GDT(_GETSET_DTR_TYPE *dtr) {
   __asm mov eax, dtr
   __asm sgdt [eax]
}

static INLINE void _Get_IDT(_GETSET_DTR_TYPE *dtr) {
   __asm mov eax, dtr
   __asm sidt [eax]
}

static INLINE Selector _Get_LDT(void) {
   Selector sel;
   __asm sldt sel
   return sel;
}

static INLINE Selector _Get_TR(void) {
   Selector sel;
   __asm str sel
   return sel;
}


static INLINE void
MEMCOPY_TO_FS(VA to,
              char * from,
              unsigned long n)
{
   unsigned long i =0;
   while (i+4  <=n) {
      uint32 x = *(uint32*) (from + i);
	  uint32 _faddr = (uint32) (to+i);
	  __asm mov eax, _faddr
	  __asm mov ebx, x
	  __asm mov fs:[eax], ebx
      i +=4;
   }
   while (i<n) {
      uint8 x = from[i];
	  uint32 _faddr = (uint32) (to+i);
	  __asm mov eax, _faddr
	  __asm mov bl, x
	  __asm mov fs:[eax], bl
      i++;
   }
}



static INLINE void
MEMCOPY_FROM_FS(char * to,
                VA from,
                unsigned long n)
{
   unsigned long i =0;
   while (i+4  <=n) {
      uint32 x;
	  uint32 _faddr = (uint32)(from+i);
	  __asm mov eax, _faddr
	  __asm mov ebx, fs:[eax]
	  __asm mov x,ebx
      *(uint32*)(to+i)=x;
      i +=4;
   }
   while (i<n) {
      uint8 x;
	  uint32 _faddr = (uint32) (from+i);
	  __asm mov eax, _faddr;
      __asm mov bl, fs:[eax]
	  __asm mov x, bl
      *(uint8*)(to+i)=x;
      i++;
   }
}

#else
#error No compiler defined for get/set
#endif /* !__GNUC__ && !_MSC_VER */


#ifdef __GNUC__
static INLINE unsigned CURRENT_CPL(void)
{
   return SELECTOR_RPL(GET_CS());
}
#elif defined _MSC_VER
static INLINE unsigned CURRENT_CPL(void) {
   volatile Selector _v;
   __asm mov ax, cs _asm mov _v, ax
   return SELECTOR_RPL(_v);
}
#else
#error No compiler defined for CURRENT_CPL
#endif


#ifdef __GNUC__
/* Checked against the Intel manual and GCC --hpreg

   volatile because the msr can change without the compiler knowing it
   (when we use wrmsr). */
static INLINE uint64 __GET_MSR(int cx)
{
   uint64 msr;
#ifdef VM_X86_64
   __asm__ __volatile__(
      "rdmsr; shlq $32, %%rdx; orq %%rdx, %%rax"
      : "=a" (msr)
      : "c" (cx)
      : "%rdx"
   );
#else
   __asm__ __volatile__(
      "rdmsr"
      : "=A" (msr)
      : "c" (cx)
   );
#endif
   return msr;
}
#elif defined _MSC_VER
#pragma warning( disable : 4035)
static INLINE uint64 __GET_MSR(int input)
{
   __asm push ecx
   __asm mov  ecx, input
   __asm _emit 0x0f __asm _emit 0x32
   __asm pop ecx
}

static INLINE void __SET_MSR(int input, uint64 val)
{
   uint32 hival = (uint32)(val >> 32);
   uint32 loval = (uint32)val;
   __asm push edx
   __asm push ecx
   __asm push eax
   __asm mov  eax, loval
   __asm mov  edx, hival
   __asm mov  ecx, input
   __asm _emit 0x0f __asm _emit 0x30
   __asm pop eax
   __asm pop ecx
   __asm pop edx
}
#pragma warning (default: 4035)
#else
#error
#endif

#ifdef __GNUC__
static INLINE void __SET_MSR(int cx, uint64 val)
{
#ifdef VM_X86_64
   __asm__ __volatile__(
      "wrmsr"
      : /* no outputs */
      : "a" ((uint32) val), "d" ((uint32)(val >> 32)), "c" (cx)
    );
#else
   __asm__ __volatile__(
      "wrmsr"
      : /* no outputs */
      : "A" (val),
        "c" (cx)
    );
#endif
}
#endif


/*
 * RDMSR/WRMSR access the 64bit MSRs as two
 * 32 bit quantities, whereas GET_MSR/SET_MSR
 * above access the MSRs as one 64bit quantity.
 */
#ifdef __GNUC__
#undef RDMSR
#undef WRMSR
#define RDMSR(msrNum, low, high) do { \
   __asm__ __volatile__(              \
      "rdmsr"                         \
      : "=a" (low), "=d" (high)       \
      : "c" (msrNum)                  \
   );                                 \
} while (0)

#define WRMSR(msrNum, low, high) do { \
   __asm__ __volatile__(              \
      "wrmsr"                         \
      : /* no outputs */              \
      : "c" (msrNum),                 \
        "a" (low),                    \
        "d" (high)                    \
    );                                \
} while (0)

static INLINE uint64 RDPMC(int cx)
{
#ifdef VM_X86_64
   uint64 pmcLow;
   uint64 pmcHigh;

   __asm__ __volatile__(
      "rdpmc"
      : "=a" (pmcLow), "=d" (pmcHigh)
      : "c" (cx)
   );

   return pmcHigh << 32 | pmcLow;
#else
   uint64 pmc;

   __asm__ __volatile__(
      "rdpmc"
      : "=A" (pmc)
      : "c" (cx)
   );

   return pmc;
#endif
}
#elif defined _MSC_VER
#ifndef VM_X86_64 // XXX Switch to intrinsics with the new 32 and 64-bit compilers.

static INLINE uint64 RDPMC(int counter)
{
   __asm mov ecx, counter
   __asm rdpmc
}

static INLINE void WRMSR(uint32 msrNum, uint32 lo, uint32 hi)
{
   __asm mov ecx, msrNum
   __asm mov eax, lo
   __asm mov edx, hi
   __asm wrmsr
}
#endif
#endif


#if defined(__GNUC__) && (defined(VMM) || defined(VMKERNEL) || defined(FROBOS))
static INLINE uint64 __XGETBV(int cx)
{
#ifdef VM_X86_64
   uint64 lowval, hival;
   __asm__ __volatile__(
#if __GNUC__ < 4 || __GNUC__ == 4 && __GNUC_MINOR__ == 1
      ".byte 0x0f, 0x01, 0xd0"
#else
      "xgetbv"
#endif
      : "=a" (lowval), "=d" (hival)
      : "c" (cx)
   );
   return hival << 32 | lowval;
#else
   uint64 val;
   __asm__ __volatile__(
#if __GNUC__ < 4 || __GNUC__ == 4 && __GNUC_MINOR__ == 1
      ".byte 0x0f, 0x01, 0xd0"
#else
      "xgetbv"
#endif
      : "=A" (val)
      : "c" (cx)
   );
   return val;
#endif
}

static INLINE void __XSETBV(int cx, uint64 val)
{
   __asm__ __volatile__(
#if __GNUC__ < 4 || __GNUC__ == 4 && __GNUC_MINOR__ == 1
      ".byte 0x0f, 0x01, 0xd1"
#else
      "xsetbv"
#endif
      : /* no outputs */
      : "a" ((uint32)val), "d" ((uint32)(val >> 32)), "c" (cx)
    );
}

static INLINE uint64 GET_XCR0(void)
{
   return __XGETBV(0);
}

#define SET_XCR0(val) __XSETBV(0, val)

static INLINE void SET_XCR0_IF_NEEDED(uint64 newVal, uint64 oldVal)
{
#ifndef VMM_BOOTSTRAP
   ASSERT(oldVal == GET_XCR0());
   if (newVal != oldVal) {
      SET_XCR0(newVal);
   }
#endif
}
#endif


#define START_TRACING() { \
   uintptr_t flags;       \
   SAVE_FLAGS(flags);     \
   flags |= EFLAGS_TF;    \
   RESTORE_FLAGS(flags);  \
}

#define STOP_TRACING() {  \
   uintptr_t flags;       \
   SAVE_FLAGS(flags);     \
   flags &= ~EFLAGS_TF;   \
   RESTORE_FLAGS(flags);  \
}


static INLINE Bool
INTERRUPTS_ENABLED(void)
{
   uintptr_t flags;
   SAVE_FLAGS(flags);
   return ((flags & EFLAGS_IF) != 0);
}

static INLINE void
SET_KERNEL_PER_CORE(uint64 val)
{
   __SET_MSR(MSR_GSBASE, val);
}

#endif
