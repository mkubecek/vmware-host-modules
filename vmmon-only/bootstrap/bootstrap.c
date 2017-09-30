/*********************************************************
 * Copyright (C) 2015 VMware, Inc. All rights reserved.
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
 * bootstrap.c --
 *
 *    Implements the early VMM bootstraping code that is executed
 *    by the host (vmmon/VMKernel) to create the VMM context.
 */

#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "bootstrap_vmm.h"

/*
 *---------------------------------------------------------------------
 *
 * BSVMM_Validate --
 *
 *    Validates the VMM bootstrap blob. For now, we do it by checking
 *    the magic number. Returns the bootstrap parameter table if
 *    successful, NULL otherwise.
 *
 *---------------------------------------------------------------------
 */
BSVMM_HostParams *
BSVMM_Validate(void *buf, uint32 nbytes)
{
   BSVMM_HostParams *bsParams = buf;

   if (nbytes < sizeof *bsParams) {
      return NULL;
   }
   if (bsParams->magic != BOOTSTRAP_MAGIC) {
      return NULL;
   }
   return bsParams;
}
