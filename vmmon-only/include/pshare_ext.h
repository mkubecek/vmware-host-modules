/*********************************************************
 * Copyright (C) 2001,2014 VMware, Inc. All rights reserved.
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
 * pshare_ext.h --
 *
 *      VMKernel/VMMon <-> VMM transparent page sharing info.
 */

#ifndef _PSHARE_EXT_H
#define _PSHARE_EXT_H

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "pagelist.h"
#include "vm_basic_types.h"
#include "vm_assert.h"

/*
 * constants
 */

#define PSHARE_PAGELIST_MAX             (PAGELIST_MAX)
#define PSHARE_P2M_BUFFER_MPNS_MAX      (16)
#define PSHARE_P2M_BUFFER_MPNS_DEFAULT  (4)
#define PSHARE_P2M_BUFFER_SLOTS_PER_MPN (PAGE_SIZE / sizeof(PShare_P2MUpdate))
#define PSHARE_P2M_BUFFER_SLOTS_MIN     (2)

#define PSHARE_POISON_MARKER            (CONST64U(0xAAAAAAAAAAAAAAAA))

#define PSHARE_SALT_UNSET       0
#define PSHARE_SALT_DEFAULT     1

MY_ASSERTS(PSHARE_EXT,
           ASSERT_ON_COMPILE(PSHARE_PAGELIST_MAX <= PAGELIST_MAX);)

/*
 * types
 */

typedef union {
   MPN mpn;
   uint64 vpmemRef;
} PShare_P2MUpdateReference;

#define PSHARE_SET_INVALID_P2MUPDATE_REFERENCE(ref) ((ref).vpmemRef = CONST64U(-1))
#define PSHARE_IS_INVALID_P2MUPDATE_REFERENCE(ref) ((ref).vpmemRef == CONST64U(-1))

typedef struct PShare_P2MUpdate {
   BPN bpn;
   PShare_P2MUpdateReference reference;
} PShare_P2MUpdate;
#endif
