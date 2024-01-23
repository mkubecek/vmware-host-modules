/*********************************************************
 * Copyright (C) 1998-2013 VMware, Inc. All rights reserved.
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

#include "driver-config.h"

#define EXPORT_SYMTAB

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/mm.h>
#include "compat_skbuff.h"
#include <linux/sockios.h>
#include "compat_sock.h"

#define __KERNEL_SYSCALLS__
#include <asm/io.h>

#include <linux/proc_fs.h>
#include <linux/file.h>

#include "vnetInt.h"
#include "compat_version.h"
#include "compat_autoconf.h"
#include "compat_netdevice.h"
#include "vmnetInt.h"


typedef struct VNetNetIF {
   VNetPort                port;
   struct net_device      *dev;
   struct net_device_stats stats;
} VNetNetIF;


static void VNetNetIfFree(VNetJack *this);
static void VNetNetIfReceive(VNetJack *this, struct sk_buff *skb);
static Bool VNetNetIfCycleDetect(VNetJack *this, int generation);

static int  VNetNetifOpen(struct net_device *dev);
static int  VNetNetifProbe(struct net_device *dev);
static int  VNetNetifClose(struct net_device *dev);
static int  VNetNetifStartXmit(struct sk_buff *skb, struct net_device *dev);
static struct net_device_stats *VNetNetifGetStats(struct net_device *dev);
static int  VNetNetifSetMAC(struct net_device *dev, void *addr);
static void VNetNetifSetMulticast(struct net_device *dev);
static int  VNetNetIfProcRead(char *page, char **start, off_t off,
                              int count, int *eof, void *data);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0) && \
    (LINUX_VERSION_CODE > KERNEL_VERSION(4, 19, 0) && LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 291)) && \
    (LINUX_VERSION_CODE > KERNEL_VERSION(5, 4, 0) && LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 251)) && \
    (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0) && LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 188)) && \
    !defined(RHEL86_BACKPORTS) && !defined(SLE15_SP3_BACKPORTS)
static void
__dev_addr_set(struct net_device *dev, const void *addr, size_t len)
{
	memcpy(dev->dev_addr, addr, len);
}

static void dev_addr_set(struct net_device *dev, const u8 *addr)
{
	__dev_addr_set(dev, addr, dev->addr_len);
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * VNetNetIfSetup --
 *
 *      Sets initial netdevice state.
 * 
 * Results: 
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
VNetNetIfSetup(struct net_device *dev)  // IN:
{
#if !COMPAT_LINUX_VERSION_CHECK_LT(3, 1, 0) || defined(HAVE_NET_DEVICE_OPS)
   static const struct net_device_ops vnetNetifOps = {
      .ndo_init = VNetNetifProbe,
      .ndo_open = VNetNetifOpen,
      .ndo_start_xmit = VNetNetifStartXmit,
      .ndo_stop = VNetNetifClose,
      .ndo_get_stats = VNetNetifGetStats,
      .ndo_set_mac_address = VNetNetifSetMAC,
#if COMPAT_LINUX_VERSION_CHECK_LT(3, 2, 0)
      .ndo_set_multicast_list = VNetNetifSetMulticast,
#else
      .ndo_set_rx_mode = VNetNetifSetMulticast,
#endif

   };
#endif

   ether_setup(dev); // turns on IFF_BROADCAST, IFF_MULTICAST
#if COMPAT_LINUX_VERSION_CHECK_LT(3, 1, 0) && !defined(HAVE_NET_DEVICE_OPS) 
   dev->init = VNetNetifProbe;
   dev->open = VNetNetifOpen;
   dev->hard_start_xmit = VNetNetifStartXmit;
   dev->stop = VNetNetifClose;
   dev->get_stats = VNetNetifGetStats;
   dev->set_mac_address = VNetNetifSetMAC;
   dev->set_multicast_list = VNetNetifSetMulticast;
#else
   dev->netdev_ops = &vnetNetifOps;
   #endif /* HAVE_NET_DEVICE_OPS */

}


