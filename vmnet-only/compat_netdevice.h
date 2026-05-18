/*********************************************************
 * Copyright (c) 2002-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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

#ifndef __COMPAT_NETDEVICE_H__
#   define __COMPAT_NETDEVICE_H__


#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#define compat_netdev_priv(netdev)   netdev_priv(netdev)

/*
 * In 3.1 merge window feature maros were removed from mainline,
 * so let's add back ones we care about.
 */
#if !defined(HAVE_NET_DEVICE_OPS)
#   define HAVE_NET_DEVICE_OPS 1
#endif

#define COMPAT_NETDEV_TX_OK    NETDEV_TX_OK
#define COMPAT_NETDEV_TX_BUSY  NETDEV_TX_BUSY

#define compat_unregister_netdevice_notifier(_nb) \
        unregister_netdevice_notifier(_nb);

#define compat_netif_napi_add(dev, napi, poll, quota) \
      netif_napi_add(dev, napi, poll, quota)

#define compat_napi_complete(dev, napi) napi_complete(napi)
#define compat_napi_schedule(dev, napi) napi_schedule(napi)

#define compat_napi_enable(dev, napi)  napi_enable(napi)
#define compat_napi_disable(dev, napi) napi_disable(napi)


#ifdef NETIF_F_TSO6
#  define COMPAT_NETIF_F_TSO (NETIF_F_TSO6 | NETIF_F_TSO)
#else
#  define COMPAT_NETIF_F_TSO (NETIF_F_TSO)
#endif


#define compat_netif_tx_lock(dev) netif_tx_lock(dev)
#define compat_netif_tx_unlock(dev) netif_tx_unlock(dev)

#define COMPAT_VLAN_GROUP_ARRAY_LEN VLAN_N_VID
#define compat_flush_scheduled_work(work) cancel_work_sync(work)

#define compat_multiqueue_allowed(dev) pci_msi_enabled()

#define compat_vlan_get_protocol(skb) vlan_get_protocol(skb)

typedef netdev_features_t compat_netdev_features_t;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0) || defined(VMW_NETIF_TRANS_UPDATE)
#define compat_netif_trans_update(d) netif_trans_update(d)
#else
#define compat_netif_trans_update(d) do { (d)->trans_start = jiffies; } while (0)
#endif

#endif /* __COMPAT_NETDEVICE_H__ */
