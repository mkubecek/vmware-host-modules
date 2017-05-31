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
 * vnetUserListener.c --
 *
 *    The user listener module implements an event queue that can be accessed
 *    by the vmx process.
 *
 *    It registers an event listener with a given classMask. The listener
 *    enqueues events and the vmx process dequeues them. The vmx process can
 *    use blocking or non-blocking reads to consume the events. The user
 *    listener is thread safe.
 */

#include "driver-config.h" /* must be first */

#include <linux/netdevice.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include "compat_skbuff.h"
#include "vnetInt.h"

typedef struct VNetUserListener_EventNode VNetUserListener_EventNode;

struct VNetUserListener_EventNode {
   VNetUserListener_EventNode *nextEvent;
   VNet_EventHeader event;
};

#define EVENT_NODE_HEADER_SIZE offsetof(struct VNetUserListener_EventNode, event)

typedef struct VNetUserListener {
   VNetPort port;                          /* base port/jack */
   VNetEvent_Listener *eventListener;      /* event listener */
   spinlock_t lock;                        /* listener lock */
   wait_queue_head_t readerQueue;          /* reader queue */
   VNetUserListener_EventNode *firstEvent; /* first event to be read */
   VNetUserListener_EventNode *lastEvent;  /* last event to be read*/
} VNetUserListener;

static void VNetUserListenerFree(VNetJack *jack);
static void VNetUserListenerEventHandler(void *context, VNet_EventHeader *e);
static int VNetUserListenerRead(VNetPort *port, struct file *filp, char *buf,
                                size_t count);
static int VNetUserListenerPoll(VNetPort *port, struct file *filp,
                                poll_table *wait);


/*
 *----------------------------------------------------------------------
 *
 * VNetUserListener_Create --
 *
 *      Creates a user listener. Initializes the jack, the port, and itself.
 *      Finally, registers the event listener.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VNetUserListener_Create(uint32 classMask,  // IN: the listener's class mask
                        VNetJack *hubJack, // IN: the future hub jack
                        VNetPort **port)   // OUT: port to virtual hub
{
   static unsigned id = 0;
   VNetUserListener *userListener;
   int res;

   /* allocate user listener */
   userListener = kmalloc(sizeof *userListener, GFP_USER);
   if (userListener == NULL) {
      return -ENOMEM;
   }

   /* initialize jack */
   userListener->port.jack.peer = NULL;
   userListener->port.jack.numPorts = 1;
   VNetSnprintf(userListener->port.jack.name,
                sizeof userListener->port.jack.name, "userListener%u", id);
   userListener->port.jack.private = userListener;
   userListener->port.jack.index = 0;
   userListener->port.jack.procEntry = NULL;
   userListener->port.jack.free = VNetUserListenerFree;
   userListener->port.jack.rcv = NULL;
   userListener->port.jack.cycleDetect = NULL;
   userListener->port.jack.portsChanged = NULL;
   userListener->port.jack.isBridged = NULL;

   /* initialize port */
   userListener->port.id = id++;
   userListener->port.flags = 0;
   memset(userListener->port.paddr, 0, sizeof userListener->port.paddr);
   memset(userListener->port.ladrf, 0, sizeof userListener->port.ladrf);
   userListener->port.next = NULL;
   userListener->port.fileOpRead = VNetUserListenerRead;
   userListener->port.fileOpWrite = NULL;
   userListener->port.fileOpIoctl = NULL;
   userListener->port.fileOpPoll = VNetUserListenerPoll;

   /* initialize user listener */
   userListener->eventListener = NULL;
   spin_lock_init(&userListener->lock);
   init_waitqueue_head(&userListener->readerQueue);
   userListener->firstEvent = NULL;
   userListener->lastEvent = NULL;

   /*
    * create listener, must be after initialization because it fires right away
    * and populates the event queue, i.e. the event handler callback is called
    * before create listener returns
    */
   res = VNetHub_CreateListener(hubJack, VNetUserListenerEventHandler,
                                userListener, classMask,
                                &userListener->eventListener);
   if (res != 0) {
      LOG(0, (KERN_DEBUG "VNetUserListener_Create, can't create listener "
              "(%d)\n", res));
      kfree(userListener);
      return res;
   }

   /* return listener */
   *port = (VNetPort*)userListener;
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetUserListenerFree --
 *
 *      Frees a user listenere. Unregisters the event listener and drains the
 *      event queue.
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
VNetUserListenerFree(VNetJack *jack) // IN: jack to free
{
   VNetUserListener *userListener;
   int res;
   VNetUserListener_EventNode *p;

   /* destroy event listener */
   userListener = (VNetUserListener*)jack;
   res = VNetEvent_DestroyListener(userListener->eventListener);
   if (res != 0) {
      LOG(0, (KERN_DEBUG "VNetUserListenerFree, can't destroy listener"
              "(%d)\n", res));
   }

   /* clear event queue */
   spin_lock(&userListener->lock);
   p = userListener->firstEvent;
   while (p != NULL) {
      VNetUserListener_EventNode *t = p;
      p = p->nextEvent;
      kfree(t);
   }
   spin_unlock(&userListener->lock);

   /* free user listener */
   kfree(userListener);
}


