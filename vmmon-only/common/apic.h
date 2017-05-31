/*********************************************************
 * Copyright (C) 2011 VMware, Inc. All rights reserved.
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
 * apic.h --
 *
 *      Some host APIC helper functions
 */

#ifndef APIC_H
#define APIC_H

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_types.h"

typedef struct {
   Bool isX2;
   volatile uint32 (*base)[4];
} APICDescriptor;

MA APIC_GetMA(void);
uint32 APIC_Read(const APICDescriptor *desc, int regNum);
void APIC_Write(const APICDescriptor *desc, int regNum, uint32 val);
void APIC_WriteICR(const APICDescriptor *desc, uint32 id, uint32 icrLo);
uint64 APIC_ReadICR(const APICDescriptor *desc);
uint32 APIC_ReadID(const APICDescriptor *desc);
uint32 APIC_MaxLVT(const APICDescriptor *desc);

#endif
