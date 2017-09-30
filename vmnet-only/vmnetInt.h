/*********************************************************
 * Copyright (C) 1998, 2017 VMware, Inc. All rights reserved.
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


extern struct proto vmnet_proto;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0) || defined(sk_net_refcnt)
#   define compat_sk_alloc(_bri, _pri) sk_alloc(&init_net, \
                                                PF_NETLINK, _pri, &vmnet_proto, 1)
#else
#   define compat_sk_alloc(_bri, _pri) sk_alloc(&init_net, \
                                                PF_NETLINK, _pri, &vmnet_proto)
#endif


#ifdef NF_IP_LOCAL_IN
#define VMW_NF_INET_LOCAL_IN     NF_IP_LOCAL_IN
#define VMW_NF_INET_POST_ROUTING NF_IP_POST_ROUTING
#else
#define VMW_NF_INET_LOCAL_IN     NF_INET_LOCAL_IN
#define VMW_NF_INET_POST_ROUTING NF_INET_POST_ROUTING
#endif


#endif /* __VMNETINT_H__ */
