/*********************************************************
 * Copyright (C) 2017 VMware, Inc. All rights reserved.
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
 * Detect whether there is netif_trans_update, which got introduced from 4.7.0
 * Older kernels may have this function backported by vendors.
 */

#include "compat_version.h"
#include "compat_autoconf.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 0, 0)
#   error This compile test intentionally fails.
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
#include <linux/netdevice.h>

void test_netif_trans_update(struct net_device *dev)
{
   netif_trans_update(dev);
}

#endif

