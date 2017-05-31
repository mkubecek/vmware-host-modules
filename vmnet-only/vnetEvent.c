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
 * vnetEvent.c --
 *
 *    The event notification mechanism for the vmnet module. It consists of
 *    mechanisms, senders, listeners, and events. A mechanism is the scope of
 *    a single notification mechanism. Within this scope, senders send events
 *    to listeners and listeners handle events by means of their registered
 *    event handler.
 *
 *    Mechanisms, senders, and listeners can be created and destroyed in any
 *    order. The implementation ensures proper destruction independent of the
 *    destruction order.
 *
 *    The event handlers registered by the listeners are not allowed to
 *    recursively enter the mechanism. The implementation enforces this rule.
 *    The event handlers are not allowed to block.
 *
 *    Mechanisms, senders, and listeners are thread-safe, i.e. they can be
 *    accessed concurrently by multiple threads. Event handlers must be thread-
 *    safe.
 *
 *    Callers into the event notification mechanism can assume that they are
 *    not called recursively by event handlers. Furthermore, they can assume
 *    that they do not block.
 *
 *    Implementation Notes
 *
 *    The mechanism, including senders, listeners, ands event lists are
 *    guarded by the mechanism's 'lock' spinlock. The listener's event
 *    handlers are called holding this lock.
 *
 *    To avoid deadlock from event handlers recursively calling the
 *    notification mechanism, the mechanism's 'currentHandler' field stores the
 *    calling task during invocation of an event handler.
 *
 */

#include "vnetKernel.h"
#include "vnetEvent.h"

typedef struct VNetEvent_EventNode VNetEvent_EventNode;

struct VNetEvent_EventNode {
   VNetEvent_EventNode *nextEvent;
   VNet_EventHeader event;
};

#define EVENT_NODE_HEADER_SIZE offsetof(struct VNetEvent_EventNode, event)

struct VNetEvent_Mechanism {
   VNetKernel_SpinLock lock;          /* mechanism lock */
   void *handlerTask;                 /* task calling an event handler */
   uint32 refCount;                   /* ref count */
   uint32 senderId;                   /* next sender id */
   VNetEvent_Sender *firstSender;     /* first sender */
   VNetEvent_Listener *firstListener; /* first listener */
};

struct VNetEvent_Sender {
   VNetEvent_Mechanism *m;            /* mechanism */
   uint32 senderId;                   /* sender id */
   VNetEvent_Sender *nextSender;      /* next sender */
   VNetEvent_EventNode *firstEvent;   /* first event */
};

struct VNetEvent_Listener {
   VNetEvent_Mechanism *m;            /* mechanism */
   VNetEvent_Listener *nextListener;  /* next listener */
   VNetEvent_Handler handler;         /* event handler */
   void *data;                        /* event handler data */
   uint32 classMask;                  /* event handler class mask */
};


/*
 *-----------------------------------------------------------------------------
 * VNetEvent_Mechanism
 *-----------------------------------------------------------------------------
 */