/*
 *----------------------------------------------------------------------
 *
 * VNetNetIf_Create --
 *
 *      Create a net level port to the wonderful world of virtual
 *      networking.
 * 
 * Results: 
 *      Errno. Also returns an allocated port to connect to,
 *      NULL on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VNetNetIf_Create(char *devName,  // IN:
                 VNetPort **ret, // OUT:
                 int hubNum)     // IN: 
{
   VNetNetIF *netIf;
   struct net_device *dev;
   int retval;
   static unsigned id = 0;
   char deviceName[VNET_NAME_LEN];

   memcpy(deviceName, devName, sizeof deviceName);
   NULL_TERMINATE_STRING(deviceName);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
   dev = alloc_netdev(sizeof *netIf, deviceName, NET_NAME_USER, VNetNetIfSetup);
#else
   dev = alloc_netdev(sizeof *netIf, deviceName, VNetNetIfSetup);
#endif
   if (!dev) {
      retval = -ENOMEM;
      goto out;
   }

   netIf = netdev_priv(dev);

   netIf->dev = dev;

   netIf->port.id = id++;
   netIf->port.next = NULL;

   netIf->port.jack.peer = NULL;
   netIf->port.jack.numPorts = 1;
   VNetSnprintf(netIf->port.jack.name, sizeof netIf->port.jack.name,
                "netif%u", netIf->port.id);
   netIf->port.jack.private = netIf;
   netIf->port.jack.index = 0;
   netIf->port.jack.procEntry = NULL;
   netIf->port.jack.free = VNetNetIfFree;
   netIf->port.jack.rcv = VNetNetIfReceive;
   netIf->port.jack.cycleDetect = VNetNetIfCycleDetect;
   netIf->port.jack.portsChanged = NULL;
   netIf->port.jack.isBridged = NULL;
   netIf->port.exactFilterLen = 0;

   /*
    * Make proc entry for this jack.
    */

   retval = VNetProc_MakeEntry(netIf->port.jack.name, S_IFREG, netIf,
                               VNetNetIfProcRead, &netIf->port.jack.procEntry);
   if (retval) {
      netIf->port.jack.procEntry = NULL;
      if (retval != -ENXIO) {
         goto outFreeDev;
      }
   }

   /*
    * Rest of fields.
    */

   netIf->port.flags = IFF_RUNNING;

   memset(netIf->port.paddr, 0, sizeof netIf->port.paddr);
   memset(netIf->port.ladrf, 0, sizeof netIf->port.ladrf);
   memset(netIf->port.exactFilter, 0, sizeof netIf->port.exactFilter);

   /* This will generate the reserved MAC address c0:00:?? where ?? == hubNum. */
   VMX86_BUILD_MAC(netIf->port.paddr, hubNum);

   /* Make sure the MAC is unique. */
   retval = VNetSetMACUnique(&netIf->port, netIf->port.paddr);
   if (retval) {
     goto outRemoveProc;
   }

   netIf->port.fileOpRead = NULL;
   netIf->port.fileOpWrite = NULL;
   netIf->port.fileOpIoctl = NULL;
   netIf->port.fileOpPoll = NULL;

   memset(&netIf->stats, 0, sizeof netIf->stats);

   __dev_addr_set(dev, netIf->port.paddr, sizeof(netIf->port.paddr));

   if (register_netdev(dev) != 0) {
      LOG(0, (KERN_NOTICE "%s: could not register network device\n",
          dev->name));
      retval = -ENODEV;
      goto outRemoveProc;
   }

   *ret = &netIf->port;
   return 0;

outRemoveProc:
   if (netIf->port.jack.procEntry) {
      VNetProc_RemoveEntry(netIf->port.jack.procEntry);
   }
outFreeDev:
   free_netdev(dev);
out:
   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetNetIfFree --
 *
 *      Free the net interface port.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VNetNetIfFree(VNetJack *this) // IN: jack
{
   VNetNetIF *netIf = container_of(this, VNetNetIF, port.jack);

   if (this->procEntry) {
      VNetProc_RemoveEntry(this->procEntry);
   }

   unregister_netdev(netIf->dev);
   free_netdev(netIf->dev);
}


/*
 *----------------------------------------------------------------------
 *
 * VNetNetIfReceive --
 *
 *      This jack is receiving a packet. Take appropriate action.
 *
 * Results: 
 *      None.
 *
 * Side effects:
 *      Frees skb.
 *
 *----------------------------------------------------------------------
 */

void
VNetNetIfReceive(VNetJack        *this, // IN: jack
		 struct sk_buff  *skb)  // IN: packet 
{
   VNetNetIF *netIf = this->private;
   uint8 *dest = SKB_2_DESTMAC(skb);
 
   if (!NETDEV_UP_AND_RUNNING(netIf->dev)) {
      goto drop_packet;
   }

   if (!VNetPacketMatch(dest,
                        netIf->dev->dev_addr,
                        NULL,
                        0,
                        allMultiFilter,
                        netIf->dev->flags)) {
      goto drop_packet;
   }
   
   /* send to the host interface */
   skb->dev = netIf->dev;
   skb->protocol = eth_type_trans(skb, netIf->dev);
   compat_netif_rx_ni(skb);
   netIf->stats.rx_packets++;

   return;
   
 drop_packet:
   dev_kfree_skb(skb);
}


