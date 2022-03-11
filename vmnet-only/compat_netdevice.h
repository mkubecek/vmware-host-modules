/*********************************************************
 * Copyright (C) 2002-2021 VMware, Inc. All rights reserved.
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

/* netdev_priv() appeared in 2.6.3 */
#if  LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 3)
#   define compat_netdev_priv(netdev)   (netdev)->priv
#else
#   define compat_netdev_priv(netdev)   netdev_priv(netdev)
#endif

/*
 * In 3.1 merge window feature maros were removed from mainline,
 * so let's add back ones we care about.
 */
#if !defined(HAVE_NET_DEVICE_OPS) && \
         LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
#   define HAVE_NET_DEVICE_OPS 1
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
#   define COMPAT_NETDEV_TX_OK    NETDEV_TX_OK
#   define COMPAT_NETDEV_TX_BUSY  NETDEV_TX_BUSY
#else
#   define COMPAT_NETDEV_TX_OK    0
#   define COMPAT_NETDEV_TX_BUSY  1
#endif

/* unregister_netdevice_notifier was not safe prior to 2.6.17 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 17) && \
    !defined(ATOMIC_NOTIFIER_INIT)
/* pre 2.6.17 and not patched */
static inline int compat_unregister_netdevice_notifier(struct notifier_block *nb) {
   int err;

   rtnl_lock();
   err = unregister_netdevice_notifier(nb);
   rtnl_unlock();
   return err;
}
#else
/* post 2.6.17 or patched */
#define compat_unregister_netdevice_notifier(_nb) \
        unregister_netdevice_notifier(_nb);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24) || defined(__VMKLNX__)

#   define compat_netif_napi_add(dev, napi, poll, quota) \
      netif_napi_add(dev, napi, poll, quota)

#   if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30) || \
       defined VMW_NETIF_SINGLE_NAPI_PARM
#      define compat_napi_complete(dev, napi) napi_complete(napi)
#      define compat_napi_schedule(dev, napi) napi_schedule(napi)
#   else
#      define compat_napi_complete(dev, napi) netif_rx_complete(dev, napi)
#      define compat_napi_schedule(dev, napi) netif_rx_schedule(dev, napi)
#   endif

#   define compat_napi_enable(dev, napi)  napi_enable(napi)
#   define compat_napi_disable(dev, napi) napi_disable(napi)

#else

#   define compat_napi_complete(dev, napi) netif_rx_complete(dev)
#   define compat_napi_schedule(dev, napi) netif_rx_schedule(dev)
#   define compat_napi_enable(dev, napi)   netif_poll_enable(dev)
#   define compat_napi_disable(dev, napi)  netif_poll_disable(dev)

/* RedHat ported GRO to 2.6.18 bringing new napi_struct with it */
#   if defined NETIF_F_GRO
#      define compat_netif_napi_add(netdev, napi, pollcb, quota) \
      do {                        \
         (netdev)->poll = (pollcb);    \
         (netdev)->weight = (quota);\
         (napi)->dev = (netdev); \
      } while (0)

#   else
       struct napi_struct {
          int dummy;
       };
#      define compat_netif_napi_add(dev, napi, pollcb, quota) \
       do {                        \
          (dev)->poll = (pollcb);    \
          (dev)->weight = (quota);\
       } while (0)

#   endif

#endif

#ifdef NETIF_F_TSO6
#  define COMPAT_NETIF_F_TSO (NETIF_F_TSO6 | NETIF_F_TSO)
#else
#  define COMPAT_NETIF_F_TSO (NETIF_F_TSO)
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
#   define compat_netif_tx_lock(dev) netif_tx_lock(dev)
#   define compat_netif_tx_unlock(dev) netif_tx_unlock(dev)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
#   define compat_netif_tx_lock(dev) spin_lock(&dev->xmit_lock)
#   define compat_netif_tx_unlock(dev) spin_unlock(&dev->xmit_lock)
#else
/* Vendor backporting (SLES 10) has muddled the tx_lock situation. Pick whichever
 * of the above works for you. */
#   define compat_netif_tx_lock(dev) do {} while (0)
#   define compat_netif_tx_unlock(dev) do {} while (0)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
#   define COMPAT_VLAN_GROUP_ARRAY_LEN VLAN_N_VID
#   define compat_flush_scheduled_work(work) cancel_work_sync(work)
#else
#   define COMPAT_VLAN_GROUP_ARRAY_LEN VLAN_GROUP_ARRAY_LEN
#   define compat_flush_scheduled_work(work) flush_scheduled_work()
#endif



/*
 * For kernel versions older than 2.6.29, where pci_msi_enabled is not
 * available, check if
 *	1. CONFIG_PCI_MSI is present
 *	2. kernel version is newer than 2.6.25 (because multiqueue is not
 *	   supporter) in kernels older than that)
 *	3. msi can be enabled. If it fails it means that MSI is not available.
 * When all the above are true, return non-zero so that multiple queues will be
 * allowed in the driver.
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
#   define compat_multiqueue_allowed(dev) pci_msi_enabled()
#else
#   if defined CONFIG_PCI_MSI && LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 25)
static inline int
compat_multiqueue_allowed(struct pci_dev *dev)
{
   int ret;

   if (!pci_enable_msi(dev))
      ret = 1;
   else
      ret = 0;

   pci_disable_msi(dev);
   return ret;
}

#   else
#      define compat_multiqueue_allowed(dev) (0)
#   endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
#   define compat_vlan_get_protocol(skb) vlan_get_protocol(skb)
#else
#   define compat_vlan_get_protocol(skb) (skb->protocol)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
typedef netdev_features_t compat_netdev_features_t;
#else
typedef u32 compat_netdev_features_t;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0) || defined(VMW_NETIF_TRANS_UPDATE)
#define compat_netif_trans_update(d) netif_trans_update(d)
#else
#define compat_netif_trans_update(d) do { (d)->trans_start = jiffies; } while (0)
#endif

#endif /* __COMPAT_NETDEVICE_H__ */
