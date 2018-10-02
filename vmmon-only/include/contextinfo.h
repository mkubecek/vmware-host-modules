/*********************************************************
 * Copyright (C) 2005-2014,2017 VMware, Inc. All rights reserved.
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
 * contextinfo.h
 *
 *    Context structures shared across all products
 */

#ifndef _CONTEXTINFO_H
#define _CONTEXTINFO_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMX
#include "includeCheck.h"

#include "x86desc.h"

#pragma pack(push, 1)
typedef struct Context64 {
   uint64 cr3;
   uint64 rax;
   uint64 rcx;
   uint64 rdx;
   uint64 rbx;
   uint64 rsi;
   uint64 rdi;
   uint64 rbp;
   uint64 rsp;
   uint64 r8;
   uint64 r9;
   uint64 r10;
   uint64 r11;
   uint64 r12;
   uint64 r13;
   uint64 r14;
   uint64 r15;
   uint32 cs;
   uint32 ds;
   uint32 ss;
   uint32 es;
   uint32 fs;
   uint32 gs;
   uint64 rip;
   uint64 eflags;
   uint16 ldt;
   uint16 _pad[3];
} Context64;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct ContextInfo64 {
   DTRWords64 gdtr;
   DTRWords64 idtr;
   Context64  context;
   uint16     tr;
   uint16     _pad0;
} ContextInfo64;
#pragma pack(pop)

#endif
