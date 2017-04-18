/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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

#ifndef __VMNETINT_H__
#define __VMNETINT_H__


#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"
#include "driver-config.h"


/*
 * Hide all kernel compatibility stuff in those macros.  This part of code
 * is used only when building prebuilt modules, when autoconf code is disabled.
 */

/* All kernels above 2.6.23 have net namespaces. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24) && !defined(VMW_NETDEV_HAS_NET)
#   define VMW_NETDEV_HAS_NET
#endif

/* All kernels above 2.6.23 have skb argument in nf_hookfn. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24) && !defined(VMW_NFHOOK_USES_SKB)
#   define VMW_NFHOOK_USES_SKB
#endif

/* All kernels above 2.6.25 have dev_net & friends. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26) && !defined(VMW_NETDEV_HAS_DEV_NET)
#   define VMW_NETDEV_HAS_DEV_NET
#endif


#ifdef skb_shinfo
#  define SKB_IS_CLONE_OF(clone, skb)   (  \
      skb_shinfo(clone) == skb_shinfo(skb) \
   )
#else
#  define SKB_IS_CLONE_OF(clone, skb)   (      \
      skb_datarefp(clone) == skb_datarefp(skb) \
   )
#endif
#define DEV_QUEUE_XMIT(skb, dev, pri)   (                 \
    (skb)->dev = (dev),                                   \
    (skb)->priority = (pri),                              \
    compat_skb_reset_mac_header(skb),                     \
    compat_skb_set_network_header(skb, sizeof (struct ethhdr)),  \
    dev_queue_xmit(skb)                                   \
  )
#define dev_lock_list()    read_lock(&dev_base_lock)
#define dev_unlock_list()  read_unlock(&dev_base_lock)
#ifdef VMW_NETDEV_HAS_NET
#   define DEV_GET(x)      __dev_get_by_name(&init_net, (x)->name)
#   ifdef VMW_NETDEV_HAS_DEV_NET
#      define compat_dev_net(x) dev_net(x)
#   else
#      define compat_dev_net(x) (x)->nd_net
#   endif
#else
#   define DEV_GET(x)      __dev_get_by_name((x)->name)
#endif


extern struct proto vmnet_proto;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
#   define compat_sk_alloc(_bri, _pri) sk_alloc(&init_net, \
                                                PF_NETLINK, _pri, &vmnet_proto, 1)
#elif defined(VMW_NETDEV_HAS_NET)
#   define compat_sk_alloc(_bri, _pri) sk_alloc(&init_net, \
                                                PF_NETLINK, _pri, &vmnet_proto)
#else
#   define compat_sk_alloc(_bri, _pri) sk_alloc(PF_NETLINK, _pri, &vmnet_proto, 1)
#endif


#ifdef NF_IP_LOCAL_IN
#define VMW_NF_INET_LOCAL_IN     NF_IP_LOCAL_IN
#define VMW_NF_INET_POST_ROUTING NF_IP_POST_ROUTING
#else
#define VMW_NF_INET_LOCAL_IN     NF_INET_LOCAL_IN
#define VMW_NF_INET_POST_ROUTING NF_INET_POST_ROUTING
#endif


#endif /* __VMNETINT_H__ */
