/*********************************************************
 * Copyright (C) 2001 VMware, Inc. All rights reserved.
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
 * hashFunc.c --
 *
 *	The core implementation lives in lib/shared/hashFunc.h because it 
 *      is shared by the vmkernel and vmmon.
 */

#include "hashFunc.h"

/*
 * Wrappers
 */

// arbitrary constant
#define	HASH_INIT_VALUE	(42)

// 64-bit hash for one 4K page
uint64 
HashFunc_HashPage(const void *addr)
{
   return hash2((uint64 *)addr, PAGE_SIZE / sizeof (uint64), HASH_INIT_VALUE);
}
