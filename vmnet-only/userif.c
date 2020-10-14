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

#define __KERNEL_SYSCALLS__

#include <linux/file.h>
#include <linux/highmem.h>
#include <linux/if_ether.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/sockios.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/wait.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/taskstats_kern.h>  // For <linux/sched/signal.h> without version dependency
#endif

#include <net/checksum.h>
#include <net/sock.h>

#include <asm/io.h>

#include "vnetInt.h"
#include "compat_skbuff.h"
#include "compat_mmap_lock.h"
#include "vmnetInt.h"
#include "vm_atomic.h"
#include "vm_assert.h"
#include "monitorAction_exported.h"

typedef struct VNetUserIFStats {
   unsigned    read;
   unsigned    written;
   unsigned    queued;
   unsigned    droppedDown;
   unsigned    droppedMismatch;
   unsigned    droppedOverflow;
   unsigned    droppedLargePacket;
} VNetUserIFStats;

typedef struct VNetUserIF {
   VNetPort               port;
   struct sk_buff_head    packetQueue;
   Atomic_uint32         *pollPtr;
   MonitorActionIntr     *actionIntr;
   uint32                 pollMask;
   MonitorIdemAction      actionID;
   uint32*                recvClusterCount;
   wait_queue_head_t      waitQueue;
   struct page*           actPage;
   struct page*           pollPage;
   struct page*           recvClusterPage;
   VNetUserIFStats        stats;
   VNetEvent_Sender      *eventSender;
} VNetUserIF;

static void VNetUserIfUnsetupNotify(VNetUserIF *userIf);
static int  VNetUserIfSetupNotify(VNetUserIF *userIf, VNet_Notify *vn);
static int  VNetUserIfSetUplinkState(VNetPort *port, uint8 linkUp);
extern unsigned int  vnet_max_qlen;

#if COMPAT_LINUX_VERSION_CHECK_LT(3, 2, 0)
#   define compat_kmap_frag(frag) kmap((frag)->page)
#   define compat_kunmap_frag(page) kunmap((frag)->page)
#else
#   define compat_kmap_frag(frag) kmap(skb_frag_page(frag))
#   define compat_kunmap_frag(frag) kunmap(skb_frag_page(frag))
#endif

static unsigned int compat_skb_frag_size(const skb_frag_t *frag)
{
#if COMPAT_LINUX_VERSION_CHECK_LT(3, 2, 0)
	return frag->size;
#else
	return skb_frag_size(frag);
#endif
}

static unsigned int compat_skb_frag_off(const skb_frag_t *frag)
{
#if COMPAT_LINUX_VERSION_CHECK_LT(5, 4, 0) && \
	!(defined(CONFIG_SUSE_VERSION) && CONFIG_SUSE_VERSION == 15 && \
	  defined(CONFIG_SUSE_PATCHLEVEL) && CONFIG_SUSE_PATCHLEVEL >= 2)
	return frag->page_offset;
#else
	return skb_frag_off(frag);
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0) && defined(VERIFY_WRITE)
	#define write_access_ok(addr, size) access_ok(VERIFY_WRITE, addr, size)
#else
	#define write_access_ok(addr, size) access_ok(addr, size)
#endif

#if COMPAT_LINUX_VERSION_CHECK_LT(5, 10, 0)
static inline
__wsum compat_csum_and_copy_to_user(const void *src, void __user *dst, int len)
{
	int err;
	__wsum ret;

	ret = csum_and_copy_to_user(src, dst, len, 0, &err);
	return err ? 0 : ret;
}
#else
static inline
__wsum compat_csum_and_copy_to_user(const void *src, void __user *dst, int len)
{
	return csum_and_copy_to_user(src, dst, len);
}
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * UserifLockPage --
 *
 *    Lock in core the physical page associated to a valid virtual
 *    address.
 *
 * Results:
 *    The page structure on success
 *    NULL on failure: memory pressure. Retry later
 *
 * Side effects:
 *    Loads page into memory
 *
 *-----------------------------------------------------------------------------
 */

