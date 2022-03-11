/*********************************************************
 * Copyright (C) 1998-2021 VMware, Inc. All rights reserved.
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

#ifndef _VNETINT_H
#define _VNETINT_H

#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"
#include "vnet.h"
#include "vm_oui.h"
#include "net.h"
#include "vnetEvent.h"

#include <asm/page.h>

#include <linux/mutex.h>

#define INLINE inline


/*
 * Logging
 */

#define LOGLEVEL 0


#if LOGLEVEL >= 0
#define LOG(level, args) ((void) (LOGLEVEL >= (level) ? (printk args) : 0))
#else
#define LOG(level, args)
#endif

#define MAX(_a, _b)   (((_a) > (_b)) ? (_a) : (_b))

/*
 * Ethernet
 */

#define MAC_EQ(_a, _b)         !memcmp((_a), (_b), ETH_ALEN)
#define SKB_2_DESTMAC(_skb)    (((struct ethhdr *)(_skb)->data)->h_dest)
#define SKB_2_SRCMAC(_skb)     (((struct ethhdr *)(_skb)->data)->h_source)
#define UP_AND_RUNNING(_flags) (((_flags) & (IFF_RUNNING|IFF_UP)) == \
				(IFF_RUNNING|IFF_UP))
#define NETDEV_UP_AND_RUNNING(dev) ((((dev)->flags) & IFF_UP) && netif_running(dev))

/*
 * Misc defines 
 */

#define NULL_TERMINATE_STRING(a) (a)[sizeof (a) - 1] = '\0'

/*
 * Fundamental sizes
 */

#define VNET_NUM_VNETS         256
#define VNET_MAJOR_NUMBER      119

/* We support upto 32 adapters with LSP + DHCP + NAT + netif + sniffer */
#define NUM_JACKS_PER_HUB      68 
#define VNET_MAX_QLEN          1024

#define VNET_NUM_IPBASED_MACS  64
#define VNET_MAX_JACK_NAME_LEN 16

#define VNET_LADRF_LEN         8

#if ( defined(IFNAMSIZ) && (IFNAMSIZ >= 16) )
#define VNET_NAME_LEN          IFNAMSIZ
#else
#define VNET_NAME_LEN          16
#endif

/*
 * Data structures
 */

/*
 * Newer kernels and those without CONFIG_PROC_FS don't have read_proc_t,
 * so define our own here to make things a bit simpler.
 */
typedef int (VNetProcReadFn)(char *page, char **start, off_t off,
                             int count, int *eof, void *data);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
typedef struct VNetProcEntry {
   struct proc_dir_entry *pde;   /* Procfs node entry. */
   void *data;                   /* User data. */
   VNetProcReadFn *fn;           /* Callback fuction to read node. */
} VNetProcEntry;
#else
typedef struct proc_dir_entry VNetProcEntry;
#endif

typedef struct VNetJack VNetJack;
typedef struct VNetPort VNetPort;

/*
 *  The jack is the basic mechanism for connecting to objects
 *  that send packet between them.
 */

extern struct mutex vnetStructureMutex;

struct VNetJack {
   VNetJack      *peer;
   int            numPorts;
   char           name[VNET_MAX_JACK_NAME_LEN];
   void          *private;     // private field for containing object
   int            index;       // private field for containing object
   VNetProcEntry *procEntry;   // private field for containing object
   Bool           state;       // TRUE for enabled
   struct kref    kref;        // ref count

   void         (*free)(VNetJack *this);
   void         (*rcv)(VNetJack *this, struct sk_buff *skb);
   Bool         (*cycleDetect)(VNetJack *this, int generation);
   void         (*portsChanged)(VNetJack *this);
   int          (*isBridged)(VNetJack *this);
};


/*
 *  The port is an extension of the jack. It has a user level
 *  interface and an ethernet address. There are 3 types of ports:
 *  userif, netif, and bridge.
 */

struct VNetPort {
   VNetJack    jack;     // must be first
   unsigned    id;
   unsigned    hubNum;
   uint32      flags;
   uint8       paddr[ETH_ALEN];
   uint8       ladrf[VNET_LADRF_LEN];
   uint8       exactFilter[VNET_MAX_EXACT_FILTER_LEN][ETHER_ADDR_LEN];
   uint32      exactFilterLen;
   
