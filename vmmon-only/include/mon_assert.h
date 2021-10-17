/*********************************************************
 * Copyright (C) 2007-2015,2018,2020,2021 VMware, Inc. All rights reserved.
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

#ifndef _MON_ASSERT_H_
#define _MON_ASSERT_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_assert.h"
#include "vm_basic_asm.h"

/*
 * Monitor Source Location
 *
 * The monitor encodes source locations -- file name & line number --
 * in just 32 bits; the process is arcane enough that it deserves a
 * little discussion.
 *
 *   o The ASSERT family of macros are expanded in the monitor to take
 *     an 'Assert_MonSrcLoc' rather than the standard '<file>,
 *     <line-number>' couplet.
 *
 *   o The '<file>, <line-number>' couplet is encoded into
 *     Assert_MonSrcLoc, which is an unsigned 32-bit integer.
 *
 *   o The upper 16-bits of Assert_MonSrcLoc are the line number.
 *
 *     Source lines above 65535 will be silently masked to 16-bits.
 *
 *   o The lower 16-bits of Assert_MonSrcLoc are the offset to the file
 *     name from the start of the file name table.
 *
 *     This, of course, implies that the size of the table containing
 *     the file names cannot exceed 64K.
 *
 *   o If we use '__FILE__' directly, gcc will coalesce all equivalent
 *     strings into a single occurrence (in '.rodata').
 *
 *     Using the full pathname for __FILE__ is undesirable because
 *     different source trees frequently have different path name
 *     lengths, and this causes the 'monitor-size' script to report
 *     differences in '.rodata'.
 *
 *   o Each __FILE__ is put into its own .assert_pathname_##__FILE
 *     section.  The monitor's runtime linker (not ld) then uses the name
 *     of this section to recover the name of the corresponding source file.
 *
 *   o At run time, prior to loading, when our linker is creating an
 *     executable image of the monitor and extensions, all the file
 *     names are extracted from these sections, the '${VMTREE}' prefix
 *     is removed, and the resulting table of shortened file names is
 *     added to a non-loadable section called '.assert_pathnames' in the
 *     VMX linker (not the bootstrap linker which does not need them).
 *
 *     Further, during linkage, each relocation to the original
 *     section containing the path name is modified so that the low
 *     16-bits contain an offset from the start of the .assert_pathnames
 *     section rather than the base of the original containing section.
 *
 *     Only three types of relocations to the assertion strings are
 *     supported (32-bit PC-relative and 32-bit/64-bit absolute) because that
 *     is all the compiler has been seen to generate.
 */

#define ALL_ASSERT_TYPES \
   ADEF(AssertType_AssertAssert,         _AssertAssertFmt),                \
   ADEF(AssertType_AssertVerify,         _AssertVerifyFmt),                \
   ADEF(AssertType_AssertNotImplemented, _AssertNotImplementedFmt),        \
   ADEF(AssertType_AssertNotReached,     _AssertNotReachedFmt),            \
   ADEF(AssertType_AssertAssertBug,      _AssertAssertFmt " bugNr=%d"),    \
   ADEF(AssertType_AssertVerifyBug,      _AssertVerifyFmt " bugNr=%d"),    \
   ADEF(AssertType_AssertNotImplementedBug,                                \
        _AssertNotImplementedFmt " bugNr=%d"),                             \
   ADEF(AssertType_AssertNotReachedBug,  _AssertNotReachedFmt " bugNr=%d"),\
   ADEF(AssertType_AssertNotTested,      _AssertNotTestedFmt)

typedef uint32 Assert_MonSrcLoc;

#define ADEF(type, fmt) type
typedef enum Assert_Type {  
   ALL_ASSERT_TYPES
} Assert_Type;
#undef ADEF

typedef struct Assert_Info {
   VA faultAddr;
   struct {
      uint32 type:4;
      int bugNr:28;
   } misc;
   Assert_MonSrcLoc loc;
} Assert_Info;

#define ASSERT_MONSRCFILEOFFSET(loc)    LOWORD(loc)
#define ASSERT_MONSRCLINE(loc)          HIWORD(loc)