static INLINE struct page *
UserifLockPage(VA addr) // IN
{
   struct page *page = NULL;
   int retval;

   mmap_read_lock(current->mm);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
   retval = get_user_pages(addr, 1, FOLL_WRITE, &page, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
   retval = get_user_pages(addr, 1, 1, 0, &page, NULL);
#else
   retval = get_user_pages(current, current->mm, addr,
                           1, 1, 0, &page, NULL);
#endif
   mmap_read_unlock(current->mm);

   if (retval != 1) {
      return NULL;
   }

   return page;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VNetUserIfMapUint32Ptr --
 *
 *    Maps a portion of user-space memory into the kernel.
 *
 * Results:
 *    0 on success
 *    < 0 on failure: the actual value determines the type of failure
 *
 * Side effects:
 *    Might sleep.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
VNetUserIfMapPtr(VA uAddr,        // IN: pointer to user memory
                 size_t size,     // IN: size of data
                 struct page **p, // OUT: locked page
                 void **ptr)      // OUT: kernel mapped pointer
{
   if (!write_access_ok((void *)uAddr, size) ||
       (((uAddr + size - 1) & ~(PAGE_SIZE - 1)) !=
        (uAddr & ~(PAGE_SIZE - 1)))) {
      return -EINVAL;
   }

   *p = UserifLockPage(uAddr);
   if (*p == NULL) {
      return -EAGAIN;
   }

   *ptr = (uint8 *)kmap(*p) + (uAddr & (PAGE_SIZE - 1));
   return 0;
}

static INLINE int
VNetUserIfMapUint32Ptr(VA uAddr,        // IN: pointer to user memory
                       struct page **p, // OUT: locked page
                       uint32 **ptr)    // OUT: kernel mapped pointer
{
   return VNetUserIfMapPtr(uAddr, sizeof **ptr, p, (void **)ptr);
}

/*
 *-----------------------------------------------------------------------------
 *
 * VNetUserIfSetupNotify --
 *
 *    Sets up notification by filling in pollPtr, actPtr, and recvClusterCount
 *    fields.
 * 
 * Results: 
 *    0 on success
 *    < 0 on failure: the actual value determines the type of failure
 *
 * Side effects:
 *    Fields pollPtr, actPtr, recvClusterCount, pollPage, actPage, and 
 *    recvClusterPage are filled in VNetUserIf structure.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
VNetUserIfSetupNotify(VNetUserIF *userIf, // IN
                      VNet_Notify *vn)    // IN
{
   unsigned long flags;
   struct sk_buff_head *q = &userIf->packetQueue;
   uint32 *pollPtr;
   MonitorActionIntr *actionIntr;
   uint32 *recvClusterCount;
   struct page *pollPage = NULL;
   struct page *actPage = NULL;
   struct page *recvClusterPage = NULL;
   int retval;

   if (userIf->pollPtr || userIf->actionIntr || userIf->recvClusterCount) {
      LOG(0, (KERN_DEBUG "vmnet: Notification mechanism already active\n"));
      return -EBUSY;
   }

   if ((retval = VNetUserIfMapUint32Ptr((VA)vn->pollPtr, &pollPage,
                                        &pollPtr)) < 0) {
      return retval;
   }

   /* Atomic operations require proper alignment */
   if ((uintptr_t)pollPtr & (sizeof *pollPtr - 1)) {
      LOG(0, (KERN_DEBUG "vmnet: Incorrect notify alignment\n"));
      retval = -EFAULT;
      goto error_free;
   }

   if ((retval = VNetUserIfMapPtr((VA)vn->actPtr, sizeof *actionIntr,
                                  &actPage,
                                  (void **)&actionIntr)) < 0) {
      goto error_free;
   }

   if ((retval = VNetUserIfMapUint32Ptr((VA)vn->recvClusterPtr,
                                        &recvClusterPage,
                                        &recvClusterCount)) < 0) {
      goto error_free;
   }

   spin_lock_irqsave(&q->lock, flags);
   if (userIf->pollPtr || userIf->actionIntr || userIf->recvClusterCount) {
      spin_unlock_irqrestore(&q->lock, flags);
      retval = -EBUSY;
      LOG(0, (KERN_DEBUG "vmnet: Notification mechanism already active\n"));
      goto error_free;
   }

   userIf->pollPtr = (Atomic_uint32 *)pollPtr;
   userIf->pollPage = pollPage;
   userIf->actionIntr = actionIntr;
   userIf->actPage = actPage;
   userIf->recvClusterCount = recvClusterCount;
   userIf->recvClusterPage = recvClusterPage;
   userIf->pollMask = vn->pollMask;
   userIf->actionID = vn->actionID;
   spin_unlock_irqrestore(&q->lock, flags);
   return 0;

 error_free:
   if (pollPage) {
      kunmap(pollPage);
      put_page(pollPage);
   }
   if (actPage) {
      kunmap(actPage);
      put_page(actPage);
   }
   if (recvClusterPage) {
      kunmap(recvClusterPage);
      put_page(recvClusterPage);
   }
   return retval;
}

/*
 *----------------------------------------------------------------------
 *
 * VNetUserIfUnsetupNotify --
 *
 *      Destroys permanent mapping for notify structure provided by user.
 * 
 * Results: 
 *      None.
 *
 * Side effects:
 *      Fields pollPtr, actPtr, recvClusterCount, etc. in VNetUserIf
 *      structure are cleared.
 *
 *----------------------------------------------------------------------
 */

static void
VNetUserIfUnsetupNotify(VNetUserIF *userIf) // IN
{
   unsigned long flags;
   struct page *pollPage = userIf->pollPage;
   struct page *actPage = userIf->actPage;
   struct page *recvClusterPage = userIf->recvClusterPage;

   struct sk_buff_head *q = &userIf->packetQueue;

   spin_lock_irqsave(&q->lock, flags);
   userIf->pollPtr = NULL;
   userIf->pollPage = NULL;
   userIf->actionIntr = NULL;
   userIf->actPage = NULL;
   userIf->recvClusterCount = NULL;
   userIf->recvClusterPage = NULL;
   userIf->pollMask = 0;
   userIf->actionID = -1;
   spin_unlock_irqrestore(&q->lock, flags);

   /* Release */
   if (pollPage) {
      kunmap(pollPage);
      put_page(pollPage);
   }
   if (actPage) {
      kunmap(actPage);
      put_page(actPage);
   }
   if (recvClusterPage) {
      kunmap(recvClusterPage);
      put_page(recvClusterPage);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VNetUserIfFree --
 *
 *      Free the user interface port.
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
VNetUserIfFree(VNetJack *this) // IN
{
   VNetUserIF *userIf = (VNetUserIF*)this;
   struct sk_buff *skb;

   for (;;) {
      skb = skb_dequeue(&userIf->packetQueue);
      if (skb == NULL) {
	 break;
      }
      dev_kfree_skb(skb);
   }
   
   if (userIf->pollPtr) {
      VNetUserIfUnsetupNotify(userIf);
   }

   if (userIf->eventSender) {
      VNetEvent_DestroySender(userIf->eventSender);
   }

   if (this->procEntry) {
      VNetProc_RemoveEntry(this->procEntry);
   }

   kfree(userIf);
}


/*
 *----------------------------------------------------------------------
 *
 * VNetUserIfReceive --
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

static void
VNetUserIfReceive(VNetJack       *this, // IN
                  struct sk_buff *skb)  // IN
{
   VNetUserIF *userIf = (VNetUserIF*)this->private;
   uint8 *dest = SKB_2_DESTMAC(skb);
   unsigned long flags;
   
   if (!UP_AND_RUNNING(userIf->port.flags)) {
      userIf->stats.droppedDown++;
      goto drop_packet;
   }
   
   if (!VNetPacketMatch(dest,
                        userIf->port.paddr,
                        (const uint8 *)userIf->port.exactFilter,
                        userIf->port.exactFilterLen,
                        userIf->port.ladrf,
                        userIf->port.flags)) {
      userIf->stats.droppedMismatch++;
      goto drop_packet;
   }
   
   if (skb_queue_len(&userIf->packetQueue) >= vnet_max_qlen) {
      userIf->stats.droppedOverflow++;
      goto drop_packet;
   }
   
   if (skb->len > ETHER_MAX_QUEUED_PACKET) {
      userIf->stats.droppedLargePacket++;
      goto drop_packet;
   }

   userIf->stats.queued++;

   spin_lock_irqsave(&userIf->packetQueue.lock, flags);
   /*
    * __skb_dequeue_tail does not take any locks so must be used with
    * appropriate locks held only.
    */
   __skb_queue_tail(&userIf->packetQueue, skb);
   if (userIf->pollPtr) {
      Atomic_Or(userIf->pollPtr, userIf->pollMask);
      if (skb_queue_len(&userIf->packetQueue) >= (*userIf->recvClusterCount)) {
         MonitorAction_SetBits(userIf->actionIntr, userIf->actionID);
      }
   }
   spin_unlock_irqrestore(&userIf->packetQueue.lock, flags);

   wake_up(&userIf->waitQueue);
   return;
   
 drop_packet:
   dev_kfree_skb(skb);
}


/*
 *----------------------------------------------------------------------
 *
 * VNetUserIfProcRead --
 *
 *      Callback for read operation on this userif entry in vnets proc fs.
 *
 * Results: 
 *      Length of read operation.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
VNetUserIfProcRead(char    *page,  // IN/OUT: buffer to write into
                   char   **start, // OUT: 0 if file < 4k, else offset into
                                   //      page
                   off_t    off,   // IN: offset of read into the file
                   int      count, // IN: maximum number of bytes to read
                   int     *eof,   // OUT: TRUE if there is nothing more to
                                   //      read
                   void    *data)  // IN: client data - not used
{
   VNetUserIF *userIf = (VNetUserIF*)data; 
   int len = 0;
   
   if (!userIf) {
      return len;
   }
   
   len += VNetPrintPort(&userIf->port, page+len);
   
   len += sprintf(page+len, "read %u written %u queued %u ",
                  userIf->stats.read,
                  userIf->stats.written,
                  userIf->stats.queued);
   
   len += sprintf(page+len, 
		  "dropped.down %u dropped.mismatch %u "
		  "dropped.overflow %u dropped.largePacket %u",
                  userIf->stats.droppedDown,
                  userIf->stats.droppedMismatch,
                  userIf->stats.droppedOverflow,
		  userIf->stats.droppedLargePacket);

   len += sprintf(page+len, "\n");
   
   *start = 0;
   *eof   = 1;
   return len;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetCopyDatagram --
 *
 *      Copy part of datagram to userspace.
 *
 * Results: 
 *	zero    on success,
 *	-EFAULT if buffer is an invalid area
 *
 * Side effects:
 *      Data copied to the buffer.
 *
 *----------------------------------------------------------------------
 */

static int
VNetCopyDatagram(const struct sk_buff *skb,	// IN: skb to copy
		 char *buf,			// OUT: where to copy data
		 int len)			// IN: length
{
   struct iovec iov = {
      .iov_base = buf,
      .iov_len  = len,
   };
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
   return skb_copy_datagram_iovec(skb, 0, &iov, len);
#else
   struct iov_iter ioviter;

   iov_iter_init(&ioviter, READ, &iov, 1, len);
   return skb_copy_datagram_iter(skb, 0, &ioviter, len);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * VNetCsumCopyDatagram --
 *
 *      Copy part of datagram to userspace doing checksum at same time.
 *
 *	Do not mark this function INLINE, it is recursive! With all gcc's 
 *	released up to now (<= gcc-3.3.1) inlining this function just
 *	consumes 120 more bytes of code and goes completely mad on
 *	register allocation, storing almost everything in the memory.
 *
 * Results: 
 *	folded checksum (non-negative value) on success,
 *	-EINVAL if offset is too big,
 *	-EFAULT if buffer is an invalid area
 *
 * Side effects:
 *      Data copied to the buffer.
 *
 *----------------------------------------------------------------------
 */

static int
VNetCsumCopyDatagram(const struct sk_buff *skb,	// IN: skb to copy
		     unsigned int offset,	// IN: how many bytes skip
		     char *buf)			// OUT: where to copy data
{
   unsigned int csum;
   int len = skb_headlen(skb) - offset;
   char *curr = buf;
   const skb_frag_t *frag;

   /* 
    * Something bad happened. We skip only up to skb->nh.raw, and skb->nh.raw
    * must be in the header, otherwise we are in the big troubles.
    */
   if (len < 0) {
      return -EINVAL;
   }

   csum = compat_csum_and_copy_to_user(skb->data + offset, curr, len);
   if (!csum)
	   return -EFAULT;
   curr += len;

   for (frag = skb_shinfo(skb)->frags;
	frag != skb_shinfo(skb)->frags + skb_shinfo(skb)->nr_frags;
	frag++) {
      if (compat_skb_frag_size(frag) > 0) {
	 unsigned int tmpCsum;
	 const void *vaddr;

	 vaddr = compat_kmap_frag(frag);
	 tmpCsum = compat_csum_and_copy_to_user(vaddr + compat_skb_frag_off(frag),
						curr, compat_skb_frag_size(frag));
	 compat_kunmap_frag(frag);

	 if (!tmpCsum)
		 return -EFAULT;
	 csum = csum_block_add(csum, tmpCsum, curr - buf);
	 curr += compat_skb_frag_size(frag);
      }
   }

   for (skb = skb_shinfo(skb)->frag_list; skb != NULL; skb = skb->next) {
      int tmpCsum;

      tmpCsum = VNetCsumCopyDatagram(skb, 0, curr);
      if (tmpCsum < 0) {
	 return tmpCsum;
      }
      /* Folded checksum must be inverted before we can use it */
      csum = csum_block_add(csum, tmpCsum ^ 0xFFFF, curr - buf);
      curr += skb->len;
   }
   return csum_fold(csum);
}


/*
 *----------------------------------------------------------------------
 *
 * VNetCopyDatagramToUser --
 *
 *      Copy complete datagram to the user space. Fill correct checksum
 *	into the copied datagram if nobody did it yet.
 *
 * Results: 
 *      On success byte count, on failure -EFAULT.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE_SINGLE_CALLER int
VNetCopyDatagramToUser(const struct sk_buff *skb,	// IN
		       char *buf,			// OUT
		       size_t count)			// IN
{
   if (count > skb->len) {
      count = skb->len;
   }
   /*
    * If truncation occurs, we do not bother with checksumming - caller cannot
    * verify checksum anyway in such case, and copy without checksum is
    * faster.
    */
   if (skb->pkt_type == PACKET_OUTGOING && 	/* Packet must be outgoing */
       skb->ip_summed == VM_TX_CHECKSUM_PARTIAL &&	/* Without checksum */
       compat_skb_network_header_len(skb) &&    /* We must know where header is */
       skb->len == count) {			/* No truncation may occur */
      size_t skl;
      int csum;
      u_int16_t csum16;
     
      skl = compat_skb_csum_start(skb);
      if (VNetCopyDatagram(skb, buf, skl)) {
	 return -EFAULT;
      }
      csum = VNetCsumCopyDatagram(skb, skl, buf + skl);
      if (csum < 0) {
	 return csum;
      }
      csum16 = csum;
      if (copy_to_user(buf + skl + compat_skb_csum_offset(skb),
                       &csum16, sizeof csum16)) {
	 return -EFAULT;
      }
   } else {
      if (VNetCopyDatagram(skb, buf, count)) {
	 return -EFAULT;
      }
   }
   return count;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetUserIfRead --
 *
 *      The virtual network's read file operation. Reads the next pending
 *      packet for this network connection.
 *
 * Results: 
 *      On success the len of the packet received,
 *      else if no packet waiting and nonblocking 0,
 *      else -errno.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int 
VNetUserIfRead(VNetPort    *port, // IN
               struct file *filp, // IN
               char        *buf,  // OUT
               size_t      count) // IN
{
   VNetUserIF *userIf = (VNetUserIF*)port->jack.private;
   struct sk_buff *skb;
   int ret;
   unsigned long flags;
   DECLARE_WAITQUEUE(wait, current);

   add_wait_queue(&userIf->waitQueue, &wait);
   for (;;) {
      set_current_state(TASK_INTERRUPTIBLE);
      skb = skb_peek(&userIf->packetQueue);
      if (skb && (skb->len > count)) {
         skb = NULL;
         ret = -EMSGSIZE;
         break;
      }
      ret = -EAGAIN;

      spin_lock_irqsave(&userIf->packetQueue.lock, flags);
      /*
       * __skb_dequeue does not take any locks so must be used with
       * appropriate locks held only.
       */
      skb = __skb_dequeue(&userIf->packetQueue);
      if (userIf->pollPtr) {
         if (!skb) {
            /* List empty */
            Atomic_And(userIf->pollPtr, ~userIf->pollMask);
         }
      }
      spin_unlock_irqrestore(&userIf->packetQueue.lock, flags);

      if (skb != NULL || filp->f_flags & O_NONBLOCK) {
         break;
      }
      ret = -EINTR;
      if (signal_pending(current)) {
         break;
      }
      schedule();
   }
   __set_current_state(TASK_RUNNING);
   remove_wait_queue(&userIf->waitQueue, &wait);
   if (! skb) {
      return ret;
   }

   userIf->stats.read++;

   count = VNetCopyDatagramToUser(skb, buf, count);
   dev_kfree_skb(skb);
   return count;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetUserIfWrite --
 *
 *      The virtual network's write file operation. Send the raw packet
 *      to the network.
 *
 * Results: 
 *      On success the count of bytes written else errno.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int 
VNetUserIfWrite(VNetPort    *port, // IN
                struct file *filp, // IN
                const char  *buf,  // IN
                size_t      count) // IN
{
   VNetUserIF *userIf = (VNetUserIF*)port->jack.private;
   struct sk_buff *skb;

   /*
    * Check size
    */
   
   if (count < sizeof (struct ethhdr) || 
       count > ETHER_MAX_QUEUED_PACKET) {
      return -EINVAL;
   }

   /*
    * Required to enforce the downWhenAddrMismatch policy in the MAC
    * layer. --hpreg
    */
   if (!UP_AND_RUNNING(userIf->port.flags)) {
      userIf->stats.droppedDown++;
      return count;
   }

   /*
    * Allocate an sk_buff.
    */
   
   skb = dev_alloc_skb(count + 7);
   if (skb == NULL) {
      // XXX obey O_NONBLOCK?
      return -ENOBUFS;
   }
   
   skb_reserve(skb, 2);
   
   /*
    * Copy the data and send it.
    */
   
   userIf->stats.written++;
   if (copy_from_user(skb_put(skb, count), buf, count)) {
      dev_kfree_skb(skb);
      return -EFAULT;
   }
   
   VNetSend(&userIf->port.jack, skb);

   return count;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VNetUserIfIoctl --
 *
 *      XXX
 *
 * Results: 
 *      0 on success
 *      -errno on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static int
VNetUserIfIoctl(VNetPort      *port,  // IN
                struct file   *filp,  // IN
                unsigned int   iocmd, // IN
                unsigned long  ioarg) // IN or OUT depending on iocmd
{
   VNetUserIF *userIf = (VNetUserIF*)port->jack.private;

   switch (iocmd) {
   case SIOCSETNOTIFY:
      return -EINVAL;
   case SIOCSETNOTIFY2:
#ifdef VMX86_SERVER
      /* 
       * This ioctl always return failure on ESX since we cannot map pages into 
       * the console os that are from the VMKernel address space which  was the
       * only case we used this.
       */
      return -EINVAL;
#else // VMX86_SERVER
   /*
    * ORs pollMask into the integer pointed to by ptr if pending packet. Is
    * cleared when all packets are drained.
    */
   {
      int retval;
      VNet_Notify vn;

      if (copy_from_user(&vn, (void *)ioarg, sizeof vn)) {
         return -EFAULT;
      }

      ASSERT_ON_COMPILE(VNET_NOTIFY_VERSION == 5);
      ASSERT_ON_COMPILE(ACTION_EXPORTED_VERSION == 2);
      if (vn.version != VNET_NOTIFY_VERSION ||
          vn.actionVersion != ACTION_EXPORTED_VERSION ||
          vn.actionID / ACTION_WORD_SIZE >= ACTION_NUM_WORDS) {
         return -ENOTTY;
      }

      retval = VNetUserIfSetupNotify(userIf, &vn);
      if (retval < 0) {
         return retval;
      }

      break;
   }
#endif // VMX86_SERVER
   case SIOCUNSETNOTIFY:
      if (!userIf->pollPtr) {
	 /* This should always happen on ESX. */
         return -EINVAL;
      }
      VNetUserIfUnsetupNotify(userIf);
      break;

   case SIOCSIFFLAGS:
      /* 
       * Drain queue when interface is no longer active. We drain the queue to 
       * avoid having old packets delivered to the guest when reneabled.
       */
      
      if (!UP_AND_RUNNING(userIf->port.flags)) {
         struct sk_buff *skb;
         unsigned long flags;
         struct sk_buff_head *q = &userIf->packetQueue;
         
         while ((skb = skb_dequeue(q)) != NULL) {
            dev_kfree_skb(skb);
         }
         
         spin_lock_irqsave(&q->lock, flags);
         if (userIf->pollPtr) {
            if (skb_queue_empty(q)) {
               /*
                * Clear the pending bit as no packets are pending at this
                * point.
                */
               Atomic_And(userIf->pollPtr, ~userIf->pollMask);
            }
         }
         spin_unlock_irqrestore(&q->lock, flags);
      }
      break;
   case SIOCINJECTLINKSTATE:
      {
         uint8 linkUpFromUser;
         if (copy_from_user(&linkUpFromUser, (void *)ioarg, 
                            sizeof linkUpFromUser)) {
            return -EFAULT;
         }
         
         if (linkUpFromUser != 0 && linkUpFromUser != 1) {
            return -EINVAL;
         }

         return VNetUserIfSetUplinkState(port, linkUpFromUser);
      }
      break;
   default:
      return -ENOIOCTLCMD;
      break;
   }
   
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * VNetUserIfPoll --
 *
 *      The virtual network's file poll operation.
 *
 * Results: 
 *      Return POLLIN if success, else sleep and return 0.
 *      FIXME: Should not we always return POLLOUT?
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
VNetUserIfPoll(VNetPort     *port, // IN
               struct file  *filp, // IN
               poll_table   *wait) // IN
{
   VNetUserIF *userIf = (VNetUserIF*)port->jack.private;
   
   poll_wait(filp, &userIf->waitQueue, wait);
   if (!skb_queue_empty(&userIf->packetQueue)) {
      return POLLIN;
   }

   return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * VNetUserIfSetUplinkState --
 *
 *      Sends link state change event.
 * 
 * Results: 
 *      0 on success, errno on failure.
 *
 * Side effects:
 *      Link state event is sent to all the event listeners
 *
 *----------------------------------------------------------------------
 */

int
VNetUserIfSetUplinkState(VNetPort *port, uint8 linkUp)
{
   VNetUserIF *userIf;
   VNetJack *hubJack;
   VNet_LinkStateEvent event;
   int retval;

   userIf = (VNetUserIF *)port->jack.private;
   hubJack = port->jack.peer;

   if (port->jack.state == FALSE || hubJack == NULL) {
      return -EINVAL;
   }

   if (userIf->eventSender == NULL) {
      /* create event sender */
      retval = VNetHub_CreateSender(hubJack, &userIf->eventSender);
      if (retval != 0) {
         return retval;
      }
   }

   event.header.size = sizeof event;
   retval = VNetEvent_GetSenderId(userIf->eventSender, &event.header.senderId);
   if (retval != 0) {
      LOG(1, (KERN_NOTICE "userif-%d: can't send link state event, "
              "getSenderId failed (%d)\n", userIf->port.id, retval));
      return retval;
   }
   event.header.eventId = 0;
   event.header.classSet = VNET_EVENT_CLASS_UPLINK;
   event.header.type = VNET_EVENT_TYPE_LINK_STATE;
   /* 
    * XXX kind of a hack, vmx will coalesce linkup/down if they come from the
    * same adapter.
    */
   event.adapter = linkUp;
   event.up = linkUp;
   retval = VNetEvent_Send(userIf->eventSender, &event.header);
   if (retval != 0) {
      LOG(1, (KERN_NOTICE "userif-%d: can't send link state event, send "
              "failed (%d)\n", userIf->port.id, retval));
   }

   LOG(0, (KERN_NOTICE "userif-%d: sent link %s event.\n",
        userIf->port.id, linkUp ? "up" : "down"));

   return retval;
}

/*
 *----------------------------------------------------------------------
 *
 * VNetUserIf_Create --
 *
 *      Create a user level port to the wonderful world of virtual
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
VNetUserIf_Create(VNetPort **ret) // OUT
{
   VNetUserIF *userIf;
   static unsigned id = 0;
   int retval;
   
   userIf = kmalloc(sizeof *userIf, GFP_USER);
   if (!userIf) {
      return -ENOMEM;
   }

   /*
    * Initialize fields.
    */
   
   userIf->port.id = id++;

   userIf->port.jack.peer = NULL;
   userIf->port.jack.numPorts = 1;
   VNetSnprintf(userIf->port.jack.name, sizeof userIf->port.jack.name,
		"userif%u", userIf->port.id);
   userIf->port.jack.private = userIf;
   userIf->port.jack.index = 0;
   userIf->port.jack.procEntry = NULL;
   userIf->port.jack.free = VNetUserIfFree;
   userIf->port.jack.rcv = VNetUserIfReceive;
   userIf->port.jack.cycleDetect = NULL;
   userIf->port.jack.portsChanged = NULL;
   userIf->port.jack.isBridged = NULL;
   userIf->pollPtr = NULL;
   userIf->actionIntr = NULL;
   userIf->recvClusterCount = NULL;
   userIf->pollPage = NULL;
   userIf->actPage = NULL;
   userIf->recvClusterPage = NULL;
   userIf->pollMask = 0;
   userIf->actionID = -1;
   userIf->port.exactFilterLen = 0;
   userIf->eventSender = NULL;

   /*
    * Make proc entry for this jack.
    */

   retval = VNetProc_MakeEntry(userIf->port.jack.name, S_IFREG, userIf,
                               VNetUserIfProcRead,
                               &userIf->port.jack.procEntry);
   if (retval) {
      if (retval == -ENXIO) {
         userIf->port.jack.procEntry = NULL;
      } else {
         kfree(userIf);
         return retval;
      }
   }

   /*
    * Rest of fields.
    */
   
   userIf->port.flags = IFF_RUNNING;

   memset(userIf->port.paddr, 0, sizeof userIf->port.paddr);
   memset(userIf->port.ladrf, 0, sizeof userIf->port.ladrf);
   memset(userIf->port.exactFilter, 0, sizeof userIf->port.exactFilter);

   VNet_MakeMACAddress(&userIf->port);

   userIf->port.fileOpRead = VNetUserIfRead;
   userIf->port.fileOpWrite = VNetUserIfWrite;
   userIf->port.fileOpIoctl = VNetUserIfIoctl;
   userIf->port.fileOpPoll = VNetUserIfPoll;
   
   skb_queue_head_init(&(userIf->packetQueue));
   init_waitqueue_head(&userIf->waitQueue);

   memset(&userIf->stats, 0, sizeof userIf->stats);
   
   *ret = &userIf->port;
   return 0;
}

