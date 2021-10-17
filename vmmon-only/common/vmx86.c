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

/*
 * vmx86.c --
 *
 *     Platform independent routines for creating/destroying/running
 *     virtual machine monitors.
 */

#ifdef __linux__
/* Must come before any kernel header file --hpreg */
#   include "driver-config.h"

#   include <linux/string.h> /* memset() in the kernel  */
#   include <linux/sched.h>  /* jiffies from the kernel */
#else
#   include <string.h>
#endif

#ifdef __APPLE__
#include <IOKit/IOLib.h>
#endif

#include "vm_assert.h"
#include "vm_basic_math.h"
#include "vmx86.h"
#include "task.h"
#include "iocontrols.h"
#include "hostif.h"
#include "cpuid.h"
#include "vcpuset.h"
#include "memtrack.h"
#if defined(_WIN64)
#include "vmmon-asm-x86-64.h"
#endif
#include "x86vt.h"
#include "x86svm.h"
#include "x86cpuid_asm.h"
#if defined(__linux__)
#include <asm/timex.h>
#endif
#include "perfctr.h"
#include "x86vtinstr.h"
#include "bootstrap_vmm.h"
#include "monLoader.h"
#include "vmmblob.h"
#include "sharedAreaVmmon.h"
#include "statVarsVmmon.h"
#include "intelVT.h"
#include "cpu_defs.h"

PseudoTSC pseudoTSC;

/*
 * Keep track of the virtual machines that have been
 * created using the following structures.
 */

static VMDriver *vmDriverList = NULL;

static LockedPageLimit lockedPageLimit = {
   0,                        // host: does not need to be initialized.
   0,                        // configured: must be set by some VM as it is powered on.
};

/* Percentage of guest "paged" memory that must fit within the hard limit. */
static Percent minVmMemPct;

/* Number of pages actually locked by all virtual machines */
static PageCnt numLockedPages;

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

/* 3 physically contiguous pages for the I/O bitmap.  SVM only. */
HostIFContigMemMap *hvIOBitmap;

typedef struct {
   Atomic_uint32 *index; // OUT: array of cpu counters for queries.
   MSRQuery *query;  // IN/OUT: array of query items
   uint32 numItems;  // IN
} Vmx86GetMSRData;

static Bool hostUsesNX;

typedef struct NXData {
   Atomic_uint32 responded;
   Atomic_uint32 hasNX;
} NXData;

/*
 * A structure for holding MSR indexes, values for MSR uniformity checks.
 */
typedef struct VMX86MSRCacheInfo {
   uint32 msrIndex;
   uint64 msrValue;
} VMX86MSRCacheInfo;

struct MSRCache {
   Vmx86GetMSRData *queryCache;
   uint32 nPCPUs;
};

static Vmx86GetMSRData msrCacheQueryData;

/*
 * A MSR cache list for checking uniformity across physical cpus and for
 * generating least common denominated values across pcpus.
 * {MSR_Index, Member_Name}
 */
#define UNIFORMITY_CACHE_MSRS                                                  \
   MSRNUM(IA32_MSR_ARCH_CAPABILITIES,     ArchCap)                             \
   MSRNUM(MSR_BIOS_SIGN_ID,               BIOSSignID)                          \
   MSRNUM(MSR_PLATFORM_INFO,              Join)                                \
   MSRNUM(MSR_TSX_CTRL,                   Join)                                \
   MSRNUM(MSR_VM_CR,                      VMCR)                                \
   MSRNUMVT(MSR_FEATCTL,                  FeatureCtl)                          \
   MSRNUMVT(MSR_VMX_BASIC,                Basic)                               \
   MSRNUMVT(MSR_VMX_MISC,                 Misc)                                \
   MSRNUMVT(MSR_VMX_VMCS_ENUM,            Enum)                                \
   MSRNUMVT(MSR_VMX_EPT_VPID,             EPT)                                 \
   MSRNUMVT(MSR_VMX_VMFUNC,               VMFunc)                              \
   MSRNUMVT(MSR_VMX_3RD_CTLS,             3rd)                                 \
   MSRNUMVT2(MSR_VMX_PINBASED_CTLS,       Ctls)                                \
   MSRNUMVT2(MSR_VMX_PROCBASED_CTLS,      Ctls)                                \
   MSRNUMVT2(MSR_VMX_EXIT_CTLS,           Ctls)                                \
   MSRNUMVT2(MSR_VMX_ENTRY_CTLS,          Ctls)                                \
   MSRNUMVT2(MSR_VMX_2ND_CTLS,            Ctls)                                \
   MSRNUMVT2(MSR_VMX_TRUE_PINBASED_CTLS,  Ctls)                                \
   MSRNUMVT2(MSR_VMX_TRUE_PROCBASED_CTLS, Ctls)                                \
   MSRNUMVT2(MSR_VMX_TRUE_EXIT_CTLS,      Ctls)                                \
   MSRNUMVT2(MSR_VMX_TRUE_ENTRY_CTLS,     Ctls)                                \
   MSRNUMVT2(MSR_VMX_CR0_FIXED0,          Fixed0)                              \
   MSRNUMVT2(MSR_VMX_CR4_FIXED0,          Fixed0)                              \
   MSRNUMVT2(MSR_VMX_CR0_FIXED1,          Fixed1)                              \
   MSRNUMVT2(MSR_VMX_CR4_FIXED1,          Fixed1)                              \

static VMX86MSRCacheInfo msrUniformityCacheInfo[] = {
#define MSRNUM(msr, member) {msr, CONST64(0)},
#define MSRNUMVT    MSRNUM
#define MSRNUMVT2   MSRNUM
   UNIFORMITY_CACHE_MSRS
};
#undef MSRNUM
#undef MSRNUMVT
#undef MSRNUMVT2


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