#define ASSERT_NULL_MONSRCLOC     0             // there is never line 0

#if defined(VMM) && !defined(MONITOR_APP) // {
/*
 * Assertion information is collected in a non-loadable section
 * named .assert_info.  Each record in this section contains
 * a VMM address, an assertion type, an optional bug number, and
 * the MonSrcLoc described previously.  The VMM address is a key
 * used by the VMX to look up the information associated with
 * a particular assertion failure.
 *
 * Assertion failures are fired by executing a ud2 instruction.
 *
 * For assertions which always result in a terminal user RPC, we use
 * __builtin_trap to generate the ud2, so that gcc knows that the
 * subsequent code is unreachable.  For assertions which are
 * recoverable (e.g any assertion triggered on the BackToHost path),
 * we generate the ud2 manually, so that gcc will treat the subsequent
 * code as reachable.
 *
 * The memory barriers work around a gcc bug that results from having
 * to continue past an assertion.  Without these barriers, gcc has been
 * seen to hoist code into the failing arm of the assertion, where it
 * can then tell that, because of the assertion failure, the code ends
 * up accessing an array out of bounds.
 *
 * Assertion failures for the monitor's bootstrap are reduced to panics
 * logging the current %rip.  As such, no .assert_info section is created
 * and VMX merely processes the panic.
 */

#ifndef VMM_BOOTSTRAP
/*
 * The portion of the __attribute__ line after __FILE__ is there so that
 * the .assert_pathname_* sections are not marked as ALLOC, since we only
 * need them in the vmx and do not need them loaded.
 */
#define __VMM__FILE__SECTION \
      __attribute__((section (".assert_pathname_" __FILE__ ",\"\"#")))
#define __VMM__FILE__ ({                                                \
         static __VMM__FILE__SECTION const char file[] = "";            \
         file;                                                          \
      })

#define ASSERT_RECORDINFO(assembly, assertType, bugNr)                   \
   __asm__ __volatile__(".pushsection .assert_info;"                     \
                        ".quad 0f;"                                      \
                        ".long %c[type] + (%c[bug] << 4);"               \
                        ".long (%c[line] << 16) + %c[file];"             \
                        ".popsection;"                                   \
                        "0: " assembly : :                               \
                        [line] "i" (__LINE__),                           \
                        [file] "i" (__VMM__FILE__),                      \
                        [type] "i" (assertType),                         \
                        [bug]  "i" (bugNr))
#else

extern uint64 bsAssertRIP;
#define ASSERT_RECORDINFO(assembly, assertType, bugNr)                   \
   __asm__ __volatile__("lea 0(%%rip), %0\n\t"                           \
                        : "=r"(bsAssertRIP));                            \
   Panic("Bootstrap: %s failure at rip=0x%lx",                           \
         assertType == AssertType_AssertVerify ? "VERIFY" : "ASSERT",    \
         bsAssertRIP);
#endif /* VMM_BOOTSTRAP */

#define _ASSERT_PANIC(name)                                              \
   ({COMPILER_MEM_BARRIER();                                             \
     ASSERT_RECORDINFO("ud2", AssertType_##name, 0);})

#define _ASSERT_PANIC_NORETURN(name)                                     \
   ({COMPILER_MEM_BARRIER();                                             \
     ASSERT_RECORDINFO("", AssertType_##name, 0);                        \
     __builtin_trap();})

#define _ASSERT_PANIC_BUG(bug, name)                                     \
   ({COMPILER_MEM_BARRIER();                                             \
     ASSERT_RECORDINFO("ud2", AssertType_##name##Bug, bug);})

#define _ASSERT_PANIC_BUG_NORETURN(bug, name)                            \
   ({COMPILER_MEM_BARRIER();                                             \
     ASSERT_RECORDINFO("", AssertType_##name##Bug, bug);                 \
     __builtin_trap();})
#endif // VMM && !MONITOR_APP}

#endif // _MON_ASSERT_H_
