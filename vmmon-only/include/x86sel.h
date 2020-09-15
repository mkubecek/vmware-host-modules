/*********************************************************
 * Copyright (C) 2006-2014,2020 VMware, Inc. All rights reserved.
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
 * x86sel.h --
 *
 *      Definitions and macros for dealing with x86 segment selectors.
 */

#ifndef _X86SEL_H_
#define _X86SEL_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#if defined __cplusplus
extern "C" {
#endif

#include "vm_assert.h"

#define SELECTOR_GDT             0
#define SELECTOR_LDT             1
#define SELECTOR_RPL_SHIFT       0
#define SELECTOR_RPL_MASK        0x03u
#define SELECTOR_TI_SHIFT        2
#define SELECTOR_TI_MASK         0x4
#define SELECTOR_INDEX_SHIFT     3
#define SELECTOR_INDEX_MASK      0xfff8

#define SELECTOR_RPL(_sel)       (((Selector)(_sel)) & SELECTOR_RPL_MASK)
#define SELECTOR_TABLE(_sel)     ((((Selector)(_sel)) & SELECTOR_TI_MASK) >> SELECTOR_TI_SHIFT)
#define SELECTOR_INDEX(_sel)     (((Selector)(_sel)) >> SELECTOR_INDEX_SHIFT)
#define SELECTOR_CLEAR_RPL(_sel) ((Selector)(((Selector)(_sel)) & ~SELECTOR_RPL_MASK))
#define NULL_SELECTOR(_sel)      (!SELECTOR_CLEAR_RPL(_sel))

#define MAKE_SELECTOR_UNCHECKED(_index, _ti, _RPL)      \
   ((Selector)(((_index) << SELECTOR_INDEX_SHIFT)  |    \
               ((_ti   ) << SELECTOR_TI_SHIFT)     |    \
               ((_RPL  ) << SELECTOR_RPL_SHIFT)))

static INLINE Selector
MAKE_SELECTOR(unsigned index, unsigned ti, unsigned rpl)
{
   ASSERT(index <= (SELECTOR_INDEX_MASK >> SELECTOR_INDEX_SHIFT) &&
          ti    <= (SELECTOR_TI_MASK >> SELECTOR_TI_SHIFT)       &&
          rpl   <= (SELECTOR_RPL_MASK >> SELECTOR_RPL_SHIFT));
   return MAKE_SELECTOR_UNCHECKED(index, ti, rpl);
}


#if defined __cplusplus
} // extern "C"
#endif

#endif /* !defined _X86SEL_H_ */