   VNetPort   *next;
   
   int       (*fileOpRead)(VNetPort *this, struct file *filp,
                           char *buf, size_t count);
   int       (*fileOpWrite)(VNetPort *this, struct file *filp,
                            const char *buf, size_t count);
   int       (*fileOpIoctl)(VNetPort *this, struct file *filp,
                            unsigned int iocmd, unsigned long ioarg);   
   int       (*fileOpPoll)(VNetPort *this, struct file *filp,
                           poll_table *wait);
};



/*
 *  Functions exported from vnet module
 */

VNetJack *VNetHub_AllocVnet(int hubNum);
VNetJack *VNetHub_AllocPvn(uint8 id[VNET_PVN_ID_LEN]);
int VNetHub_CreateSender(VNetJack *jack, VNetEvent_Sender **s);
int VNetHub_CreateListener(VNetJack *jack, VNetEvent_Handler h, void* data,
                           uint32 classMask, VNetEvent_Listener **l);

int VNetConnect(VNetJack *jack1, VNetJack *jack2);

VNetJack *VNetDisconnect(VNetJack *jack);

void VNetSend(VNetJack *jack, struct sk_buff *skb);

int VNetProc_MakeEntry(char *name, int mode, void *data,
                       VNetProcReadFn *fn, VNetProcEntry **ret);

void VNetProc_RemoveEntry(VNetProcEntry *node);

int VNetPrintJack(const VNetJack *jack, char *buf);

int VNet_MakeMACAddress(VNetPort *port);

int VNetSetMACUnique(VNetPort *port, const uint8 mac[ETH_ALEN]);

     
/*
 *  Utility functions
 */

extern const uint8 allMultiFilter[VNET_LADRF_LEN];
extern const uint8 broadcast[ETH_ALEN];
 
Bool VNetPacketMatch(const uint8 *destAddr, const uint8 *ifAddr, 
                     const uint8 *exactFilter, const uint32 exactFilterLen,
		     const uint8 *ladrf, uint32 flags);

Bool VNetCycleDetectIf(const char *name, int generation);

int VNetPrintPort(const VNetPort *port, char *buf);

int VNetSnprintf(char *str, size_t size, const char *format, ...);

/*
 *  Procfs file system
 */

extern int VNetProc_Init(void);

extern void VNetProc_Cleanup(void);


/*
 *----------------------------------------------------------------------
 *
 * VNetCycleDetect --
 *
 *      Perform the cycle detect alogorithm for this generation.
 *
 * Results: 
 *      TRUE if a cycle was detected, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
VNetCycleDetect(VNetJack *jack,       // IN: jack
                int       generation) // IN: 
{
   if (jack && jack->cycleDetect) {
      return jack->cycleDetect(jack, generation);
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetPortsChanged --
 *
 *      Notify a jack that the number of connected ports has changed.
 *	vnetStructureSemaphore must be held.
 *
 * Results: 
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VNetPortsChanged(VNetJack *jack) // IN:
{
   if (jack && jack->portsChanged) {
      jack->portsChanged(jack);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VNetIsBridged --
 *
 *      Check whether we are bridged.
 *      vnetPeerLock must be held.
 *
 * Results:
 *      0 - not bridged
 *      1 - we are bridged but the interface is down
 *      2 - we are bridged and the interface is up
 *      3 - some bridges are down
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE int
VNetIsBridged(VNetJack *jack) // IN: jack
{
   if (jack && jack->state && jack->peer && jack->peer->state &&
       jack->peer->isBridged) {
      return jack->peer->isBridged(jack->peer);
   }

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetFree --
 *
 *      Free the resources owned by the jack.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VNetFree(VNetJack *jack) // IN: jack
{
   if (jack && jack->free) {
      jack->free(jack);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VNetGetAttachedPorts --
 *
 *      Get the number of ports attached to this jack through its peer.
 *
 * Results:
 *      The number of attached ports to this jack through its peer.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE int
VNetGetAttachedPorts(VNetJack *jack) // IN: jack
{
   if (jack && jack->state && jack->peer && jack->peer->state) {
      return jack->peer->numPorts;
   }
   return 0;
}

#endif