/*
 *----------------------------------------------------------------------
 *
 * VNetNetIfCycleDetect --
 *
 *      Cycle detection algorithm.
 * 
 * Results: 
 *      TRUE if a cycle was detected, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
VNetNetIfCycleDetect(VNetJack *this,       // IN: jack
                     int       generation) // IN: 
{
   VNetNetIF *netIf = this->private;

   return VNetCycleDetectIf(netIf->dev->name, generation);
}


/*
 *----------------------------------------------------------------------
 *
 * VNetNetifOpen --
 *
 *      The virtual network's open dev operation. 
 *
 * Results: 
 *      errno.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VNetNetifOpen(struct net_device *dev) // IN:
{
   /*
    * The host interface is not available if the hub is bridged.
    *
    * It's actually okay to support both.  We just need
    * to tag packets when VNetXmitPacket gives them to the interface
    * so they can be dropped by VNetBridgeReceive().
    *
    *  if so return -EBUSY;
    */

   netif_start_queue(dev);
   // xxx need to change flags
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetNetifProbe --
 *
 *      ???
 *
 * Results: 
 *      0.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VNetNetifProbe(struct net_device *dev) // IN: unused
{
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetNetifClose --
 *
 *      The virtual network's close dev operation. 
 *
 * Results: 
 *      errno.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VNetNetifClose(struct net_device *dev) // IN:
{
   netif_stop_queue(dev);
   // xxx need to change flags
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetNetifStartXmit --
 *
 *      The virtual network's start xmit dev operation. 
 *
 * Results: 
 *      ???, 0.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VNetNetifStartXmit(struct sk_buff    *skb, // IN:
                   struct net_device *dev) // IN:
{
   VNetNetIF *netIf = netdev_priv(dev);

   if(skb == NULL) {
      return 0;
   }

   /*
    * Block a timer-based transmit from overlapping.  This could better be
    * done with atomic_swap(1, dev->tbusy), but set_bit() works as well.
    * If this ever occurs the queue layer is doing something evil!
    */

   VNetSend(&netIf->port.jack, skb);

   netIf->stats.tx_packets++;
   compat_netif_trans_update(dev);

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetNetifSetMAC --
 *
 *      Sets MAC address (i.e. via ifconfig) of netif device.
 *
 * Results: 
 *      Errno.
 *
 * Side effects:
 *      The MAC address may be changed.
 *
 *----------------------------------------------------------------------
 */

int
VNetNetifSetMAC(struct net_device *dev, // IN:
                void *p)                // IN:
{
   VNetNetIF *netIf = netdev_priv(dev);
   struct sockaddr const *addr = p;

   if (!VMX86_IS_STATIC_MAC(addr->sa_data)) {
      return -EINVAL;
   }
   memcpy(netIf->port.paddr, addr->sa_data, dev->addr_len);
   dev_addr_set(dev, addr->sa_data);
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetNetifSetMulticast --
 *
 *      Sets or clears the multicast address list.  This information
 *      comes from an array in dev->mc_list, and with a counter in
 *      dev->mc_count.
 *
 *      Since host-only network ifaces can't be bridged, it's debatable
 *      whether this is at all useful, but at least now you can turn it 
 *      on from ifconfig without getting an ioctl error.
 * Results: 
 *      Void.
 *
 * Side effects:
 *      Multicast address list might get changed.
 *
 *----------------------------------------------------------------------
 */

void
VNetNetifSetMulticast(struct net_device *dev) // IN: unused
{
}


/*
 *----------------------------------------------------------------------
 *
 * VNetNetifGetStats --
 *
 *      The virtual network's get stats dev operation. 
 *
 * Results: 
 *      A struct full of stats.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static struct net_device_stats *
VNetNetifGetStats(struct net_device *dev) // IN:
{
   VNetNetIF *netIf = netdev_priv(dev);

   return &netIf->stats;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetNetIfProcRead --
 *
 *      Callback for read operation on this netif entry in vnets proc fs.
 *
 * Results: 
 *      Length of read operation.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VNetNetIfProcRead(char   *page,  // IN/OUT: buffer to write into
                  char  **start, // OUT: 0 if file < 4k, else offset into page
                  off_t   off,   // IN: (unused) offset of read into the file
                  int     count, // IN: (unused) maximum number of bytes to read
                  int    *eof,   // OUT: TRUE if there is nothing more to read
                  void   *data)  // IN: client data
{
   VNetNetIF *netIf = data; 
   int len = 0;
   
   if (!netIf) {
      return len;
   }
   
   len += VNetPrintPort(&netIf->port, page+len);

   len += sprintf(page+len, "dev %s ", netIf->dev->name);
   
   len += sprintf(page+len, "\n");

   *start = 0;
   *eof   = 1;
   return len;
}
