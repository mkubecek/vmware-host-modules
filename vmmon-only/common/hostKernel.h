/*********************************************************
 * Copyright (C) 1998,2019 VMware, Inc. All rights reserved.
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
 * hostKernel.h --
 *
 *    Definition of HOST_KERNEL_* --hpreg
 */


#ifndef __HOST_KERNEL_H__
#   define __HOST_KERNEL_H__


#   ifdef __linux__
/*
 * In some cases, this files needs to include Linux kernel header file
 * asm/page.h.
 *
 * However, asm/page.h defines PAGE_SHIFT, PAGE_SIZE, PAGE_MASK, PAGE_OFFSET
 * and VMware header file vm_basic_types.h defines PAGE_SHIFT, PAGE_SIZE,
 * PAGE_MASK, PAGE_OFFSET. PAGE_MASK and PAGE_OFFSET are defined differently
 * (XXX we should really prefix the VMware version with VM_ to prevent any
 * further confusion), but fortunately the driver does not use them.
 *
 * So in this file, we must solve the definition conflict for files that
 * include both this file and vm_basic_types.h. 2 cases can occur:
 *
 * 1) this file is included before vm_basic_types.h is included. This is fine,
 *    because vm_basic_types.h only defines PAGE_* if they don't exist yet.
 *
 * 2) vm_basic_types.h is included before this file is included.
 * We must undefine
 *    PAGE_* in between. But this only works if asm/page.h is not included
 *    before this file is included.
 *
 * In summary: if you make sure you do not include asm/page.h before you
 * include this file, then we guarantee that:
 * . This file and vm_basic_types.h can be included in any order
 * . asm/page.h will be included
 * . The PAGE_* definitions will come from asm/page.h
 *
 *   --hpreg
 */

/* Must come before any kernel header file --hpreg */
#      include "driver-config.h"

#      undef PAGE_SHIFT
#      undef PAGE_SIZE
#      undef PAGE_MASK
#      undef PAGE_OFFSET

/* For __PAGE_OFFSET --hpreg */
#      include <asm/page.h>

#      define HOST_KERNEL_VA_2_LA(_x) (_x)
#      define HOST_KERNEL_LA_2_VA(_x) (_x)
#  else
/* For VA and LA --hpreg */
#      include "vm_basic_types.h"

#      define HOST_KERNEL_VA_2_LA(_addr) ((LA)(_addr))
#      define HOST_KERNEL_LA_2_VA(_addr) ((VA)(_addr))
#  endif


#endif /* __HOST_KERNEL_H__ */
