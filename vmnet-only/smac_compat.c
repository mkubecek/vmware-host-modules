/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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
 * smac_compat.c --
 *
 *      This file defines an abstraction layer to handling
 *      differences among the Linux kernel and avoiding
 *      symbol match issues.
 */

#include "driver-config.h"

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/spinlock.h>       // for spinlock_t

#include <linux/slab.h>
#include <linux/poll.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/mm.h>
#include "compat_skbuff.h"
#include <linux/sockios.h>

#define __KERNEL_SYSCALLS__
#include <asm/io.h>

#include <linux/proc_fs.h>
#include <linux/file.h>

#include "vnetInt.h"
#include "vmnetInt.h"
#include "smac_compat.h"

#ifdef VMX86_DEVEL
#define DBG 1
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
#define read_skb_users(skb) atomic_read(&skb->users)
#else
#define read_skb_users(skb) refcount_read(&skb->users)
#endif
#else
#undef DBG
#endif /* VMX86_DEVEL */



/*
 *----------------------------------------------------------------------
 * SMACL_GetUptime --
 *
 *      Wrapper for jiffies.
 *
 * Results:
 *      Uptime in ticks
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

unsigned long SMACINT
SMACL_GetUptime()
{
   return jiffies;
}

/*
 *----------------------------------------------------------------------
 * SMACL_Memcpy --
 *
 *      Wrapper for memcpy().
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Copies memory
 *
 *----------------------------------------------------------------------
 */

void SMACINT
SMACL_Memcpy(void *d,        // IN: destination pointer
	     const void *s,  // IN: source pointer
	     size_t l)       // IN: length to copy
{
   memcpy(d, s, l);
}


/*
 *----------------------------------------------------------------------
 * SMACL_Memcmp --
 *
 *      Wrapper for memcmp().
 *
 * Results:
 *      refer to documentation for memcmp().
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int SMACINT
SMACL_Memcmp(const void *p1, // IN: pointer to data
	     const void *p2, // IN: pointer to data
	     size_t l)       // IN: length to compare
{
   return memcmp(p1, p2, l);
}


/*
 *----------------------------------------------------------------------
 * SMACL_Memset --
 *
 *      Wrapper for memset().
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void SMACINT
SMACL_Memset(void *p1, // IN: pointer to data
	     int val,  // IN: value to set
	     size_t l) // IN: length to set
{
   memset(p1, val, l);
   return;
}

/*
 *----------------------------------------------------------------------
 * SMACL_Alloc --
 *
 *      Wrapper for kmalloc().
 *
 * Results:
 *      Pointer to memory if successful, NULL otherwise
 *
 * Side effects:
 *      Allocates memory
 *
 *----------------------------------------------------------------------
 */

void* SMACINT
SMACL_Alloc(size_t size) // IN: size to allocate
{
   return kmalloc(size, GFP_ATOMIC);
}


/*
 *----------------------------------------------------------------------
 * SMACL_Free --
 *
 *      Wrapper for kfree().
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Frees memory
 *
 *----------------------------------------------------------------------
 */

void SMACINT
SMACL_Free(void *ptr) // IN: pointer to free
{
   kfree(ptr);
}


/*
 *----------------------------------------------------------------------
 * SMACL_InitSpinlock --
 *
 *      Wrapper for spin_lock_init().
 *      The reason we have to use void is to avoid dependency on kernel.
 *      The spinlock_t structure can change for different compilation options.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      see spin_lock_init()
 *
 *----------------------------------------------------------------------
 */

void SMACINT
SMACL_InitSpinlock(void  **s) // IN: spinlock
{
   *s = kmalloc(sizeof(spinlock_t), GFP_ATOMIC);
   spin_lock_init((spinlock_t *)*s);
}


/*
 *----------------------------------------------------------------------
 * SMACL_AcquireSpinlock --
 *
 *      Wrapper for spin_lock_irqsave().
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Grabs lock (see spin_lock_irqsave())
 *
 *----------------------------------------------------------------------
 */

void SMACINT
SMACL_AcquireSpinlock(void  **s,            // IN: spinlock
		      unsigned long *flags) // IN/OUT: flags
{
   unsigned long f = *flags;
   spin_lock_irqsave((spinlock_t *)*s, f);
   *flags = f;
}