static INLINE PageCnt
Vmx86AdjustLimitForOverheads(const VMDriver* vm,
                             const PageCnt limit)
{
   PageCnt extraCost = (vm != NULL) ? vmCount * vm->memInfo.perVMOverhead : 0;
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

static INLINE PageCnt
Vmx86LockedPageLimit(const VMDriver* vm)  // IN:
{
   PageCnt overallLimit;
   ASSERT(HostIF_GlobalLockIsHeld());
   lockedPageLimit.host = HostIF_EstimateLockedPageLimit(vm, numLockedPages);
   overallLimit = MIN(MIN(lockedPageLimit.configured, lockedPageLimit.host),
                      MAX_LOCKED_PAGES);

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
                  PageCnt numPages,
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
      /* Check the per-vm limit. */
      ASSERT(HostIF_VMLockIsHeld(vm));
      if (vm->memInfo.admitted) {
         if (vm->memInfo.maxAllocation <= vm->memInfo.locked) {
            return FALSE;
         } else if (vm->memInfo.maxAllocation - vm->memInfo.locked < numPages) {
            return FALSE;
         }
      }
   } else {
      /* Check the global limit. */
      PageCnt limit = Vmx86LockedPageLimit(vm);
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
 *      Increment count of VMs.
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
 *      Decrement count of VMs.
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
 *-----------------------------------------------------------------------------
 *
 * Vmx86_Free --
 *
 *      A wrapper around HostIF_FreeKernelMem that checks if the given
 *      pointer is NULL before freeing memory.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
Vmx86_Free(void *ptr)
{
   if (ptr != NULL) {
      HostIF_FreeKernelMem(ptr);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86_Calloc --
 *
 *      A wrapper around HostIF_AllocKernelMem that zeroes memory and
 *      fails if integer overflow would occur in the computed
 *      allocation size.
 *
 * Results:
 *      Pointer to allocated memory or NULL on failure. Use
 *      HostIF_FreeKernelMem or Vmx86_Free to free.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void *
Vmx86_Calloc(size_t numElements, // IN
             size_t elementSize, // IN
             Bool nonPageable)   // IN
{
   size_t numBytes = numElements * elementSize;
   void *retval;

   if (UNLIKELY(numBytes / numElements != elementSize)) { // Overflow.
      return NULL;
   }

   retval = HostIF_AllocKernelMem(numBytes, nonPageable);
   if (retval != NULL) {
      memset(retval, 0, numBytes);
   }
   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86AllocCrossPages --
 *
 *      Allocate numVCPUs pages suitable to be used as the VCPU's
 *      crosspage area.
 *
 * Results:
 *      TRUE if the required crosspages are allocated successfully.
 *      FALSE otherwise.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
Vmx86AllocCrossPages(VMDriver *vm)
{
   Vcpuid v;

   for (v = 0; v < vm->numVCPUs; v++) {
      MPN unused;

      UNUSED_VARIABLE(unused);
      vm->crosspage[v] = HostIF_AllocKernelPages(1, &unused);

      if (vm->crosspage[v] == NULL) {
         return FALSE;
      }
      memset(vm->crosspage[v], 0, PAGE_SIZE);
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86FreeCrossPages --
 *
 *      Free the crosspages allocated for the given VM.
 *
 *-----------------------------------------------------------------------------
 */

static void
Vmx86FreeCrossPages(VMDriver *vm)
{
   Vcpuid v;

   if (vm->crosspage != NULL) {
      for (v = 0; v < vm->numVCPUs; v++) {
         if (vm->crosspage[v] != NULL) {
            HostIF_FreeKernelPages(1, vm->crosspage[v]);
         }
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86FreeVMDriver --
 *
 *      Release kernel memory allocated for the driver structure.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
Vmx86FreeVMDriver(VMDriver *vm)
{
   Vmx86_Free(vm->ptRootMpns);
   Vmx86_Free(vm->crosspage);
   Vmx86_Free(vm->crosscallWaitSet);
   Vmx86_Free(vm->ptscOffsets);
   Vmx86_Free(vm->currentHostCpu);
   vm->ptRootMpns       = NULL;
   vm->crosspage        = NULL;
   vm->crosscallWaitSet = NULL;
   vm->ptscOffsets      = NULL;
   vm->currentHostCpu   = NULL;
   HostIF_FreeKernelMem(vm);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86AllocVMDriver --
 *
 *      Allocate the driver structure for a virtual machine.
 *
 * Results:
 *      Zeroed VMDriver structure or NULL on error.
 *
 * Side effects:
 *      May allocate kernel memory.
 *
 *-----------------------------------------------------------------------------
 */

VMDriver *
Vmx86AllocVMDriver(uint32 numVCPUs)
{
   VMDriver *vm = Vmx86_Calloc(1, sizeof *vm, TRUE);
   if (vm == NULL) {
      return NULL;
   }
   if ((vm->ptRootMpns =
        Vmx86_Calloc(numVCPUs, sizeof *vm->ptRootMpns, TRUE))       != NULL &&
       (vm->crosspage =
        Vmx86_Calloc(numVCPUs, sizeof *vm->crosspage, TRUE))        != NULL &&
       (vm->crosscallWaitSet =
        Vmx86_Calloc(numVCPUs, sizeof *vm->crosscallWaitSet, TRUE)) != NULL &&
       (vm->ptscOffsets =
        Vmx86_Calloc(numVCPUs, sizeof *vm->ptscOffsets, TRUE))      != NULL &&
       (vm->currentHostCpu =
        Vmx86_Calloc(numVCPUs, sizeof *vm->currentHostCpu, TRUE))   != NULL) {
      return vm;
   }
   Vmx86FreeVMDriver(vm);
   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86VMMPageFree --
 *
 *     Unmaps the VMM page corresponding to this entry from in the host
 *     kernel. This function is used as a callback by MemTrack_Cleanup().
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
Vmx86VMMPageFree(void *unused, MemTrackEntry *entry)
{
   ASSERT(entry->vpn != 0 && entry->mpn != 0);
   Vmx86_UnmapPage(entry->vpn);
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_CleanupVMMPages --
 *
 *     Ummaps all VMM pages from the host kernel address space and frees
 *     the VMM MemTracker.
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
Vmx86_CleanupVMMPages(VMDriver *vm)
{
   MemTrack_Cleanup(vm->vmmTracker, Vmx86VMMPageFree, NULL);
   vm->vmmTracker = NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86CleanupContigMappings --
 *
 *     Frees all allocations from HostIF_AllocContigPages that are associated
 *     with the given vm.
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
Vmx86CleanupContigMappings(VMDriver *vm)
{
   HostIFContigMemMap *m, *next;

   HostIF_VMLock(vm, 48);
   for (m = vm->contigMappings; m != NULL; m = next) {
      next = m->next;
      HostIF_FreeContigPages(vm, m);
   }
   HostIF_VMUnlock(vm, 48);
   vm->contigMappings = NULL;
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

      Vmx86FreeCrossPages(vm);
      if (vm->ptpTracker != NULL) {
         Task_SwitchPTPPageCleanup(vm);
      }
      if (vm->vmmTracker != NULL) {
         Vmx86_CleanupVMMPages(vm);
      }
      if (vm->blobInfo != NULL) {
         VmmBlob_Cleanup(vm->blobInfo);
         vm->blobInfo = NULL;
      }
      if (vm->sharedArea != NULL) {
         SharedAreaVmmon_Cleanup(vm->sharedArea);
         vm->sharedArea = NULL;
      }
      if (vm->statVars != NULL) {
         StatVarsVmmon_Cleanup(vm->statVars);
         vm->statVars = NULL;
      }
      if (vm->contigMappings != NULL) {
         Vmx86CleanupContigMappings(vm);
      }
      HostIF_FreeAllResources(vm);

      Vmx86FreeVMDriver(vm);
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
                      PageCnt numPages,
                      Bool ignoreLimits)
{
   Bool retval = FALSE;
   int retries = 3;

   ASSERT(vm);

   for (retries = 3; !retval && (retries > 0); retries--) {
      HostIF_GlobalLock(17);
      HostIF_VMLock(vm, 0);
      /* Check VM's limit and don't wait. */
      retval = Vmx86HasFreePages(vm, numPages, TRUE);
      if (!retval) {
         HostIF_VMUnlock(vm, 0);
         HostIF_GlobalUnlock(17);
         break;
      } else {
         /* Wait to satisfy the global limit. */
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
                        PageCnt numPages)
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
 *----------------------------------------------------------------------
 *
 * Vmx86GetNX --
 *
 *       Checks whether NX is enabled on the current CPU.
 *
 * Results:
 *       None.
 *
 * Side effects:
 *       Increments responded-CPU counter, may increment NX CPU counter.
 *
 *----------------------------------------------------------------------
 */

static void
Vmx86GetNX(void *clientData) // IN/OUT: A NXData *
{
   NXData *nxData = (NXData *)clientData;
   uint64 efer = X86MSR_GetMSR(MSR_EFER);

   Atomic_Inc32(&nxData->responded);
   if ((efer & MSR_EFER_NXE) == MSR_EFER_NXE) {
      Atomic_Inc32(&nxData->hasNX);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_CacheNXState --
 *
 *       Checks whether every CPU on the host has NX/XD enabled and
 *       caches this value.
 *
 * Results:
 *       None.
 *
 * Side effects:
 *       Caches host NX value.
 *
 *----------------------------------------------------------------------
 */

void
Vmx86_CacheNXState(void)
{
   NXData nxData;
   Atomic_Write32(&nxData.responded, 0);
   Atomic_Write32(&nxData.hasNX, 0);
   HostIF_CallOnEachCPU(Vmx86GetNX, &nxData);
   hostUsesNX = Atomic_Read32(&nxData.hasNX) ==
                Atomic_Read32(&nxData.responded);
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
Vmx86_CreateVM(VA64 bsBlob, uint32 bsBlobSize, uint32 numVCPUs)
{
   VMDriver *vm;
   Vcpuid v;
   void *bsBuf = NULL;
   BSVMM_HostParams *bsParams;

   /* Disallow VM creation if the vmx passes us an invalid number of vcpus. */
   if (numVCPUs == 0 || numVCPUs > MAX_VCPUS) {
      return NULL;
   }

   /* Disallow VM creation if NX is disabled on the host as VMM requires NX. */
   if (!hostUsesNX) {
      Log("NX/XD must be enabled.  Cannot create VM.\n");
      return NULL;
   }

   vm = Vmx86AllocVMDriver(numVCPUs);
   if (vm == NULL) {
      return NULL;
   }

   vm->userID = 0;
   vm->numVCPUs = numVCPUs;
   vm->memInfo.admitted = FALSE;

   for (v = 0; v < numVCPUs; v++) {
      Atomic_Write32(&vm->currentHostCpu[v], INVALID_PCPU);
      vm->ptRootMpns[v] = INVALID_MPN;
   }
   if (!HostIF_Init(vm, numVCPUs)) {
      goto cleanup;
   }

   /* The ULM does not use the cross GDT. */
   if (bsBlobSize != 0) {
      bsBuf = HostIF_AllocKernelMem(bsBlobSize, FALSE);
      if (bsBuf == NULL) {
         goto cleanup;
      }
      if (HostIF_CopyFromUser(bsBuf, bsBlob, bsBlobSize) != 0) {
         goto cleanup;
      }
      bsParams = BSVMM_Validate(bsBuf, bsBlobSize);
      if (bsParams == NULL) {
         Warning("Could not validate the VMM bootstrap blob");
         goto cleanup;
      }

      if (!Task_CreateCrossGDT(&bsParams->gdtInit)) {
         goto cleanup;
      }
   }

   vm->ptpTracker = MemTrack_Init(vm);
   if (vm->ptpTracker == NULL) {
      goto cleanup;
   }
   vm->vmmTracker = MemTrack_Init(vm);
   if (vm->vmmTracker == NULL) {
      goto cleanup;
   }
   vm->sharedArea = SharedAreaVmmon_Init(vm);
   if (vm->sharedArea == NULL) {
      goto cleanup;
   }
   vm->statVars = StatVarsVmmon_Init(vm);
   if (vm->statVars == NULL) {
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

   if (bsBuf != NULL) {
      HostIF_FreeKernelMem(bsBuf);
   }
   return vm;

cleanup:
   if (bsBuf != NULL) {
      HostIF_FreeKernelMem(bsBuf);
   }
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
 * Vmx86SetPageTableRoots  --
 *
 *      Translates the user VA corresponding to the root page tables
 *      for all VCPUs into MPNs and stores them in VMDriver.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 *----------------------------------------------------------------------
 */

static Bool
Vmx86SetPageTableRoots(VMDriver *vm, PerVcpuPages *perVcpuPages,
                       uint16 numVCPUs)
{
   uint16 vcpu;

   if (numVCPUs > vm->numVCPUs) {
      return FALSE;
   }
   for (vcpu = 0; vcpu < numVCPUs; vcpu++) {
      VA64 ptRoot = perVcpuPages[vcpu].ptRoot;

      if ((ptRoot & (PAGE_SIZE - 1)) != 0) {
         Warning("Error: page table VA %"FMT64"x is not page-aligned\n",
                 ptRoot);
         return FALSE;
      }
      ASSERT(vm->ptRootMpns[vcpu] == INVALID_MPN);
      HostIF_VMLock(vm, 38);
      if (HostIF_LookupUserMPN(vm, ptRoot, &vm->ptRootMpns[vcpu]) !=
          PAGE_LOOKUP_SUCCESS) {
         HostIF_VMUnlock(vm, 38);
         Warning("Failure looking up page table root MPN for VCPU %d\n", vcpu);
         return FALSE;
      }
      HostIF_VMUnlock(vm, 38);
   }
   return TRUE;
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
 * Vmx86_ProcessBootstrap  --
 *
 *     Copies the VMM bootstrap blob header and processes it by invoking
 *     MonLoader.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 *----------------------------------------------------------------------
 */

Bool
Vmx86_ProcessBootstrap(VMDriver *vm,
                       VA64 bsBlobAddr,
                       uint32 numBytes,
                       uint32 headerOffset,
                       uint16 numVCPUs,
                       PerVcpuPages *perVcpuPages,
                       VMSharedRegion *shRegions)
{
   VmmBlobInfo *bi = NULL;
   unsigned errLine;
   Vcpuid errVcpu;
   MonLoaderError ret;
   MonLoaderArgs args;
   MonLoaderHeader *header;

   if (!VmmBlob_Load(bsBlobAddr, numBytes, headerOffset, &bi)) {
      Warning("Error loading VMM bootstrap blob\n");
      goto error;
   }
   vm->blobInfo = bi;
   header = bi->header;
   if (!Vmx86SetPageTableRoots(vm, perVcpuPages, numVCPUs)) {
      goto error;
   }

   if (!pseudoTSC.initialized) {
      Warning("%s: PseudoTSC has not been initialized\n", __FUNCTION__);
      goto error;
   }

   if (!Vmx86AllocCrossPages(vm)) {
      Warning("Failed to allocate cross pages.\n");
      goto error;
   }
   /*
    * Initialize the driver's part of the cross-over page used to
    * talk to the monitor.
    */
   if (!Task_InitCrosspage(vm, header->monStartLPN, header->monEndLPN,
                           perVcpuPages)) {
      Warning("Error initializing crosspage\n");
      goto error;
   }

   args.vm = vm;
   args.shRegions = shRegions;
   ret = MonLoader_Process(header, numVCPUs, &args, &errLine, &errVcpu);
   if (ret != ML_OK) {
      Warning("Error processing bootstrap: error %d at line %u, vcpu %u\n",
               ret, errLine, errVcpu);
      goto error;
   }
   return TRUE;

error:
   if (bi != NULL) {
      VmmBlob_Cleanup(bi);
      vm->blobInfo = NULL;
   }
   return FALSE;
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
 *      Bumps fdCount.
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
 *      May de-initialize ptsc.
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
Vmx86_ReadTSCAndUptime(VmTimeStart *st)  // OUT: return value
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
Vmx86GetkHzEstimate(VmTimeStart *st)   // IN: start time
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
Vmx86_GetkHzEstimate(VmTimeStart *st)   // IN: start time
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
      Bool hasWork = FALSE;
      VCPUSet_Empty(&expiredVCPUs);

      for (v = 0; v < vm->numVCPUs; v++) {
         VMCrossPageData *crosspage = vm->crosspage[v];

         if (crosspage == NULL) {
            continue;  // VCPU is not initialized yet
         }
         expiry = crosspage->monTimerExpiry;
         if (expiry != 0 && expiry <= pNow) {
            VCPUSet_Include(&expiredVCPUs, v);
            hasWork = TRUE;
         }
      }
      if (hasWork) {
         HostIF_IPI(vm, &expiredVCPUs);
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


static INLINE PageCnt
Vmx86MinAllocationFunc(PageCnt nonpaged,
                       PageCnt anonymous,
                       PageCnt mainmem,
                       Percent memPct)
{
   return (memPct * mainmem) / 100 + nonpaged + anonymous;
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
 *      The minAllocation for this vm.
 *
 *
 * Side effects:
 *      Analyzes the vm info, requiring the vm lock.
 *
 *----------------------------------------------------------------------
 */

static INLINE PageCnt
Vmx86MinAllocation(VMDriver *vm,
                   Percent memPct)
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
 *      Number of pages that must fit within host ram for a given overcommit
 *      level.
 *
 *
 * Side effects:
 *      None. The actual minAllocations of each vm are NOT updated during
 *      this computation.
 *
 *----------------------------------------------------------------------
 */

static PageCnt
Vmx86CalculateGlobalMinAllocation(Percent memPct)
{
   VMDriver *vm;
   PageCnt minAllocation = 0;
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
Vmx86UpdateMinAllocations(Percent memPct)  // IN:
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
Vmx86_SetConfiguredLockedPagesLimit(PageCnt limit)  // IN:
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
 *      Negative system specific error code on error (NTSTATUS on Windows,
 *      etc.)
 *
 * Side effects:
 *      Number of global and per-VM locked pages is increased.
 *
 *-----------------------------------------------------------------------------
 */

int64
Vmx86_AllocLockedPages(VMDriver *vm,         // IN: VMDriver
                       VA64 addr,            // OUT: VA of an array for
                                             //      allocated MPNs.
                       PageCnt numPages,     // IN: number of pages to allocate
                       Bool kernelMPNBuffer, // IN: is the MPN buffer in kernel
                                             //     or user address space?
                       Bool ignoreLimits)    // IN: should limits be ignored?
{
   int64 allocatedPages;
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
 *      0 on success,
 *      non-0 system specific error code on error (NTSTATUS on Windows, etc.)
 *
 * Side effects:
 *      Number of global and per-VM locked pages is decreased.
 *
 *----------------------------------------------------------------------
 */

int
Vmx86_FreeLockedPages(VMDriver *vm,         // IN: VM instance pointer
                      MPN *mpns,            // IN: MPNs to free
                      PageCnt numPages)     // IN: number of pages to free
{
   int ret;

   HostIF_VMLock(vm, 8);
   ret = HostIF_FreeLockedPages(vm, mpns, numPages);
   HostIF_VMUnlock(vm, 8);

   if (ret == 0) {
      Vmx86UnreserveFreePages(vm, numPages);
   }

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86_AllocLowPage --
 *
 *      Allocate a zeroed locked low page.
 *
 * Results:
 *      Allocated MPN on success. INVALID_MPN on failure.
 *
 * Side effects:
 *      Number of global and per-VM locked pages is increased.
 *
 *-----------------------------------------------------------------------------
 */

MPN
Vmx86_AllocLowPage(VMDriver *vm,      // IN: VMDriver
                   Bool ignoreLimits) // IN: should limits be ignored?
{
   MPN mpn;

   if (!Vmx86ReserveFreePages(vm, 1, ignoreLimits)) {
      return INVALID_MPN;
   }

   HostIF_VMLock(vm, 49);
   mpn = HostIF_AllocLowPage(vm);
   HostIF_VMUnlock(vm, 49);

   if (mpn == INVALID_MPN) {
      Vmx86UnreserveFreePages(vm, 1);
   }

   return mpn;
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
 * Vmx86_GetNumAnonPages --
 *
 *      Queries the driver for the total number of anonymous pages.
 *
 * Results:
 *      Total number of anonymous pages
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

PageCnt
Vmx86_GetNumAnonPages(VMDriver *vm)       // IN: VM instance pointer
{
   PageCnt ret;
   HostIF_VMLock(vm, 45);
   ret = HostIF_GetNumAnonPages(vm);
   HostIF_VMUnlock(vm, 45);
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
                    PageCnt  paged,        // IN
                    PageCnt  nonpaged,     // IN
                    PageCnt  anonymous,    // IN
                    Percent  aminVmMemPct) // IN
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
   PageCnt globalMinAllocation;
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
      PageCnt allocatedPages, nonpaged;
      int64 pages;
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

      Vmx86_FreeLockedPages(curVM, mpns, allocatedPages);
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
   PageCnt globalMinAllocation, newMinAllocation;
   Bool retval = FALSE;
   int64 paged;
   int64 nonpaged;
   int64 anonymous;

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
 * Vmx86VMXEnabled --
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

static Bool
Vmx86VMXEnabled(void)
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
   if (CPUID_HostSupportsSVM()) {
      uint64 vmCR = X86MSR_GetMSR(MSR_VM_CR);
      if (!SVM_LockedFromFeatures(vmCR)) {
         CPUIDRegs regs;
         __GET_CPUID(0x8000000A, &regs);
         if (CPUID_GET(0x8000000A, EDX, SVM_LOCK, regs.edx) != 0) {
            X86MSR_SetMSR(MSR_VM_CR, (vmCR & ~MSR_VM_CR_SVME_DISABLE) |
                                      MSR_VM_CR_SVM_LOCK);
         }
      }
   } else if (CPUID_HostSupportsVT()) {
      uint64 featCtl = X86MSR_GetMSR(MSR_FEATCTL);
      if (!VT_LockedFromFeatures(featCtl)) {
         X86MSR_SetMSR(MSR_FEATCTL,
                       featCtl | MSR_FEATCTL_LOCK | MSR_FEATCTL_VMXE);
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
      Bool logParams = pseudoTSC.hz != params->tscHz ||
                       pseudoTSC.hwTSCsSynced != params->hwTSCsSynced ||
                       pseudoTSC.useRefClock != params->forceRefClock;

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
      if (logParams) {
         Log("PTSC: initialized at %"FMT64"u Hz using %s, TSCs are "
             "%ssynchronized.\n", pseudoTSC.hz,
             pseudoTSC.useRefClock ? "reference clock" : "TSC",
             pseudoTSC.hwTSCsSynced ? "" : "not ");
      }
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


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86GetMSR --
 *
 *      Collect MSR value on the current logical CPU.
 *
 *      Function must not block (it is invoked from interrupt context).
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
   uint32 i;
   Vmx86GetMSRData *data = (Vmx86GetMSRData *)clientData;
   uint32 numPCPUs = data->query->numLogicalCPUs;
   size_t offset = sizeof(MSRQuery) + sizeof(MSRReply) * numPCPUs;
   ASSERT(data && data->index && data->query);

   for (i = 0; i < data->numItems; ++i) {
      uint32 index;
      int err;
      Atomic_uint32 *cpus = &data->index[i];
      MSRQuery *query = (MSRQuery *) ((uint8 *)&data->query[0] + i * offset);

      index = Atomic_ReadInc32(cpus);
      if (index >= numPCPUs) {
         continue;
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
          * buggy firmware that forgets to re-enable HV after waking from
          * deep sleep. [PR 1020692]
          */
         if (query->msrNum == MSR_FEATCTL || query->msrNum == MSR_VM_CR) {
            Vmx86EnableHVOnCPU();
         }
         err =
            HostIF_SafeRDMSR(query->msrNum, &query->logicalCPUs[index].msrVal);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86_GetAllMSRs --
 *
 *      Collect MSR value on number of logical CPUs requested.
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
   unsigned i, cpu;
   Atomic_uint32 index;
   Vmx86GetMSRData data;
   data.index = &index;
   data.numItems = 1;

   /* Check MSR uniformity cache first. */
   for (i = 0; i < ARRAYSIZE(msrUniformityCacheInfo); ++i) {
      if (msrUniformityCacheInfo[i].msrIndex == query->msrNum) {
         for (cpu = 0; cpu < query->numLogicalCPUs; cpu++) {
            query->logicalCPUs[cpu].msrVal = msrUniformityCacheInfo[i].msrValue;
            query->logicalCPUs[cpu].tag = cpu;
         }
         return TRUE;
      }
   }

   Atomic_Write32(data.index, 0);
   data.query = query;

   HostIF_CallOnEachCPU(Vmx86GetMSR, &data);

   /*
    * At this point, Atomic_Read32(data.index) is the number of logical CPUs
    * who replied.
    */

   if (Atomic_Read32(data.index) > query->numLogicalCPUs) {
      return FALSE;
   }

   ASSERT(Atomic_Read32(data.index) <= query->numLogicalCPUs);
   query->numLogicalCPUs = Atomic_Read32(data.index);

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86CheckVMXStatus --
 *
 *      Checks the status of the given operation and issues a warning if it was
 *      not successful. If it is a valid failure, the error code will be read
 *      and logged.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
Vmx86CheckVMXStatus(const char *operation, // IN: Operation string
                    VMXStatus status)      // IN: Status to check
{
   if (status != VMX_Success) {
      Warning("%s failed with status %s.\n", operation,
              status == VMX_FailValid ? "VMX_FailValid" :
              status == VMX_FailInvalid ? "VMX_FailInvalid" : "UNKNOWN");
/*
 * We use a broken in-house version of binutils (2.16.1-vt) with gcc 4.3 which
 * doesn't handle VMREAD/VMWRITE operands properly.
 */
#ifdef __GNUC__
#if __GNUC__== 4 && __GNUC_MINOR__ > 3
      if (status == VMX_FailValid) {
         size_t errorCode;
         VMREAD_2_STATUS(VT_VMCS_VMINSTR_ERR, &errorCode);
         Log("VM-instruction error: Error %"FMTSZ"d\n", errorCode);
      }
#endif
#endif
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86FlushVMCSPage --
 *
 *      VMCLEAR the given VMCS page on the current logical CPU. We first enable
 *      HV if necessary, and execute a VMXON using the given VMXON region MPN.
 *      If HV was already enabled, it will remain enabled. If we enabled HV or
 *      executed a VMXON in non-root operation, we will restore the state of
 *      each respectively after the VMCLEAR.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The hardware VMCS cache will be flushed.
 *
 *-----------------------------------------------------------------------------
 */

static void
Vmx86FlushVMCSPage(void *clientData) // IN: The MA of the VMCS to VMCLEAR
{
   MA vmxonRegion;
   Bool hvWasEnabled;
   MA vmcs = (MA)clientData;
   Bool vmxWasInRootOperation = FALSE;
   VMXStatus vmxonStatus, vmclearStatus, vmxoffStatus;

   ASSERT(vmcs);

   /* Enable HV if it isn't already enabled. */
   hvWasEnabled = Vmx86VMXEnabled();
   if (!hvWasEnabled) {
      uintptr_t cr4reg;
      ASSERT(VT_CapableCPU());
      Vmx86EnableHVOnCPU();
      GET_CR4(cr4reg);
      SET_CR4(cr4reg | CR4_VMXE);
   }

   /* VMXON using this CPUs's VMXON region. */
   vmxonRegion = MPN_2_MA(Task_GetHVRootPageForPCPU(HostIF_GetCurrentPCPU()));
   vmxonStatus = VMXON_2_STATUS(&vmxonRegion);
   if (vmxonStatus != VMX_Success) {
      /* VMXON failed, we must already be in VMX root operation. */
      vmxWasInRootOperation = TRUE;
   }

   /* VMCLEAR the given VMCS page. */
   vmclearStatus = VMCLEAR_2_STATUS(&vmcs);
   Vmx86CheckVMXStatus("VMCLEAR", vmclearStatus);

   /* VMXOFF if we were initially in VMX non-root operation. */
   if (!vmxWasInRootOperation) {
      vmxoffStatus = VMXOFF_2_STATUS();
      Vmx86CheckVMXStatus("VMXOFF", vmxoffStatus);
   }

   /* Disable HV if it was initially disabled. */
   if (!hvWasEnabled) {
      uintptr_t cr4reg;
      GET_CR4(cr4reg);
      SET_CR4(cr4reg & ~CR4_VMXE);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86_FlushVMCSAllCPUs --
 *
 *      Enable HV (if necessary) and VMCLEAR a VMCS page on all logical CPUs.
 *      This will prevent stale data from surfacing out of the VMCS cache when
 *      executing VMREADs.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      HV will be enabled and hardware VMCS caches will be flushed across all
 *      CPUs.
 *
 *-----------------------------------------------------------------------------
 */

void
Vmx86_FlushVMCSAllCPUs(MA vmcs) // IN: The MA of the VMCS to VMCLEAR
{
   HostIF_CallOnEachCPU(Vmx86FlushVMCSPage, (void *)vmcs);
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

   ASSERT(currVcpu < vm->numVCPUs);
   
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
   FOR_EACH_VCPU_IN_SET_WITH_MAX(req, vcpuid, vm->numVCPUs) {
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

      if (Atomic_Read32(&vm->currentHostCpu[vcpuid]) != INVALID_PCPU) {
         VCPUSet_AtomicRemove(&vm->crosscallWaitSet[vcpuid], currVcpu);
      } else {
         if (VCPUSet_AtomicIsMember(&vm->crosscallWaitSet[vcpuid], currVcpu)) {
            VCPUSet_Include(&vcpus, vcpuid);
         }
      }
   } ROF_EACH_VCPU_IN_SET_WITH_MAX();

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

   FOR_EACH_VCPU_IN_SET_WITH_MAX(&vcpus, vcpuid, vm->numVCPUs) {
      VCPUSet_AtomicRemove(&vm->crosscallWaitSet[vcpuid], currVcpu);
   } ROF_EACH_VCPU_IN_SET_WITH_MAX();

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
   uint64 origPGC = hasPGC ? X86MSR_GetMSR(PERFCTR_CORE_GLOBAL_CTRL_ADDR) : 0;
   uint64 pmcCtrl;
   uint64 pmcCount, count;
   uint64 ctrlEna, pgcEna;

   pmcCtrl = X86MSR_GetMSR(ctrlMSR);
   if (isGen) {
      ASSERT(pmcNum < 32);
      if ((pmcCtrl & PERFCTR_CPU_ENABLE) != 0) {
         return TRUE;
      }
      ctrlEna = PERFCTR_CPU_ENABLE | PERFCTR_CPU_KERNEL_MODE |
                PERFCTR_CORE_INST_RETIRED;
      pgcEna = CONST64U(1) << pmcNum;
   } else {
      ASSERT(pmcNum < PERFCTR_CORE_NUM_FIXED_COUNTERS);
      if ((pmcCtrl & PERFCTR_CORE_FIXED_ENABLE_MASKn(pmcNum)) != 0) {
         return TRUE;
      }
      ctrlEna = pmcCtrl | PERFCTR_CORE_FIXED_KERNEL_MASKn(pmcNum);
      pgcEna = CONST64U(1) << (pmcNum + 32);
   }
   pmcCount = X86MSR_GetMSR(cntMSR);
   /* Enable the counter. */
   X86MSR_SetMSR(ctrlMSR, ctrlEna);
   if (hasPGC) {
      X86MSR_SetMSR(PERFCTR_CORE_GLOBAL_CTRL_ADDR, pgcEna | origPGC);
   }
   /* Retire some instructions and wait a few cycles. */
   for (delay = 0; delay < 100; delay++) ;
   /* Disable the counter. */
   if (hasPGC) {
      X86MSR_SetMSR(PERFCTR_CORE_GLOBAL_CTRL_ADDR, origPGC);
   }
   count = X86MSR_GetMSR(cntMSR);
   X86MSR_SetMSR(ctrlMSR, pmcCtrl);
   X86MSR_SetMSR(cntMSR, pmcCount);
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
   } else if (CPUID_GetVendor() == CPUID_VENDOR_AMD ||
              CPUID_GetVendor() == CPUID_VENDOR_HYGON) {
     if (CPUID_ISSET(0x80000001, ECX, PERFCORE,
        __GET_ECX_FROM_CPUID(0x80000001))) {
         numGen  = 6;
         selBase = PERFCTR_AMD_EXT_BASE_ADDR + PERFCTR_AMD_EXT_EVENTSEL;
         ctrBase = PERFCTR_AMD_EXT_BASE_ADDR + PERFCTR_AMD_EXT_CTR;
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
      numFix = MIN(numFix, PERFCTR_CORE_NUM_FIXED_COUNTERS);
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


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_GetPageRoot --
 *
 *      Get the page root MPN for the specified VCPU.
 *
 * Results:
 *      TRUE and an MPN on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
Vmx86_GetPageRoot(VMDriver *vm,  // IN:
                  Vcpuid vcpuid, // IN:
                  MPN *mpn)      // OUT:
{
   if (vcpuid >= vm->numVCPUs) {
      return FALSE;
   }
   *mpn = vm->ptRootMpns[vcpuid];
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_MapPage --
 *
 *      Maps the specified MPN into the host kernel address space.
 *      returns the VPN of the mapping.
 *
 * Results:
 *      The VPN in the kernel address space of the new mapping, or 0 if
 *      the mapping failed.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VPN
Vmx86_MapPage(MPN mpn) // IN:
{
   return HostIF_MapPage(mpn);
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_UnmapPage --
 *
 *      Unmaps the specified VPN from the host kernel address space.
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
Vmx86_UnmapPage(VPN vpn) // IN:
{
   HostIF_UnmapPage(vpn);
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_GetMonitorContext --
 *
 *      Gets most of the monitor's saved context (as of the last world switch)
 *      from a given VCPU's crosspage.  CR3 is omitted as it is privileged,
 *      while DS/SS/ES are returned due to their potential utility in debugging.
 *
 * Results:
 *      On success, TRUE and context is (partially) populated.  FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
Vmx86_GetMonitorContext(VMDriver *vm,       // IN: The VM instance.
                        Vcpuid vcpuid,      // IN: VCPU in question.
                        Context64 *context) // OUT: context.
{
   VMCrossPageData *cpData;

   if (vcpuid >= vm->numVCPUs || vm->crosspage[vcpuid] == NULL) {
      return FALSE;
   }
   cpData = vm->crosspage[vcpuid];

   memset(context, 0, sizeof *context);
   context->es  = cpData->monES;
   context->ss  = cpData->monSS;
   context->ds  = cpData->monDS;
   context->rbx = cpData->monRBX;
   context->rsp = cpData->monRSP;
   context->rbp = cpData->monRBP;
   context->r12 = cpData->monR12;
   context->r13 = cpData->monR13;
   context->r14 = cpData->monR14;
   context->r15 = cpData->monR15;
   context->rip = cpData->monRIP;
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_CleanupHVIOBitmap --
 *
 *      Free any resources that were allocated for the HV I/O bitmap.
 *
 * Results:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
Vmx86_CleanupHVIOBitmap(void)
{
   if (hvIOBitmap != NULL) {
      HostIF_FreeContigPages(NULL, hvIOBitmap);
      hvIOBitmap = NULL;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86_CreateHVIOBitmap --
 *
 *      Called on driver load to create and initialize the host wide SVM I/O
 *      bitmap.  This item is a physically contiguous region of
 *      SVM_VMCB_IO_BITMAP_PAGES pages and is initialized to all-bits-set.
 *
 * Results:
 *      TRUE on success or FALSE on failure.
 *
 *----------------------------------------------------------------------
 */

Bool
Vmx86_CreateHVIOBitmap(void)
{
   if (!CPUID_HostSupportsSVM()) {
      return TRUE;
   }
   if (vmx86_apple) {
      /*
       * This function is not called on MacOS.  No supported MacOS system is
       * available for AMD so that platform has no need to create the SVM I/O
       * bitmap.
       */
      return TRUE;
   }
   hvIOBitmap = HostIF_AllocContigPages(NULL, SVM_VMCB_IO_BITMAP_PAGES);
   if (hvIOBitmap == NULL) {
      Warning("Failed to allocate SVM I/O bitmap.\n");
      return FALSE;
   }
   memset(hvIOBitmap->addr, 0xff, SVM_VMCB_IO_BITMAP_SIZE);
   return TRUE;
}

/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86RegisterCPU --
 *
 *      Registers each logical CPU by incrementing a counter.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      counter value pointed by 'data' is incremented by one.
 *
 *-----------------------------------------------------------------------------
 */

static void
Vmx86RegisterCPU(void *data) // IN: *data
{
   Atomic_uint32 *numLogicalCPUs = data;
   ASSERT(numLogicalCPUs);
   Atomic_Inc32(numLogicalCPUs);
}


/*
 *----------------------------------------------------------------------
 *
 * Vmx86VTMSRCacheGet --
 *
 *      Retrieve the requested VT MSR value from the cache.  Returns zero
 *      for uncached values.
 *
 *----------------------------------------------------------------------
 */

static uint64
Vmx86VTMSRCacheGet(const MSRCache *cache, uint32 msrNum, unsigned cpu)
{
   ASSERT((msrNum >= MSR_VMX_BASIC && msrNum < MSR_VMX_BASIC + NUM_VMX_MSRS) ||
          msrNum == MSR_FEATCTL);
   if (cache != NULL && cache->queryCache != NULL) {
      size_t offset = sizeof(MSRQuery) + sizeof(MSRReply) * cache->nPCPUs;
      MSRQuery *query;
      unsigned ix;
      ASSERT(cpu < cache->nPCPUs);
      for (ix = 0; ix < cache->queryCache->numItems; ix++) {
         query = (MSRQuery *) ((uint8 *)&cache->queryCache->query[0] +
                               ix * offset);
         if (query->msrNum == msrNum) {
            return query->logicalCPUs[cpu].msrVal;
         }
      }
   }
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86AllocMSRUniformityCache --
 * Vmx86FreeMSRUniformityCache --
 *
 *      Allocate/populate and cleanup MSR uniformity cache.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
Vmx86AllocMSRUniformityCache(uint32 numPCPUs)
{
   MSRQuery *query = NULL;
   uint32 i;
   uint32 numQueries = ARRAYSIZE(msrUniformityCacheInfo);
   Atomic_uint32 *cpuCounters;
   MSRQuery *multMSRQueryAllPcpus = Vmx86_Calloc(numQueries,
         sizeof(MSRQuery) + sizeof(MSRReply) * numPCPUs, FALSE);
   if (multMSRQueryAllPcpus == NULL) {
      return FALSE;
   }

   cpuCounters = Vmx86_Calloc(numQueries, sizeof(Atomic_uint32), FALSE);
   if (cpuCounters == NULL) {
      Vmx86_Free(multMSRQueryAllPcpus);
      return FALSE;
   }
   msrCacheQueryData.query = multMSRQueryAllPcpus;
   msrCacheQueryData.index = cpuCounters;
   msrCacheQueryData.numItems = numQueries;

   /*
    * Enumerates a MSR list and initializes MSR msrCacheQueryData structure
    * before the actual (safe) MSR query takes place.
    */
   for (i = 0; i < ARRAYSIZE(msrUniformityCacheInfo); ++i) {
      query = (MSRQuery *) ((uint8 *)&msrCacheQueryData.query[0] +
                 i * (sizeof(MSRQuery) + sizeof(MSRReply) * numPCPUs));
      Atomic_Write32(&msrCacheQueryData.index[i], 0);
      query->msrNum = msrUniformityCacheInfo[i].msrIndex;
      query->numLogicalCPUs = numPCPUs;
   }

   /* Perform a single query for all of the MSRs in the uniformity check list.*/
   HostIF_CallOnEachCPU(Vmx86GetMSR, &msrCacheQueryData);
   return TRUE;
}


static void
Vmx86FreeMSRUniformityCache(void)
{
   Vmx86_Free(msrCacheQueryData.index);
   Vmx86_Free(msrCacheQueryData.query);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86CheckMSRUniformity --
 *
 *      Iterate MSR uniformity cache and test uniformity of each MSR across all
 *      physical cpu(s).
 *
 *-----------------------------------------------------------------------------
 */

static void
Vmx86CheckMSRUniformity(uint32 numPCPUs)
{
   uint32 i, j;
   MSRQuery *query = NULL;

   for (i = 0; i < ARRAYSIZE(msrUniformityCacheInfo); ++i) {
      uint32 msrIndex = msrUniformityCacheInfo[i].msrIndex;
      query = (MSRQuery *)((uint8 *)&msrCacheQueryData.query[0] +
                 i * (sizeof(MSRQuery) + sizeof(MSRReply) * numPCPUs));
      ASSERT(Atomic_Read32(&msrCacheQueryData.index[i]) == numPCPUs);
      for (j = 1; j < numPCPUs; j++) {
         uint64 msrValuePCPU = query->logicalCPUs[j].msrVal;
         if (msrValuePCPU != query->logicalCPUs[0].msrVal) {
            Warning("Found a mismatch on MSR feature 0x%x; logical cpu%u "
                    "value = 0x%llx, but logical cpu%u value = 0x%llx\n",
                    msrIndex, j, msrValuePCPU, 0, query->logicalCPUs[0].msrVal);
         }
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86FindMSRQueryFromCache --
 *
 *      Iterate MSR uniformity cache and find query position for the given msr.
 *
 *-----------------------------------------------------------------------------
 */

static MSRQuery*
Vmx86FindMSRQueryFromCache(uint32 msrIndex, uint32 numPCPUs)
{
   uint32 i;
   MSRQuery *query = NULL;
   size_t offset = sizeof(MSRQuery) + sizeof(MSRReply) * numPCPUs;
   MSRQuery *first = &msrCacheQueryData.query[0];

   for (i = 0; i < ARRAYSIZE(msrUniformityCacheInfo); ++i) {
      if (msrIndex == msrUniformityCacheInfo[i].msrIndex) {
         query = (MSRQuery *)((uint8 *)first + i * offset);
         break;
      }
   }
   return query;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86FindCommonMSRArchCap --
 * Vmx86FindCommonMSRBIOSSignID --
 * Vmx86FindCommonMSRVMCR --
 * Vmx86FindCommonMSRJoin --
 *
 *      Calculate least common denominator for IA32_MSR_ARCH_CAPABILITIES,
 *      MSR_BIOS_SIGN_ID, MSR_VM_CR, and general case respectively.
 *
 *-----------------------------------------------------------------------------
 */

static uint64
Vmx86FindCommonMSRArchCap(uint32 msrIndex, uint32 numPCPUs)
{
   uint32 j;
   uint64 msrCommonVal;

   MSRQuery *query = Vmx86FindMSRQueryFromCache(msrIndex, numPCPUs);
   ASSERT(query != NULL);
   ASSERT(msrIndex == IA32_MSR_ARCH_CAPABILITIES);

   msrCommonVal = query->logicalCPUs[0].msrVal;
   /*
    * MSR_ARCH_CAPABILITIES_RSBA bit 1 represents lack of feature while 0
    * represents presence. Therefore, bit is flipped for calculating the
    * least common set and flipped again on the final value for resetting.
    */
   msrCommonVal ^= MSR_ARCH_CAPABILITIES_RSBA;
   for (j = 1; j < numPCPUs; j++) {
      uint64 msrValuePCPU = query->logicalCPUs[j].msrVal;
      if (msrValuePCPU != query->logicalCPUs[0].msrVal) {
         msrValuePCPU ^= MSR_ARCH_CAPABILITIES_RSBA;
         msrCommonVal &= msrValuePCPU;
      }
   }
   msrCommonVal ^= MSR_ARCH_CAPABILITIES_RSBA;
   return msrCommonVal;
}


static uint64
Vmx86FindCommonMSRBIOSSignID(uint32 msrIndex, uint32 numPCPUs)
{
   unsigned cpu;
   uint64 commonVal;

   MSRQuery *query = Vmx86FindMSRQueryFromCache(msrIndex, numPCPUs);
   ASSERT(query != NULL);
   commonVal = ~0ULL;

   for (cpu = 0; cpu < numPCPUs; cpu++) {
      if (query->logicalCPUs[cpu].msrVal < commonVal) {
         commonVal = query->logicalCPUs[cpu].msrVal;
      }
   }

   return commonVal;
}


static uint64
Vmx86FindCommonMSRVMCR(uint32 msrIndex, uint32 numPCPUs)
{
   unsigned cpu;
   uint64 commonVal;

   MSRQuery *query = Vmx86FindMSRQueryFromCache(msrIndex, numPCPUs);
   ASSERT(query != NULL);
   commonVal = query->logicalCPUs[0].msrVal;

   for (cpu = 1; cpu < numPCPUs; cpu++) {
      uint64 msrValuePCPU = query->logicalCPUs[cpu].msrVal;
      commonVal &= msrValuePCPU & MSR_VM_CR_R_INIT;
      commonVal |= msrValuePCPU & ~MSR_VM_CR_R_INIT;
   }

   return commonVal;
}


static uint64
Vmx86FindCommonMSRJoin(uint32 msrIndex, uint32 numPCPUs)
{
   uint32 j;
   uint64 msrCommonVal;

   MSRQuery *query = Vmx86FindMSRQueryFromCache(msrIndex, numPCPUs);
   ASSERT(query != NULL);

   msrCommonVal = query->logicalCPUs[0].msrVal;
   for (j = 1; j < numPCPUs; j++) {
      msrCommonVal &= query->logicalCPUs[j].msrVal;
   }
   return msrCommonVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86GenFindCommonCap --
 * Vmx86GenFindCommonIntelVTCap --
 * Vmx86FindCommonMSR --
 *
 *      Generate common MSR calculation routines by deriving appropriate
 *      function with 'member' name.
 *
 *-----------------------------------------------------------------------------
 */

static uint64
Vmx86GenFindCommonCap(uint32 msrIndex, uint32 numPCPUs)
{
#define MSRNUMVT(msr, member)
#define MSRNUMVT2 MSRNUMVT

#define MSRNUM(msr, member)                                                    \
   if (msrIndex == msr) {                                                      \
      return Vmx86FindCommonMSR##member(msrIndex, numPCPUs);                   \
   } else {                                                                    \
      return Vmx86FindCommonMSRJoin(msrIndex, numPCPUs);                       \
   }

   UNIFORMITY_CACHE_MSRS
#undef MSRNUM
#undef MSRNUMVT
#undef MSRNUMVT2

   return CONST64(0);
}


static uint64
Vmx86GenFindCommonIntelVTCap(uint32 msrIndex, uint32 numPCPUs)
{
   MSRCache vt;
   IntelVTMSRGet_Fn fn = Vmx86VTMSRCacheGet;

   /* Prepare a special cache for VT MSR uniformity checks. */
   vt.queryCache = &msrCacheQueryData;
   vt.nPCPUs = numPCPUs;

#define MSRNUM(msr, member)

#define MSRNUMVT(msr, member)                                                  \
   if (msrIndex == msr) {                                                      \
      return IntelVT_FindCommon##member(&vt, fn, numPCPUs);                    \
   }

#define MSRNUMVT2(msr, member)                                                 \
   if (msrIndex == msr) {                                                      \
      return IntelVT_FindCommon##member(&vt, fn, numPCPUs, msr);               \
   }

   UNIFORMITY_CACHE_MSRS
#undef MSRNUM
#undef MSRNUMVT
#undef MSRNUMVT2

   return CONST64(0);
}


static uint64
Vmx86FindCommonMSR(uint32 msrIndex, uint32 numPCPUs)
{
#define MSRNUM(msr, member)                                                    \
   if (msrIndex == msr) {                                                      \
      return Vmx86GenFindCommonCap(msrIndex, numPCPUs);                        \
   }

#define MSRNUMVT(msr, member)                                                  \
   if (msrIndex == msr) {                                                      \
      return Vmx86GenFindCommonIntelVTCap(msrIndex, numPCPUs);                 \
   }

#define MSRNUMVT2 MSRNUMVT

   UNIFORMITY_CACHE_MSRS
#undef MSRNUM
#undef MSRNUMVT
#undef MSRNUMVT2

   return CONST64(0);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vmx86_CheckMSRUniformity --
 *
 *      Provides basic hardware MSR feature checks for x86 hosted platform. VMM
 *      requires and prefers uniformity of certain MSRs. This function iterates
 *      through a list of MSR features (i.e. msrUniformityCacheInfo), checking
 *      uniformity for MSR value on each logical CPU. A Uniformity check is
 *      ignored for MSRs are that are not available for the target architecture
 *      or cpu family. If MSRs are non uniform then, a common bit field is
 *      calculated by taking the intersection of MSR values across cpu(s).
 *
 * Results:
 *      Returns TRUE if MSR uniformity checks complete successfully, FALSE
 *      otherwise.
 *
 * Side effects:
 *      updates msrUniformityCacheInfo cache with MSR values.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Vmx86_CheckMSRUniformity(void)
{
   uint32 i;
   Atomic_uint32 numLogicalCPUs;
   uint32 numPCPUs = 0;

   Atomic_Write32(&numLogicalCPUs, 0);
   /*
    * Calculates number of logical CPUs by counting and then uses this
    * information to set up MSR queries; will be executed on each logical CPU.
    */
   HostIF_CallOnEachCPU(Vmx86RegisterCPU, &numLogicalCPUs);
   numPCPUs = Atomic_Read32(&numLogicalCPUs);
   ASSERT(numPCPUs > 0);

   if (!Vmx86AllocMSRUniformityCache(numPCPUs)) {
      Warning("Fatal, not enough memory for MSR feature uniformity checks");
      return FALSE;
   }

   Vmx86CheckMSRUniformity(numPCPUs);

   for (i = 0; i < ARRAYSIZE(msrUniformityCacheInfo); ++i) {
      uint32 msrIndex = msrUniformityCacheInfo[i].msrIndex;
      msrUniformityCacheInfo[i].msrValue = Vmx86FindCommonMSR(msrIndex,
                                                              numPCPUs);
   }

   Vmx86FreeMSRUniformityCache();

   return TRUE;
}