/*
 *-----------------------------------------------------------------------------
 *
 * VNetEvent_CreateMechanism --
 *
 *    Creates a mechanism.
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
VNetEvent_CreateMechanism(VNetEvent_Mechanism **m) // OUT: the new mechanism
{
   VNetEvent_Mechanism *t;

   /* allocate mechanism */
   t = VNetKernel_MemoryAllocate(sizeof *t);
   if (t == NULL) {
      return VNetKernel_ENOMEM;
   }

   /* initialize mechanism */
   VNetKernel_SpinLockInit(&t->lock);
   t->handlerTask = NULL;
   t->refCount = 1;
   t->senderId = 0;
   t->firstSender = NULL;
   t->firstListener = NULL;

   /* return mechanism */
   *m = t;
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VNetEvent_DestroyMechanism --
 *
 *    Destroys a mechanism.
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
VNetEvent_DestroyMechanism(VNetEvent_Mechanism *m) // IN: a mechanism
{
   uint32 refCount;

   /* check handler recursion */
   if (m->handlerTask == VNetKernel_ThreadCurrent()) {
      return VNetKernel_EBUSY;
   }

   /* Warning: The implementation may not be a spinlock. Eg. on Mac OS */
   VNetKernel_SpinLockAcquire(&m->lock);
   /* decrement ref count */
   refCount = --m->refCount;
   VNetKernel_SpinLockRelease(&m->lock);

   /* free mechanism */
   if (refCount == 0) {
      VNetKernel_SpinLockFree(&m->lock);
      VNetKernel_MemoryFree(m);
   }
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 * VNetEvent_Sender
 *-----------------------------------------------------------------------------
 */

/*
 *-----------------------------------------------------------------------------
 *
 * VNetEvent_CreateSender --
 *
 *    Creates a sender.
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
VNetEvent_CreateSender(VNetEvent_Mechanism *m, // IN: a mechanism
                       VNetEvent_Sender **s)   // OUT: the new sender
{
   VNetEvent_Sender *t;

   /* check handler recursion */
   if (m->handlerTask == VNetKernel_ThreadCurrent()) {
      return VNetKernel_EBUSY;
   }

   /* allocate sender */
   t = VNetKernel_MemoryAllocate(sizeof *t);
   if (t == NULL) {
      return VNetKernel_ENOMEM;
   }

   /* initialize sender and insert it into sender list */
   VNetKernel_SpinLockAcquire(&m->lock);
   t->m = m;
   m->refCount++;
   t->senderId = m->senderId;
   m->senderId++;
   t->nextSender = m->firstSender;
   m->firstSender = t;
   t->firstEvent = NULL;
   VNetKernel_SpinLockRelease(&m->lock);

   /* return sender */
   *s = t;
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VNetEvent_DestroySender --
 *
 *    Destroys a sender.
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
VNetEvent_DestroySender(VNetEvent_Sender *s) // IN: a sender
{
   VNetEvent_Mechanism *m;
   VNetEvent_Sender *p;
   VNetEvent_Sender **q;
   VNetEvent_EventNode *n;

   /* check handler recursion */
   m = s->m;

   /*
    * m->handlerTask might get updated while doing this check, we must acquire
    * a read lock if we want to make this foolproof.
    */
   if (m->handlerTask == VNetKernel_ThreadCurrent()) {
      return VNetKernel_EBUSY;
   }

   /* remove sender from sender list */
   VNetKernel_SpinLockAcquire(&m->lock);
   q = &m->firstSender;
   while (TRUE) {
      p = *q;
      if (p == NULL) {
         /* not found */
         VNetKernel_SpinLockRelease(&m->lock);
         return VNetKernel_EINVAL;
      } else if (p == s) {
        /* found */
        break;
      }
      q = &p->nextSender;
   }
   *q = p->nextSender;
   VNetKernel_SpinLockRelease(&m->lock);
   VNetEvent_DestroyMechanism(m);

   /* free sender and events */
   n = s->firstEvent;
   while (n != NULL) {
      VNetEvent_EventNode *t = n;
      n = n->nextEvent;
      VNetKernel_MemoryFree(t);
   }
   VNetKernel_MemoryFree(s);
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VNetEvent_GetSenderId --
 *
 *    Returns the sender id of a sender.
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
VNetEvent_GetSenderId(const VNetEvent_Sender *s, // IN: a sender
                      uint32 *senderId)          // OUT: the sender id
{

   /* we don't check handler recursion */

   /* return senderId */
   *senderId = s->senderId;
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VNetEvent_Send --
 *
 *    Sends an event to all listeners registered with a sender. The
 *    precondition 's->senderId == e->senderId' must hold.
 *    If an identical event (sender + type + size) exists in the sent queue,
 *    the function reuses the event node.
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
VNetEvent_Send(VNetEvent_Sender *s, // IN: a sender
               VNet_EventHeader *e) // IN: an event
{
   VNetEvent_Mechanism *m;
   VNetEvent_EventNode *p;
   VNetEvent_EventNode **q;
   VNetEvent_Listener *l;
   uint32 classSet;

   /* check handler recursion */
   m = s->m;
   if (m->handlerTask == VNetKernel_ThreadCurrent()) {
      return VNetKernel_EBUSY;
   }

   /* precondition */
   if (s->senderId != e->senderId) {
      return VNetKernel_EINVAL;
   }

   /* lock */
   VNetKernel_SpinLockAcquire(&m->lock);
   m->handlerTask = VNetKernel_ThreadCurrent();

   /* find previously sent event */
   q = &s->firstEvent;
   while (TRUE) {
       p = *q;
       if (p == NULL ||
           (p->event.eventId == e->eventId && p->event.type == e->type)) {
               break;
           }
      q = &p->nextEvent;
   }

   /* remove previously sent event */
   if (p != NULL && p->event.size != e->size) {
      *q = p->nextEvent;
      VNetKernel_MemoryFree(p);
      p = NULL;
   }

   /* insert new event into event list*/
   if (p == NULL) {
      p = VNetKernel_MemoryAllocate(EVENT_NODE_HEADER_SIZE + e->size);
      if (p == NULL) {
         m->handlerTask = NULL;
         VNetKernel_SpinLockRelease(&m->lock);
         return VNetKernel_ENOMEM;
      }
      p->nextEvent = s->firstEvent;
      s->firstEvent = p;
   }
   memcpy(&p->event, e, e->size);

   /* send event */
   classSet = e->classSet;
   l = m->firstListener;
   while (l != NULL) {
      if ((classSet & l->classMask) != 0) {
         l->handler(l->data, e);
      }
      l = l->nextListener;
   }

   /* unlock */
   m->handlerTask = NULL;
   VNetKernel_SpinLockRelease(&m->lock);
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 * VNetEvent_Listener
 *-----------------------------------------------------------------------------
 */

/*
 *-----------------------------------------------------------------------------
 *
 * VNetEvent_CreateListener --
 *
 *    Creates a listener and re-sends all existing events to the listener's
 *    event handler. The listener will receive events that satisfy
 *    'event.class & classMask != 0'.
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
VNetEvent_CreateListener(VNetEvent_Mechanism *m, // IN: a mechanism
                         VNetEvent_Handler h,    // IN: a handler
                         void *data,             // IN: the handler's data
                         uint32 classMask,       // IN: a class mask
                         VNetEvent_Listener **l) // OUT: the new listener
{
   VNetEvent_Listener *t;
   VNetEvent_Sender *s;
   VNetEvent_EventNode *e;

   /* check handler recursion */
   if (m->handlerTask == VNetKernel_ThreadCurrent()) {
      return VNetKernel_EBUSY;
   }

   /* allocate listener */
   t = VNetKernel_MemoryAllocate(sizeof *t);
   if (t == NULL) {
      return VNetKernel_ENOMEM;
   }

   /* lock */
   VNetKernel_SpinLockAcquire(&m->lock);
   m->handlerTask = VNetKernel_ThreadCurrent();

   /* initialize listener and insert it into listener list */
   t->m = m;
   m->refCount++;
   t->nextListener = m->firstListener;
   m->firstListener = t;
   t->handler = h;
   t->data = data;
   t->classMask = classMask;

   /* creation done, so send all events */
   s = m->firstSender;
   while (s != NULL) {
      e = s->firstEvent;
      while (e != NULL) {
         if ((e->event.classSet & classMask) != 0) {
            h(data, &e->event);
         }
         e = e->nextEvent;
      }
      s = s->nextSender;
   }

   /* unlock */
   m->handlerTask = NULL;
   VNetKernel_SpinLockRelease(&m->lock);

   /* return listener */
   *l = t;
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VNetEvent_DestroyListener --
 *
 *    Destroys a listener.
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
VNetEvent_DestroyListener(VNetEvent_Listener *l) // IN: a listener
{
   VNetEvent_Mechanism *m;
   VNetEvent_Listener *p;
   VNetEvent_Listener **q;

   /* check handler recursion */
   m = l->m;
   if (m->handlerTask == VNetKernel_ThreadCurrent()) {
      return VNetKernel_EBUSY;
   }

   /* remove listener from listener list */
   VNetKernel_SpinLockAcquire(&m->lock);
   q = &m->firstListener;
   while (TRUE) {
      p = *q;
      if (p == NULL) {
         /* not found */
         VNetKernel_SpinLockRelease(&m->lock);
         return VNetKernel_EINVAL;
      } else if (p == l) {
        /* found */
        break;
      }
      q = &p->nextListener;
   }
   *q = p->nextListener;
   VNetKernel_SpinLockRelease(&m->lock);
   VNetEvent_DestroyMechanism(m);

   /* free listener */
   VNetKernel_MemoryFree(l);
   return 0;
}