/*
 *----------------------------------------------------------------------
 *
 * VNetUserListenerEventHandler --
 *
 *      Enqueues an event.
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
VNetUserListenerEventHandler(void *context,       // IN: the user listener
                             VNet_EventHeader *e) // IN: an event
{
   VNetUserListener *userListener;
   VNetUserListener_EventNode *t;

   /* allocate and initialize event node */
   t = kmalloc(EVENT_NODE_HEADER_SIZE + e->size, GFP_ATOMIC);
   if (t == NULL) {
      LOG(0, (KERN_DEBUG "VNetUserListenerEventHandler, out of memory\n"));
      return;
   }
   t->nextEvent = NULL;
   memcpy(&t->event, e, e->size);

   /* append event to event list */
   userListener = (VNetUserListener*)context;
   spin_lock(&userListener->lock);
   if (userListener->lastEvent != NULL) {
      userListener->lastEvent->nextEvent = t;
   } else {
      userListener->firstEvent = t;
   }
   userListener->lastEvent = t;
   spin_unlock(&userListener->lock);

   /* wake up readers */
   wake_up_interruptible(&userListener->readerQueue);
}

/*
 *----------------------------------------------------------------------
 *
 * VNetUserListenerRead --
 *
 *      Dequeues an event. May or may not block depending of the filp flags.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
VNetUserListenerRead(VNetPort    *port, // IN: the user listener
                     struct file *filp, // IN: the filp
                     char        *buf,  // OUT: the buffer
                     size_t      count) // IN: the buffer size
{
   VNetUserListener *userListener;
   VNetUserListener_EventNode *t;
   size_t n;
   int res;

   /* wait until there is data */
   userListener = (VNetUserListener*)port->jack.private;
   spin_lock(&userListener->lock);
   while (userListener->firstEvent == NULL) {
      spin_unlock(&userListener->lock);

      /* can we block? */
      if (filp->f_flags & O_NONBLOCK) {
         return -EAGAIN;
      }

      /* wait until there is data or we get interrupted */
      if (wait_event_interruptible(userListener->readerQueue,
                                   userListener->firstEvent != NULL)) {
         return -ERESTARTSYS;
      }

      spin_lock(&userListener->lock);
   }

   /* remove event from event list */
   t = userListener->firstEvent;
   userListener->firstEvent = t->nextEvent;
   if (userListener->firstEvent == NULL) {
      userListener->lastEvent = NULL;
   }
   spin_unlock(&userListener->lock);

   /* return data and free event */
   n = t->event.size;
   if (count < n) {
      n = count;
   }
   res = copy_to_user(buf, &t->event, n);
   kfree(t);
   return res ? -EFAULT : n;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetUserListenerPoll --
 *
 *      Polls an event.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
VNetUserListenerPoll(VNetPort     *port, // IN: the user listener
                     struct file  *filp, // IN: the filp
                     poll_table   *wait) // IN: the poll table
{
   VNetUserListener *userListener = (VNetUserListener*)port->jack.private;
   poll_wait(filp, &userListener->readerQueue, wait);
   return userListener->firstEvent != NULL ? POLLIN | POLLRDNORM : 0;
}
