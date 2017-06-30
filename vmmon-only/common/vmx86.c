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
 * vmx86.c --
 *
 *     Platform independent routines for creating/destroying/running
 *     virtual machine monitors.
 */

#ifdef linux
/* Must come before any kernel header file --hpreg */
#   include "driver-config.h"

#   include <linux/string.h> /* memset() in the kernel  */
#   include <linux/sched.h>  /* jiffies from the kernel */
#else
#   include <string.h>
#endif

#ifdef __APPLE__
#include <IOKit/IOLib.h>  // must come before "vmware.h"
#endif

#include "vmware.h"
#include "vm_assert.h"
#include "vm_basic_math.h"
#include "vmx86.h"
#include "task.h"
#include "initblock.h"
#include "vm_asm.h"
#include "iocontrols.h"
#include "hostif.h"
#include "cpuid.h"
#include "vcpuset.h"
#include "memtrack.h"
#include "hashFunc.h"
#if defined(_WIN64)
#include "x86.h"
#include "vmmon-asm-x86-64.h"
#endif
#include "x86vt.h"
#include "x86svm.h"
#include "x86cpuid_asm.h"
#if defined(linux)
#include <asm/timex.h>
#endif
#include "x86perfctr.h"


PseudoTSC pseudoTSC;

/*
 * Keep track of the virtual machines that have been
 * created using the following structures.
 */

static VMDriver *vmDriverList = NULL;

static LockedPageLimit lockedPageLimit = {
   0,                        // host: does not need to be initialized.
   0,                        // configured: must be set by some VM as it is powered on.
   (uint32)MAX_LOCKED_PAGES, // dynamic
};

/* Percentage of guest "paged" memory that must fit within the hard limit. */
static unsigned minVmMemPct;

/* Number of pages actually locked by all virtual machines */
static unsigned numLockedPages;

/* Total virtual machines on this host */
static unsigned vmCount;

/* Total number of open vmmon file handles. */
static unsigned fdCount;

/*
 * We implement a list of allocated VM ID's using an array.
 * The array is initialized with the values 1...MAX_VMS-1, INVALID_VMID.
 * vmIDsAllocated holds the last VM ID given out and vmIDsUnused
 * holds the next VM ID to give out.
 */

#define INVALID_VMID (-1)
static int vmIDList[MAX_VMS];
static int vmIDsAllocated;
static int vmIDsUnused;

/* Max rate requested for fast clock by any virtual machine. */
static unsigned globalFastClockRate;

/*
 *----------------------------------------------------------------------
 *
 * Vmx86AdjustLimitForOverheads --
 *
 *        This function adjusts an overall limit on the number of
 *        locked pages to take into account overhead for the vmx processes, etc.
 *        since the hostOS will also see this as overhead. We do this for all
 *        vmx processes, not just ones whose vms have been admitted.
 *
 *        If "vm" is NULL, we are allocating a global page and have no
 *        perVMOverhead term to take into account.
 *
 * Results:
 *       Number of remaining pages considered to be lockable on this host.
 *
 * Side effects:
 *       None.
 *
 *----------------------------------------------------------------------
 */

