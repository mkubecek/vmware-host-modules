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
 * Detect whether nf_hookfn takes struct sk_buff* skb, or struct sk_buff** pskb.
 * Kernels before 2.6.23 take pskb, kernels since 2.6.24 take skb, and we
 * are not sure about 2.6.23 itself, as change occured between 2.6.23 and
 * 2.6.24-rc1.
 */

#include "compat_version.h"
#include "compat_autoconf.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
#   error This compile test intentionally fails.
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
#   include <linux/netfilter.h>

nf_hookfn test_function;

unsigned int
test_function(unsigned int hooknum,
              struct sk_buff *skb,
	      const struct net_device *in,
	      const struct net_device *out,
	      int (*defn)(struct sk_buff*))
{
   return 1234;
}
#endif
