/*********************************************************
 * Copyright (C) 1998-2017 VMware, Inc. All rights reserved.
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
 * hostif.c --
 *
 *    This file implements the platform-specific (here Linux) interface that
 *    the cross-platform code uses --hpreg
 *
 */


/* Must come before any kernel header file --hpreg */
#include "driver-config.h"

/* Must come before vmware.h --hpreg */
#include "compat_page.h"
#include "compat_timer.h"
#include <linux/binfmts.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <linux/kernel.h>

#include <linux/vmalloc.h>
#include <linux/slab.h>

#include <linux/preempt.h>
#include <linux/poll.h>
#include <linux/mman.h>

#include <linux/smp.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
#   include <asm/asm.h>
#endif
#if defined(_ASM_EXTABLE)
#   define VMW_ASM_EXTABLE(from, to) _ASM_EXTABLE(from, to)
#else
    /* Compat version copied from asm.h of 2.6.25 kernel */
#   define VMW_ASM_FORM(x)  " " #x " "
#   define VMW_ASM_EX_SEC   " .section __ex_table,\"a\"\n"
#   ifdef CONFIG_X86_32
#      define VMW_ASM_SEL(a,b) VMW_ASM_FORM(a)
#   else
#      define VMW_ASM_SEL(a,b) VMW_ASM_FORM(b)
#   endif
#   define VMW_ASM_PTR        VMW_ASM_SEL(.long, .quad)
#   define VMW_ASM_ALIGN      VMW_ASM_SEL(.balign 4, .balign 8)
#   define VMW_ASM_EXTABLE(from,to) \
           VMW_ASM_EX_SEC    \
           VMW_ASM_ALIGN "\n" \
           VMW_ASM_PTR #from "," #to "\n" \
           " .previous\n"
#endif

#include <asm/io.h>
#include <asm/page.h>
#include <asm/tlbflush.h>
#include <asm/uaccess.h>
#include <asm/irq_vectors.h>
#include <linux/mc146818rtc.h>
#include <linux/capability.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/signal.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/taskstats_kern.h> // For linux/sched/signal.h without version check
#endif

#include "vmware.h"
#include "x86apic.h"
#include "vm_asm.h"
#include "modulecall.h"
#include "driver.h"
#include "memtrack.h"
#include "phystrack.h"
#include "cpuid.h"
#include "cpuid_info.h"
#include "hostif.h"
#include "hostif_priv.h"
#include "vmhost.h"
#include "x86msr.h"
#include "apic.h"
#include "memDefaults.h"
#include "vcpuid.h"

#include "pgtbl.h"
#include "vmmonInt.h"
#include "versioned_atomic.h"
#include "compat_poll.h"
#include "compat_mmap_lock.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
#   define global_zone_page_state global_page_state
#endif

static unsigned long get_nr_slab_unreclaimable(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
   return global_node_page_state_pages(NR_SLAB_UNRECLAIMABLE_B);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
   return global_node_page_state(NR_SLAB_UNRECLAIMABLE);
#else
   return global_page_state(NR_SLAB_UNRECLAIMABLE);
#endif
}

static unsigned long get_nr_unevictable(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
   return global_node_page_state(NR_UNEVICTABLE);
#else
   return global_page_state(NR_UNEVICTABLE);
#endif
}

static unsigned long get_nr_anon_mapped(void)
{
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
   return global_node_page_state(NR_ANON_MAPPED);
 #else
   return global_page_state(NR_ANON_PAGES);
 #endif
}

/*
 * Determine if we can use high resolution timers.
 */

#ifdef CONFIG_HIGH_RES_TIMERS
#   include <linux/hrtimer.h>
#   define VMMON_USE_HIGH_RES_TIMERS
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28)
#      define VMMON_USE_SCHEDULE_HRTIMEOUT
#   else
#      define VMMON_USE_COMPAT_SCHEDULE_HRTIMEOUT
static void HostIFWakeupClockThread(unsigned long data);
static DECLARE_TASKLET(timerTasklet, HostIFWakeupClockThread, 0);
#   endif
#   define close_rtc(filp, files) do {} while(0)
#else
#   define close_rtc(filp, files) filp_close(filp, files)
#endif

#define UPTIME_FREQ CONST64(1000000)

/*
 * When CONFIG_NO_HZ_FULL is set processors can run tickless
 * if there is only one runnable process.  When set, the rate
 * checks in HostIF_SetFastClockRate and HostIFFastClockThread
 * need to be relaxed to allow any non-zero rate to run.
 *
 * This code can potentially be removed if/when we stop using
 * HostIFFastClockThread to drive MonTimer.  See PR1088247.
 */
#ifdef CONFIG_NO_HZ_FULL
#define MIN_RATE (0)
#else
#define MIN_RATE ((HZ) + (HZ) / 16)
#endif

/*
 * Linux seems to like keeping free memory around 30MB
 * even under severe memory pressure.  Let's give it a little
 * more leeway than that for safety.
 */
#define LOCKED_PAGE_SLACK 10000

u64 uptime_base;

/*
 * First Page Locking strategy
 * ---------------------------
 *
 * An early implementation hacked the lock bit for the purpose of locking
 * memory. This had a couple of advantages:
 *   - the vmscan algorithm would never eliminate mappings from the process
 *     address space
 *   - easy to assert that things are ok
 *   - it worked with anonymous memory. Basically, vmscan jumps over these
 *     pages, their use count stays high, ....
 *
 * This approach however had a couple of problems:
 *
 *   - it relies on an undocumented interface. (in another words, a total hack)
 *   - it creates deadlock situations if the application gets a kill -9 or
 *     otherwise dies ungracefully. linux first tears down the address space,
 *     then closes file descriptors (including our own device). Unfortunately,
 *     this leads to a deadlock of the process on pages with the lock bit set.
 *
 *     There is a workaround for that, namely to detect that condition using
 *     a linux timer. (ugly)
 *
 * Current Page Locking strategy
 * -----------------------------
 *
 * The current scheme does not use the lock bit, rather it increments the use
 * count on the pages that need to be locked down in memory.
 *
 * The problem is that experiments on certain linux systems (e.g. 2.2.0-pre9)
 * showed that linux somehow swaps out anonymous pages, even with the
 * increased ref counter.
 * Swapping them out to disk is not that big of a deal, but bringing them back
 * to a different location is.  In any case, anonymous pages in linux are not
 * intended to be write-shared (e.g. try to MAP_SHARED /dev/zero).
 *
 * As a result, the current locking strategy requires that all locked pages are
 * backed by the filesystem, not by swap. For now, we use both mapped files and
 * sys V shared memory. The user application is responsible to cover these
 * cases.
 *
 */


#define HOST_UNLOCK_PFN(_vm, _pfn) do {                  \
   _vm = _vm;                                            \
   put_page(pfn_to_page(_pfn));                          \
} while (0)

#define HOST_UNLOCK_PFN_BYMPN(_vm, _pfn) do {            \
   PhysTrack_Remove((_vm)->vmhost->lockedPages, (_pfn)); \
   put_page(pfn_to_page(_pfn));                          \
} while (0)

uint8 monitorIPIVector;
uint8 hvIPIVector;

static unsigned long compat_totalram_pages(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
	return totalram_pages;
#else
	return totalram_pages();
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0) && defined(VERIFY_WRITE)
	#define write_access_ok(addr, size) access_ok(VERIFY_WRITE, addr, size)
#else
	#define write_access_ok(addr, size) access_ok(addr, size)
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * MutexInit --
 *
 *      Initialize a Mutex. --hpreg
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

#ifdef VMX86_DEBUG
static INLINE void
MutexInit(Mutex *mutex,     // IN
          char const *name) // IN
{
   ASSERT(mutex);
   ASSERT(name);

   sema_init(&mutex->sem, 1);
   mutex->name = name;
   mutex->cur.pid = -1;
}
#else
#   define MutexInit(_mutex, _name) sema_init(&(_mutex)->sem, 1)
#endif


#ifdef VMX86_DEBUG
/*
 *-----------------------------------------------------------------------------
 *
 * MutexIsLocked --
 *
 *      Determine if a Mutex is locked by the current thread. --hpreg
 *
 * Results:
 *      TRUE if yes
 *      FALSE if no
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
MutexIsLocked(Mutex *mutex) // IN
{
   ASSERT(mutex);

   return mutex->cur.pid == current->pid;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * MutexLock --
 *
 *      Acquire a Mutex. --hpreg
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

#ifdef VMX86_DEBUG
static INLINE void
MutexLock(Mutex *mutex, // IN
          int callerID) // IN
{
   ASSERT(mutex);
   ASSERT(!MutexIsLocked(mutex));

   down(&mutex->sem);
   mutex->cur.pid = current->pid;
   mutex->cur.callerID = callerID;
}
#else
#   define MutexLock(_mutex, _callerID) down(&(_mutex)->sem)
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * MutexUnlock --
 *
 *      Release a Mutex. --hpreg
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

#ifdef VMX86_DEBUG
static INLINE void
MutexUnlock(Mutex *mutex, // IN
            int callerID) // IN
{
   ASSERT(mutex);

   ASSERT(MutexIsLocked(mutex) && mutex->cur.callerID == callerID);
   mutex->prev = mutex->cur;
   mutex->cur.pid = -1;
   up(&mutex->sem);
}
#else
#   define MutexUnlock(_mutex, _callerID) up(&(_mutex)->sem)
#endif


/* This mutex protects the driver-wide state. --hpreg */
static Mutex globalMutex;

/*
 * This mutex protects the fast clock rate and is held while
 * creating/destroying the fastClockThread.  It ranks below
 * globalMutex.  We can't use globalMutex for this purpose because the
 * fastClockThread itself acquires the globalMutex, so trying to hold
 * the mutex while destroying the thread can cause a deadlock.
 */
static Mutex fastClockMutex;

/* This mutex protects linuxState.pollList.  */
static Mutex pollListMutex;


/*
 *----------------------------------------------------------------------
 *
 * HostIF_PrepareWaitForThreads --
 *
 *      Prepare to wait for another vCPU thread.
 *
 * Results:
 *      FALSE: no way on Linux to determine we've already been signalled.
 *
 * Side effects:
 *      Current task is interruptible.
 *
 *----------------------------------------------------------------------
 */