/*
 *----------------------------------------------------------------------
 * SMACL_ReleaseSpinlock --
 *
 *      Wrapper for spin_unlock_irqrestore().
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Ungrabs lock (see spin_unlock_irqrestore())
 *
 *----------------------------------------------------------------------
 */

void SMACINT
SMACL_ReleaseSpinlock(void **s,             // IN: spinlock
		      unsigned long *flags) // IN/OUT: flags
{
   unsigned long f = *flags;
   spin_unlock_irqrestore((spinlock_t *)*s, f);
   *flags = f;
}


#ifdef DBG
/*
 *----------------------------------------------------------------------
 * SMACL_Print --
 *
 *      Wrapper for printk().
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Creates output
 *
 *----------------------------------------------------------------------
 */

void SMACINT
SMACL_Print(const char * msg, // IN: format message
	    ...)              // IN: params (currently ignored)
{
   char buf[512];
   int len;
   va_list ap;

   va_start(ap, msg);
   len = vsnprintf(buf, sizeof buf, msg, ap);
   va_end(ap);
   buf[sizeof buf - 1] = '\0';
   printk(KERN_DEBUG "%s", buf);
}

#endif

/*
 *----------------------------------------------------------------------
 * SMACL_DupPacket --
 *
 *      Wrapper for skb_copy().
 *
 * Results:
 *      Pointer to packet if successful, NULL otherwise
 *
 * Side effects:
 *      Creates a private duplicate of packet
 *
 *----------------------------------------------------------------------
 */

struct sk_buff* SMACINT
SMACL_DupPacket(struct sk_buff *skb) // IN: packet to duplicate
{
   return skb_copy(skb, GFP_ATOMIC);
}

/*
 *----------------------------------------------------------------------
 * SMACL_PacketData --
 *
 *      Wrapper to get data from sk_buff.  This function might be
 *      a bit extreme, but it's good to be able to handle changes
 *      to the layout of the sk_buff struct.
 *
 * Results:
 *      Pointer to data in sk_buff.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void* SMACINT
SMACL_PacketData(struct sk_buff *skb) // IN: pointer to packet buffer
{
   return skb->data;
}


/*
 *----------------------------------------------------------------------
 *  SMACL_IsSkbHostBound --
 *
 *      Checks if the direction of the packet is host bound.
 *
 * Results:
 *      Returns non zero if host bound
 *              0 for anything else
 i
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

int SMACINT
SMACL_IsSkbHostBound(struct sk_buff* skb) // IN: packet to process
{
   return (skb->pkt_type == PACKET_HOST);
}


#ifdef DBG
/*
 *----------------------------------------------------------------------
 * SMACL_PrintSkb --
 *
 *      Print information about the skb.
 *
 * Results:
 *      prints the sk_buff structure
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void SMACINT
SMACL_PrintSkb(struct sk_buff *skb,          // IN: sk_buff structure
               char *str)                    // IN: type of process
{
   LOG(4, (KERN_DEBUG "%s pointer %p shared? %d cloned? %d\n",
            str, skb, skb_shared(skb), skb_cloned(skb)));
   LOG(4, (KERN_DEBUG "next %p, prev %p\n",
            skb->next, skb->prev));
   LOG(4, (KERN_DEBUG "sock %p, dev %p\n",
            skb->sk, skb->dev));
   LOG(4, (KERN_DEBUG "pkt_type %x truesize %u protocol %u\n",
            skb->pkt_type, skb->truesize, skb->protocol));
   LOG(4, (KERN_DEBUG "users %d, tail %p, end %p\n",
            read_skb_users(skb), compat_skb_tail_pointer(skb),
            compat_skb_end_pointer(skb)));
#if 0
#define C skb->mac.raw
   if(skb->mac.raw) {
      LOG(4, (KERN_DEBUG "dest %02x:%02x:%02x:%02x:%02x:%02x"
            " source %02x:%02x:%02x:%02x:%02x:%02x\n",
            C[0],C[1],C[2],C[3],C[4],C[5] ,
            C[6],C[7],C[8],C[9],C[10],C[11])); 
   }
#undef C
   if(skb->dst) {
      LOG(4, (KERN_DEBUG "dst_entry->__refcount %d\n",
           atomic_read(&(skb->dst->__refcnt))));
   }
#endif
   LOG(4, (KERN_DEBUG "dataref %d end\n",
           atomic_read(&(skb_shinfo(skb)->dataref))));
}
#endif
