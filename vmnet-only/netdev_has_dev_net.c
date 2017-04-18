/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * Detect whether there is dev_net accessor for dev->nd_net.
 * It appeared between 2.6.25 and 2.6.26-rc1.
 */

#include "compat_version.h"
#include "compat_autoconf.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 25)
#   error This compile test intentionally fails.
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
#   include <linux/netdevice.h>

struct net * 
vmware_dev_net(struct net_device *dev)
{
   return dev_net(dev);
}
#endif
