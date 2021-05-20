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
#include <linux/if_ether.h>
#include <linux/sockios.h>
#include "compat_sock.h"

#define __KERNEL_SYSCALLS__
#include <asm/io.h>

#include <linux/proc_fs.h>
#include <linux/file.h>

#include "vnetInt.h"

#define HUB_TYPE_VNET         0x1
#define HUB_TYPE_PVN          0x2

typedef struct VNetHubStats {
   unsigned      tx;
} VNetHubStats;

typedef struct VNetHub {
   uint32        hubType;                  // HUB_TYPE_xxx
   union {
     int         vnetNum;                  // vnet number (HUB_TYPE_VNET)
     uint8       pvnID[VNET_PVN_ID_LEN];   // PVN ID      (HUB_TYPE_PVN)
   } id;
   Bool		 used[NUM_JACKS_PER_HUB];  // tracks which jacks in use
   VNetJack      jack[NUM_JACKS_PER_HUB];  // jacks for the hub
   VNetHubStats  stats[NUM_JACKS_PER_HUB]; // stats for the jacks
   int           totalPorts;               // num devices reachable from hub
   int           myGeneration;             // used for cycle detection
   struct VNetHub *next;                   // next hub in linked list
   VNetEvent_Mechanism *eventMechanism;    // event notification mechanism
} VNetHub;

static VNetJack *VNetHubAlloc(Bool allocPvn, int hubNum,
			      uint8 id[VNET_PVN_ID_LEN]);
static void VNetHubFree(VNetJack *this);
static void VNetHubReceive(VNetJack *this, struct sk_buff *skb);
static Bool VNetHubCycleDetect(VNetJack *this, int generation);
static void VNetHubPortsChanged(VNetJack *this);
static int  VNetHubIsBridged(VNetJack *this);
static int  VNetHubProcRead(char *page, char **start, off_t off,
                            int count, int *eof, void *data);

static VNetHub *vnetHub;
static DEFINE_SPINLOCK(vnetHubLock);


