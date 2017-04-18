/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * Detect whether there is separate net namespace.  It got introduced after
 * 2.6.23.  If this builds, there are two arguments to __dev_get_by_name...
 * For lower boundary use 2.6.23 - hopefully nobody crossports patch to
 * older kernels.  Note that this also affects sk_alloc interface -
 * for that there are two versions: sk_alloc(net, family, gfp, proto, 1) for
 * kernels 2.6.23 < x <= 2.6.24-rc1, and 4 argument version
 * sk_alloc(net, family, gfp, proto) for 2.6.24-rc1 < x.  We do ignore 2.6.24-rc1
 * as hopefully in few weeks all 2.6.24-rc1 users will be gone.
 */

#include "compat_version.h"
#include "compat_autoconf.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
#   error This compile test intentionally fails.
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
#   include <linux/netdevice.h>

struct net_device * 
vmware_get_by_name(void)
{
   return __dev_get_by_name(0, "dummy");
}
#endif
