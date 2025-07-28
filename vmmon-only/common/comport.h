/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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

#ifndef _COMPORT_H
#define _COMPORT_H

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMX
#include "includeCheck.h"

#include "vm_basic_types.h"  // for uint8, et al

void CP_Init(void);
void CP_PutChr(uint8 ch);
void CP_PutDec(uint32 value);
void CP_PutHexPtr(void *value);
void CP_PutHex64(uint64 value);
void CP_PutHex32(uint32 value);
void CP_PutHex16(uint16 value);
void CP_PutHex8(uint8 value);
void CP_PutSp(void);
void CP_PutCrLf(void);
void CP_PutStr(char const *s);

#endif