/*
 *----------------------------------------------------------------------
 *
 * VNetHubFindHubByNum --
 *
 *      Find a hub for a specified vnet number.
 *      Caller must be holding vnetHubLock.
 *
 * Results:
 *      Pointer to hub, or NULL if not found.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static VNetHub *
VNetHubFindHubByNum(int hubNum) // IN: vnet number to find
{
   VNetHub *currHub = vnetHub;
   while (currHub && (currHub->hubType != HUB_TYPE_VNET ||
		      currHub->id.vnetNum != hubNum)) {
      currHub = currHub->next;
   }
   return currHub;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetHubFindHubByID --
 *
 *      Find a hub for a specified PVN id.
 *      Caller must be holding vnetHubLock.
 *
 * Results:
 *      Pointer to hub, or NULL if not found.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static VNetHub *
VNetHubFindHubByID(uint8 idNum[VNET_PVN_ID_LEN]) // IN: PVN id to find
{
   VNetHub *currHub = vnetHub;
   while (currHub && (currHub->hubType != HUB_TYPE_PVN ||
                      memcmp(idNum, currHub->id.pvnID,
                             sizeof currHub->id.pvnID))) {
      currHub = currHub->next;
   }
   return currHub;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetHubAddHubToList --
 *
 *      Add hub to list of known hubs.
 *	Caller must be holding vnetHubLock.
 *
 * Results:
 *
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VNetHubAddHubToList(VNetHub *hub) // IN: hub to add to list
{
   hub->next = vnetHub;
   vnetHub = hub;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetHubRemoveHubFromList --
 *
 *      Remove hub from list of known hubs.
 *	Caller must be holding vnetHubLock.
 *
 * Results:
 *
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VNetHubRemoveHubFromList(VNetHub *hub) // IN: hub to remove from list
{
   VNetHub **h;

   for (h = &vnetHub; *h; h = &(*h)->next) {
      if (*h == hub) {
         *h = hub->next;
         break;
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VNetHub_AllocVnet --
 *
 *      Allocate a jack on a hub for a vnet.
 *
 * Results:
 *      The jack to connect to, NULL on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VNetJack *
VNetHub_AllocVnet(int hubNum) // IN: the vnet number to alloc on
{
   return VNetHubAlloc(FALSE, hubNum, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * VNetHub_AllocPvn --
 *
 *      Allocate a jack on a hub for a PVN.
 *
 * Results:
 *      The jack to connect to, NULL on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VNetJack *
VNetHub_AllocPvn(uint8 id[VNET_PVN_ID_LEN]) // IN: the PVN ID to alloc on
{
   return VNetHubAlloc(TRUE, -1, id);
}

/*
 *----------------------------------------------------------------------
 *
 * VNetHubAlloc --
 *
 *      Allocate a jack on this hub.
 *
 * Results:
 *      The jack to connect to, NULL on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VNetJack *
VNetHubAlloc(Bool allocPvn, // IN: TRUE for PVN, FALSE for vnet
	     int hubNum,    // IN: vnet # to use (-1 if allocPvn == TRUE)
	     uint8 id[VNET_PVN_ID_LEN])    // IN: PVN ID to use (NULL if allocPvn == FALSE)
{
   VNetHub *hub;
   VNetJack *jack;
   int i;
   int retval;
   unsigned long flags;
   static uint32 pvnInstance = 0;

   spin_lock_irqsave(&vnetHubLock, flags);

   hub = allocPvn ? VNetHubFindHubByID(id) : VNetHubFindHubByNum(hubNum);
   if (!hub) {
      spin_unlock_irqrestore(&vnetHubLock, flags);
      LOG(1, (KERN_DEBUG "/dev/vmnet: hub %d does not exist, allocating memory.\n",
              hubNum));

      hub = kmalloc(sizeof *hub, GFP_KERNEL);
      if (hub == NULL) {
         LOG(1, (KERN_DEBUG "/dev/vmnet: no memory to allocate hub %d\n", hubNum));
         return NULL;
      }
      for (i = 0; i < NUM_JACKS_PER_HUB; i++) {
         jack = &hub->jack[i];

         /*
          * The private field indicates if this jack is allocated.
          * NULL means free, otherwise the jack is allocated and it
          * should point back to the hub.
          */

         jack->peer = NULL;
         jack->numPorts = 0;
	 if (allocPvn) {
	    VNetSnprintf(jack->name, sizeof jack->name, "pvn%d.%d",
			 pvnInstance, i);
	 } else {
	    VNetSnprintf(jack->name, sizeof jack->name, "hub%d.%d", hubNum, i);
	 }
         jack->private = NULL;
         jack->index = i;
         jack->procEntry = NULL;
         jack->free = VNetHubFree;
         jack->rcv = VNetHubReceive;
         jack->cycleDetect = VNetHubCycleDetect;
         jack->portsChanged = VNetHubPortsChanged;
         jack->isBridged = VNetHubIsBridged;

         memset(&hub->stats[i], 0, sizeof hub->stats[i]);

	 hub->used[i] = FALSE;
      }

      if (allocPvn) {
	 hub->hubType = HUB_TYPE_PVN;
         memcpy(hub->id.pvnID, id, sizeof hub->id.pvnID);
	 ++pvnInstance;
      } else {
	 hub->hubType = HUB_TYPE_VNET;
	 hub->id.vnetNum = hubNum;
      }
      hub->next = NULL;
      hub->totalPorts = 0;
      hub->myGeneration = 0;

      /* create event mechanism */
      retval = VNetEvent_CreateMechanism(&hub->eventMechanism);
      if (retval != 0) {
         LOG(1, (KERN_DEBUG "can't create event mechanism (%d)\n", retval));
         kfree(hub);
         return NULL;
      }

      spin_lock_irqsave(&vnetHubLock, flags);
      if (allocPvn ? VNetHubFindHubByID(id) : VNetHubFindHubByNum(hubNum)) {
         /*
	  * Someone else just allocated this hub. Free our structure
	  * and use already present hub.
	  */

	 kfree(hub);
	 hub = allocPvn ? VNetHubFindHubByID(id) : VNetHubFindHubByNum(hubNum);
      } else {
	 VNetHubAddHubToList(hub);
      }
   }

   for (i = 0; i < NUM_JACKS_PER_HUB; i++) {
      jack = &hub->jack[i];
      if (!hub->used[i]) {
         hub->used[i] = TRUE;
	 spin_unlock_irqrestore(&vnetHubLock, flags);

         /*
          * Make proc entry for this jack.
          */

         retval = VNetProc_MakeEntry(jack->name, S_IFREG, jack,
                                     VNetHubProcRead, &jack->procEntry);
         if (retval) {
            if (retval == -ENXIO) {
               jack->procEntry = NULL;
            } else {
               hub->used[i] = FALSE;
               return NULL;
            }
         }

         /*
          *  OK, now allocate this jack.
          */

         jack->numPorts = hub->totalPorts;
         jack->peer = NULL;
         jack->private = hub;

         return jack;
      }
   }
   spin_unlock_irqrestore(&vnetHubLock, flags);

   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetHubFree --
 *
 *      Free the jack on this hub.
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
VNetHubFree(VNetJack *this)
{
   VNetHub *hub = (VNetHub*)this->private;
   int i = 0;
   int retval;
   unsigned long flags;

   if (this != &hub->jack[this->index]) {
      LOG(1, (KERN_DEBUG "/dev/vmnet: bad free of hub jack\n"));
      return;
   }

   if (this->procEntry) {
      VNetProc_RemoveEntry(this->procEntry);
      this->procEntry = NULL;
   }

   this->private = NULL;

   spin_lock_irqsave(&vnetHubLock, flags);

   hub->used[this->index] = FALSE;

   for (i = 0; i < NUM_JACKS_PER_HUB; i++) {
      if (hub->used[i]) {
	 spin_unlock_irqrestore(&vnetHubLock, flags);
	 return;
      }
   }
   VNetHubRemoveHubFromList(hub);

   spin_unlock_irqrestore(&vnetHubLock, flags);

   /* destroy event mechanism */
   retval = VNetEvent_DestroyMechanism(hub->eventMechanism);
   if (retval != 0) {
      LOG(1, (KERN_DEBUG "can't destroy event mechanism (%d)\n", retval));
   }
   hub->eventMechanism = NULL;

   kfree(hub);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VNetHub_CreateSender --
 *
 *    Creates an event sender for the mechanism of this hub.
 *
 * Results:
 *    Returns 0 if successful, or a negative value if an error occurs.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
VNetHub_CreateSender(VNetJack *jack,       // IN: a jack to a hub
                     VNetEvent_Sender **s) // OUT: the new sender
{
   if (jack != NULL && jack->private != NULL) {
      VNetHub *hub = (VNetHub*)jack->private;
      return VNetEvent_CreateSender(hub->eventMechanism, s);
   } else {
      return -EINVAL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VNetHub_CreateListener --
 *
 *    Creates an event listener for the mechanism of this hub.
 *
 * Results:
 *    Returns 0 if successful, or a negative value if an error occurs.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
VNetHub_CreateListener(VNetJack *jack,         // IN: a jack to a hub
                       VNetEvent_Handler h,    // IN: a handler
                       void *data,             // IN: the handler's data
                       uint32 classMask,       // IN: a class mask
                       VNetEvent_Listener **l) // OUT: the new listener
{
   if (jack != NULL && jack->private != NULL) {
      VNetHub *hub = (VNetHub*)jack->private;
      return VNetEvent_CreateListener(hub->eventMechanism, h, data, classMask,
                                      l);
   } else {
      return -EINVAL;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VNetHubReceive --
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
VNetHubReceive(VNetJack       *this, // IN:
               struct sk_buff *skb)  // IN:
{
   VNetHub *hub = (VNetHub*)this->private;
   VNetJack *jack;
   struct sk_buff *clone;
   int i;

   hub->stats[this->index].tx++;

   for (i = 0; i < NUM_JACKS_PER_HUB; i++) {
      jack = &hub->jack[i];
      if (jack->private &&   /* allocated */
          jack->peer &&      /* and connected */
          jack->state &&     /* and enabled */
          jack->peer->state && /* and enabled */
          jack->peer->rcv && /* and has a receiver */
          (jack != this)) {  /* and not a loop */
         clone = skb_clone(skb, GFP_ATOMIC);
         if (clone) {
            VNetSend(jack, clone);
         }
      }
   }

   dev_kfree_skb(skb);
}


/*
 *----------------------------------------------------------------------
 *
 * VNetHubCycleDetect --
 *
 *      Cycle detection algorithm.
 *
 * Results:
 *      TRUE if a cycle was detected, FALSE otherwise.
 *
 * Side effects:
 *      Will generate other cycleDetect events to other jacks on hub.
 *
 *----------------------------------------------------------------------
 */

Bool
VNetHubCycleDetect(VNetJack *this,
                   int       generation)
{
   VNetHub *hub = (VNetHub *)this->private;
   Bool foundCycle;
   int i;

   if (hub->myGeneration == generation) {
      return TRUE;
   }

   hub->myGeneration = generation;

   for (i = 0; i < NUM_JACKS_PER_HUB; i++) {
      if (hub->jack[i].private && hub->jack[i].state && (i != this->index)) {
         foundCycle = VNetCycleDetect(hub->jack[i].peer, generation);
         if (foundCycle) {
            return TRUE;
         }
      }
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetHubPortsChanged --
 *
 *      The number of ports connected to this jack has change, react
 *      accordingly.
 *      This function presumes that the caller has the semaphore.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May generate other portsChanged events to other jacks on hub.
 *
 *----------------------------------------------------------------------
 */

void
VNetHubPortsChanged(VNetJack *this)
{
   VNetHub *hub = (VNetHub *)this->private;
   int num, new;
   int i;

   hub->totalPorts = 0;

   for (i=0; i<NUM_JACKS_PER_HUB; i++) {
      if (hub->jack[i].private) {
         hub->totalPorts += VNetGetAttachedPorts(&hub->jack[i]);
      }
   }

   for (i=0; i<NUM_JACKS_PER_HUB; i++) {
      if (hub->jack[i].private) {
         num = VNetGetAttachedPorts(&hub->jack[i]);
         new = hub->totalPorts - num;
         if (i == this->index) {
            if (new != hub->jack[i].numPorts) {
               /* basically an assert failure */
               LOG(0, (KERN_DEBUG "/dev/vmnet: numPorts mismatch.\n"));
            }
         } else {
            hub->jack[i].numPorts = new;
            if (hub->jack[i].state)
               VNetPortsChanged(hub->jack[i].peer);
         }
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VNetHubIsBridged --
 *
 *      Check whether we are bridged.
 *
 * Results:
 *      0 - not bridged
 *      1 - we are bridged but the interface is not up
 *      2 - we are bridged and the interface is up
 *      3 - some bridges are down
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VNetHubIsBridged(VNetJack *this)
{
   VNetHub *hub = (VNetHub*)this->private;
   int ret = 0;
   int num;
   int i;

   for (i=0; i<NUM_JACKS_PER_HUB; i++) {
      if ((hub->jack[i].private) && (i != this->index)) {
         num = VNetIsBridged(&hub->jack[i]);
         ret = MAX(ret, num);
         if ((num == 1) && (ret == 2)) {
            ret = 3;
         }
      }
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetHubProcRead --
 *
 *      Callback for read operation on hub entry in vnets proc fs.
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
VNetHubProcRead(char    *page,   // IN/OUT: buffer to write into
                char   **start,  // OUT: 0 if file < 4k, else offset into page
                off_t    off,    // IN: offset of read into the file
                int      count,  // IN: maximum number of bytes to read
                int     *eof,    // OUT: TRUE if there is nothing more to read
                void    *data)   // IN: client data - not used
{
   VNetJack *jack = (VNetJack*)data;
   VNetHub *hub;
   int len = 0;

   if (!jack || !jack->private) {
      return len;
   }
   hub = (VNetHub*)jack->private;

   len += VNetPrintJack(jack, page+len);

   len += sprintf(page+len, "tx %u ", hub->stats[jack->index].tx);

   len += sprintf(page+len, "\n");

   *start = 0;
   *eof   = 1;
   return len;
}