Bool
HostIF_PrepareWaitForThreads(VMDriver *vm,     // IN:
                             Vcpuid currVcpu)  // IN:
{
   set_current_state(TASK_INTERRUPTIBLE);
   vm->vmhost->vcpuSemaTask[currVcpu] = current;
   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_WaitForThreads --
 *
 *      Wait for another vCPU thread.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Current task may block.
 *
 *----------------------------------------------------------------------
 */

void
HostIF_WaitForThreads(VMDriver *vm,     // UNUSED:
                      Vcpuid currVcpu)  // UNUSED:

{
#ifdef VMMON_USE_SCHEDULE_HRTIMEOUT
   ktime_t timeout = ktime_set(0, CROSSCALL_SLEEP_US * 1000);
   schedule_hrtimeout(&timeout, HRTIMER_MODE_REL);
#else
   /* Fallback to ms timer resolution is fine for older kernels. */
   schedule_timeout(msecs_to_jiffies(CROSSCALL_SLEEP_US / 1000) + 1);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_CancelWaitForThreads --
 *
 *      Cancel waiting for another vCPU thread.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Current task is running and no longer interruptible.
 *
 *----------------------------------------------------------------------
 */

void
HostIF_CancelWaitForThreads(VMDriver *vm,     // IN:
                            Vcpuid currVcpu)  // IN:
{
   vm->vmhost->vcpuSemaTask[currVcpu] = NULL;
   set_current_state(TASK_RUNNING);
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_WakeUpYielders --
 *
 *      Wakeup vCPUs that are waiting for the current vCPU.
 *      
 * Results:
 *      The requested vCPUs are nudged if they are sleeping due to
 *      Vmx86_YieldToSet.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
HostIF_WakeUpYielders(VMDriver *vm,     // IN:
                      Vcpuid currVcpu)  // IN:
{
   VCPUSet req;
   Vcpuid vcpuid;
   uint64 subset;

   /*
    * PR 1142958: if the VCPUs woken in the crosscallWaitSet re-add themselves
    * to this set faster than it can be fully drained, this function never
    * exits.  Instead, we copy and remove a snapshot of the crosscallWaitSet
    * and locally wake up just that snapshot.  It is ok that we don't get a
    * fully coherent snapshot, as long as the subset copy-and-remove is atomic
    * so no VCPU added is lost entirely.
    */

   VCPUSet_Empty(&req);
   FOR_EACH_SUBSET_IN_SET(subIdx) {
      subset = VCPUSet_AtomicReadWriteSubset(&vm->crosscallWaitSet[currVcpu],
                                             0, subIdx);
      VCPUSet_UnionSubset(&req, subset, subIdx);
   } ROF_EACH_SUBSET_IN_SET();

   preempt_disable();
   while ((vcpuid = VCPUSet_FindFirst(&req)) != VCPUID_INVALID) {
      struct task_struct *t = vm->vmhost->vcpuSemaTask[vcpuid];
      VCPUSet_Remove(&req, vcpuid);
      if (t && (t->state & TASK_INTERRUPTIBLE)) {
         wake_up_process(t);
      }
   }
   preempt_enable();
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_InitGlobalLock --
 *
 *      Initialize the global (across all VMs and vmmon) locks.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
HostIF_InitGlobalLock(void)
{
   MutexInit(&globalMutex, "global");
   MutexInit(&fastClockMutex, "fastClock");
   MutexInit(&pollListMutex, "pollList");
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_GlobalLock --
 *
 *      Grabs the global data structure lock.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Should be a very low contention lock. 
 *      The current thread is rescheduled if the lock is busy.
 *
 *-----------------------------------------------------------------------------
 */

void
HostIF_GlobalLock(int callerID) // IN
{
   MutexLock(&globalMutex, callerID);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_GlobalUnlock --
 *
 *      Releases the global data structure lock.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
HostIF_GlobalUnlock(int callerID) // IN
{
   MutexUnlock(&globalMutex, callerID);
}


#ifdef VMX86_DEBUG
/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_GlobalLockIsHeld --
 *
 *      Determine if the global lock is held by the current thread.
 * 
 * Results:
 *      TRUE if yes
 *      FALSE if no
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HostIF_GlobalLockIsHeld(void)
{
   return MutexIsLocked(&globalMutex);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_FastClockLock --
 *
 *      Grabs the fast clock data structure lock.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Should be a very low contention lock. 
 *      The current thread is rescheduled if the lock is busy.
 *
 *-----------------------------------------------------------------------------
 */

void
HostIF_FastClockLock(int callerID) // IN
{
   MutexLock(&fastClockMutex, callerID);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_FastClockUnlock --
 *
 *      Releases the fast clock data structure lock.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
HostIF_FastClockUnlock(int callerID) // IN
{
   MutexUnlock(&fastClockMutex, callerID);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_PollListLock --
 *
 *      Grabs the linuxState.pollList lock.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The current thread is rescheduled if the lock is busy.
 *
 *-----------------------------------------------------------------------------
 */

void
HostIF_PollListLock(int callerID) // IN
{
   MutexLock(&pollListMutex, callerID);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_PollListUnlock --
 *
 *      Releases the linuxState.pollList lock.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
HostIF_PollListUnlock(int callerID) // IN
{
   MutexUnlock(&pollListMutex, callerID);
}


/*
 *----------------------------------------------------------------------
 *
 * MapCrossPage & UnmapCrossPage
 *
 *    Both x86-64 and ia32 need to map crosspage to an executable
 *    virtual address. We use the vmap interface instead of kmap
 *    due to bug 43907.
 *
 * Side effects:
 *
 *    UnmapCrossPage assumes that the page has been refcounted up
 *    so it takes care of the put_page.
 *
 *----------------------------------------------------------------------
 */
static void *
MapCrossPage(struct page *p)  // IN:
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
   return vmap(&p, 1, VM_MAP, VM_PAGE_KERNEL_EXEC);
#else
   /* Starting with 5.8, vmap() always sets the NX bit, but the cross
    * page needs to be executable. */
   pte_t *ptes[1];
   struct vm_struct *area = alloc_vm_area(1UL << PAGE_SHIFT, ptes);
   if (area == NULL)
      return NULL;

   set_pte(ptes[0], mk_pte(p, VM_PAGE_KERNEL_EXEC));

   preempt_disable();
   __flush_tlb_all();
   preempt_enable();

   return area->addr;
#endif
}


static void
UnmapCrossPage(struct page *p,  // IN:
               void *va)        // IN:
{
   vunmap(va);
   put_page(p);
}


/*
 *----------------------------------------------------------------------
 *
 * HostIFHostMemInit --
 *
 *      Initialize per-VM pages lists.
 *
 * Results:
 *      0 on success,
 *      non-zero on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
HostIFHostMemInit(VMDriver *vm)  // IN:
{
   VMHost *vmh = vm->vmhost;
   
   vmh->lockedPages = PhysTrack_Alloc(vm);
   if (!vmh->lockedPages) {
      return -1;
   }
   vmh->AWEPages = PhysTrack_Alloc(vm);
   if (!vmh->AWEPages) {
      return -1;
   }

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIFHostMemCleanup --
 *
 *      Release per-VM pages lists.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Locked and AWE pages are released.
 *
 *----------------------------------------------------------------------
 */

static void
HostIFHostMemCleanup(VMDriver *vm)  // IN:
{
   MPN mpn;
   VMHost *vmh = vm->vmhost;

   if (!vmh) {
      return;
   }

   HostIF_VMLock(vm, 32); // Debug version of PhysTrack wants VM's lock.
   if (vmh->lockedPages) {
      for (mpn = 0;
           INVALID_MPN != (mpn = PhysTrack_GetNext(vmh->lockedPages, mpn));) {
         HOST_UNLOCK_PFN_BYMPN(vm, mpn);
      }
      PhysTrack_Free(vmh->lockedPages);
      vmh->lockedPages = NULL;
   }

   if (vmh->AWEPages) {
      for (mpn = 0;
           INVALID_MPN != (mpn = PhysTrack_GetNext(vmh->AWEPages, mpn));) {
         PhysTrack_Remove(vmh->AWEPages, mpn);
         put_page(pfn_to_page(mpn));
      }
      PhysTrack_Free(vmh->AWEPages);
      vmh->AWEPages = NULL;
   }
   HostIF_VMUnlock(vm, 32);
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_AllocMachinePage --
 *
 *      Alloc non-swappable memory page. The page is not billed to
 *      a particular VM. Preferably the page should not be mapped into
 *      the kernel addresss space.
 *
 * Results:
 *      INVALID_MPN or a valid host mpn.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

MPN
HostIF_AllocMachinePage(void)
{
  struct page *pg = alloc_page(GFP_HIGHUSER);

  return (pg) ? ((MPN)page_to_pfn(pg)) : INVALID_MPN;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_FreeMachinePage --
 *
 *      Free an anonymous machine page allocated by 
 *      HostIF_AllocMachinePage().  This page is not tracked in any 
 *      phystracker.
 *
 * Results:
 *      Host page is unlocked.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
HostIF_FreeMachinePage(MPN mpn)  // IN:
{
  struct page *pg = pfn_to_page(mpn);

  __free_page(pg);
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_AllocLockedPages --
 *
 *      Alloc non-swappable memory.
 *
 * Results:
 *      negative value on complete failure
 *      non-negative value on partial/full completion, number of MPNs
 *          allocated & filled in pmpn returned.
 *
 * Side effects:
 *      Pages allocated.
 *
 *----------------------------------------------------------------------
 */

int
HostIF_AllocLockedPages(VMDriver *vm,	     // IN: VM instance pointer
			VA64 addr,	     // OUT: pointer to user or kernel buffer for MPNs
			unsigned numPages,   // IN: number of pages to allocate
			Bool kernelMPNBuffer)// IN: is the MPN buffer in kernel or user address space?
{
   MPN *pmpn = VA64ToPtr(addr);

   VMHost *vmh = vm->vmhost;
   unsigned int cnt;
   int err = 0;

   if (!vmh || !vmh->AWEPages) {
      return -EINVAL;
   }
   for (cnt = 0; cnt < numPages; cnt++) {
      struct page* pg;
      MPN mpn;

      pg = alloc_page(GFP_HIGHUSER);
      if (!pg) {
         err = -ENOMEM;
	 break;
      }
      mpn = (MPN)page_to_pfn(pg);
      if (kernelMPNBuffer) {
         *pmpn = mpn;
      } else if (HostIF_CopyToUser(pmpn, &mpn, sizeof *pmpn) != 0) {
	__free_page(pg);
	err = -EFAULT;
	break;
      }
      pmpn++;
      if (PhysTrack_Test(vmh->AWEPages, mpn)) {
	Warning("%s: duplicate MPN %016" FMT64 "x\n", __func__, mpn);
      }
      PhysTrack_Add(vmh->AWEPages, mpn);
   }

   return cnt ? cnt : err;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_FreeLockedPages --
 *
 *      Free non-swappable memory.
 *
 * Results:
 *      On success: 0. All pages were unlocked.
 *      On failure: Non-zero system error code. No page was unlocked.
 *
 * Side effects:
 *      Pages freed.
 *
 *----------------------------------------------------------------------
 */

int
HostIF_FreeLockedPages(VMDriver *vm,	     // IN: VM instance pointer
		       VA64 addr,            // IN: user or kernel array of MPNs
		       unsigned numPages,    // IN: number of pages to free
		       Bool kernelMPNBuffer) // IN: is the MPN buffer in kernel or user address space?
{
   const int MPN_BATCH = 64;
   MPN const *pmpn = VA64ToPtr(addr);
   VMHost *vmh = vm->vmhost;
   unsigned int cnt;
   struct page *pg;
   MPN *mpns;

   mpns = HostIF_AllocKernelMem(sizeof *mpns * MPN_BATCH, TRUE);

   if (mpns == NULL) {
      return -ENOMEM;
   }
   if (!vmh || !vmh->AWEPages) {
      HostIF_FreeKernelMem(mpns);
      return -EINVAL;
   }

   if (!kernelMPNBuffer) {
      if (numPages > MPN_BATCH) {
         HostIF_FreeKernelMem(mpns);
         return -EINVAL;
      }

      if (HostIF_CopyFromUser(mpns, pmpn, numPages * sizeof *pmpn)) {
         printk(KERN_DEBUG "Cannot read from process address space at %p\n",
                pmpn);
         HostIF_FreeKernelMem(mpns);
         return -EINVAL;
      }

      pmpn = mpns;
   }

   for (cnt = 0; cnt < numPages; cnt++) {
      if (!PhysTrack_Test(vmh->AWEPages, pmpn[cnt])) {
         printk(KERN_DEBUG "Attempted to free unallocated MPN %016" FMT64 "X\n",
                pmpn[cnt]);
         HostIF_FreeKernelMem(mpns);
         return -EINVAL;
      }

      pg = pfn_to_page(pmpn[cnt]);
      if (page_count(pg) != 1) {
         // should this case be considered a failure?
         printk(KERN_DEBUG "Page %016" FMT64 "X is still used by someone "
                "(use count %u, VM %p)\n", pmpn[cnt],
                 page_count(pg), vm);
      }
   }

   for (cnt = 0; cnt < numPages; cnt++) {
      pg = pfn_to_page(pmpn[cnt]);
      PhysTrack_Remove(vmh->AWEPages, pmpn[cnt]);
      __free_page(pg);
   }
   HostIF_FreeKernelMem(mpns);
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_Init --
 *
 *      Initialize the host-dependent part of the driver.
 *
 * Results:
 *     zero on success, non-zero on error.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

int
HostIF_Init(VMDriver *vm)  // IN:
{
   vm->memtracker = MemTrack_Init(vm);
   if (vm->memtracker == NULL) {
      return -1;
   }

   vm->vmhost = (VMHost *) HostIF_AllocKernelMem(sizeof *vm->vmhost, TRUE);
   if (vm->vmhost == NULL) {
      return -1;
   }
   memset(vm->vmhost, 0, sizeof *vm->vmhost);

   if (HostIFHostMemInit(vm)) {
      return -1;
   }
   MutexInit(&vm->vmhost->vmMutex, "vm");

   return 0;
}


/*
 *------------------------------------------------------------------------------
 *
 * HostIF_LookupUserMPN --
 *
 *      Lookup the MPN of a locked user page by user VA.
 *
 * Results:
 *      A status code and the MPN on success.
 *
 * Side effects:
 *     None
 *
 *------------------------------------------------------------------------------
 */

int
HostIF_LookupUserMPN(VMDriver *vm, // IN: VMDriver
                     VA64 uAddr,   // IN: user VA of the page
                     MPN *mpn)     // OUT
{
   void *uvAddr = VA64ToPtr(uAddr);
   int retval = PAGE_LOCK_SUCCESS;

   *mpn = PgtblVa2MPN((VA)uvAddr);

   /*
    * On failure, check whether the page is locked.
    *
    * While we don't require the page to be locked by HostIF_LockPage(),
    * it does provide extra information.
    *
    * -- edward
    */
   if (*mpn == INVALID_MPN) {
      if (vm == NULL) {
         retval += PAGE_LOOKUP_NO_VM;
      } else {
         MemTrackEntry *entryPtr =
            MemTrack_LookupVPN(vm->memtracker, PTR_2_VPN(uvAddr));
         if (entryPtr == NULL) {
            retval += PAGE_LOOKUP_NOT_TRACKED;
         } else if (entryPtr->mpn == 0) {
            retval += PAGE_LOOKUP_NO_MPN;
         } else {
            /*
             * Kernel can remove PTEs/PDEs from our pagetables even if pages
             * are locked...
             */
            volatile int c;

            get_user(c, (char *)uvAddr);
            *mpn = PgtblVa2MPN((VA)uvAddr);
            if (*mpn == entryPtr->mpn) {
#ifdef VMX86_DEBUG
               printk(KERN_DEBUG "Page %p disappeared from %s(%u)... "
                      "now back at %016" FMT64 "x\n",
                      uvAddr, current->comm, current->pid, *mpn);
#endif
            } else if (*mpn != INVALID_MPN) {
               printk(KERN_DEBUG "Page %p disappeared from %s(%u)... "
                      "now back at %016" FMT64"x (old=%016" FMT64 "x)\n",
                      uvAddr, current->comm, current->pid, *mpn,
                      entryPtr->mpn);
               *mpn = INVALID_MPN;
            } else {
               printk(KERN_DEBUG "Page %p disappeared from %s(%u)... "
                      "and is lost (old=%016" FMT64 "x)\n", uvAddr, current->comm,
                      current->pid, entryPtr->mpn);
               *mpn = entryPtr->mpn;
            }
         }
      }
   }

   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_InitFP --
 *
 *      masks IRQ13 if not previously the case.
 *
 * Results:
 *      prevents INTR #0x2d (IRQ 13) from being generated --
 *      assume that Int16 works for interrupt reporting
 *      
 *
 * Side effects:
 *      PIC
 *
 *----------------------------------------------------------------------
 */

void
HostIF_InitFP(VMDriver *vm)  // IN:
{
   int mask = (1 << (0xD - 0x8));

   uint8 val = inb(0xA1);

   if (!(val & mask)) { 
      val = val | mask;
      outb(val, 0xA1);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIFGetUserPages --
 *
 *      Lock the pages of an user-level address space in memory.
 *      If ppages is NULL, pages are only marked as dirty.
 *
 * Results:
 *      Zero on success, non-zero on failure. 
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static int
HostIFGetUserPages(void *uvAddr,          // IN
                   struct page **ppages,  // OUT
                   unsigned int numPages) // IN
{
   int retval;

   mmap_read_lock(current->mm);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
   retval = get_user_pages((unsigned long)uvAddr, numPages, 0, ppages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
   retval = get_user_pages((unsigned long)uvAddr, numPages, 0, 0, ppages, NULL);
#else
   retval = get_user_pages(current, current->mm, (unsigned long)uvAddr,
                           numPages, 0, 0, ppages, NULL);
#endif
   mmap_read_unlock(current->mm);

   return retval != numPages;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_IsLockedByMPN --
 *
 *      Checks if mpn was locked using allowMultipleMPNsPerVA.  
 *
 * Results:
 *      TRUE if mpn is present in the physTracker.
 *      
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

Bool
HostIF_IsLockedByMPN(VMDriver *vm,  // IN:
                     MPN mpn)       // IN:
{
  return PhysTrack_Test(vm->vmhost->lockedPages, mpn);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_LockPage --
 *
 *     Lockup the MPN of an pinned user-level address space
 *
 * Results:
 *     A PAGE_LOCK_* status code and the MPN on success.
 *
 * Side effects:
 *      Adds the page to the MemTracker, if allowMultipleMPNsPerVA then the page
 *      is added to the VM's PhysTracker.
 *
 *-----------------------------------------------------------------------------
 */

int
HostIF_LockPage(VMDriver *vm,                // IN: VMDriver
                VA64 uAddr,                  // IN: user VA of the page
                Bool allowMultipleMPNsPerVA, // IN: allow to lock many pages per VA
                MPN *mpn)                    // OUT: pinned page
{
   void *uvAddr = VA64ToPtr(uAddr);
   struct page *page;
   VPN vpn;
   MemTrackEntry *entryPtr = NULL;

   vpn = PTR_2_VPN(uvAddr);
   if (!allowMultipleMPNsPerVA) {
      entryPtr = MemTrack_LookupVPN(vm->memtracker, vpn);

      /*
       * Already tracked and locked
       */

      if (entryPtr != NULL && entryPtr->mpn != 0) {
         return PAGE_LOCK_ALREADY_LOCKED;
      }
   }

   if (HostIFGetUserPages(uvAddr, &page, 1)) {
      return PAGE_LOCK_FAILED;
   }

   *mpn = (MPN)page_to_pfn(page);

   if (allowMultipleMPNsPerVA) {
      /*
       *  Add the MPN to the PhysTracker that tracks locked pages.
       */

      struct PhysTracker* const pt = vm->vmhost->lockedPages;

      if (PhysTrack_Test(pt, *mpn)) {
         put_page(page);
         return PAGE_LOCK_ALREADY_LOCKED;
      }
      PhysTrack_Add(pt, *mpn);
   } else {
      /*
       * If the entry doesn't exist, add it to the memtracker
       * otherwise we just update the mpn.
       */

      if (entryPtr == NULL) {
         entryPtr = MemTrack_Add(vm->memtracker, vpn, *mpn);
         if (entryPtr == NULL) {
            HOST_UNLOCK_PFN(vm, *mpn);
            return PAGE_LOCK_MEMTRACKER_ERROR;
         }
      } else {
         entryPtr->mpn = *mpn;
      }
   }

   return PAGE_LOCK_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_UnlockPage --
 *
 *      Unlock an pinned user-level page.
 *
 * Results:
 *      Status PAGE_UNLOCK_* code.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

int
HostIF_UnlockPage(VMDriver *vm,  // IN:
                  VA64 uAddr)    // IN:
{
   void *addr = VA64ToPtr(uAddr);
   VPN vpn;
   MemTrackEntry *e;

   vpn = VA_2_VPN((VA)addr);
   e = MemTrack_LookupVPN(vm->memtracker, vpn);
    
   if (e == NULL) {
      return PAGE_UNLOCK_NOT_TRACKED;
   }
   if (e->mpn == 0) {
      return PAGE_UNLOCK_NO_MPN;
   }

   HOST_UNLOCK_PFN(vm, e->mpn);
   e->mpn = 0;

   return PAGE_UNLOCK_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_UnlockPageByMPN --
 *
 *      Unlock a locked user mode page. The page doesn't need to be mapped
 *      anywhere.
 *
 * Results:
 *      Status code. Returns a PAGE_LOOKUP_* error if the page can't be found or
 *      a PAGE_UNLOCK_* error if the page can't be unlocked.
 *
 * Side effects:
 *     Removes the MPN from from VM's PhysTracker.
 *
 *----------------------------------------------------------------------
 */

int
HostIF_UnlockPageByMPN(VMDriver *vm, // IN: VMDriver
                       MPN mpn,      // IN: the MPN to unlock
                       VA64 uAddr)   // IN: optional(debugging) VA for the MPN
{
   if (!PhysTrack_Test(vm->vmhost->lockedPages, mpn)) {
      return PAGE_UNLOCK_NO_MPN;
   }

#ifdef VMX86_DEBUG
   {
      void *va = VA64ToPtr(uAddr);
      MemTrackEntry *e;
      
      /*
       * Verify for debugging that VA and MPN make sense.
       * PgtblVa2MPN() can fail under high memory pressure.
       */

      if (va != NULL) {
         MPN lookupMpn = PgtblVa2MPN((VA)va);

         if (lookupMpn != INVALID_MPN && mpn != lookupMpn) {
            Warning("Page lookup fail %#"FMT64"x %016" FMT64 "x %p\n",
                    mpn, lookupMpn, va);

            return PAGE_LOOKUP_INVALID_ADDR;
         }
      }

      /*
       * Verify that this MPN was locked with 
       * HostIF_LockPage(allowMultipleMPNsPerVA = TRUE).
       * That means that this MPN should not be in the MemTracker.
       */

      e = MemTrack_LookupMPN(vm->memtracker, mpn);
      if (e) {
         Warning("%s(): mpn=%#"FMT64"x va=%p was permanently locked with "
                 "vpn=0x%"FMT64"x\n", __func__, mpn, va, e->vpn);

         return PAGE_UNLOCK_MISMATCHED_TYPE;
      }
   }
#endif 

   HOST_UNLOCK_PFN_BYMPN(vm, mpn);

   return PAGE_UNLOCK_SUCCESS;
}


static void 
UnlockEntry(void *clientData,         // IN:
            MemTrackEntry *entryPtr)  // IN:
{
   VMDriver *vm = (VMDriver *)clientData;

   if (entryPtr->mpn) {
      HOST_UNLOCK_PFN(vm,entryPtr->mpn);
      entryPtr->mpn = 0;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_FreeAllResources --
 *
 *      Free all host-specific VM resources.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
HostIF_FreeAllResources(VMDriver *vm) // IN
{
   unsigned int cnt;

   HostIFHostMemCleanup(vm);
   if (vm->memtracker) {
      MemTrack_Cleanup(vm->memtracker, UnlockEntry, vm);
      vm->memtracker = NULL;
   }
   if (vm->vmhost) {
      for (cnt = vm->vmhost->crosspagePagesCount; cnt > 0; ) {
         struct page* p = vm->vmhost->crosspagePages[--cnt];
         UnmapCrossPage(p, vm->crosspage[cnt]);
      }
      vm->vmhost->crosspagePagesCount = 0;
      if (vm->vmhost->hostAPICIsMapped) {
	 ASSERT(vm->hostAPIC.base != NULL);
	 iounmap((void*)vm->hostAPIC.base);
	 vm->hostAPIC.base = NULL;
	 vm->vmhost->hostAPICIsMapped = FALSE;
      }
      HostIF_FreeKernelMem(vm->vmhost);
      vm->vmhost = NULL;
   }
}



/*
 *----------------------------------------------------------------------
 *
 * HostIF_AllocKernelMem
 *
 *      Allocate some kernel memory for the driver. 
 *
 * Results:
 *      The address allocated or NULL on error. 
 *      
 *
 * Side effects:
 *      memory is malloced
 *----------------------------------------------------------------------
 */

void *
HostIF_AllocKernelMem(size_t size,  // IN:
                      int wired)    // IN:
{
   void * ptr = kmalloc(size, GFP_KERNEL);
   
   if (ptr == NULL) { 
      Warning("%s failed (size=%p)\n", __func__, (void*)size);
   }

   return ptr;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_AllocPage --
 *
 *    Allocate a page (whose content is undetermined)
 *
 * Results:
 *    The kernel virtual address of the page
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void *
HostIF_AllocPage(void)
{
   VA kvAddr;
   
   kvAddr = __get_free_page(GFP_KERNEL);
   if (kvAddr == 0) {
      Warning("%s: __get_free_page() failed\n", __func__);
   }

   return (void *)kvAddr;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_FreeKernelMem
 *
 *      Free kernel memory allocated for the driver. 
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      memory is freed.
 *----------------------------------------------------------------------
 */

void
HostIF_FreeKernelMem(void *ptr)  // IN:
{
   kfree(ptr);
}


void
HostIF_FreePage(void *ptr)  // IN:
{
   VA vAddr = (VA)ptr;

   if (vAddr & (PAGE_SIZE-1)) {
      Warning("%s %p misaligned\n", __func__, (void*)vAddr);
   } else {
      free_page(vAddr);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_EstimateLockedPageLimit --
 *
 *      Estimates how many memory pages can be locked or allocated
 *      from the kernel without causing the host to die or to be really upset.
 *
 * Results:
 *	The maximum number of pages that can be locked. 
 *
 * Side effects:
 *      none
 *
 *----------------------------------------------------------------------
 */

unsigned int
HostIF_EstimateLockedPageLimit(const VMDriver* vm,                // IN
			       unsigned int currentlyLockedPages) // IN
{
   unsigned int totalPhysicalPages = compat_totalram_pages();

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
   return MemDefaults_CalcMaxLockedPages(totalPhysicalPages);
#else
   /*
    * Use the memory information linux exports as of late for a more
    * precise estimate of locked memory.  All kernel page-related structures
    * (slab, pagetable) are as good as locked.  Unevictable includes things
    * that are explicitly marked as such (like mlock()).  Huge pages are 
    * also as good as locked, since we don't use them.  Lastly, without 
    * available swap, anonymous pages become locked in memory as well. 
    */

   unsigned int forHost;
   unsigned int reservedPages = MEMDEFAULTS_MIN_HOST_PAGES;
   unsigned int hugePages = (vm == NULL) ? 0 :
      BYTES_2_PAGES(vm->memInfo.hugePageBytes);
   unsigned int lockedPages = global_zone_page_state(NR_PAGETABLE) +
                              get_nr_slab_unreclaimable() +
                              get_nr_unevictable() +
                              hugePages + reservedPages;
   unsigned int anonPages = get_nr_anon_mapped();
   unsigned int swapPages = BYTES_2_PAGES(linuxState.swapSize);

   if (anonPages > swapPages) {
      lockedPages += anonPages - swapPages; 
   }
   forHost = lockedPages + LOCKED_PAGE_SLACK;
   if (forHost > totalPhysicalPages) {
      forHost = totalPhysicalPages;
   }

   return totalPhysicalPages - forHost;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_Wait --
 *
 *      Waits for specified number of milliseconds.
 *
 *----------------------------------------------------------------------
 */

void
HostIF_Wait(unsigned int timeoutMs)
{
   msleep_interruptible(timeoutMs);
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_WaitForFreePages --
 *
 *      Waits for pages to be available for allocation or locking.
 *
 * Results:
 *	New pages are likely to be available for allocation or locking.
 *
 * Side effects:
 *      none
 *
 *----------------------------------------------------------------------
 */

void 
HostIF_WaitForFreePages(unsigned int timeoutMs)  // IN:
{
   static unsigned count;
   msleep_interruptible(timeoutMs);
   count++;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_InitUptime --
 *
 *      Initialize the uptime clock's state.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Sets the initial value for the uptime base.
 *
 *----------------------------------------------------------------------
 */

void
HostIF_InitUptime(void)
{
   uptime_base = ktime_get_ns();
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_CleanupUptime --
 *
 *      No-op, left for backward compatibility.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

void
HostIF_CleanupUptime(void)
{
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_ReadUptime --
 *
 *      Read the system time.  Returned value has no particular absolute
 *      value, only difference since previous call should be used.
 *
 * Results:
 *      Units are given by HostIF_UptimeFrequency.
 *
 * Side effects:
 *      See HostIFReadUptimeWork
 *
 *----------------------------------------------------------------------
 */

uint64
HostIF_ReadUptime(void)
{
   u64 tm;

   tm = ktime_get_ns();
   return (tm - uptime_base) / (NSEC_PER_SEC / UPTIME_FREQ);
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_UptimeFrequency
 *
 *      Return the frequency of the counter that HostIF_ReadUptime reads.
 *
 * Results:
 *      Frequency in Hz.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

uint64
HostIF_UptimeFrequency(void)
{
   return UPTIME_FREQ;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_CopyFromUser --
 *
 *      Copy memory from the user application into a kernel buffer. This
 *      function may block, so don't call it while holding any kind of
 *      lock. --hpreg
 *
 * Results:
 *      0 on success
 *      -EFAULT on failure.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
HostIF_CopyFromUser(void *dst,        // OUT
                    const void *src,  // IN
                    unsigned int len) // IN
{
   return copy_from_user(dst, src, len) ? -EFAULT : 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_CopyToUser --
 *
 *      Copy memory to the user application from a kernel buffer. This
 *      function may block, so don't call it while holding any kind of
 *      lock. --hpreg
 *
 * Results:
 *      0 on success
 *      -EFAULT on failure.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int 
HostIF_CopyToUser(void *dst,        // OUT
                  const void *src,  // IN
                  unsigned int len) // IN
{
   return copy_to_user(dst, src, len) ? -EFAULT : 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_MapCrossPage --
 *    
 *    Obtain kernel pointer to crosspage. 
 *
 *    We must return a VA that is obtained through a kernel mapping, so that 
 *    the mapping never goes away (see bug 29753).
 *
 *    However, the LA corresponding to that VA must not overlap with the 
 *    monitor (see bug 32922). The userland code ensures that by only 
 *    allocating cross pages from low memory. For those pages, the kernel 
 *    uses a permanent mapping, instead of a temporary one with a high LA.
 *
 * Results:
 *    The kernel virtual address on success
 *    NULL on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void *
HostIF_MapCrossPage(VMDriver *vm, // IN
                    VA64 uAddr)   // IN
{
   void *p = VA64ToPtr(uAddr);
   struct page *page;
   VA           vPgAddr;
   VA           ret;

   if (HostIFGetUserPages(p, &page, 1)) {
      return NULL;
   }
   vPgAddr = (VA) MapCrossPage(page);
   HostIF_VMLock(vm, 27);
   if (vm->vmhost->crosspagePagesCount >= MAX_INITBLOCK_CPUS) {
      HostIF_VMUnlock(vm, 27);
      UnmapCrossPage(page, (void*)vPgAddr);

      return NULL;
   }
   vm->vmhost->crosspagePages[vm->vmhost->crosspagePagesCount++] = page;
   HostIF_VMUnlock(vm, 27);

   ret = vPgAddr | (((VA)p) & (PAGE_SIZE - 1));

   return (void*)ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_AllocCrossGDT --
 *
 *      Allocate the per-vmmon cross GDT page set.
 *
 *      See bora/doc/worldswitch-pages.txt for the requirements on the cross
 *      GDT page set addresses.
 *
 * Results:
 *      On success: Host kernel virtual address of the first cross GDT page.
 *                  Use HostIF_FreeCrossGDT() with the same value to free.
 *                  The 'crossGDTMPNs' array is filled with the MPNs of all the
 *                  cross GDT pages.
 *      On failure: NULL.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void *
HostIF_AllocCrossGDT(uint32 numPages,     // IN: Number of pages
                     MPN maxValidFirst,   // IN: Highest valid MPN of first page
                     MPN *crossGDTMPNs)   // OUT: Array of MPNs
{
   MPN startMPN;
   struct page *pages;
   uint32 i;
   void *crossGDT;

   /*
    * In practice, allocating a low page (MPN <= 0x100000 - 1) is equivalent to
    * allocating a page with MPN <= 0xFEC00 - 1:
    *
    * o PC architecture guarantees that there is no RAM in top 16MB of 4GB
    *   range.
    *
    * o 0xFEC00000 is IOAPIC base.  There could be RAM immediately below,
    *   but not above.
    *
    * How do we allocate a low page? We can safely use GFP_DMA32 when
    * available.  On 64bit kernels before GFP_DMA32 was introduced we
    * fall back to DMA zone (which is not quite necessary for boxes
    * with less than ~3GB of memory).  On 32bit kernels we are using
    * normal zone - which is usually 1GB, and at most 4GB (for 4GB/4GB
    * kernels).  And for 4GB/4GB kernels same restriction as for 64bit
    * kernels applies - there is no RAM in top 16MB immediately below
    * 4GB so alloc_pages() cannot return such page.
    */

   ASSERT(0xFEC00 - 1 <= maxValidFirst);
   for (i = 0; (1 << i) < numPages; i++) { }
#ifdef GFP_DMA32
   pages = alloc_pages(GFP_KERNEL | GFP_DMA32, i);
#else
   pages = alloc_pages(GFP_KERNEL | GFP_DMA, i);
#endif
   crossGDT = NULL;
   if (pages == NULL) {
      Warning("%s: unable to alloc crossGDT (%u)\n", __func__, i);
   } else {
      startMPN = page_to_pfn(pages);
      for (i = 0; i < numPages; i++) {
         crossGDTMPNs[i] = startMPN + i;
      }
      crossGDT = (void *)page_address(pages);
   }

   return crossGDT;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_FreeCrossGDT --
 *
 *      Free the per-vmmon cross GDT page set allocated with
 *      HostIF_AllocCrossGDT().
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
HostIF_FreeCrossGDT(uint32 numPages, // IN: Number of pages
                    void *crossGDT)  // IN: Kernel VA of first cross GDT page
{
   uint32 i;

   for (i = 0; (1 << i) < numPages; i++) { }
   free_pages((VA)crossGDT, i);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_VMLock --
 *
 *      Grabs per-VM data structure lock. The lock is not recursive.
 *      The global lock has lower rank so the global lock should be grabbed
 *      first if both locks are acquired.
 *
 *      It should be a medium contention lock. Also it should be fast:
 *      it is used for protecting of frequent page allocation and locking.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The current thread is rescheduled if the lock is busy.
 *
 *-----------------------------------------------------------------------------
 */

void
HostIF_VMLock(VMDriver *vm, // IN
              int callerID) // IN
{
   ASSERT(vm);

   ASSERT(vm->vmhost);
   MutexLock(&vm->vmhost->vmMutex, callerID);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_VMUnlock --
 *
 *      Releases per-VM data structure lock.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Can wake up the thread blocked on this lock. 
 *
 *-----------------------------------------------------------------------------
 */

void
HostIF_VMUnlock(VMDriver *vm, // IN
                int callerID) // IN
{
   ASSERT(vm);

   ASSERT(vm->vmhost);
   MutexUnlock(&vm->vmhost->vmMutex, callerID);
}


#ifdef VMX86_DEBUG
/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_VMLockIsHeld --
 *
 *      Determine if the per-VM lock is held by the current thread.
 * 
 * Results:
 *      TRUE if yes
 *      FALSE if no
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HostIF_VMLockIsHeld(VMDriver *vm) // IN
{
   ASSERT(vm);
   ASSERT(vm->vmhost);

   return MutexIsLocked(&vm->vmhost->vmMutex);
}
#endif


/*
 * Utility routines for accessing and enabling the APIC
 */

/*
 * Defines for accessing the APIC.  We use readl/writel to access the APIC
 * which is how Linux wants you to access I/O memory (though on the x86
 * just dereferencing a pointer works just fine).
 */
#define APICR_TO_ADDR(apic, reg)      (apic + (reg << 4))
#define GET_APIC_REG(apic, reg)       (readl(APICR_TO_ADDR(apic, reg)))
#define SET_APIC_REG(apic, reg, val)  (writel(val, APICR_TO_ADDR(apic, reg)))

#define APIC_MAXLVT(apic)             ((GET_APIC_REG(apic, APICR_VERSION) >> 16) & 0xff)
#define APIC_VERSIONREG(apic)         (GET_APIC_REG(apic, APICR_VERSION) & 0xff)


#if defined(CONFIG_SMP) || defined(CONFIG_X86_UP_IOAPIC) || \
    defined(CONFIG_X86_UP_APIC) || defined(CONFIG_X86_LOCAL_APIC)
/*
 *----------------------------------------------------------------------
 *
 * isVAReadable --
 *
 *      Verify that passed VA is accessible without crash...
 *
 * Results:
 *      TRUE if address is readable, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
 
static Bool
isVAReadable(VA r)  // IN:
{
   mm_segment_t old_fs;
   uint32 dummy;
   int ret;
   
   old_fs = get_fs();
   set_fs(KERNEL_DS);
   r = APICR_TO_ADDR(r, APICR_VERSION);
   ret = HostIF_CopyFromUser(&dummy, (void*)r, sizeof(dummy));
   set_fs(old_fs);

   return ret == 0;
}


/*
 *----------------------------------------------------------------------
 *
 * SetVMAPICAddr --
 *
 *      Maps the host cpu's APIC.  The virtual address is stashed in
 *      the VMDriver structure.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The VMDriver structure is updated.
 *
 *----------------------------------------------------------------------
 */

static void
SetVMAPICAddr(VMDriver *vm, // IN/OUT: driver state
              MA ma)	    // IN: host APIC's ma
{
   volatile void *hostapic;

   ASSERT_ON_COMPILE(APICR_SIZE <= PAGE_SIZE);
   hostapic = (volatile void *) ioremap(ma, PAGE_SIZE);
   if (hostapic) {
      if ((APIC_VERSIONREG(hostapic) & 0xF0) == 0x10) {
	 vm->hostAPIC.base = (volatile uint32 (*)[4]) hostapic;
	 ASSERT(vm->vmhost != NULL);
	 vm->vmhost->hostAPICIsMapped = TRUE;
      } else {
	 iounmap((void*)hostapic);
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * ProbeAPIC --
 *
 *      Attempts to map the host APIC.
 *
 *      Most versions of Linux already provide access to a mapped
 *      APIC.  This function is just a backup.
 *
 *      Caveat: We assume that the APIC physical address is the same
 *      on all host cpus.
 *
 * Results:
 *      TRUE if APIC was found, FALSE if not.
 *
 * Side effects:
 *      May map the APIC.
 *
 *----------------------------------------------------------------------
 */

static Bool
ProbeAPIC(VMDriver *vm,   // IN/OUT: driver state
	  Bool setVMPtr)  // IN: set a pointer to the APIC's virtual address
{
   MA ma = APIC_GetMA();
   
   if (ma == (MA)-1) {
      return FALSE;
   }

   if (setVMPtr) {
      SetVMAPICAddr(vm, ma);
   } else {
      vm->hostAPIC.base = NULL;
   }

   return TRUE;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * HostIF_APICInit --
 *
 *      Initialize APIC behavior.
 *      Attempts to map the host APIC into vm->hostAPIC.
 *
 *      We don't attempt to refresh the mapping after a host cpu
 *      migration.  Fortunately, hosts tend to use the same address
 *      for all APICs.
 *
 *      Most versions of Linux already provide a mapped APIC.  We
 *      have backup code to read APIC_BASE and map it, if needed.
 *
 * Results:
 *      TRUE
 *
 * Side effects:
 *      May map the host APIC.
 *
 *----------------------------------------------------------------------
 */
Bool
HostIF_APICInit(VMDriver *vm,   // IN:
                Bool setVMPtr,  // IN:
                Bool probe)     // IN: force probing
{
#if defined(CONFIG_SMP)         || defined(CONFIG_X86_UP_IOAPIC) || \
    defined(CONFIG_X86_UP_APIC) || defined(CONFIG_X86_LOCAL_APIC)
   static Bool apicIPILogged = FALSE;
   VA kAddr;

   monitorIPIVector = SPURIOUS_APIC_VECTOR;
#if defined(POSTED_INTR_VECTOR)
   hvIPIVector      = POSTED_INTR_VECTOR;
#else
   hvIPIVector      = 0;
#endif


   if (!apicIPILogged) {
      Log("Monitor IPI vector: %x\n", monitorIPIVector);
      Log("HV      IPI vector: %x\n", hvIPIVector);
      apicIPILogged = TRUE;
   }

   if ((__GET_MSR(MSR_APIC_BASE) & APIC_MSR_X2APIC_ENABLED) != 0) {
      if (setVMPtr) {
         vm->hostAPIC.base = NULL;
         vm->vmhost->hostAPICIsMapped = FALSE;
         vm->hostAPIC.isX2 = TRUE;
      }
      return TRUE;
   }

   if (probe && ProbeAPIC(vm, setVMPtr)) {
      return TRUE;
   }

   /*
    * Normal case: use Linux's pre-mapped APIC.
    */
   kAddr = __fix_to_virt(FIX_APIC_BASE);
   if (!isVAReadable(kAddr)) {
      return TRUE;
   }
   if (setVMPtr) {
      vm->hostAPIC.base = (void *)kAddr;
   } else {
      vm->hostAPIC.base = NULL;
   }
#endif
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_SemaphoreWait --
 *
 *    Perform the semaphore wait (P) operation, possibly blocking.
 *
 * Result:
 *    1 (which equals MX_WAITNORMAL) if success, 
 *    negated error code otherwise.
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int   
HostIF_SemaphoreWait(VMDriver *vm,   // IN:
                     Vcpuid vcpuid,  // IN:
                     uint64 *args)   // IN:
{
   struct file *file;
   int res;
   int waitFD = args[0];
   int timeoutms = args[2];
   uint64 value;
   struct poll_wqueues table;
   unsigned int mask;

   file = vmware_fget(waitFD);
   if (file == NULL) {
      return MX_WAITERROR;
   }

   poll_initwait(&table);
   current->state = TASK_INTERRUPTIBLE;
   mask = compat_vfs_poll(file, &table.pt);
   if (!(mask & (POLLIN | POLLERR | POLLHUP))) {
      vm->vmhost->vcpuSemaTask[vcpuid] = current;
      schedule_timeout(timeoutms * HZ / 1000);  // convert to Hz
      vm->vmhost->vcpuSemaTask[vcpuid] = NULL;
   }
   current->state = TASK_RUNNING;
   poll_freewait(&table);

   /*
    * Userland only writes in multiples of sizeof(uint64). This will allow
    * the code to happily deal with a pipe or an eventfd. We only care about
    * reading no bytes (EAGAIN - non blocking fd) or sizeof(uint64).
    *
    * Upstream Linux changed the function parameter types/ordering in 4.14.0.
    */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
   res = kernel_read(file, file->f_pos, (char *)&value, sizeof value);
#else
   res = kernel_read(file, &value, sizeof value, &file->f_pos);
#endif
   if (res == sizeof value) {
      res = MX_WAITNORMAL;
   } else {
      if (res == 0) {
         res = -EBADF;
      }
   }

   fput(file);

   /*
    * Handle benign errors:
    * EAGAIN is MX_WAITTIMEDOUT.
    * The signal-related errors are all mapped into MX_WAITINTERRUPTED.
    */

   switch (res) {
   case -EAGAIN:
      res = MX_WAITTIMEDOUT;
      break;
   case -EINTR:
   case -ERESTART:
   case -ERESTARTSYS:
   case -ERESTARTNOINTR:
   case -ERESTARTNOHAND:
      res = MX_WAITINTERRUPTED;
      break;
   case -EBADF:
      res = MX_WAITERROR;
      break;
   }
   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_SemaphoreForceWakeup --
 *
 *    For each VCPU in the set whose target process is lightly sleeping (i.e.
 *    TASK_INTERRUPTIBLE), wake it up.  The target process can be waiting on a
 *    semaphore or due to a call to Vmx86_YieldToSet.
 *
 * Result:
 *    None.
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void 
HostIF_SemaphoreForceWakeup(VMDriver *vm,       // IN:
                            const VCPUSet *vcs) // IN:
{
   FOR_EACH_VCPU_IN_SET(vcs, vcpuid) {
      struct task_struct *t = vm->vmhost->vcpuSemaTask[vcpuid];
      vm->vmhost->vcpuSemaTask[vcpuid] = NULL;
      if (t && (t->state & TASK_INTERRUPTIBLE)) {
         wake_up_process(t);
      }
   } ROF_EACH_VCPU_IN_SET();
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_SemaphoreSignal --
 *
 *      Perform the semaphore signal (V) operation.
 *
 * Result:
 *      On success: MX_WAITNORMAL (1).
 *      On error: MX_WAITINTERRUPTED (3) if interrupted by a Unix signal (we
 *                   can block on a preemptive kernel).
 *                MX_WAITERROR (0) on generic error.
 *                Negated system error (< 0).
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
HostIF_SemaphoreSignal(uint64 *args)  // IN:
{
   struct file *file;
   int res;
   int signalFD = args[1];
   uint64 value = 1;  // make an eventfd happy should it be there

   file = vmware_fget(signalFD);
   if (!file) {
      return MX_WAITERROR;
   }

   /*
    * Always write sizeof(uint64) bytes. This works fine for eventfd and
    * pipes. The data written is formatted to make an eventfd happy should
    * it be present.
    *
    * Upstream Linux changed the function parameter types/ordering in 4.14.0.
    */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
   res = kernel_write(file, (char *)&value, sizeof value, file->f_pos);
#else
   res = kernel_write(file, &value, sizeof value, &file->f_pos);
#endif

   if (res == sizeof value) {
      res = MX_WAITNORMAL;
   }

   fput(file);

   /*
    * Handle benign errors:
    * EAGAIN is MX_WAITTIMEDOUT.
    * The signal-related errors are all mapped into MX_WAITINTERRUPTED.
    */

   switch (res) {
   case -EAGAIN:
      // The pipe is full, so it is already signalled. Success.
      res = MX_WAITNORMAL;
      break;
   case -EINTR:
   case -ERESTART:
   case -ERESTARTSYS:
   case -ERESTARTNOINTR:
   case -ERESTARTNOHAND:
      res = MX_WAITINTERRUPTED;
      break;
   }
   return res;
}

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)) || !defined(CONFIG_SMP))
#   define VMMON_USE_CALL_FUNC
#endif

#if defined(VMMON_USE_CALL_FUNC)
/*
 *----------------------------------------------------------------------
 *
 * LinuxDriverIPIHandler  --
 *
 *      Null IPI handler - for monitor to notice AIO completion
 *
 *----------------------------------------------------------------------
 */
void
LinuxDriverIPIHandler(void *info)
{
   return;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 17)
#define VMMON_CALL_FUNC_SYNC 0  // async; we've not seen any problems
#else
#define VMMON_CALL_FUNC_SYNC 1  // sync; insure no problems from old releases
#endif

#endif


/*
 *----------------------------------------------------------------------
 *
 * HostIF_IPI --
 *
 *    If the passed VCPU threads are on some CPUs in the system,
 *    attempt to hit them with an IPI.
 *
 *    On older Linux systems we do a broadcast.
 *
 * Result:
 *    The mode used to send IPIs.
 *
 *----------------------------------------------------------------------
 */

HostIFIPIMode
HostIF_IPI(VMDriver *vm,                // IN:
           const VCPUSet *ipiTargets)   // IN:
{
   HostIFIPIMode mode = IPI_NONE;

   ASSERT(vm);

   FOR_EACH_VCPU_IN_SET(ipiTargets, v) {
      uint32 targetHostCpu = vm->currentHostCpu[v];
      if (targetHostCpu != INVALID_PCPU) {
         ASSERT(targetHostCpu < MAX_PCPUS);
#if defined(VMMON_USE_CALL_FUNC)
         /* older kernels IPI broadcast; use async when possible */
         (void) compat_smp_call_function(LinuxDriverIPIHandler,
                                         NULL, VMMON_CALL_FUNC_SYNC);
	 mode = IPI_BROADCAST;
	 break;
#else
         /* Newer kernels have (async) IPI targetting */
         arch_send_call_function_single_ipi(targetHostCpu);
	 mode = IPI_UNICAST;
#endif
      }
   } ROF_EACH_VCPU_IN_SET();

   return mode;
}


typedef struct {
   Atomic_uint32 index;
   CPUIDQuery *query;
} HostIFGetCpuInfoData;


/*
 *-----------------------------------------------------------------------------
 *
 * HostIFGetCpuInfo --
 *
 *      Collect CPUID information on the current logical CPU.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      'data->index' is atomically incremented by one.
 *
 *-----------------------------------------------------------------------------
 */

static void
HostIFGetCpuInfo(void *clientData) // IN/OUT: A HostIFGetCpuInfoData *
{
   HostIFGetCpuInfoData *data = (HostIFGetCpuInfoData *)clientData;
   CPUIDQuery *query;
   uint32 index;

   ASSERT(data);
   query = data->query;
   ASSERT(query);

   index = Atomic_ReadInc32(&data->index);
   if (index >= query->numLogicalCPUs) {
      return;
   }

   query->logicalCPUs[index].tag = HostIF_GetCurrentPCPU();
   __GET_CPUID2(query->eax, query->ecx, &query->logicalCPUs[index].regs);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_GetAllCpuInfo --
 *
 *      Collect CPUID information on all logical CPUs.
 *
 *      'query->numLogicalCPUs' is the size of the 'query->logicalCPUs' output
 *      array.
 *
 * Results:
 *      On success: TRUE. 'query->logicalCPUs' is filled and
 *                  'query->numLogicalCPUs' is adjusted accordingly.
 *      On failure: FALSE. Happens if 'query->numLogicalCPUs' was too small.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HostIF_GetAllCpuInfo(CPUIDQuery *query) // IN/OUT
{
   HostIFGetCpuInfoData data;

   Atomic_Write32(&data.index, 0);
   data.query = query;

   /*
    * XXX Linux has userland APIs to bind a thread to a processor, so we could
    *     probably implement this in userland like we do on Win32.
    */

   HostIF_CallOnEachCPU(HostIFGetCpuInfo, &data);

   /*
    * At this point, Atomic_Read32(&data.index) is the number of logical CPUs
    * who replied.
    */

   if (Atomic_Read32(&data.index) > query->numLogicalCPUs) {
      return FALSE;
   }

   ASSERT(Atomic_Read32(&data.index) <= query->numLogicalCPUs);
   query->numLogicalCPUs = Atomic_Read32(&data.index);

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_CallOnEachCPU --
 *
 *      Call specified function once on each CPU.  No ordering guarantees.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.  May be slow.
 *
 *----------------------------------------------------------------------
 */

void
HostIF_CallOnEachCPU(void (*func)(void*), // IN: function to call
                     void *data)          // IN/OUT: argument to function
{
   preempt_disable();
   (*func)(data);
   (void)compat_smp_call_function(*func, data, 1);
   preempt_enable();
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIFCheckTrackedMPN --
 *
 *      Check if a given MPN is tracked for the specified VM.
 *
 * Result:
 *      TRUE if the MPN is tracked in one of the trackers for the specified VM,
 *      FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HostIFCheckTrackedMPN(VMDriver *vm, // IN: The VM instance
                      MPN mpn)      // IN: The MPN
{
   VMHost * const vmh = vm->vmhost;

   if (vmh == NULL) {
      return FALSE;
   }

   HostIF_VMLock(vm, 32); // Debug version of PhysTrack wants VM's lock.
   if (vmh->lockedPages) {
      if (PhysTrack_Test(vmh->lockedPages, mpn)) {
         HostIF_VMUnlock(vm, 32);
         return TRUE;
      }
   }

   if (vmh->AWEPages) {
      if (PhysTrack_Test(vmh->AWEPages, mpn)) {
         HostIF_VMUnlock(vm, 32);
         return TRUE;
      }
   }

   if (vm->memtracker) {
      if (MemTrack_LookupMPN(vm->memtracker, mpn) != NULL) {
         HostIF_VMUnlock(vm, 32);
         return TRUE;
      }
   }
   HostIF_VMUnlock(vm, 32);

   if (vmx86_debug) {
      /*
       * The monitor may have old KSeg mappings to pages which it no longer
       * owns.  Minimize customer noise by only logging this for developers.
       */
      Log("%s: MPN %" FMT64 "x not owned by this VM\n", __FUNCTION__, mpn);
   }
   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_ReadPage --
 *
 *      Reads one page of data from a machine page and returns it in the
 *      specified kernel or user buffer.  The machine page must be owned by
 *      the specified VM.
 *
 * Results:
 *      0 on success
 *      negative error code on error
 *
 * Side effects:
 *      none
 *
 *----------------------------------------------------------------------
 */

int
HostIF_ReadPage(VMDriver *vm,        // IN: The VM instance
                MPN mpn,             // MPN of the page
                VA64 addr,           // buffer for data
                Bool kernelBuffer)   // is the buffer in kernel space?
{
   void *buf = VA64ToPtr(addr);
   int ret = 0;
   const void* ptr;
   struct page* page;

   if (mpn == INVALID_MPN) {
      return -EFAULT;
   }
   if (HostIFCheckTrackedMPN(vm, mpn) == FALSE) {
      return -EFAULT;
   }

   page = pfn_to_page(mpn);
   ptr = kmap(page);
   if (ptr == NULL) {
      return -ENOMEM;
   }
   
   if (kernelBuffer) {
      memcpy(buf, ptr, PAGE_SIZE);
   } else {
      ret = HostIF_CopyToUser(buf, ptr, PAGE_SIZE);
   }
   kunmap(page);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_WritePage --
 *
 *      Writes one page of data from a kernel or user buffer onto the specified
 *      machine page.  The machine page must be owned by the specified VM.
 *
 * Results:
 *      0 on success
 *      negative error code on error
 *
 * Side effects:
 *      none
 *
 *----------------------------------------------------------------------
 */

int
HostIFWritePageWork(MPN mpn,              // MPN of the page
                    VA64 addr,            // data to write to the page
                    Bool kernelBuffer)    // is the buffer in kernel space?
{
   void const *buf = VA64ToPtr(addr);
   int ret = 0;
   void* ptr;
   struct page* page;

   if (mpn == INVALID_MPN) {
      return -EFAULT;
   }

   page = pfn_to_page(mpn);
   ptr = kmap(page);
   if (ptr == NULL) {
      return -ENOMEM;
   }

   if (kernelBuffer) {
      memcpy(ptr, buf, PAGE_SIZE);
   } else {
      ret = HostIF_CopyFromUser(ptr, buf, PAGE_SIZE);
   }
   kunmap(page);

   return ret;
}

int
HostIF_WritePage(VMDriver *vm,      // IN: The VM instance
                 MPN mpn,              // MPN of the page
                 VA64 addr,            // data to write to the page
                 Bool kernelBuffer)    // is the buffer in kernel space?
{
   if (HostIFCheckTrackedMPN(vm, mpn) == FALSE) {
      return -EFAULT;
   }
   return HostIFWritePageWork(mpn, addr, kernelBuffer);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_WriteMachinePage --
 *
 *      Puts the content of a machine page into a kernel or user mode
 *      buffer.  This should only be used for host-global pages, not any
 *      VM-owned pages.
 *
 * Results:
 *      On success: 0
 *      On failure: a negative error code
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
HostIF_WriteMachinePage(MPN mpn,   // IN: MPN of the page
                        VA64 addr) // IN: data to write to the page
{
   return HostIFWritePageWork(mpn, addr, TRUE);
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_GetLockedPageList --
 *
 *      puts MPNs of pages that were allocated by HostIF_AllocLockedPages()
 *      into user mode buffer.
 *
 * Results:
 *      non-negative number of the MPNs in the buffer on success.
 *      negative error code on error (-EFAULT)
 *
 * Side effects:
 *      none
 *
 *----------------------------------------------------------------------
 */

int
HostIF_GetLockedPageList(VMDriver* vm,          // IN: VM instance pointer
                         VA64 uAddr,            // OUT: user mode buffer for MPNs
                         unsigned int numPages) // IN: size of the buffer in MPNs
{
   MPN *mpns = VA64ToPtr(uAddr);
   MPN mpn;
   unsigned count;

   struct PhysTracker* AWEPages;

   if (!vm->vmhost || !vm->vmhost->AWEPages) {
      return 0;
   }
   AWEPages = vm->vmhost->AWEPages;

   for (mpn = 0, count = 0;
        (count < numPages) &&
        (INVALID_MPN != (mpn = PhysTrack_GetNext(AWEPages, mpn)));
        count++) {

      if (HostIF_CopyToUser(&mpns[count], &mpn, sizeof *mpns) != 0) {
         return -EFAULT;
      }
   }

   return count;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_GetNextAnonPage --
 *
 *      If "inMPN" is INVALID_MPN gets the first MPN in the anon mpn list else
 *      gets the anon mpn after "inMPN" in the anon mpn list.
 *
 * Results:
 *      Next anon MPN. If the list has been exhausted, returns INVALID_MPN.
 *
 *-----------------------------------------------------------------------------
 */

MPN
HostIF_GetNextAnonPage(VMDriver *vm, MPN inMPN)
{
   if (!vm->vmhost || !vm->vmhost->AWEPages) {
      return INVALID_MPN;
   }
   return PhysTrack_GetNext(vm->vmhost->AWEPages, inMPN);
}


/*
 *----------------------------------------------------------------------
 *
 * HostIF_GetCurrentPCPU --
 *
 *    Get current physical CPU id.  Interrupts should be disabled so
 *    that the thread cannot move to another CPU.
 *
 * Results:
 *    Host CPU number.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------- 
 */

uint32
HostIF_GetCurrentPCPU(void)
{
   return smp_processor_id();
}


#ifdef VMMON_USE_COMPAT_SCHEDULE_HRTIMEOUT
/*
 *----------------------------------------------------------------------
 *
 * HostIFWakeupClockThread --
 *
 *      Wake up the fast clock thread.  Can't do this from the timer
 *      callback, because it holds locks that the scheduling code
 *      might take. 
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
HostIFWakeupClockThread(unsigned long data)  //IN:
{
   wake_up_process(linuxState.fastClockThread);
}


/*
 *----------------------------------------------------------------------
 *
 * HostIFTimerCallback --
 *      
 *      Schedule a tasklet to wake up the fast clock thread.
 *
 * Results:
 *      Tell the kernel not to restart the timer.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
 
static enum hrtimer_restart 
HostIFTimerCallback(struct hrtimer *timer)  //IN:
{
   tasklet_schedule(&timerTasklet);

   return HRTIMER_NORESTART;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIFScheduleHRTimeout --
 *      
 *      Schedule an hrtimer to wake up the fast clock thread.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Sleep.
 *
 *----------------------------------------------------------------------
 */

static void 
HostIFScheduleHRTimeout(ktime_t *expires)  //IN:
{
   struct hrtimer t;

   if (expires && !expires->tv64) {
      __set_current_state(TASK_RUNNING);

      return;
   }

   hrtimer_init(&t, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
   t.function = HostIFTimerCallback;
   hrtimer_start(&t, *expires, HRTIMER_MODE_REL);

   if (hrtimer_active(&t)) {
      schedule();
   }
   
   hrtimer_cancel(&t);
   __set_current_state(TASK_RUNNING);
}
#endif //VMMON_USE_COMPAT_SCHEDULE_HRTIMEOUT


#ifndef VMMON_USE_HIGH_RES_TIMERS
/*
 *----------------------------------------------------------------------
 *
 * HostIFDoIoctl --
 *
 *    Issue ioctl.  Assume kernel is not locked.  It is not true now,
 *    but it makes things easier to understand, and won't surprise us
 *    later when we get rid of kernel lock from our code.
 *
 * Results:
 *    Same as ioctl method.
 *
 * Side effects:
 *    none.
 *
 *---------------------------------------------------------------------- 
 */

static long
HostIFDoIoctl(struct file *filp,
              u_int iocmd,
              unsigned long ioarg)
{
   if (filp->f_op->unlocked_ioctl) {
      return filp->f_op->unlocked_ioctl(filp, iocmd, ioarg);
   }
   return -ENOIOCTLCMD;
}
#endif //VMON_USE_HIGH_RES_TIMERS


/*
 *----------------------------------------------------------------------
 *
 * HostIFStartTimer --
 *
 *      Starts the timer using either /dev/rtc or high-resolution timers.
 *
 * Results:
 *      Returns 0 on success, -1 on failure.
 *
 * Side effects:
 *      Sleep until timer expires.
 *
 *----------------------------------------------------------------------
 */

int
HostIFStartTimer(Bool rateChanged,  //IN: Did rate change? 
		 unsigned int rate, //IN: current clock rate
                 struct file *filp) //IN: /dev/rtc descriptor
{
#ifdef VMMON_USE_HIGH_RES_TIMERS
   static unsigned long slack = 0;
   static ktime_t expires;
   int timerPeriod;

   if (rateChanged) {
      timerPeriod = NSEC_PER_SEC / rate; 
      expires = ktime_set(0, timerPeriod);
      /*
       * Allow the kernel to expire the timer at its convenience.
       * ppoll() uses 0.1% of the timeout value.  I think we can
       * tolerate 1%.
       */
          
      slack = timerPeriod / 100;
   }
   set_current_state(TASK_INTERRUPTIBLE);
#   ifdef VMMON_USE_SCHEDULE_HRTIMEOUT
   schedule_hrtimeout_range(&expires, slack, HRTIMER_MODE_REL);
#   else
   HostIFScheduleHRTimeout(&expires);
#   endif
#else
   unsigned p2rate;
   int res;
   unsigned long buf;
   loff_t pos = 0;

   if (rateChanged) {
      /*
       * The host will already have HZ timer interrupts per second.  So
       * in order to satisfy the requested rate, we need up to (rate -
       * HZ) additional interrupts generated by the RTC.  That way, if
       * the guest ask for a bit more than 1024 virtual interrupts per
       * second (which is a common case for Windows with multimedia
       * timers), we'll program the RTC to 1024 rather than 2048, which
       * saves a considerable amount of CPU.  PR 519228.
       */
      if (rate > HZ) {
         rate -= HZ;
      } else {
         rate = 0;
      }
      /*
       * Don't set the RTC rate to 64 Hz or lower: some kernels have a
       * bug in the HPET emulation of RTC that will cause the RTC
       * frequency to get stuck at 64Hz.  See PR 519228 comment #23.
       */
      p2rate = 128;
      // Hardware rate must be a power of 2
      while (p2rate < rate && p2rate < 8192) {
         p2rate <<= 1;
      }

      res = HostIFDoIoctl(filp, RTC_IRQP_SET, p2rate);
      if (res < 0) {
         Warning("/dev/rtc set rate %d failed: %d\n", p2rate, res);

         return -1;
      }
      if (kthread_should_stop()) {
         return -1;
      }
   }
   res = filp->f_op->read(filp, (void *) &buf, sizeof(buf), &pos);
   if (res <= 0) {
      if (res != -ERESTARTSYS) {
         Log("/dev/rtc read failed: %d\n", res);
      }

      return -1;
   }
#endif

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HostIFFastClockThread --
 *
 *      Kernel thread that provides finer-grained wakeups than the
 *      main system timers by using /dev/rtc.  We can't do this at
 *      user level because /dev/rtc is not sharable (PR 19266).  Also,
 *      we want to avoid the overhead of a context switch out to user
 *      level on every RTC interrupt.
 *
 * Results:
 *      Returns 0.
 *
 * Side effects:
 *      Wakeups and IPIs.
 *
 *----------------------------------------------------------------------
 */

static int
HostIFFastClockThread(void *data)  // IN:
{
   struct file *filp = (struct file *) data;
   int res;
   mm_segment_t oldFS;
   unsigned int rate = 0;
   unsigned int prevRate = 0;

   oldFS = get_fs();
   set_fs(KERNEL_DS);
   allow_signal(SIGKILL);
   set_user_nice(current, linuxState.fastClockPriority);

   while ((rate = linuxState.fastClockRate) > MIN_RATE) {
      if (kthread_should_stop()) {
         goto out;
      }
      res = HostIFStartTimer(rate != prevRate, rate, filp);
      if (res < 0) {
         goto out;
      }
      prevRate = rate;

#if defined(CONFIG_SMP)
      /*
       * IPI each VCPU thread that is in the monitor and is due to
       * fire a MonTimer callback.
       */
      Vmx86_MonTimerIPI();
#endif

      /*
       * Wake threads that are waiting for a fast poll timeout at
       * userlevel.  This is needed only on Linux.  On Windows,
       * we get shorter timeouts simply by increasing the host
       * clock rate.
       */

      LinuxDriverWakeUp(TRUE);
   }

 out:
   LinuxDriverWakeUp(TRUE);
   set_fs(oldFS);

   /*
    * Do not exit thread until we are told to do so.
    */

   do {
      set_current_state(TASK_UNINTERRUPTIBLE);
      if (kthread_should_stop()) {
         break;
      }
      schedule();
   } while (1);
   set_current_state(TASK_RUNNING);

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_SetFastClockRate --
 *
 *      The monitor wants to poll for events at the given rate.
 *      Ensure that the host OS's timer interrupts come at least at
 *      this rate.  If the requested rate is greater than the rate at
 *      which timer interrupts will occur on CPUs other than 0, then
 *      also arrange to call Vmx86_MonitorPollIPI on every timer
 *      interrupt, in order to relay IPIs to any other CPUs that need
 *      them.
 *
 * Locking:
 *      The caller must hold the fast clock lock.
 *
 * Results:
 *      0 for success; positive error code if /dev/rtc could not be opened.
 *
 * Side effects:
 *      As described above.
 *
 *-----------------------------------------------------------------------------
 */

int
HostIF_SetFastClockRate(unsigned int rate) // IN: Frequency in Hz.
{
   ASSERT(MutexIsLocked(&fastClockMutex));
   linuxState.fastClockRate = rate;

   /*
    * Overview
    * --------
    * An SMP Linux kernel programs the 8253 timer (to increment the 'jiffies'
    * counter) _and_ all local APICs (to run the scheduler code) to deliver
    * interrupts HZ times a second.
    *
    * Time
    * ----
    * The kernel tries very hard to spread all these interrupts evenly over
    * time, i.e. on a 1 CPU system, the 1 local APIC phase is shifted by 1/2
    * period compared to the 8253, and on a 2 CPU system, the 2 local APIC
    * phases are respectively shifted by 1/3 and 2/3 period compared to the
    * 8253. This is done to reduce contention on locks guarding the global task
    * queue.
    *
    * Space
    * -----
    * The 8253 interrupts are distributed between physical CPUs, evenly on a P3
    * system, whereas on a P4 system physical CPU 0 gets all of them.
    *
    * Long story short, unless the monitor requested rate is significantly
    * higher than HZ, we don't need to send IPIs or exclusively grab /dev/rtc
    * to periodically kick vCPU threads running in the monitor on all physical
    * CPUs.
    */

   if (rate > MIN_RATE) {
      if (!linuxState.fastClockThread) {
         struct task_struct *rtcTask;
         struct file *filp = NULL;

#if !defined(VMMON_USE_HIGH_RES_TIMERS)
         int res;

         filp = filp_open("/dev/rtc", O_RDONLY, 0);
         if (IS_ERR(filp)) {
            Warning("/dev/rtc open failed: %d\n", (int)(VA)filp);

            return -(int)(VA)filp;
         }
         res = HostIFDoIoctl(filp, RTC_PIE_ON, 0);
         if (res < 0) {
            Warning("/dev/rtc enable interrupt failed: %d\n", res);
            filp_close(filp, current->files);

            return -res;
         }
#endif
         rtcTask = kthread_run(HostIFFastClockThread, filp, "vmware-rtc");
         if (IS_ERR(rtcTask)) {
            long err = PTR_ERR(rtcTask);

            /*
             * Ignore ERESTARTNOINTR silently, it occurs when signal is
             * pending, and syscall layer automatically reissues operation
             * after signal is handled.
             */

            if (err != -ERESTARTNOINTR) {
               Warning("/dev/rtc cannot start watch thread: %ld\n", err);
            }
	    close_rtc(filp, current->files);

            return -err;
         }
         linuxState.fastClockThread = rtcTask;
	 linuxState.fastClockFile = filp;
      }
   } else {
      if (linuxState.fastClockThread) {
         send_sig(SIGKILL, linuxState.fastClockThread, 1);
         kthread_stop(linuxState.fastClockThread);
	 close_rtc(linuxState.fastClockFile, current->files);

         linuxState.fastClockThread = NULL;
	 linuxState.fastClockFile = NULL;
      }
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_MapUserMem --
 *
 *	Obtain kernel pointer to user memory. The pages backing the user memory
 *      address are locked into memory (this allows the pointer to be used in
 *      contexts where paging is undesirable or impossible).
 *
 * Results:
 *      On success, returns the kernel virtual address, along with a handle to
 *      be used for unmapping.
 *      On failure, returns NULL.
 *
 * Side effects:
 *	Yes.
 *
 *-----------------------------------------------------------------------------
 */

void *
HostIF_MapUserMem(VA addr,                  // IN: User memory virtual address
                  size_t size,              // IN: Size of memory desired
                  VMMappedUserMem **handle) // OUT: Handle to mapped memory
{
   void *p = (void *) (uintptr_t) addr;
   VMMappedUserMem *newHandle;
   VA offset = addr & (PAGE_SIZE - 1);
   size_t numPagesNeeded = ((offset + size) / PAGE_SIZE) + 1;
   size_t handleSize =
      sizeof *newHandle + numPagesNeeded * sizeof newHandle->pages[0];
   void *mappedAddr;

   ASSERT(handle);

   if (!write_access_ok(p, size)) {
      printk(KERN_ERR "%s: Couldn't verify write to uva 0x%p with size %"
             FMTSZ"u\n", __func__, p, size);

      return NULL;
   }

   newHandle = kmalloc(handleSize, GFP_KERNEL);
   if (newHandle == NULL) {
      printk(KERN_ERR "%s: Couldn't allocate %"FMTSZ"u bytes of memory\n",
             __func__, handleSize);

      return NULL;
   }

   if (HostIFGetUserPages(p, newHandle->pages, numPagesNeeded)) {
      kfree(newHandle);
      printk(KERN_ERR "%s: Couldn't get %"FMTSZ"u %s for uva 0x%p\n", __func__,
             numPagesNeeded, numPagesNeeded > 1 ? "pages" : "page", p);

      return NULL;
   }

   if (numPagesNeeded > 1) {
      /*
       * Unlike kmap(), vmap() can fail. If it does, we need to release the
       * pages that we acquired in HostIFGetUserPages().
       */

      mappedAddr = vmap(newHandle->pages, numPagesNeeded, VM_MAP, PAGE_KERNEL);
      if (mappedAddr == NULL) {
         unsigned int i;
         for (i = 0; i < numPagesNeeded; i++) {
            put_page(newHandle->pages[i]);
         }
         kfree(newHandle);
         printk(KERN_ERR "%s: Couldn't vmap %"FMTSZ"u %s for uva 0x%p\n",
                __func__, numPagesNeeded,
                numPagesNeeded > 1 ? "pages" : "page", p);

         return NULL;
      }
   } else {
      mappedAddr = kmap(newHandle->pages[0]);
   }

   printk(KERN_DEBUG "%s: p = 0x%p, offset = 0x%p, numPagesNeeded = %"FMTSZ"u,"
          " handleSize = %"FMTSZ"u, mappedAddr = 0x%p\n",
          __func__, p, (void *)offset, numPagesNeeded, handleSize, mappedAddr); 

   newHandle->numPages = numPagesNeeded;
   newHandle->addr = mappedAddr;
   *handle = newHandle;

   return mappedAddr + offset;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_UnmapUserMem --
 *
 *	Unmap user memory from HostIF_MapUserMem().
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Yes.
 *
 *-----------------------------------------------------------------------------
 */

void
HostIF_UnmapUserMem(VMMappedUserMem *handle) // IN: Handle to mapped memory
{
   unsigned int i;

   if (handle == NULL) {
      return;
   }

   printk(KERN_DEBUG "%s: numPages = %"FMTSZ"u, addr = 0x%p\n",
          __func__, handle->numPages, handle->addr); 

   if (handle->numPages > 1) {
      vunmap(handle->addr);
   } else {
      kunmap(handle->pages[0]);
   }

   for (i = 0; i < handle->numPages; i++) {
      put_page(handle->pages[i]);
   }
   kfree(handle);
}

/*
 *-----------------------------------------------------------------------------
 *
 * HostIF_SafeRDMSR --
 *
 *      Attempt to read a MSR, and handle the exception if the MSR
 *      is unimplemented.
 *
 * Results:
 *      0 if successful, and MSR value is returned via *val.
 *
 *      If the MSR is unimplemented, *val is set to 0, and a
 *      non-zero value is returned: -1 for Win32, -EFAULT for Linux,
 *      and 1 for MacOS.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */
int
HostIF_SafeRDMSR(unsigned int msr,   // IN
                 uint64 *val)        // OUT: MSR value
{
   int ret;
   unsigned low, high;
   asm volatile("2: rdmsr ; xor %0,%0\n"
                "1:\n\t"
                ".section .fixup,\"ax\"\n\t"
                "3: mov %4,%0 ; jmp 1b\n\t"
                ".previous\n\t"
                VMW_ASM_EXTABLE(2b, 3b)
                : "=r"(ret), "=a"(low), "=d"(high)
                : "c"(msr), "i"(-EFAULT), "1"(0), "2"(0)); // init eax/edx to 0
   *val = (low | ((u64)(high) << 32));

   return ret;
}