static INLINE unsigned
Vmx86AdjustLimitForOverheads(const VMDriver* vm,
                             const uint32 limit)
{
   uint32 extraCost = (vm != NULL) ? vmCount * vm->memInfo.perVMOverhead : 0;
   ASSERT(HostIF_GlobalLockIsHeld());

   return (extraCost < limit) ?  (limit - extraCost) : 0;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86LockedPageLimit --
 *
 *       There are three limits controlling how many pages we can lock on
 *       a host:
 *
 *       lockedPageLimit.configured is controlled by UI,
 *       lockedPageLimit.dynamic is controlled by authd's hardLimitMonitor,
 *       lockedPageLimit.host is calculated dynamically based on kernel stats
 *       by vmmon using kernel stats.
 *
 *       We can lock the MIN of these values.
 *
 * Results:
 *       Number of pages to lock on this host.
 *
 * Side effects:
 *       Updates the host locked pages limit.
 *
 *----------------------------------------------------------------------
 */

static INLINE unsigned
Vmx86LockedPageLimit(const VMDriver* vm)  // IN:
{
   uint32 overallLimit;
   ASSERT(HostIF_GlobalLockIsHeld());

   lockedPageLimit.host = HostIF_EstimateLockedPageLimit(vm, numLockedPages);
   overallLimit = MIN(MIN(lockedPageLimit.configured, lockedPageLimit.dynamic),
                      lockedPageLimit.host);

   return Vmx86AdjustLimitForOverheads(vm, overallLimit);
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86HasFreePages --
 *
 *       Returns TRUE if the vm can lock more pages.  This is true if
 *       we are below the host's hard memory limit and this vm has not
 *       exceeded its maximum allocation.
 *       Callers must ensure driver-wide and VM serialization
 *       typically by using HostIF_GlobalLock() and  HostIF_VMLock().
 *
 * Results:
 *       TRUE if pages can be locked, FALSE otherwise
 *
 * Side effects:
 *       None
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Vmx86HasFreePages(VMDriver *vm,
		  unsigned int numPages,
                  Bool checkVM)
{
   /*
    * 1) Be careful with overflow.
    * 2) lockedPageLimit and vm->memInfo.maxAllocation can be decreased below
    *    the current numLockedPages and vm->memInfo.locked
    * 3) lockedPageLimit.host can go lower than numLockedPages.
    */

   ASSERT(HostIF_GlobalLockIsHeld() &&
          (!checkVM || HostIF_VMLockIsHeld(vm)));

   if (checkVM) {
      /*
       * Check the per-vm limit.
       */

      ASSERT(HostIF_VMLockIsHeld(vm));
      if (vm->memInfo.admitted) {
	 if (vm->memInfo.maxAllocation <= vm->memInfo.locked) {
	    return FALSE;
	 } else if (vm->memInfo.maxAllocation - vm->memInfo.locked < numPages) {
	    return FALSE;
	 }
      }
   } else {
      /*
       * Check the global limit.
       */

      unsigned limit = Vmx86LockedPageLimit(vm);

      if (limit <= numLockedPages) {
	 return FALSE;
      } else if (limit - numLockedPages < numPages) {
	 return FALSE;
      }
   }

   return TRUE;
}


#ifdef VMX86_DEBUG
/*
 *----------------------------------------------------------------------
 *
 * Vmx86VMIsRegistered --
 *
 *      Check if "vm" is on the list of VMDrivers.
 *
 * Results:
 *      Return TRUE iff "vm" is on the list of VMDrivers.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------
 */

static Bool
Vmx86VMIsRegistered(VMDriver *vm, Bool needsLock)
{
   VMDriver *tmp;
   Bool      found = FALSE;

   ASSERT(needsLock || HostIF_GlobalLockIsHeld());

   if (needsLock) {
      HostIF_GlobalLock(5);
   }

   for (tmp = vmDriverList; tmp != NULL; tmp = tmp->nextDriver) {
      if (tmp == vm) {
         found = TRUE;
         break;
      }
   }

   if (needsLock) {
      HostIF_GlobalUnlock(5);
   }

   return found;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_InitIDList --
 *
 *       Called when the driver is initialized.
 *       Set up the list of available VM ID's.
 *
 * Results:
 *       None. Sets up global data.
 *
 * Side effects:
 *       None.
 *
 *----------------------------------------------------------------------
 */

void
Vmx86_InitIDList(void)
{
   int i;

   HostIF_GlobalLock(32);

   for (i = 0; i < MAX_VMS; i++) {
      vmIDList[i] = i + 1;
   }
   vmIDList[MAX_VMS - 1] = INVALID_VMID;
   vmIDsUnused = 0;
   vmIDsAllocated = INVALID_VMID;

   HostIF_GlobalUnlock(32);
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86FreeVMID --
 *
 *       Return a VM ID to the list of available VM ID's.
 *
 * Results:
 *       None
 *
 * Side effects:
 *       None
 *
 *----------------------------------------------------------------------
 */

static void
Vmx86FreeVMID(int vmID) // IN
{
   int i;

   ASSERT(HostIF_GlobalLockIsHeld());

   /* Deleting head of the list. */
   if (vmID == vmIDsAllocated) {
      int tmp;

      tmp = vmIDList[vmIDsAllocated];
      vmIDList[vmIDsAllocated] = vmIDsUnused;
      vmIDsAllocated = tmp;
      vmIDsUnused = vmID;

      return;
   }

   for (i = vmIDsAllocated; vmIDList[i] != INVALID_VMID; i = vmIDList[i]) {
      if (vmIDList[i] == vmID) {
         vmIDList[i] = vmIDList[vmID];
         vmIDList[vmID] = vmIDsUnused;
         vmIDsUnused = vmID;

         return;
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86AllocVMID --
 *
 *       Grab a VM ID from the list of available VM ID's.
 *
 * Results:
 *       The VM ID, in the range [ 0 ; MAX_VMS ).
 *
 * Side effects:
 *       None
 *
 *----------------------------------------------------------------------
 */

static int
Vmx86AllocVMID(void)
{
   int vmID;

   ASSERT(HostIF_GlobalLockIsHeld());

   vmID = vmIDsUnused;
   ASSERT(0 <= vmID && vmID < MAX_VMS);
   vmIDsUnused = vmIDList[vmID];
   vmIDList[vmID] = vmIDsAllocated;
   vmIDsAllocated = vmID;

   return vmID;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86RegisterVMOnList --
 *
 *      Add a VM to the list of registered VMs and increment
 *      the count of VMs.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Add VM to linked list.
 *	Increment count of VMs.
 *
 *----------------------------------------------------------------
 */

static void
Vmx86RegisterVMOnList(VMDriver *vm) // IN
{
   int vmID;
   VMDriver **vmp;

   ASSERT(HostIF_GlobalLockIsHeld());
   vmCount++;
   vmID = Vmx86AllocVMID();
   ASSERT(vm->userID == 0);
   vm->userID = vmID + 1;
   ASSERT(vm->userID > 0);

   for (vmp = &vmDriverList; *vmp != NULL; vmp = &(*vmp)->nextDriver) {
      if (*vmp == vm) {
         Warning("VM already registered on the list of VMs.\n");
         return;
      }
   }
   *vmp = vm;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86DeleteVMFromList --
 *
 *      Delete a VM from the list of registered VMs and decrement
 *      the count of VMs. This function should be called on any
 *      VM registered on the VMDriverList before invoking
 *      Vmx86FreeAllVMResources to free its memory.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Remove VM from linked list.
 *	Decrement count of VMs.
 *
 *----------------------------------------------------------------
 */

static void
Vmx86DeleteVMFromList(VMDriver *vm)
{
   VMDriver **vmp;

   ASSERT(vm);
   ASSERT(HostIF_GlobalLockIsHeld());
   for (vmp = &vmDriverList; *vmp != vm; vmp = &(*vmp)->nextDriver) {
      if (*vmp == NULL) {
         Warning("VM is not on the list of registered VMs.\n");
         return;
      }
   }
   *vmp = vm->nextDriver;
   vmCount--;

   Vmx86FreeVMID(vm->userID - 1);
   numLockedPages -= vm->memInfo.locked;

   /*
    * If no VM is running, reset the configured locked-page limit so
    * that the next VM to power on sets it appropriately.
    */

   if (vmCount == 0) {
      lockedPageLimit.configured = 0;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86FreeAllVMResources
 *
 *     Free the resources allocated for a vm that is not registered
 *     on the VMDriverList.  Except in the case of Vmx86_CreateVM(),
 *     this should be called only after a call to Vmx86DeleteVMFromList().
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Memory freed.
 *
 *----------------------------------------------------------------------
 */

static void
Vmx86FreeAllVMResources(VMDriver *vm)
{
   ASSERT(!HostIF_GlobalLockIsHeld());
   if (vm) {
      ASSERT(!Vmx86VMIsRegistered(vm, TRUE));

      Vmx86_SetHostClockRate(vm, 0);

      HostIF_FreeAllResources(vm);

      HostIF_FreeKernelMem(vm);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86ReserveFreePages --
 *
 *       Returns TRUE and increases locked page counts if the vm can lock
 *       more pages.  This is true if we are below the host's hard memory
 *       limit and this vm has not exceeded its maximum allocation.
 *       The function is thread-safe.
 *
 *       If ignoreLimits is TRUE then additional pages may be reserved even
 *       if limits are violated. The request to ignore limits may come in
 *       cases of anonymous page allocations. Swapping is not always possible 
 *       at those points but a swap target will have been posted so that the
 *       vmm will release memory shortly allowing the excessive reservation 
 *       to be reduced.
 *
 * Results:
 *       TRUE if pages are reserved for locking, FALSE otherwise
 *
 * Side effects:
 *       The global lock and VM's lock are acquired and released.
 *
 *----------------------------------------------------------------------
 */

static Bool
Vmx86ReserveFreePages(VMDriver *vm,
		      unsigned int numPages,
                      Bool ignoreLimits)
{
   Bool retval = FALSE;
   int retries = 3;

   ASSERT(vm);

   for (retries = 3; !retval && (retries > 0); retries--) {
      HostIF_GlobalLock(17);
      HostIF_VMLock(vm, 0);

      // Check VM's limit and don't wait.
      retval = Vmx86HasFreePages(vm, numPages, TRUE);
      if (!retval) {
         HostIF_VMUnlock(vm, 0);
         HostIF_GlobalUnlock(17);
	 break;
      } else {
	 // Wait to satisfy the global limit.
	 retval = Vmx86HasFreePages(vm, numPages, FALSE);
	 if (retval) {
	    numLockedPages += numPages;
	    vm->memInfo.locked += numPages;
            HostIF_VMUnlock(vm, 0);
	    HostIF_GlobalUnlock(17);
	    break;
	 } else {
            /*
             * There are not enough pages -- drop the locks and wait for
             * the host and/or other VMs to produce free pages.
	     */

            HostIF_VMUnlock(vm, 0);
	    HostIF_GlobalUnlock(17);
	    HostIF_WaitForFreePages(10);
	 }
      }
   }

   if (!retval && ignoreLimits) {
      HostIF_GlobalLock(17);
      HostIF_VMLock(vm, 0);
      numLockedPages += numPages;
      vm->memInfo.locked += numPages;
      HostIF_VMUnlock(vm, 0);
      HostIF_GlobalUnlock(17);
      retval = TRUE;
   }

   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86UnreserveFreePages --
 *
 *       Decreases the global and VM's locked page counts.
 *       The function is thread-safe.
 *
 * Results:
 *       void
 *
 * Side effects:
 *       The global lock and VM's lock are acquired and released.
 *
 *----------------------------------------------------------------------
 */

static void
Vmx86UnreserveFreePages(VMDriver *vm,
			unsigned int numPages)
{
   ASSERT(vm);

   HostIF_GlobalLock(18);
   HostIF_VMLock(vm, 1);

   ASSERT(numLockedPages >= numPages);
   ASSERT(vm->memInfo.locked >= numPages);

   numLockedPages -= numPages;
   vm->memInfo.locked -= numPages;

   HostIF_VMUnlock(vm, 1);
   HostIF_GlobalUnlock(18);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86_CreateVM --
 *
 *      Allocate and initialize a driver structure for a virtual machine.
 *
 * Results:
 *      VMDriver structure or NULL on error.
 *
 * Side effects:
 *      May allocate kernel memory.
 *
 *-----------------------------------------------------------------------------
 */

VMDriver *
Vmx86_CreateVM(void)
{
   VMDriver *vm;
   Vcpuid v;

   vm = HostIF_AllocKernelMem(sizeof *vm, TRUE);
   if (vm == NULL) {
      return NULL;
   }
   memset(vm, 0, sizeof *vm);

   vm->userID = 0;
   vm->memInfo.admitted = FALSE;
   vm->fastSuspResFlag = 0;
   for (v = 0; v < MAX_INITBLOCK_CPUS; v++) {
      vm->currentHostCpu[v] = INVALID_PCPU;
   }

   if (HostIF_Init(vm)) {
      goto cleanup;
   }

   HostIF_GlobalLock(0);

#ifdef _WIN32
   if (vmCount >= MAX_VMS_WIN32) {
      HostIF_GlobalUnlock(0);
      goto cleanup;
   }
#endif
   if (vmCount >= MAX_VMS) {
      HostIF_GlobalUnlock(0);
      goto cleanup;
   }

   Vmx86RegisterVMOnList(vm);

   HostIF_GlobalUnlock(0);

   return vm;

cleanup:
   /*
    * The VM is not on a list, "vmCount" has not been incremented,
    * "vm->cowID" is INVALID_VMID, and either the VM's mutex hasn't
    * been initialized or we've only taken the global lock and checked
    * a counter since, so we know that the VM has not yet locked any
    * pages.
    */

   ASSERT(vm->memInfo.locked == 0);
   Vmx86FreeAllVMResources(vm);

   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_LookupUserMPN --
 *
 *      Look up the MPN of a locked user page by user VA under the VM lock.
 *
 * Results:
 *      A status code and the MPN on success.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

int
Vmx86_LookupUserMPN(VMDriver *vm, // IN: VMDriver
                    VA64 uAddr,   // IN: user VA of the page
                    MPN *mpn)     // OUT
{
   int ret;
   HostIF_VMLock(vm, 38);
   ret = HostIF_LookupUserMPN(vm, uAddr, mpn);
   HostIF_VMUnlock(vm, 38);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_ReleaseVM  --
 *
 *      Release a VM (either created here or from a bind).
 *
 * Results:
 *      zero if successful
 *
 * Side effects:
 *      Decrement VM reference count.
 *      Release resources (those that are left) when count reaches 0.
 *
 *----------------------------------------------------------------------
 */

int
Vmx86_ReleaseVM(VMDriver *vm)  // IN:
{
   ASSERT(vm);
   HostIF_GlobalLock(1);
   Vmx86DeleteVMFromList(vm);
   HostIF_GlobalUnlock(1);
   Vmx86FreeAllVMResources(vm);

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_Open --
 *
 *      Called on open of the fd.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Bumps fdCount.
 *
 *----------------------------------------------------------------------
 */

void
Vmx86_Open(void)
{
   HostIF_GlobalLock(123);
   ASSERT(fdCount < MAX_INT32);
   if (fdCount < MAX_INT32) {
      fdCount++;
   }
   HostIF_GlobalUnlock(123);
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_Close --
 *
 *      Called on close of the fd.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Decrements fdCount
 *	May de-initialize ptsc.
 *
 *----------------------------------------------------------------------
 */

void
Vmx86_Close(void)
{
   HostIF_GlobalLock(124);

   /*
    * If fdCount hits MAX_INT32 saturate the counter and leave it at
    * MAX_INT32.
    */

   ASSERT(fdCount > 0);
   if (fdCount < MAX_INT32) {
      fdCount--;
   }

   /*
    * If no VMs are running and there are no open file handles, reset the
    * pseudo TSC state so that the next VM to initialize is free to
    * initialize the system wide PTSC however it wants.  See PR 403505.
    */

   if (fdCount == 0) {
      ASSERT(vmCount == 0);
      pseudoTSC.initialized = FALSE;
   }
   HostIF_GlobalUnlock(124);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86_InitVM --
 *
 *    Initializaiton of the VM.  Expects all initial arguments
 *    to be part of the InitBlock structure.
 *
 * Results:
 *    0 on success
 *    != 0 on failure
 *
 * Side effects:
 *    Many
 *
 *-----------------------------------------------------------------------------
 */

int
Vmx86_InitVM(VMDriver *vm,          // IN
             InitBlock *initParams) // IN/OUT: Initial params from the VM
{
   int retval;

   if (initParams->magicNumber != INIT_BLOCK_MAGIC) {
      Warning("Bad magic number for init block 0x%x\n",
              initParams->magicNumber);

      return 1;
   }
   if (initParams->numVCPUs > MAX_INITBLOCK_CPUS) {
      Warning("Too many VCPUs for init block %d\n", initParams->numVCPUs);

      return 1;
   }
   vm->numVCPUs = initParams->numVCPUs;

   HostIF_InitFP(vm);

   /*
    * Initialize the driver's part of the cross-over page used to
    * talk to the monitor
    */

   retval = Task_InitCrosspage(vm, initParams);
   if (retval) {
      Warning("Task crosspage init died with retval=%d\n", retval);
      /*
       *  Note that any clean-up of resources will be handled during
       *  power-off when Vmx86_ReleaseVM() is called as part of
       *  MonitorLoop_PowerOff().
       */

      return 1;
   }

   /*
    *  Check if we want to arbitrarily fail every N VM initializations.
    *  Useful in testing PR 72482.
    */

   if (initParams->vmInitFailurePeriod != 0) {
      static uint32 counter = 0;

      if ((++counter) % initParams->vmInitFailurePeriod == 0) {
         Warning("VM initialization failed on %d iteration\n", counter);

         return 1;
      }
   }

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_ReadTSCAndUptime --
 *
 *      Atomically read the TSC and the uptime.
 *
 * Results:
 *      The current TSC and uptime values.
 *
 * Side effects:
 *      none
 *
 *
 *----------------------------------------------------------------------
 */

void
Vmx86_ReadTSCAndUptime(VmTimeStart *st)	// OUT: return value
{
   uintptr_t flags;

   SAVE_FLAGS(flags);
   CLEAR_INTERRUPTS();

   st->count = RDTSC();
   st->time = HostIF_ReadUptime();

   RESTORE_FLAGS(flags);
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_ComputekHz --
 *
 *      Given aggregate cycles and system uptime, computes cycle rate as,
 *
 *      khz = cycles / (uptime / HostIF_UptimeFrequency()) / 1000
 *
 *      We need to do the computation carefully to avoid overflow or
 *      undue loss of precision.  Also, on Linux we can't do a
 *      64/64=64 bit division directly, as the gcc stub for that
 *      is not linked into the kernel.
 *
 * Results:
 *      Returns the computed khz value, or 0 if uptime == 0.
 *
 * Side effects:
 *      none
 *
 *----------------------------------------------------------------------
 */

uint32
Vmx86_ComputekHz(uint64 cycles, uint64 uptime)
{
   uint64 hz;
   uint64 freq;

   freq = HostIF_UptimeFrequency();
   while (cycles > MAX_UINT64 / freq) {
      cycles >>= 1;
      uptime >>= 1;
   }
 
   if (uptime == 0) {
      return 0;
   }

   hz  = (cycles * freq) / uptime;
   return (uint32) ((hz + 500) / 1000);
}


#ifdef __APPLE__
/*
 *----------------------------------------------------------------------
 *
 * Vmx86GetBusyKHzEstimate
 *
 *      Return an estimate the of the processor's kHz rating, based on
 *      a spinloop.  This is especially useful on systems where the TSC
 *      is known to run at its maximum rate when we are using the CPU.
 *      As of 2006, Intel Macs are this way... the TSC rate is 0 if the
 *      CPU is in a deep enough sleep state, or at its max rate otherwise.
 *
 * Results:
 *      Processor speed in kHz.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE_SINGLE_CALLER uint32
Vmx86GetBusyKHzEstimate(void)
{
   static const int ITERS = 100;
   static const int CYCLES_PER_ITER = 20000;
   int i;
   uint64 j;
   uint64 aggregateCycles = 0;
   uint64 aggregateUptime = 0;

   for (i = 0; i < ITERS; i++) {
      NO_INTERRUPTS_BEGIN() {
         aggregateCycles -= RDTSC();
         aggregateUptime -= HostIF_ReadUptime();
         for (j = RDTSC() + CYCLES_PER_ITER; RDTSC() < j; )
            ;
         aggregateCycles += RDTSC();
         aggregateUptime += HostIF_ReadUptime();
      } NO_INTERRUPTS_END();
   }

   return Vmx86_ComputekHz(aggregateCycles, aggregateUptime);
}
#else // ifdef __APPLE__


/*
 *----------------------------------------------------------------------
 *
 * Vmx86GetkHzEstimate
 *
 *      Return an estimate of the processor's kHz rating, based on
 *      the ratio of the cycle counter and system uptime since the
 *      driver was loaded.
 *      This function could be called (on Windows) at IRQL DISPATCH_LEVEL.
 *
 *----------------------------------------------------------------------
 */

static INLINE_SINGLE_CALLER uint32
Vmx86GetkHzEstimate(VmTimeStart *st)	// IN: start time
{
   uint64 cDiff, tDiff;
   uintptr_t flags;

   SAVE_FLAGS(flags);
   CLEAR_INTERRUPTS();
   cDiff = RDTSC() - st->count;
   tDiff = HostIF_ReadUptime() - st->time;
   RESTORE_FLAGS(flags);

   return Vmx86_ComputekHz(cDiff, tDiff);
}
#endif // ifdef __APPLE__


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_GetkHzEstimate
 *
 *      Return an estimate of the processor's kHz rating, based on
 *      the ratio of the cycle counter and system uptime since the
 *      driver was loaded.  Or based on a spinloop.
 *
 *      This function could be called (on Windows) at IRQL DISPATCH_LEVEL.
 *
 * Results:
 *      Processor speed in kHz.
 *
 * Side effects:
 *      Result is cached.
 *
 *----------------------------------------------------------------------
 */

uint32
Vmx86_GetkHzEstimate(VmTimeStart *st)	// IN: start time
{
   static uint32 kHz;

   /*
    * Cache and return the first result for consistency.
    * TSC values can be changed without notification.
    * TSC frequency can be vary too (SpeedStep, slowing clock on HALT, etc.)
    */
   if (kHz != 0) {
      return kHz;
   }

#ifdef __APPLE__
   return kHz = Vmx86GetBusyKHzEstimate();
#else
   return kHz = Vmx86GetkHzEstimate(st);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_SetHostClockRate --
 *
 *      The monitor wants to poll for events at the given rate. If no VM
 *      is specified, then 'rate' is ignored and the last set rate is set
 *      again.
 *
 * Results:
 *      0 for success, host-specific error code for failure.
 *
 * Side effects:
 *      May increase the host timer interrupt rate, etc.
 *
 *----------------------------------------------------------------------
 */

int
Vmx86_SetHostClockRate(VMDriver *vm,  // IN: VM instance pointer
                       unsigned rate) // IN: rate in Hz
{
   unsigned newGlobalRate;
   VMDriver *cur;
   int retval = 0;

   if (!vm) {
      Log("Resetting last set host clock rate of %d\n", globalFastClockRate);
      HostIF_FastClockLock(0);
      retval = HostIF_SetFastClockRate(globalFastClockRate);
      HostIF_FastClockUnlock(0);

      return retval;
   }

   /* Quick test before locks are acquired. */
   if (vm->fastClockRate == rate) {
      return retval;
   }

   HostIF_FastClockLock(2);
   if (vm->fastClockRate == rate) {
      HostIF_FastClockUnlock(2);

      return retval;
   }

   /*
    * Loop through all vms to find new max rate.
    */
   newGlobalRate = rate;
   HostIF_GlobalLock(19);
   for (cur = vmDriverList; cur != NULL; cur = cur->nextDriver) {
      if (cur != vm && cur->fastClockRate > newGlobalRate) {
         newGlobalRate = cur->fastClockRate;
      }
   }
   HostIF_GlobalUnlock(19);

   if (newGlobalRate != globalFastClockRate) {
      retval = HostIF_SetFastClockRate(newGlobalRate);
      if (!retval) {
         globalFastClockRate = newGlobalRate;
      }
   }
   if (!retval) {
      vm->fastClockRate = rate;
   }
   HostIF_FastClockUnlock(2);

   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_MonTimerIPI --
 *
 *      Check for VCPUs that are in the monitor and need an IPI to fire
 *      their next MonTimer callback.  Should be called once per fast
 *      timer interrupt if the fast timer is in use.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May send IPIs.
 *
 *----------------------------------------------------------------------
 */

void
Vmx86_MonTimerIPI(void)
{
   VMDriver *vm;
   VmAbsoluteTS pNow, expiry;

   /*
    * Loop through all vms -- needs the global lock to protect vmDriverList.
    */

   HostIF_GlobalLock(21);

   pNow = Vmx86_GetPseudoTSC();

   for (vm = vmDriverList; vm != NULL; vm = vm->nextDriver) {
      Vcpuid v;
      VCPUSet expiredVCPUs;
      VCPUSet_Empty(&expiredVCPUs);

      for (v = 0; v < vm->numVCPUs; v++) {
         VMCrossPage *crosspage = vm->crosspage[v];

         if (!crosspage) {
            continue;  // VCPU is not initialized yet
         }
         expiry = crosspage->crosspageData.monTimerExpiry;
         if (expiry != 0 && expiry <= pNow) {
            VCPUSet_Include(&expiredVCPUs, v);
         }
      }
      if (!VCPUSet_IsEmpty(&expiredVCPUs) &&
          HostIF_IPI(vm, &expiredVCPUs) == IPI_BROADCAST) {
         // no point in doing a broadcast for more than one VM.
         break;
      }
   }
   HostIF_GlobalUnlock(21);
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_GetNumVMs  --
 *
 *      Return the number of VMs.
 *
 * Results:
 *      The number of VMs.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int32
Vmx86_GetNumVMs(void)
{
   return vmCount;
}


int32
Vmx86_GetTotalMemUsage(void)
{
   VMDriver *vm;
   int totalmem = 0;

   HostIF_GlobalLock(15);
   vm = vmDriverList;

   for (vm = vmDriverList; vm != NULL; vm = vm->nextDriver) {
      /*
       * The VM lock is not strictly necessary as the vm will
       * stay on the list until we release the global lock and
       * because of order in which "admitted" and "mainMemSize"
       * are set when each VM is admitted.
       */

      if (vm->memInfo.admitted) {
          totalmem += PAGES_2_MBYTES(ROUNDUP(vm->memInfo.mainMemSize,
                                             MBYTES_2_PAGES(1)));
      }
   }

   HostIF_GlobalUnlock(15);

   return totalmem;
}


static INLINE unsigned
Vmx86MinAllocationFunc(unsigned nonpaged,
                       unsigned anonymous,
                       unsigned mainmem,
                       unsigned memPct)
{
   return RatioOf(memPct, mainmem, 100) + nonpaged + anonymous;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86MinAllocation --
 *
 *      Computes the minimum number of pages that must be allocated to a
 *      specific vm.  The minAllocation for a vm is defined as
 *      some percentage of guest memory plus 100% of nonpagable (overhead)
 *      memory.
 *
 * Results:
 *	The minAllocation for this vm.
 *
 *
 * Side effects:
 *      Analyzes the vm info, requiring the vm lock.
 *
 *----------------------------------------------------------------------
 */

static INLINE unsigned
Vmx86MinAllocation(VMDriver *vm,
                   unsigned memPct)
{
   ASSERT(HostIF_VMLockIsHeld(vm));

   return Vmx86MinAllocationFunc(vm->memInfo.nonpaged, vm->memInfo.anonymous,
                                 vm->memInfo.mainMemSize, memPct);
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86CalculateGlobalMinAllocation --
 *
 *      Computes the sum of minimum allocations of each vm assuming a given
 *      percentage of guest memory must fit within host RAM.
 *
 * Results:
 *	Number of pages that must fit within host ram for a given overcommit
 *      level.
 *
 *
 * Side effects:
 *      None. The actual minAllocations of each vm are NOT updated during
 *      this computation.
 *
 *----------------------------------------------------------------------
 */

static unsigned
Vmx86CalculateGlobalMinAllocation(unsigned memPct)
{
   VMDriver *vm;
   unsigned minAllocation = 0;

   ASSERT(HostIF_GlobalLockIsHeld());
   /* Pages of other vms required to fit inside the hard limit. */
   for (vm = vmDriverList; vm; vm = vm->nextDriver) {
      HostIF_VMLock(vm, 2);
      if (vm->memInfo.admitted) {
         minAllocation += Vmx86MinAllocation(vm, memPct);
      }
      HostIF_VMUnlock(vm, 2);
   }

   return minAllocation;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86UpdateMinAllocations --
 *
 *      Updates the minimum allocation for each vm based on the global
 *      overcommitment percentage.
 *
 * Results:
 *      minAllocations for vms are changed.
 *
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE_SINGLE_CALLER void
Vmx86UpdateMinAllocations(unsigned memPct)  // IN:
{
   VMDriver *vm;

   ASSERT(HostIF_GlobalLockIsHeld());
   /* Pages of other vms required to fit inside the hard limit. */
   for (vm = vmDriverList; vm; vm = vm->nextDriver) {
      HostIF_VMLock(vm, 3);
      if (vm->memInfo.admitted) {
         vm->memInfo.minAllocation = Vmx86MinAllocation(vm, memPct);
      }
      HostIF_VMUnlock(vm, 3);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_SetConfiguredLockedPagesLimit --
 *
 *      Set the user defined limit on the number of pages that can
 *      be locked.  This limit can be raised at any time but not lowered.
 *      This avoids having a user lower the limit as vms are running and
 *      inadvertently cause the vms to crash because of memory starvation.
 *
 * Results:
 *      Returns TRUE on success and FALSE on failure to set the limit
 *
 * Side effects:
 *      Hard limit may be changed.
 *
 *----------------------------------------------------------------------
 */

Bool
Vmx86_SetConfiguredLockedPagesLimit(unsigned limit)  // IN:
{
   Bool retval = FALSE;

   HostIF_GlobalLock(4);
   if (limit >= lockedPageLimit.configured) {
      lockedPageLimit.configured = limit;
      retval = TRUE;
   }
   HostIF_GlobalUnlock(4);

   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_SetDynamicLockedPageLimit --
 *
 *      Set the dynamic locked page limit.  This limit is determined by
 *      authd in response to host pressure.  It can be both raised and
 *      lowered at any time.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Hard limit may be changed.
 *
 *----------------------------------------------------------------------
 */

void
Vmx86_SetDynamicLockedPagesLimit(unsigned limit)  // IN:
{
   HostIF_GlobalLock(11);
   lockedPageLimit.dynamic = limit;
   HostIF_GlobalUnlock(11);
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_LockPage --
 *
 *      Lock a page.
 *
 * Results:
 *      A PAGE_LOCK_* status code and the MPN of the locked page on success.
 *
 * Side effects:
 *      Number of global and per-VM locked pages increased.
 *
 *----------------------------------------------------------------------
 */

int
Vmx86_LockPage(VMDriver *vm,                 // IN: VMDriver
               VA64 uAddr,                   // IN: VA of the page to lock
               Bool allowMultipleMPNsPerVA,  // IN: allow locking many pages with the same VA
               MPN *mpn)                     // OUT
{
   int retval;

   /* Atomically check and reserve locked memory */
   if (!Vmx86ReserveFreePages(vm, 1, FALSE)) {
      return PAGE_LOCK_LIMIT_EXCEEDED;
   }

   HostIF_VMLock(vm, 4);
   retval = HostIF_LockPage(vm, uAddr, allowMultipleMPNsPerVA, mpn);
   HostIF_VMUnlock(vm, 4);

   if (retval != PAGE_LOCK_SUCCESS) {
      Vmx86UnreserveFreePages(vm, 1);
   }

   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_UnlockPage --
 *
 *      Unlock a page.
 *
 * Results:
 *      A PAGE_UNLOCK_* status code.
 *
 * Side effects:
 *      Number of global and per-VM locked pages decreased.
 *
 *----------------------------------------------------------------------
 */

int
Vmx86_UnlockPage(VMDriver *vm, // IN
                 VA64 uAddr)   // IN
{
   int retval;

   HostIF_VMLock(vm, 5);
   retval = HostIF_UnlockPage(vm, uAddr);
   HostIF_VMUnlock(vm, 5);

   if (retval == PAGE_UNLOCK_SUCCESS) {
      Vmx86UnreserveFreePages(vm, 1);
   }

   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_UnlockPageByMPN --
 *
 *      Unlock a page.
 *
 * Results:
 *      A PAGE_UNLOCK_* status code.
 *
 * Side effects:
 *      Number of global and per-VM locked pages decreased.
 *
 *----------------------------------------------------------------------
 */

int
Vmx86_UnlockPageByMPN(VMDriver *vm, // IN: VMDriver
                      MPN mpn,      // IN: the page to unlock
                      VA64 uAddr)   // IN: optional valid VA for this MPN
{
   int retval;

   HostIF_VMLock(vm, 6);
   retval = HostIF_UnlockPageByMPN(vm, mpn, uAddr);
   HostIF_VMUnlock(vm, 6);

   if (retval == PAGE_UNLOCK_SUCCESS) {
      Vmx86UnreserveFreePages(vm, 1);
   }

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86_AllocLockedPages --
 *
 *      Allocate physical locked pages from the kernel.
 *
 *      Initially the pages are not mapped to any user or kernel
 *      address space.
 *
 * Results:
 *      Non-negative value on partial/full completion: actual number of
 *      allocated MPNs. MPNs of the allocated pages are copied to the
 *      caller's buffer at 'addr'.
 *
 *	Negative system specific error code on error (NTSTATUS on Windows,
 *      etc.)
 *
 * Side effects:
 *      Number of global and per-VM locked pages is increased.
 *
 *-----------------------------------------------------------------------------
 */

int
Vmx86_AllocLockedPages(VMDriver *vm,	     // IN: VMDriver
		       VA64 addr,	     // OUT: VA of an array for
                                             //      allocated MPNs.
		       unsigned numPages,    // IN: number of pages to allocate
		       Bool kernelMPNBuffer, // IN: is the MPN buffer in kernel
                                             //     or user address space?
                       Bool ignoreLimits)    // IN: should limits be ignored?
{
   int allocatedPages;

   if (!Vmx86ReserveFreePages(vm, numPages, ignoreLimits)) {
      // XXX What kind of system-specific error code is that? --hpreg
      return PAGE_LOCK_LIMIT_EXCEEDED;
   }

   HostIF_VMLock(vm, 7);
   allocatedPages = HostIF_AllocLockedPages(vm, addr, numPages,
                                            kernelMPNBuffer);
   HostIF_VMUnlock(vm, 7);

   if (allocatedPages < 0) {
      Vmx86UnreserveFreePages(vm, numPages);
   } else if (allocatedPages < numPages) {
      Vmx86UnreserveFreePages(vm, numPages - allocatedPages);
   }

   return allocatedPages;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_FreeLockedPages --
 *
 *      Frees physical locked pages from the kernel previosly allocated
 *      by Vmx86_AllocLockedPages().
 *
 * Results:
 *	0 on success,
 *	non-0 system specific error code on error (NTSTATUS on Windows, etc.)
 *
 * Side effects:
 *      Number of global and per-VM locked pages is decreased.
 *
 *----------------------------------------------------------------------
 */

int
Vmx86_FreeLockedPages(VMDriver *vm,	    // IN: VM instance pointer
		      VA64 addr,            // IN: user or kernel array of MPNs to free
		      unsigned numPages,    // IN: number of pages to free
		      Bool kernelMPNBuffer) // IN: is the MPN buffer in kernel or user address space?
{
   int ret;

   HostIF_VMLock(vm, 8);
   ret = HostIF_FreeLockedPages(vm, addr, numPages, kernelMPNBuffer);
   HostIF_VMUnlock(vm, 8);

   if (ret == 0) {
      Vmx86UnreserveFreePages(vm, numPages);
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_GetNextAnonPage --
 *
 *      Queries the driver to retrieve the list of anonymous pages. 
 *      A supplied value of INVALID_MPN will start the query from
 *      the head of the list. Callers supply the previously received
 *      mpn to retrieve the next in the chain. Note: There is no
 *      guarantee of coherency. 
 *
 * Results:
 *      A valid mpn or INVALID_MPN if the list has been exhausted.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

MPN
Vmx86_GetNextAnonPage(VMDriver *vm,       // IN: VM instance pointer
                      MPN mpn)            // IN: MPN
{
   MPN ret;

   HostIF_VMLock(vm, 22);
   ret = HostIF_GetNextAnonPage(vm, mpn);
   HostIF_VMUnlock(vm, 22);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_GetLockedPageList --
 *
 *      puts MPNs of pages that were allocated by HostIF_AllocLockedPages()
 *      into user mode buffer.
 *
 * Results:
 *	non-negative number of the MPNs in the buffer on success.
 *	negative error code on error.
 *
 * Side effects:
 *      none
 *
 *----------------------------------------------------------------------
 */

int
Vmx86_GetLockedPageList(VMDriver *vm,          // IN: VM instance pointer
                        VA64 uAddr,            // OUT: user mode buffer for MPNs
		        unsigned int numPages) // IN: size of the buffer in MPNs
{
   int ret;

   HostIF_VMLock(vm, 9);
   ret = HostIF_GetLockedPageList(vm, uAddr, numPages);
   HostIF_VMUnlock(vm, 9);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_GetMemInfo --
 *
 *      Return the info about all VMs. 
 *
 * Results:
 *      TRUE if all info was successfully copied.
 *
 * Side effects:
 *      VMGetMemInfoArgs is filled in. If the supplied curVM is null 
 *      then only the baseline information will be returned. Calling
 *      with a null curVM may return results for maxLockedPages 
 *      that differ from those  when the vm is passed if huge pages 
 *      are in use. 
 *
 *----------------------------------------------------------------------
 */

Bool
Vmx86_GetMemInfo(VMDriver *curVM,
                 Bool curVMOnly,
                 VMMemInfoArgs *outArgs,
                 int outArgsLength)
{
   VMDriver *vm;
   int outSize;
   int wantedVMs;

   HostIF_GlobalLock(7);

   if (curVMOnly) {
      wantedVMs = 1;
   } else {
      wantedVMs = vmCount;
   }

   outSize = VM_GET_MEM_INFO_SIZE(wantedVMs);
   if (outSize > outArgsLength) {
      HostIF_GlobalUnlock(7);

      return FALSE;
   }

   outArgs->numVMs = wantedVMs;
   outArgs->numLockedPages = numLockedPages;
   outArgs->maxLockedPages = Vmx86LockedPageLimit(curVM);
   outArgs->lockedPageLimit = lockedPageLimit;
   outArgs->globalMinAllocation = Vmx86CalculateGlobalMinAllocation(minVmMemPct);
   outArgs->minVmMemPct = minVmMemPct;
   outArgs->callerIndex = (uint32)-1;
   outArgs->currentTime = HostIF_ReadUptime() / HostIF_UptimeFrequency();

   if (curVM == NULL) {
      HostIF_GlobalUnlock(7);

      return TRUE;
   }

   curVM->memInfo.timestamp = outArgs->currentTime;
   if (wantedVMs == 1) {
      outArgs->memInfo[0] = curVM->memInfo;
      outArgs->callerIndex = 0;
   } else {
      int i;
      for (i = 0, vm = vmDriverList;
           vm != NULL && i < vmCount;
           i++, vm = vm->nextDriver) {
         if (vm == curVM) {
            outArgs->callerIndex = i;
         }
         HostIF_VMLock(vm, 10);
         outArgs->memInfo[i] = vm->memInfo;
         HostIF_VMUnlock(vm, 10);
      }
   }

   HostIF_GlobalUnlock(7);
   if (outArgs->callerIndex == -1) {
      return FALSE;
   }
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86SetMemoryUsage --
 *
 *      Updates the paged, nonpaged, and anonymous memory reserved memory 
 *      values for the vm.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static void
Vmx86SetMemoryUsage(VMDriver *curVM,       // IN/OUT
                    unsigned paged,        // IN
                    unsigned nonpaged,     // IN
                    unsigned anonymous,    // IN
                    unsigned aminVmMemPct) // IN
{
   ASSERT(HostIF_VMLockIsHeld(curVM));
   curVM->memInfo.paged         = paged;
   curVM->memInfo.nonpaged      = nonpaged;
   curVM->memInfo.anonymous     = anonymous;
   curVM->memInfo.minAllocation = Vmx86MinAllocation(curVM, aminVmMemPct);
   curVM->memInfo.maxAllocation = curVM->memInfo.mainMemSize + nonpaged +
                                  anonymous;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_Admit --
 *
 *      Set the memory management information about this VM and handles
 *      admission control. We allow vm to power on if there is room for
 *      the minimum allocation for all running vms in memory.  Note that
 *      the hard memory limit can change dynamically in windows so we
 *      don't have guarantees due to admission control.
 *
 * Results:
 *      Returns global information about the memory state in args as well
 *      as a value indicating whether or not the virtual machine was
 *      started.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

void
Vmx86_Admit(VMDriver *curVM,     // IN
            VMMemInfoArgs *args) // IN/OUT
{
   Bool allowAdmissionCheck = FALSE;
   unsigned int globalMinAllocation;

   HostIF_GlobalLock(9);

   /*
    * Update the overcommitment level and minimums for all vms if they can
    * fit under new minimum limit.  If they do not fit, do nothing.  And of
    * course if existing VMs cannot fit under limit, likelihood that new VM
    * will fit in is zero.
    */

   globalMinAllocation = Vmx86CalculateGlobalMinAllocation(args->minVmMemPct);
   if (globalMinAllocation <= Vmx86LockedPageLimit(NULL)) {
      allowAdmissionCheck = TRUE;
      minVmMemPct = args->minVmMemPct;
      Vmx86UpdateMinAllocations(args->minVmMemPct);
   }

   HostIF_VMLock(curVM, 12);

   curVM->memInfo.shares = args->memInfo->shares;
   curVM->memInfo.touchedPct = 100;
   curVM->memInfo.dirtiedPct = 100;
   curVM->memInfo.mainMemSize = args->memInfo->mainMemSize;
   curVM->memInfo.perVMOverhead = args->memInfo->perVMOverhead;

  /*
   * Always set the allocations required for the current configuration
   * so that the user will know how bad situation really is with the
   * suggested percentage.
   */

  curVM->memInfo.admitted = FALSE;
  Vmx86SetMemoryUsage(curVM, args->memInfo->paged, args->memInfo->nonpaged,
                      args->memInfo->anonymous, args->minVmMemPct);
  if (allowAdmissionCheck &&
      globalMinAllocation + curVM->memInfo.minAllocation <=
         Vmx86LockedPageLimit(curVM)) {
      curVM->memInfo.admitted = TRUE;
   }

#if defined _WIN32
   if (curVM->memInfo.admitted) {
      unsigned int allocatedPages, nonpaged;
      signed int pages;
      MPN *mpns;

      /*
       * More admission control: Get enough memory for the nonpaged portion
       * of the VM.  Drop locks for this long operation.
       * XXX Timeout?
       */

      HostIF_VMUnlock(curVM, 12);
      HostIF_GlobalUnlock(9);

#define ALLOCATE_CHUNK_SIZE 64
      allocatedPages = 0;
      nonpaged = args->memInfo->nonpaged + args->memInfo->anonymous;
      mpns = HostIF_AllocKernelMem(nonpaged * sizeof *mpns, FALSE);
      if (mpns == NULL) {
         goto undoAdmission;
      }
      while (allocatedPages < nonpaged) {
         pages = Vmx86_AllocLockedPages(curVM,
                                        PtrToVA64(mpns + allocatedPages),
	                                MIN(ALLOCATE_CHUNK_SIZE, nonpaged - allocatedPages),
	                                TRUE,
                                        FALSE);
         if (pages <= 0) {
            break;
         }
         allocatedPages += pages;
      }

      /*
       * Free the allocated pages.
       * XXX Do not free the pages but hand them directly to the admitted VM.
       */

      for (pages = 0; pages < allocatedPages; pages += ALLOCATE_CHUNK_SIZE) {
         Vmx86_FreeLockedPages(curVM, PtrToVA64(mpns + pages),
                               MIN(ALLOCATE_CHUNK_SIZE, allocatedPages - pages), TRUE);
      }
      HostIF_FreeKernelMem(mpns);
#undef ALLOCATE_CHUNK_SIZE

undoAdmission:
      if (allocatedPages != nonpaged) {
          curVM->memInfo.admitted = FALSE; // undo admission
      }

      HostIF_GlobalLock(9);
      HostIF_VMLock(curVM, 12);
   }
#endif

   /* Return global state to the caller. */
   args->memInfo[0] = curVM->memInfo;
   args->numVMs = vmCount;
   args->numLockedPages = numLockedPages;
   args->maxLockedPages = Vmx86LockedPageLimit(curVM);
   args->lockedPageLimit = lockedPageLimit;
   args->globalMinAllocation = globalMinAllocation;
   HostIF_VMUnlock(curVM, 12);
   HostIF_GlobalUnlock(9);
}


Bool
Vmx86_Readmit(VMDriver *curVM, OvhdMem_Deltas *delta)
{
   unsigned globalMinAllocation, newMinAllocation;
   Bool retval = FALSE;
   int paged;
   int nonpaged;
   int anonymous;

   HostIF_GlobalLock(31);
   globalMinAllocation = Vmx86CalculateGlobalMinAllocation(minVmMemPct);
   HostIF_VMLock(curVM, 31);
   paged = curVM->memInfo.paged + delta->paged;
   nonpaged = curVM->memInfo.nonpaged + delta->nonpaged;
   anonymous = curVM->memInfo.anonymous + delta->anonymous;

   if (nonpaged >= 0 && paged >= 0 && anonymous >= 0) {
      globalMinAllocation -= Vmx86MinAllocation(curVM, minVmMemPct);
      newMinAllocation = Vmx86MinAllocationFunc(nonpaged, anonymous,
                                                curVM->memInfo.mainMemSize,
                                                minVmMemPct);
      if (globalMinAllocation + newMinAllocation <= Vmx86LockedPageLimit(curVM) ||
          (delta->paged <= 0 && delta->nonpaged <= 0 && delta->anonymous <= 0)) {
         Vmx86SetMemoryUsage(curVM, paged, nonpaged, anonymous, minVmMemPct);
         retval = TRUE;
      }
   }
   HostIF_VMUnlock(curVM, 31);
   HostIF_GlobalUnlock(31);

   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_UpdateMemInfo --
 *
 *      Updates information about this VM with the new data supplied in
 *      a patch.
 *
 * Results:
 *      Sets the memory usage by this vm based on its memSample data.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

void
Vmx86_UpdateMemInfo(VMDriver *curVM,
                    const VMMemMgmtInfoPatch *patch)
{
   ASSERT(patch->touchedPct <= 100 && patch->dirtiedPct <= 100);
   HostIF_VMLock(curVM, 13);
   curVM->memInfo.touchedPct = AsPercent(patch->touchedPct);
   curVM->memInfo.dirtiedPct = AsPercent(patch->dirtiedPct);
   curVM->memInfo.hugePageBytes = patch->hugePageBytes;
   HostIF_VMUnlock(curVM, 13);
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_PAEEnabled --
 *
 *      Is PAE enabled?
 *
 * Results:
 *      TRUE if PAE enabled.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
Vmx86_PAEEnabled(void)
{
   uintptr_t cr4;

   GET_CR4(cr4);

   return (cr4 & CR4_PAE) != 0;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_VMXEnabled --
 *
 *      Test the VMXE bit as an easy proxy for whether VMX operation
 *      is enabled.
 *
 * Results:
 *      TRUE if the CPU supports VT and CR4.VMXE is set.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
Vmx86_VMXEnabled(void)
{
   if (VT_CapableCPU()) {
      uintptr_t cr4;

      GET_CR4(cr4);

      return (cr4 & CR4_VMXE) != 0;
   } else {
      return FALSE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86LookupVMByUserIDLocked --
 *
 *      Lookup a VM by userID. The caller must hold the global lock.
 *
 * Returns:
 *      On success: Pointer to the driver's VM instance.
 *      On failure: NULL (not found).
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static VMDriver *
Vmx86LookupVMByUserIDLocked(int userID) // IN
{
   VMDriver *vm;

   ASSERT(HostIF_GlobalLockIsHeld());

   for (vm = vmDriverList; vm != NULL; vm = vm->nextDriver) {
      if (vm->userID == userID) {
         return vm;
      }
   }

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86_LookupVMByUserID --
 *
 *      Lookup a VM by userID.
 *
 * Returns:
 *      On success: Pointer to the driver's VM instance.
 *      On failure: NULL (not found).
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMDriver *
Vmx86_LookupVMByUserID(int userID) // IN
{
   VMDriver *vm;

   HostIF_GlobalLock(10);
   vm = Vmx86LookupVMByUserIDLocked(userID);
   HostIF_GlobalUnlock(10);

   return vm;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_FastSuspResSetOtherFlag --
 *
 *      Sets the value of other VM's fastSuspResFlag.
 *
 * Returns:
 *      TRUE if VM was found and flag was set successfully.
 *      FALSE if VM was not found.
 *
 * Side effects:
 *      The value we set the flag to is this VM's userID.
 *
 *----------------------------------------------------------------------
 */

Bool
Vmx86_FastSuspResSetOtherFlag(VMDriver *vm,      // IN
                              int otherVmUserId) // IN
{
   VMDriver *otherVM;

   HostIF_GlobalLock(35);
   otherVM = Vmx86LookupVMByUserIDLocked(otherVmUserId);
   if (otherVM != NULL) {
      ASSERT(otherVM->fastSuspResFlag == 0);
      otherVM->fastSuspResFlag = vm->userID;
   } else {
      Warning("otherVmUserId (%d) is invalid", otherVmUserId);
   }
   HostIF_GlobalUnlock(35);

   return otherVM != NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_FastSuspResGetMyFlag --
 *
 *      Gets the value of fastSuspResFlag. If blockWait is true, this
 *      function will not return until the flag is non-zero, or until
 *      timeout.
 *
 * Returns:
 *      The value of the flag which, if non-zero, should be the userID of
 *      the vm that set it.
 *
 * Side effects:
 *      The flag is reset to zero once read.
 *
 *----------------------------------------------------------------------
 */

int
Vmx86_FastSuspResGetMyFlag(VMDriver *vm,   // IN
                           Bool blockWait) // IN
{
   int retval = 0;
   int ntries = 1;
   const int waitInterval = 10;     /* Wait 10ms at a time. */
   const int maxWaitTime  = 100000; /* Wait maximum of 100 seconds. */

   if (blockWait) {
      ntries = maxWaitTime / waitInterval;
   }

   while (ntries--) {
      HostIF_GlobalLock(6);
      retval = vm->fastSuspResFlag;
      vm->fastSuspResFlag = 0;
      HostIF_GlobalUnlock(6);
      if (retval || !ntries) {
         break;
      }
      HostIF_Wait(waitInterval);
   }

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86EnableHVOnCPU --
 *
 *      Enable HV on the current CPU, if possible.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      HV will be enabled, if possible.
 *
 *-----------------------------------------------------------------------------
 */

static void
Vmx86EnableHVOnCPU(void)
{
   if (SVM_CapableCPU()) {
      uint64 vmCR = __GET_MSR(MSR_VM_CR);
      if (!SVM_LockedFromFeatures(vmCR)) {
         CPUIDRegs regs;
         __GET_CPUID(0x8000000A, &regs);
         if (CPUID_GET(0x8000000A, EDX, SVM_LOCK, regs.edx) != 0) {
            __SET_MSR(MSR_VM_CR, (vmCR & ~MSR_VM_CR_SVME_DISABLE) |
                                  MSR_VM_CR_SVM_LOCK);
         }
      }
   }
   if (VT_CapableCPU()) {
      uint64 featCtl = __GET_MSR(MSR_FEATCTL);
      if (!VT_LockedFromFeatures(featCtl)) {
         __SET_MSR(MSR_FEATCTL, featCtl | MSR_FEATCTL_LOCK | MSR_FEATCTL_VMXE);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86RefClockInCycles --
 *
 *    Convert the reference clock (HostIF_Uptime) to cycle units.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint64
Vmx86RefClockInCycles(uint64 uptime)
{
   return Mul64x3264(uptime, 
                     pseudoTSC.refClockToPTSC.ratio.mult,
                     pseudoTSC.refClockToPTSC.ratio.shift);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86RefClockToPTSC --
 *
 *    Convert from the reference clock (HostIF_Uptime) time to pseudo TSC.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint64
Vmx86RefClockToPTSC(uint64 uptime)
{
   return Vmx86RefClockInCycles(uptime) + 
      Atomic_Read64(&pseudoTSC.refClockToPTSC.add);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86_InitPseudoTSC --
 *
 *      Initialize the pseudo TSC state if it is not already initialized.
 *      If another vmx has initialized the pseudo TSC, then we continue to
 *      use the parameters specified by the first vmx.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      - Updates tscHz, the frequency of the PTSC in Hz. That frequency may
 *        differ from the value passed in if another VM is already running.
 *      - Updates the refClkToTSC parameters to be consistent with the tscHz
 *        value that's in use.
 *
 *-----------------------------------------------------------------------------
 */

void
Vmx86_InitPseudoTSC(PTSCInitParams *params) // IN/OUT
{
   VmTimeStart startTime;
   uint64 tsc, uptime;

   HostIF_GlobalLock(36);

   if (!pseudoTSC.initialized) {
      pseudoTSC.hz = params->tscHz;
      pseudoTSC.refClockToPTSC.ratio.mult  = params->refClockToPTSC.mult;
      pseudoTSC.refClockToPTSC.ratio.shift = params->refClockToPTSC.shift;

      Vmx86_ReadTSCAndUptime(&startTime);
      tsc    = startTime.count;
      uptime = startTime.time;

      /* Start Pseudo TSC at initialPTSC (usually 0). */
      pseudoTSC.tscOffset = params->initialPTSC - tsc;
      Atomic_Write64(&pseudoTSC.refClockToPTSC.add,
                     params->initialPTSC - Vmx86RefClockInCycles(uptime));

      /* forceRefClock gets priority. */
      pseudoTSC.useRefClock           = params->forceRefClock;
      pseudoTSC.neverSwitchToRefClock = params->forceTSC;
      pseudoTSC.hwTSCsSynced          = params->hwTSCsSynced;
      Log("PTSC: initialized at %"FMT64"u Hz using %s, TSCs are %ssynchronized.\n",
          pseudoTSC.hz, pseudoTSC.useRefClock ? "reference clock" : "TSC",
          pseudoTSC.hwTSCsSynced ? "" : "not ");

      pseudoTSC.initialized = TRUE;
   }
   /* 
    * Allow the calling vmx to respect ptsc.noTSC=TRUE config option
    * even if another vmx is already running (pseudoTSC was already
    * initialized).  Useful for testing.
    */
   if (params->forceRefClock) {
      Vmx86_SetPseudoTSCUseRefClock();
   }
   params->refClockToPTSC.mult  = pseudoTSC.refClockToPTSC.ratio.mult;
   params->refClockToPTSC.shift = pseudoTSC.refClockToPTSC.ratio.shift;
   params->refClockToPTSC.add   = Atomic_Read64(&pseudoTSC.refClockToPTSC.add);
   params->tscOffset    = pseudoTSC.tscOffset;
   params->tscHz        = pseudoTSC.hz;
   params->hwTSCsSynced = pseudoTSC.hwTSCsSynced;

   HostIF_GlobalUnlock(36);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86_GetPseudoTSC --
 *
 *    Read the pseudo TSC.  We prefer to implement the pseudo TSC using
 *    TSC.  On systems where the TSC varies its rate (e.g. Pentium M),
 *    stops advancing when the core is in deep sleep (e.g. Core 2 Duo),
 *    or the TSCs can get out of sync across cores (e.g. Opteron due to
 *    halt clock ramping, Core 2 Duo due to independent core deep sleep
 *    states; though WinXP does handle the Core 2 Duo out of sync case;
 *    and on IBM x-Series NUMA machines), we use a reference clock
 *    (HostIF_ReadUptime()) as the basis for pseudo TSC.
 *
 *    Note that we depend on HostIF_ReadUptime being a high resolution
 *    timer that is synchronized across all cores.
 *
 * Results:
 *    Current value of the PTSC.
 *
 *-----------------------------------------------------------------------------
 */

uint64
Vmx86_GetPseudoTSC(void)
{
   if (Vmx86_PseudoTSCUsesRefClock()) {
      return Vmx86RefClockToPTSC(HostIF_ReadUptime());
   }
   return RDTSC() + pseudoTSC.tscOffset;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86_CheckPseudoTSC --
 *
 *    Periodically called by userspace to check whether the TSC is
 *    reliable, using the reference clock as the trusted time source.
 *    If the TSC is unreliable, switch the basis of the PTSC from the
 *    TSC to the reference clock.
 *
 *    Also, recompute the "add" component of the reference clock to PTSC
 *    conversion, to periodically eliminate the drift between the two
 *    clocks.  That way, if the PTSC switches from using the TSC to the
 *    reference clock, PTSC will remain (roughly) continuous.  See PR
 *    547055.
 *
 *    Note that we might be executing concurrently with other threads,
 *    but it doesn't matter since we only ever go from using the TSC to
 *    using the reference clock, never the other direction.
 *
 * Results:
 *    TRUE if the PTSC is implemented by the reference clock.
 *    FALSE if the PTSC is implemented by the TSC.
 *
 * Side effects:
 *    May switch the basis of the PTSC from the TSC to the reference clock.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Vmx86_CheckPseudoTSC(uint64 *lastTSC, // IN/OUT: last/current value of the TSC
                     uint64 *lastRC)  // IN/OUT: last/current value of the reference clock
{
   VmTimeStart curTime;

   Vmx86_ReadTSCAndUptime(&curTime);

   if (pseudoTSC.initialized && *lastTSC && !Vmx86_PseudoTSCUsesRefClock()) {
      uint64 tsc, refClkTS, refClkLastTS;
      uint64 tscDiff, refClkDiff;

      tsc = curTime.count;

      refClkTS     = Vmx86RefClockInCycles(curTime.time);
      refClkLastTS = Vmx86RefClockInCycles(*lastRC);

      tscDiff    = tsc - *lastTSC;
      refClkDiff = refClkTS - refClkLastTS;

      if (((int64)tscDiff < 0) ||
          (tscDiff * 100 < refClkDiff * 95) ||
          (tscDiff * 95 > refClkDiff * 100)) {
         /*
          * TSC went backwards or drifted from the reference clock by
          * more than 5% over the last poll period.
          */
         Vmx86_SetPseudoTSCUseRefClock();
      } else {
         uint64 ptscFromTSC = tsc + pseudoTSC.tscOffset;
         Atomic_Write64(&pseudoTSC.refClockToPTSC.add, ptscFromTSC - refClkTS);
      }
   }
   *lastTSC = curTime.count;
   *lastRC  = curTime.time;

   return Vmx86_PseudoTSCUsesRefClock();
}


typedef struct {
   Atomic_uint32 index;
   MSRQuery *query;
} Vmx86GetMSRData;


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86GetMSR --
 *
 *      Collect MSR value on the current logical CPU.
 *
 *	Function must not block (it is invoked from interrupt context).
 *      Only VT MSRs are supported on VT-capable processors.
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
Vmx86GetMSR(void *clientData) // IN/OUT: A Vmx86GetMSRData *
{
   Vmx86GetMSRData *data = (Vmx86GetMSRData *)clientData;
   MSRQuery *query;
   uint32 index;
   int err;

   ASSERT(data);
   query = data->query;
   ASSERT(query);

   index = Atomic_ReadInc32(&data->index);
   if (index >= query->numLogicalCPUs) {
      return;
   }

   query->logicalCPUs[index].tag = HostIF_GetCurrentPCPU();

   /*
    * We treat BIOS_SIGN_ID (microcode version) specially on Intel,
    * where the preferred read sequence involves a macro.
    */

   if (CPUID_GetVendor() == CPUID_VENDOR_INTEL &&
       query->msrNum == MSR_BIOS_SIGN_ID) {
      /* safe to read: MSR_BIOS_SIGN_ID architectural since Pentium Pro */
      query->logicalCPUs[index].msrVal = INTEL_MICROCODE_VERSION();
      err = 0;
   } else {
      /*
       * Try to enable HV any time these MSRs are queried.  We have seen
       * buggy formware that forgets to re-enable HV after waking from
       * deep sleep. [PR 1020692]
       */
      if (query->msrNum == MSR_FEATCTL || query->msrNum == MSR_VM_CR) {
         Vmx86EnableHVOnCPU();
      }
      err = HostIF_SafeRDMSR(query->msrNum, &query->logicalCPUs[index].msrVal);
   }

   query->logicalCPUs[index].implemented = (err == 0) ? 1 : 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86_GetAllMSRs --
 *
 *      Collect MSR value on all logical CPUs.
 *
 *      The caller is responsible for ensuring that the requested MSR is valid
 *      on all logical CPUs.
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
Vmx86_GetAllMSRs(MSRQuery *query) // IN/OUT
{
   Vmx86GetMSRData data;

   Atomic_Write32(&data.index, 0);
   data.query = query;

   HostIF_CallOnEachCPU(Vmx86GetMSR, &data);

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
 * Vmx86_YieldToSet --
 *
 *      Yield the CPU until a vCPU from the requested set has run.
 *
 *      usecs is the total spin time in monitor.  Very low numbers
 *      indicate we detected there was a vCPU thread that was not
 *      in the monitor, so we didn't spin.  In that case, simply 
 *      nudge the threads we want and return.
 *
 * Results:
 *      The current CPU yields whenever possible.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
Vmx86_YieldToSet(VMDriver *vm,       // IN:
                 Vcpuid currVcpu,    // IN:
                 const VCPUSet *req, // IN:
                 uint32 usecs,       // IN:
                 Bool skew)          // IN:
{
   VCPUSet vcpus;

   if (VCPUSet_IsEmpty(req)) {
      return;
   }

#ifdef __APPLE__
   if (skew) {
      /* Mac scheduler yield does fine in the skew case */
      (void)thread_block(THREAD_CONTINUE_NULL);
      return;
   }
#endif

   /* Crosscalls should spin a few times before blocking */
   if (!skew && usecs < CROSSCALL_SPIN_SHORT_US) {
      HostIF_WakeUpYielders(vm, currVcpu);
      return;
   }

   if (HostIF_PrepareWaitForThreads(vm, currVcpu)) {
      return;
   }

   VCPUSet_Empty(&vcpus);
   FOR_EACH_VCPU_IN_SET(req, vcpuid) {
      if (vcpuid == currVcpu) {
         continue;
      }
      /*
       * First assume the vCPU we want to have wake up the current vCPU
       * is out of the monitor, so set its wakeup bit corresponding to
       * the current vCPU.  It may or may not actually be on the vmmon side.
       */

      VCPUSet_AtomicInclude(&vm->crosscallWaitSet[vcpuid], currVcpu);

      /*
       * Now that the bit is set, check whether the vCPU is in vmmon.  If
       * it was previously in vmmon, and then took a trip to the monitor
       * and back before we got here, then the wakeup has already been sent.
       * If it is in the monitor, either it started in vmmon and sent the
       * wakeup, or it was there the entire time.  In either case we can
       * clear the bit.  This is safe because the bit is directed solely
       * at the current vCPU.
       */

      if (vm->currentHostCpu[vcpuid] != INVALID_PCPU) {
         VCPUSet_AtomicRemove(&vm->crosscallWaitSet[vcpuid], currVcpu);
      } else {
         if (VCPUSet_AtomicIsMember(&vm->crosscallWaitSet[vcpuid], currVcpu)) {
            VCPUSet_Include(&vcpus, vcpuid);
         }
      }
   } ROF_EACH_VCPU_IN_SET();

   /*
    * Wake up any threads that had previously yielded the processor to
    * let this one run.
    */

   HostIF_WakeUpYielders(vm, currVcpu);

   /*
    * If this thread has other threads to wait for, and no other threads
    * are waiting for this thread, block until one of the threads we're
    * waiting for has run.
    */

   if (!VCPUSet_IsEmpty(&vcpus) &&
       VCPUSet_IsEmpty(&vm->crosscallWaitSet[currVcpu])) {
      HostIF_WaitForThreads(vm, currVcpu);
   }

   /*
    * Tell other vcpus that they no longer have to wake this one.
    * This is optional, the other threads will eventually clear their
    * bits anyway.
    */

   FOR_EACH_VCPU_IN_SET(&vcpus, vcpuid) {
      VCPUSet_AtomicRemove(&vm->crosscallWaitSet[vcpuid], currVcpu);
   } ROF_EACH_VCPU_IN_SET();

   HostIF_CancelWaitForThreads(vm, currVcpu);
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86PerfCtrInUse --
 *
 *      Determine which performance counters are already in use by the
 *      host on the current PCPU.  A performance counter is considered
 *      in use if its event select enable bit is set or if this method
 *      is unable to count events with the performance counter.
 *
 * Results:
 *      Return TRUE if counter is in use.
 *
 * Side effects:
 *      None.
 *----------------------------------------------------------------------
 */
static Bool
Vmx86PerfCtrInUse(Bool isGen, unsigned pmcNum, unsigned ctrlMSR,
                  unsigned cntMSR, Bool hasPGC)
{
   volatile unsigned delay;
   uint64 origPGC = hasPGC ? __GET_MSR(PERFCTR_CORE_GLOBAL_CTRL_ADDR) : 0;
   uint64 pmcCtrl;
   uint64 pmcCount, count;
   uint64 ctrlEna, pgcEna;

   pmcCtrl = __GET_MSR(ctrlMSR);
   if (isGen) {
      ASSERT(pmcNum < 32);
      if ((pmcCtrl & PERFCTR_CPU_ENABLE) != 0) {
         return TRUE;
      }
      ctrlEna = PERFCTR_CPU_ENABLE | PERFCTR_CPU_KERNEL_MODE |
                PERFCTR_CORE_INST_RETIRED;
      pgcEna = CONST64U(1) << pmcNum;
   } else {
      ASSERT(pmcNum < 3);
      if ((pmcCtrl & PERFCTR_CORE_FIXED_ENABLE_MASKn(pmcNum)) != 0) {
         return TRUE;
      }
      ctrlEna = pmcCtrl | PERFCTR_CORE_FIXED_KERNEL_MASKn(pmcNum);
      pgcEna = CONST64U(1) << (pmcNum + 32);
   }
   pmcCount = __GET_MSR(cntMSR);
   /* Enable the counter. */
   __SET_MSR(ctrlMSR, ctrlEna);
   if (hasPGC) {
      __SET_MSR(PERFCTR_CORE_GLOBAL_CTRL_ADDR, pgcEna | origPGC);
   }
   /* Retire some instructions and wait a few cycles. */
   for (delay = 0; delay < 100; delay++) ;
   /* Disable the counter. */
   if (hasPGC) {
      __SET_MSR(PERFCTR_CORE_GLOBAL_CTRL_ADDR, origPGC);
   }
   count = __GET_MSR(cntMSR);
   __SET_MSR(ctrlMSR, pmcCtrl);
   __SET_MSR(cntMSR, pmcCount);
   return count == pmcCount;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86GetUnavailPerfCtrsOnCPU --
 *
 *      Determine which performance counters are already in use by the
 *      host on the current PCPU. 
 *
 * Results:
 *      A bitset representing unavailable performance counter.
 *      Bits 0-31 represent general purpose counters, and bits 32-63
 *      represent fixed counters.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
Vmx86GetUnavailPerfCtrsOnCPU(void *data)
{
   CPUIDRegs regs;
   unsigned i, numGen = 0, numFix = 0, stride = 1;
   uint32 selBase = 0;
   uint32 ctrBase = 0;
   Bool hasPGC = FALSE;
   Atomic_uint64 *ctrs = (Atomic_uint64 *)data;
   uintptr_t flags;
   if (CPUID_GetVendor() == CPUID_VENDOR_INTEL) {
      unsigned version;
      if (__GET_EAX_FROM_CPUID(0) < 0xA) {
         return;
      }
      __GET_CPUID(0xA, &regs);
      version = CPUID_GET(0xA, EAX, PMC_VERSION, regs.eax);
      if (version == 0) {
         return;
      }
      numGen = CPUID_GET(0xA, EAX, PMC_NUM_GEN, regs.eax);
      if (version >= 2) {
         numFix = CPUID_GET(0xA, EDX, PMC_NUM_FIXED, regs.edx);
         hasPGC = TRUE;
      }
      selBase = PERFCTR_CORE_PERFEVTSEL0_ADDR;
      ctrBase = PERFCTR_CORE_PERFCTR0_ADDR;
   } else if (CPUID_GetVendor() == CPUID_VENDOR_AMD) {
      if (CPUID_FAMILY_IS_BULLDOZER(__GET_EAX_FROM_CPUID(1))) {
         numGen  = 6;
         selBase = PERFCTR_BD_BASE_ADDR + PERFCTR_BD_EVENTSEL;
         ctrBase = PERFCTR_BD_BASE_ADDR + PERFCTR_BD_CTR;
         stride  = 2;
      } else {
         numGen  = 4;
         selBase = PERFCTR_AMD_PERFEVTSEL0_ADDR;
         ctrBase = PERFCTR_AMD_PERFCTR0_ADDR;
      }
   }
   ASSERT(numGen <= 32 && numFix <= 32);

   /*
    * Vmx86PerfCtrInUse modifies performance counters to determine if
    * if they are usable, disable interrupts to avoid racing with
    * interrupt handlers.
    */
   SAVE_FLAGS(flags);
   CLEAR_INTERRUPTS();
   for (i = 0; i < numGen; i++) {
      if (Vmx86PerfCtrInUse(TRUE, i, selBase + i * stride,
                            ctrBase + i * stride, hasPGC)) {
         Atomic_SetBit64(ctrs, i);
      }
   }
   if (numFix > 0) {
      for (i = 0; i < numFix; i++) {
         if (Vmx86PerfCtrInUse(FALSE, i, PERFCTR_CORE_FIXED_CTR_CTRL_ADDR,
                               PERFCTR_CORE_FIXED_CTR0_ADDR + i, hasPGC)) {
            Atomic_SetBit64(ctrs, i + 32);
         }
      }
   }
   RESTORE_FLAGS(flags);
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_GetUnavailablePerfCtrs --
 *
 *      Determine which performance counters are already in use by the
 *      host on across all PCPUs, and therefore unavailable for use by
 *      the monitor.  A performance counter is considered in use if its
 *      event select enable bit on any PCPU is set.
 *
 * Results:
 *      A bitset representing unavailable performance counter.
 *      Bits 0-31 represent general purpose counters, and bits 32-63
 *      represent fixed counters.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

uint64
Vmx86_GetUnavailablePerfCtrs(void)
{
   Atomic_uint64 unavailCtrs;
   Atomic_Write64(&unavailCtrs, 0);
   HostIF_CallOnEachCPU(Vmx86GetUnavailPerfCtrsOnCPU, &unavailCtrs);
   return Atomic_Read64(&unavailCtrs);
}

