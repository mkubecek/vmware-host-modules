/*********************************************************
 * Copyright (C) 1998-2018 VMware, Inc. All rights reserved.
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

#ifndef _X86VENDOR_H_
#define _X86VENDOR_H_

/*
 * CPU vendors
 */

typedef enum {
   CPUID_VENDOR_UNKNOWN,
   CPUID_VENDOR_COMMON,
   CPUID_VENDOR_INTEL,
   CPUID_VENDOR_AMD,
   CPUID_VENDOR_CYRIX,
   CPUID_VENDOR_VIA,
   CPUID_VENDOR_HYGON,
   CPUID_NUM_VENDORS
} CpuidVendor;

#endif
