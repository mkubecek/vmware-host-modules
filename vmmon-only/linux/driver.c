/*********************************************************
 * Copyright (C) 1998-2020 VMware, Inc. All rights reserved.
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

/* Must come before any kernel header file */
#include "driver-config.h"

#define EXPORT_SYMTAB

#include <linux/file.h>
#include <linux/highmem.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/preempt.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/wait.h>

#include <asm/hw_irq.h> /* for CALL_FUNCTION_VECTOR */

#include "compat_version.h"
#include "compat_module.h"

#include "usercalldefs.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#error Linux kernels before 3.10 are not supported
#endif

#include <asm/io.h>

#include "vmware.h"
#include "driverLog.h"
#include "driver.h"
#include "modulecall.h"
#include "vm_asm.h"
#include "vmx86.h"
#include "task.h"
#include "memtrack.h"
#include "task.h"
#include "cpuid.h"
#include "circList.h"
#include "x86msr.h"

#ifdef VMX86_DEVEL
#include "private.h"
#endif

#include "hostif.h"
#include "hostif_priv.h"
#include "vmhost.h"
#include "sharedAreaVmmon.h"

static void LinuxDriverQueue(Device *device);
static void LinuxDriverDequeue(Device *device);
static Bool LinuxDriverCheckPadding(void);

struct VMXLinuxState linuxState;


/*
 *----------------------------------------------------------------------
 *
 * Device Driver Interface --
 *
 *      Runs the VM by implementing open/close/ioctl functions
 *
 *
 *----------------------------------------------------------------------
 */

static int LinuxDriver_Open(struct inode *inode, struct file *filp);

/*
 * gcc-4.5+ can name-mangle LinuxDriver_Ioctl, but our stack-size
 * script needs to find it.  So it shouldn't be static.  ("hidden"
 * visibility would be OK.)
 */
long LinuxDriver_Ioctl(struct file *filp, u_int iocmd,
                       unsigned long ioarg);

static int LinuxDriver_Close(struct inode *inode, struct file *filp);

static unsigned int LinuxDriverEstimateTSCkHz(void);

static const struct file_operations vmuser_fops = {
   .owner = THIS_MODULE,
   .open = LinuxDriver_Open,
   .release = LinuxDriver_Close,
   .unlocked_ioctl = LinuxDriver_Ioctl,
   .compat_ioctl = LinuxDriver_Ioctl,
};

#ifndef VMX86_DEVEL
static struct miscdevice vmmon_miscdev = {
   .name = "vmmon",
   .minor = MISC_DYNAMIC_MINOR,
   .fops = &vmuser_fops,
};
#endif

static struct timer_list tscTimer;
static Atomic_uint32 tsckHz;
static VmTimeStart tsckHzStartTime;


/*
 *----------------------------------------------------------------------
 *
 * LinuxDriverReadTSCAndUptimeSmpCB --
 * LinuxDriverReadTSCAndUptime --
 *
 *       Read TSC and uptime on CPU 0. Reading on CPU 0 is best
 *       effort, and the remote smp function call may fail for two
 *       reasons: either the function is not supportd by the kernel,
 *       or the cpu went offline. In this unlikely event, we perform
 *       the read on the local cpu.
 *
 *----------------------------------------------------------------------
 */

static void
LinuxDriverReadTSCAndUptimeSmpCB(void *arg)
{
   VmTimeStart *time = (VmTimeStart *) arg;
   Vmx86_ReadTSCAndUptime(time);
   /* Ensure the above write is visible to the remote caller. */
   SMP_RW_BARRIER_RW();
}

static void
LinuxDriverReadTSCAndUptime(VmTimeStart *time)
{
   if (smp_call_function_single(0, LinuxDriverReadTSCAndUptimeSmpCB,
                                (void *)time, 1) != 0) {
      LinuxDriverReadTSCAndUptimeSmpCB(time);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxDriverEstimateTSCkHz --
 *
 *      Estimates TSC frequency in terms of cycles and system uptime
 *      elapsed since module init. At module init, the starting cycle
 *      count and uptime are recorded (in tsckHzStartTime) and a timer
 *      is scheduled to call this function after 4 seconds.
 *
 *      It is possible that vmx queries the TSC rate after module init
 *      but before the 4s timer expires. In that case, we just go ahead
 *      and compute the rate for the duration since the driver loaded.
 *      When the timer expires, the new computed value is dropped. If the
 *      query races with the timer, the first thread to write to 'tsckHz'
 *      wins.
 *
 * Results:
 *
 *      Returns the estimated TSC khz, cached in tscKhz. If tsckHz is
 *      0, the reads uptime on CPU 0 and estimates tsc khz, followed
 *      by caching it in tsckHz.
 *
 *----------------------------------------------------------------------
 */

static uint32
LinuxDriverEstimateTSCkHz(void)
{
   uint32 khz;
   VmTimeStart curTime;
   uint64 cycles;
   uint64 uptime;

   khz = Atomic_Read(&tsckHz);
   if (khz != 0) {
      return khz;
   }

   ASSERT(tsckHzStartTime.count != 0);
   LinuxDriverReadTSCAndUptime(&curTime);
   cycles = curTime.count - tsckHzStartTime.count;
   uptime = curTime.time  - tsckHzStartTime.time;
   khz    = Vmx86_ComputekHz(cycles, uptime);

   if (khz != 0) {
       if (Atomic_ReadIfEqualWrite(&tsckHz, 0, khz) == 0) {
          Log("TSC frequency estimated using system uptime: %u\n", khz);
       }
   } else if (Atomic_ReadIfEqualWrite(&tsckHz, 0, cpu_khz) == 0) {
       Log("Failed to compute TSC frequency, using cpu_khz: %u\n", cpu_khz);
   }

   return Atomic_Read(&tsckHz);
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxDriverEstimateTSCkHzDeferred --
 *
 *      Timer callback for deferred TSC rate estimation.
 *
 *----------------------------------------------------------------------
 */
static void
LinuxDriverEstimateTSCkHzDeferred(struct timer_list *data)
{
   LinuxDriverEstimateTSCkHz();
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxDriverInitTSCkHz --
 *
 *      Initialize TSC khz rate.
 *
 *      We rely on the kernel estimated cycle rate in the exported
 *      variable tsc_khz. If the kernel has disabled tsc, tsc_khz
 *      will be 0, and we fall back on our own estimation routines.
 *
 * Side effects:
 *
 *      If tsc_khz is unusable, schedules a 4s timer for deferred
 *      khz estimation (see LinuxDriverEstimateTSCkHz).
 *
 *----------------------------------------------------------------------
 */

static void
LinuxDriverInitTSCkHz(void)
{
   if (tsc_khz != 0) {  /* Exported kernel value */
      Atomic_Write(&tsckHz, tsc_khz);
      Log("Using tsc_khz as TSC frequency: %u\n", tsc_khz);
      return;
   }

   LinuxDriverReadTSCAndUptime(&tsckHzStartTime);
   tscTimer.expires  = jiffies + 4 * HZ;
   add_timer(&tscTimer);
}


/*
 *----------------------------------------------------------------------
 *
 * init_module --
 *
 *      linux module entry point. Called by /sbin/insmod command
 *
 * Results:
 *      Release: registers a device driver with a misc minor node.
 *      Devel: registers for a major number with user-created node.
 *
 *----------------------------------------------------------------------
 */

int
init_module(void)
{
   int retval;

   DriverLog_Init("/dev/vmmon");
   HostIF_InitGlobalLock();

   if (!LinuxDriverCheckPadding()) {
      return -ENOEXEC;
   }

   CPUID_Init();
   Vmx86_CacheNXState();

   if (!Task_Initialize()) {
      return -ENOEXEC;
   }

   if (!Vmx86_CreateHVIOBitmap()) {
      return -ENOMEM;
   }

   if (!Vmx86_CheckMSRUniformity()) {
      return -EPERM;
   }

   linuxState.fastClockThread = NULL;
   linuxState.fastClockRate = 0;

#ifdef VMX86_DEVEL
   devel_init_module();
   retval = register_chrdev(linuxState.major, linuxState.deviceName,
                            &vmuser_fops);
   if (retval) {
      Warning("Module %s: error registering with major=%d\n",
              linuxState.deviceName, linuxState.major);
   } else {
      Log("Module %s: registered with major=%d\n",
          linuxState.deviceName, linuxState.major);
   }
#else
   sprintf(linuxState.deviceName, "vmmon");
   linuxState.major = 10;

   retval = misc_register(&vmmon_miscdev);
   if (retval) {
      Warning("Module %s: error registering misc device %s\n",
              linuxState.deviceName, vmmon_miscdev.name);
   } else {
      Log("Module %s: registered as misc device %s\n", linuxState.deviceName,
          vmmon_miscdev.name);
   }
#endif

   if (retval) {
      Vmx86_CleanupHVIOBitmap();
      return -ENOENT;
   }

   HostIF_InitUptime();
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0) && !defined(timer_setup)
   init_timer(&tscTimer);
   tscTimer.function = (void *)LinuxDriverEstimateTSCkHzDeferred;
   tscTimer.data = (unsigned long)&tscTimer;
#else
   timer_setup(&tscTimer, LinuxDriverEstimateTSCkHzDeferred, 0);
#endif
   LinuxDriverInitTSCkHz();
   Vmx86_InitIDList();

   Log("Module %s: initialized\n", linuxState.deviceName);

   return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * cleanup_module --
 *
 *      Called by /sbin/rmmod
 *
 *
 *----------------------------------------------------------------------
 */

void
cleanup_module(void)
{
   /*
    * XXX smp race?
    */
#ifdef VMX86_DEVEL
   unregister_chrdev(linuxState.major, linuxState.deviceName);
#else
   misc_deregister(&vmmon_miscdev);
#endif

   Log("Module %s: unloaded\n", linuxState.deviceName);

   del_timer_sync(&tscTimer);

   Vmx86_CleanupHVIOBitmap();
   Task_Terminate();
   // Make sure fastClockThread is dead
   HostIF_FastClockLock(1);
   HostIF_SetFastClockRate(0);
   HostIF_FastClockUnlock(1);

   HostIF_CleanupUptime();
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxDriver_Open  --
 *
 *      called on open of /dev/vmmon or /dev/vmx86.$USER. Use count used
 *      to determine eventual deallocation of the module
 *
 * Side effects:
 *     Increment use count used to determine eventual deallocation of
 *     the module
 *
 *----------------------------------------------------------------------
 */

static int
LinuxDriver_Open(struct inode *inode, // IN
                 struct file *filp)   // IN
{
   Device *device;

   device = kmalloc(sizeof *device, GFP_KERNEL);
   if (device == NULL) {
      return -ENOMEM;
   }
   memset(device, 0, sizeof *device);

   init_rwsem(&device->vmDriverRWSema);

   filp->private_data = device;
   LinuxDriverQueue(device);

   Vmx86_Open();

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxDriver_Close  --
 *
 *      called on close of /dev/vmmon or /dev/vmx86.$USER, most often when the
 *      process exits. Decrement use count, allowing for possible uninstalling
 *      of the module.
 *
 *----------------------------------------------------------------------
 */

static int
LinuxDriver_Close(struct inode *inode, // IN
                  struct file *filp)   // IN
{
   Device *device;

   device = (Device *)filp->private_data;
   ASSERT(device);

   LinuxDriverDequeue(device);
   down_write(&device->vmDriverRWSema);
   if (device->vm != NULL) {
      Vmx86_ReleaseVM(device->vm);
      device->vm = NULL;
   }
   up_write(&device->vmDriverRWSema);

   Vmx86_Close();

   kfree(device);
   filp->private_data = NULL;

   return 0;
}

typedef Bool (*SyncFunc)(void *data, unsigned cpu);

typedef struct {
   Atomic_uint32 numCPUs;
   Atomic_uint32 ready;
   Atomic_uint32 failures;
   Atomic_uint32 done;
   SyncFunc      func;
   void          *data;
} SyncFuncArgs;


/*
 *-----------------------------------------------------------------------------
 *
 * LinuxDriverSyncCallHook --
 *
 *      Called on each CPU, waits for them all to show up, and executes
 *      the callback.
 *
 * Results:
 *
 * Side effects:
 *      Whatever side effects the callback has.
 *
 *-----------------------------------------------------------------------------
 */

static void
LinuxDriverSyncCallHook(void *data)  // IN:
{
   Bool success;
   uint32 numCPUs;
   volatile unsigned iterations = 1000 * 1000;
   SyncFuncArgs *args = (SyncFuncArgs *)data;
   unsigned cpu = smp_processor_id();

   /*
    * We need to careful about reading cpu_online_map on kernels that
    * have hot add/remove cpu support.  The kernel's smp_call_function
    * blocks hot add from occuring between the time it computes the set
    * of cpus it will IPI and when all those cpus have entered their IPI
    * handlers.  Additionally, we disabled preemption on the initiating
    * cpu during the entire sync call sequence.  So, since a cpu hot add
    * is initiated from process context, a cpu cannot be hot added until
    * at least one cpu has exited this code, and therefore it is safe
    * for the first cpu to reach this point to read cpu_online_map.
    *
    * Hot remove works by stopping the entire machine, which is done by
    * waiting for a set of kernel threads to be scheduled on all cpus.
    * This cannot happen until all cpus are preemptible.  Since the
    * initiating cpu has preemption disabled during this entire
    * sequence, this code is also safe from cpu hot remove.
    *
    * So, the first cpu to reach this code will read the same value of
    * cpu_online_map that was used by smp_call_function, and therefore
    * we can safely assume that numCPUs cpus will execute this routine.
    */

   Atomic_CMPXCHG32(&args->numCPUs, 0, num_online_cpus());
   numCPUs = Atomic_Read(&args->numCPUs);

   Atomic_Inc(&args->ready);

   /*
    * Wait for all CPUs, but not forever since we could deadlock.  The
    * potential deadlock scenerio is this: cpu0 has IF=1 and holds a
    * lock.  cpu1 has IF=0 and is spinning waiting for the lock.
    */

   while (Atomic_Read(&args->ready) != numCPUs && --iterations) ;

   /* Now simultaneously call the routine. */
   success = args->func(args->data, cpu);

   if (!iterations || !success) {
      /* Indicate that we either timed out or the callback failed. */
      Atomic_Inc(&args->failures);
   }
   /* Indicate that we are finished. */
   Atomic_Inc(&args->done);
}


/*
 *-----------------------------------------------------------------------------
 *
 * LinuxDriverSyncCallOnEachCPU --
 *
 *      Calls func on each cpu at (nearly) the same time.
 *
 * Results:
 *      TRUE if func was called at the same time on all cpus.  Note that
 *      func is called regardless of whether all cpus showed up in time.
 *
 * Side effects:
 *      func's side effects, on all cpus.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
LinuxDriverSyncCallOnEachCPU(SyncFunc func,  // IN:
                             void *data)     // IN:
{
   SyncFuncArgs args;
   uintptr_t flags;

   ASSERT(HostIF_GlobalLockIsHeld());

   args.func = func;
   args.data = data;

   Atomic_Write(&args.numCPUs, 0); // Must be calculated inside the callback.
   Atomic_Write(&args.ready, 0);
   Atomic_Write(&args.failures, 0);
   Atomic_Write(&args.done, 0);

   preempt_disable();

   /*
    * Call all other CPUs, but do not wait so we can enter the callback
    * on this CPU too.
    */

   smp_call_function(LinuxDriverSyncCallHook, &args, 0);

   /*
    * smp_call_function doesn't return until all cpus have been
    * interrupted.  It's safe to disable interrupts now that all other
    * cpus are in their IPI handlers.
    */

   SAVE_FLAGS(flags);
   CLEAR_INTERRUPTS();

   LinuxDriverSyncCallHook(&args);

   RESTORE_FLAGS(flags);
   preempt_enable();

   /*
    * Wait for everyone else to finish so we can get an accurate
    * failures count.
    */

   while (Atomic_Read(&args.done) != Atomic_Read(&args.numCPUs)) ;

   /*
    * This routine failed if any CPU bailed out early to avoid deadlock,
    * or the callback routine failed on any CPU.  Both conditions are
    * recorded in the failures field.
    */

   return Atomic_Read(&args.failures) == 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * LinuxDriverReadTSC --
 *
 *      Callback that is executed simultaneously on all cpus to read the TSCs.
 *
 * Results:
 *      TRUE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
LinuxDriverReadTSC(void *data,   // OUT: TSC values
                   unsigned cpu) // IN: the pcpu number
{
   TSCDelta *tscDelta = (TSCDelta *)data;
   uint64 tsc, old;

   if (LIKELY(CPUID_SSE2Supported())) {
      RDTSC_BARRIER();
   }
   tsc = RDTSC();

   /* Any looping means another CPU changed min/max. */
   do {
      old = Atomic_Read64(&tscDelta->min);
   } while (old > tsc && !Atomic_CMPXCHG64(&tscDelta->min, old, tsc));
   do {
      old = Atomic_Read64(&tscDelta->max);
   } while (old < tsc && !Atomic_CMPXCHG64(&tscDelta->max, old, tsc));

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * LinuxDriverSyncReadTSCs --
 *
 *      Simultaneously read the TSCs on all cpus.
 *
 * Results:
 *      The set of all TSCs.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

__always_inline static Bool
LinuxDriverSyncReadTSCs(uint64 *delta) // OUT: TSC max - TSC min
{
   TSCDelta tscDelta;
   unsigned i;
   Bool okay = FALSE;

   /* Take the global lock to block concurrent calls. */
   HostIF_GlobalLock(14);

   /* Loop to warm up the cache. */
   for (i = 0; i < 3; i++) {
      Atomic_Write64(&tscDelta.min, ~CONST64U(0));
      Atomic_Write64(&tscDelta.max, CONST64U(0));

      if (LinuxDriverSyncCallOnEachCPU(LinuxDriverReadTSC, &tscDelta)) {
         /* We return the last successful simultaneous read of the TSCs. */
         *delta = Atomic_Read64(&tscDelta.max) - Atomic_Read64(&tscDelta.min);
         okay = TRUE;
      }
   }
   HostIF_GlobalUnlock(14);

   return okay;
}


/*
 *-----------------------------------------------------------------------------
 *
 * LinuxDriver_Ioctl --
 *
 *      Main path for UserRPC
 *
 *      Be VERY careful with stack usage; gcc's stack allocation is iffy
 *      and allocations from individual "case" statements do not overlap,
 *      so it is easy to use kilobytes of stack space here.
 *
 * Results:
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

long
LinuxDriver_Ioctl(struct file *filp,    // IN:
                  u_int iocmd,          // IN:
                  unsigned long ioarg)  // IN:
{
   Device *device = (Device *) filp->private_data;
   long retval = 0;
   Vcpuid vcpuid;
   VMDriver *vm;
   Bool needsWriteLock = iocmd == IOCTL_VMX86_CREATE_VM ||
                         iocmd == IOCTL_VMX86_RELEASE_VM;

   ASSERT_ON_COMPILE(sizeof(long) == sizeof(int64));
   if (device == NULL) {
      return -EINVAL;
   }

   if (needsWriteLock) {
      down_write(&device->vmDriverRWSema);
   } else {
      down_read(&device->vmDriverRWSema);
   }

   vm = device->vm;
   /* Validate the VM pointer for those IOCTLs that require it. */
   switch (iocmd) {
   case IOCTL_VMX86_VERSION:
   case IOCTL_VMX86_CREATE_VM:
   case IOCTL_VMX86_SET_UID:
   case IOCTL_VMX86_GET_NUM_VMS:
   case IOCTL_VMX86_SET_HARD_LIMIT:
   case IOCTL_VMX86_GET_IPI_VECTORS:
   case IOCTL_VMX86_GET_KHZ_ESTIMATE:
   case IOCTL_VMX86_GET_ALL_CPUID:
   case IOCTL_VMX86_GET_ALL_MSRS:
   case IOCTL_VMX86_GET_REFERENCE_CLOCK_HZ:
   case IOCTL_VMX86_INIT_PSEUDO_TSC:
   case IOCTL_VMX86_CHECK_PSEUDO_TSC:
   case IOCTL_VMX86_GET_PSEUDO_TSC:
   case IOCTL_VMX86_SYNC_GET_TSCS:
   case IOCTL_VMX86_GET_UNAVAIL_PERF_CTRS:
      break;

   default:
      if (vm == NULL) {
         retval = -EINVAL;
         goto exit;
      }
   }

   /*
    * Perform the IOCTL operation.
    */

   switch (iocmd) {
   case IOCTL_VMX86_VERSION:
      retval = VMMON_VERSION;
      break;

   case IOCTL_VMX86_CREATE_VM: {
      VMCreateBlock args;

      if (vm != NULL) {
         retval = -EINVAL;
         break;
      }
      retval = HostIF_CopyFromUser(&args, (VA64)ioarg, sizeof args);
      if (retval != 0) {
         break;
      }
      vm = Vmx86_CreateVM(args.bsBlob, args.bsBlobSize, args.numVCPUs);

      if (vm == NULL) {
         retval = -ENOMEM;
      } else {
         device->vm = vm;
         args.vmid = vm->userID;
         retval = HostIF_CopyToUser((VA64)ioarg, &args, sizeof args);
      }
      break;
   }

   case IOCTL_VMX86_PROCESS_BOOTSTRAP: {
      VMProcessBootstrapBlock *args;
      size_t argsSz = GetVMProcessBootstrapBlockSize(vm->numVCPUs);
      VA64 uAddr;

      retval = HostIF_CopyFromUser(&uAddr, (VA64)ioarg, sizeof uAddr);
      if (retval != 0) {
         break;
      }
      args = HostIF_AllocKernelMem(argsSz, TRUE);
      if (args == NULL) {
         retval = -ENOMEM;
         break;
      }
      retval = HostIF_CopyFromUser(args, uAddr, argsSz);
      if (retval != 0) {
         HostIF_FreeKernelMem(args);
         break;
      }
      if (args->numVCPUs != vm->numVCPUs) {
         retval = -EINVAL;
      } else if (!Vmx86_ProcessBootstrap(vm,
                                         args->bsBlobAddr,
                                         args->numBytes,
                                         args->headerOffset,
                                         args->numVCPUs,
                                         args->perVcpuPages,
                                         args->shRegions)) {
         retval = -ENOMEM;
      }
      HostIF_FreeKernelMem(args);
      break;
   }

   case IOCTL_VMX86_REGISTER_SHARED: {
      VMSharedAreaRegistrationBlock args;

      retval = HostIF_CopyFromUser(&args, (VA64)ioarg, sizeof args);
      if (retval != 0) {
         break;
      }
      if (!SharedAreaVmmon_ValidateRegionArgs(vm, &args)) {
         retval = -EINVAL;
         break;
      }
      if (!SharedAreaVmmon_RegisterRegion(vm, &args)) {
         retval = -ENOMEM;
      }
      break;
   }

   case IOCTL_VMX86_REGISTER_STATVARS: {
      VMStatVarsRegistrationBlock args;

      retval = HostIF_CopyFromUser(&args, (VA64)ioarg, sizeof args);
      if (retval != 0) {
         break;
      }
      if (!StatVarsVmmon_RegisterVCPU(vm, &args)) {
         retval = -ENOMEM;
      }
      break;
   }

   case IOCTL_VMX86_RELEASE_VM:
      device->vm = NULL;
      Vmx86_ReleaseVM(vm);
      break;

   case IOCTL_VMX86_RUN_VM:
      vcpuid = ioarg;

      if (vcpuid >= vm->numVCPUs) {
         retval = -EINVAL;
         break;
      }
      retval = Vmx86_RunVM(vm, vcpuid);
      break;

   case IOCTL_VMX86_SET_UID:
#ifdef VMX86_DEVEL
      devel_suid();
#else
      retval = -EPERM;
#endif
      break;

   case IOCTL_VMX86_LOCK_PAGE:
   case IOCTL_VMX86_LOCK_PAGE_NEW: {
      VMLockPage args;

      retval = HostIF_CopyFromUser(&args, ioarg, sizeof args);
      if (retval) {
         break;
      }
      args.ret.status = Vmx86_LockPage(vm, args.uAddr,
                                       iocmd == IOCTL_VMX86_LOCK_PAGE_NEW,
                                       &args.ret.mpn);
      retval = HostIF_CopyToUser(ioarg, &args, sizeof args);
      break;
   }

   case IOCTL_VMX86_UNLOCK_PAGE: {
      VA64 uAddr;

      retval = HostIF_CopyFromUser(&uAddr, ioarg, sizeof uAddr);
      if (retval) {
         break;
      }
      retval = Vmx86_UnlockPage(vm, uAddr);
      break;
   }

   case IOCTL_VMX86_UNLOCK_PAGE_BY_MPN: {
      VMMUnlockPageByMPN args;

      retval = HostIF_CopyFromUser(&args, ioarg, sizeof args);
      if (retval) {
         break;
      }
      retval = Vmx86_UnlockPageByMPN(vm, args.mpn, args.uAddr);
      break;
   }

   case IOCTL_VMX86_LOOK_UP_MPN: {
      VMLockPage args;

      retval = HostIF_CopyFromUser(&args, ioarg, sizeof args);
      if (retval) {
         break;
      }
      args.ret.status = Vmx86_LookupUserMPN(vm, args.uAddr, &args.ret.mpn);
      retval = HostIF_CopyToUser(ioarg, &args, sizeof args);
      break;
   }

   case IOCTL_VMX86_GET_VMM_PAGE_ROOT: {
      VcpuPageRoot args;

      retval = HostIF_CopyFromUser(&args, ioarg, sizeof args);
      if (retval) {
         break;
      }
      if (args.vcpuid >= vm->numVCPUs || vm->ptRootMpns == NULL) {
         retval = -EINVAL;
      } else {
         args.pageRoot = vm->ptRootMpns[args.vcpuid];
         retval = HostIF_CopyToUser(ioarg, &args, sizeof args);
      }
      break;
   }

   case IOCTL_VMX86_GET_NUM_VMS:
      retval = Vmx86_GetNumVMs();
      break;

   case IOCTL_VMX86_SET_HARD_LIMIT: {
      PageCnt limit;
      retval = HostIF_CopyFromUser(&limit, ioarg, sizeof limit);
      if (retval != 0) {
         break;
      }
      if (!Vmx86_SetConfiguredLockedPagesLimit(limit)) {
         retval = -EINVAL;
      }
      break;
   }

   case IOCTL_VMX86_ADMIT: {
      VMMemInfoArgs args;

      retval = HostIF_CopyFromUser(&args, ioarg, sizeof args);
      if (retval != 0) {
         break;
      }
      Vmx86_Admit(vm, &args);
      retval = HostIF_CopyToUser(ioarg, &args, sizeof args);
      break;
   }

   case IOCTL_VMX86_READMIT: {
      OvhdMem_Deltas delta;

      retval = HostIF_CopyFromUser(&delta, ioarg, sizeof delta);
      if (retval != 0) {
         break;
      }
      if (!Vmx86_Readmit(vm, &delta)) {
         retval = -1;
      }

      break;
   }

   case IOCTL_VMX86_UPDATE_MEM_INFO: {
      VMMemMgmtInfoPatch patch;

      retval = HostIF_CopyFromUser(&patch, ioarg, sizeof patch);
      if (retval == 0) {
         Vmx86_UpdateMemInfo(vm, &patch);
      }
      break;
   }

   case IOCTL_VMX86_GET_MEM_INFO: {
      VA64 uAddr;
      VMMemInfoArgs in;
      VMMemInfoArgs *out;

      retval = HostIF_CopyFromUser(&uAddr, ioarg, sizeof uAddr);
      if (retval) {
         break;
      }

      retval = HostIF_CopyFromUser(&in, uAddr, sizeof in);
      if (retval) {
         break;
      }

      if (in.numVMs < 1 || in.numVMs > MAX_VMS) {
         retval = -EINVAL;
         break;
      }
      out = HostIF_AllocKernelMem(VM_GET_MEM_INFO_SIZE(in.numVMs), TRUE);
      if (!out) {
         retval = -ENOMEM;
         break;
      }

      *out = in;
      if (!Vmx86_GetMemInfo(vm, FALSE, out, VM_GET_MEM_INFO_SIZE(in.numVMs))) {
         HostIF_FreeKernelMem(out);
         retval = -ENOBUFS;
         break;
      }

      retval = HostIF_CopyToUser(uAddr, out,
                                 VM_GET_MEM_INFO_SIZE(out->numVMs));
      HostIF_FreeKernelMem(out);
      break;
   }

   case IOCTL_VMX86_APIC_INIT: {
      /*
       * Kernel uses NMIs for deadlock detection - so we always have to find
       * APIC so that NMIs get disabled in the monitor.
       */
      HostIF_APICInit(vm);
      retval = 0;
      break;
   }

   case IOCTL_VMX86_SET_HOST_CLOCK_RATE:
      retval = -Vmx86_SetHostClockRate(vm, (unsigned)ioarg);
      break;

   case IOCTL_VMX86_SEND_ONE_IPI: {
      Vcpuid v = ioarg;
      if (v < vm->numVCPUs) {
         HostIF_OneIPI(vm, v);
      } else {
         retval = -EINVAL;
      }
      break;
   }

   case IOCTL_VMX86_SEND_IPI: {
      VCPUSet ipiTargets;

      retval = HostIF_CopyFromUser(&ipiTargets, ioarg, sizeof ipiTargets);

      if (retval == 0) {
         HostIF_IPI(vm, &ipiTargets);
      }

      break;
   }

   case IOCTL_VMX86_GET_IPI_VECTORS: {
      IPIVectors ipiVectors;

      ipiVectors.monitorIPIVector = HostIF_GetMonitorIPIVector();
      ipiVectors.hvIPIVector      = HostIF_GetHVIPIVector();

      retval = HostIF_CopyToUser(ioarg, &ipiVectors, sizeof ipiVectors);
      break;
   }

   case IOCTL_VMX86_GET_SWITCH_ERROR_ADDR: {
      VMSwitchErrorArgs args;

      retval = HostIF_CopyFromUser(&args, ioarg, sizeof args);
      if (retval != 0) {
         break;
      }
      if (args.vcpuid >= vm->numVCPUs || vm->crosspage == NULL ||
          vm->crosspage[args.vcpuid] == NULL) {
         retval = -EINVAL;
         break;
      }
      args.addr = vm->crosspage[args.vcpuid]->wsUD2;
      retval = HostIF_CopyToUser(ioarg, &args, sizeof args);
      break;
   }

   case IOCTL_VMX86_GET_KHZ_ESTIMATE:
      retval = LinuxDriverEstimateTSCkHz();
      break;

   case IOCTL_VMX86_GET_ALL_CPUID: {
      VA64 uAddr;
      CPUIDQuery in;
      CPUIDQuery *out;

      retval = HostIF_CopyFromUser(&uAddr, ioarg, sizeof uAddr);
      if (retval) {
         break;
      }

      retval = HostIF_CopyFromUser(&in, uAddr, sizeof in);
      if (retval) {
         break;
      }

      /*
       * Some kernels panic on kmalloc request larger than 128KB.
       * XXX This test should go inside HostIF_AllocKernelMem() then.
       */

      if (in.numLogicalCPUs >
                        (131072 - sizeof *out) / sizeof out->logicalCPUs[0]) {
         retval = -EINVAL;
         break;
      }
      out = HostIF_AllocKernelMem(
         sizeof *out + in.numLogicalCPUs * sizeof out->logicalCPUs[0],
         TRUE);
      if (!out) {
         retval = -ENOMEM;
         break;
      }

      *out = in;
      if (!HostIF_GetAllCpuInfo(out)) {
         HostIF_FreeKernelMem(out);
         retval = -ENOBUFS;
         break;
      }

      retval = HostIF_CopyToUser(uAddr + sizeof in,
                                 &out->logicalCPUs[0],
                                 out->numLogicalCPUs *
                                 sizeof out->logicalCPUs[0]);
      HostIF_FreeKernelMem(out);
      break;
   }

   case IOCTL_VMX86_GET_ALL_MSRS: {
      VA64 uAddr;
      MSRQuery in;
      MSRQuery *out;

      retval = HostIF_CopyFromUser(&uAddr, ioarg, sizeof uAddr);
      if (retval) {
         break;
      }

      retval = HostIF_CopyFromUser(&in, uAddr, sizeof in);
      if (retval) {
         break;
      }

      /*
       * Some kernels panic on kmalloc request larger than 128KB.
       * XXX This test should go inside HostIF_AllocKernelMem() then.
       */

      if (in.numLogicalCPUs >
                        (131072 - sizeof *out) / sizeof out->logicalCPUs[0]) {
         retval = -EINVAL;
         break;
      }
      out = HostIF_AllocKernelMem(
         sizeof *out + in.numLogicalCPUs * sizeof out->logicalCPUs[0],
         TRUE);
      if (!out) {
         retval = -ENOMEM;
         break;
      }

      *out = in;
      if (!Vmx86_GetAllMSRs(out)) {
         HostIF_FreeKernelMem(out);
         retval = -ENOBUFS;
         break;
      }

      retval = HostIF_CopyToUser(uAddr + sizeof in,
                                 &out->logicalCPUs[0],
                                 out->numLogicalCPUs *
                                 sizeof out->logicalCPUs[0]);
      HostIF_FreeKernelMem(out);
      break;
   }

   case IOCTL_VMX86_ALLOC_LOCKED_PAGES: {
         VMMPNList req;

         retval = HostIF_CopyFromUser(&req, ioarg, sizeof req);
         if (retval) {
            break;
         }
         retval = Vmx86_AllocLockedPages(vm, req.mpnList,
                                         req.mpnCount, FALSE,
                                         req.ignoreLimits);
         break;
      }

   case IOCTL_VMX86_GET_NEXT_ANON_PAGE: {
      VMMPNNext req;

      retval = HostIF_CopyFromUser(&req, ioarg, sizeof req);
      if (retval) {
         req.outMPN = INVALID_MPN;
      } else {
         req.outMPN = Vmx86_GetNextAnonPage(vm, req.inMPN);
      }
      retval = HostIF_CopyToUser(ioarg, &req, sizeof req);
      break;
   }

   case IOCTL_VMX86_GET_NUM_ANON_PAGES: {
      PageCnt numAnonPages;
      numAnonPages = Vmx86_GetNumAnonPages(vm);
      retval = HostIF_CopyToUser(ioarg, &numAnonPages, sizeof numAnonPages);
      break;
   }

   case IOCTL_VMX86_READ_PAGE: {
         VMMReadWritePage req;

         retval = HostIF_CopyFromUser(&req, ioarg, sizeof req);
         if (retval) {
            break;
         }

         retval = HostIF_ReadPhysical(vm, MPN_2_MA(req.mpn), req.uAddr, FALSE,
                                      PAGE_SIZE);
         break;
      }

   case IOCTL_VMX86_WRITE_PAGE: {
#ifdef VMX86_DEVEL
         VMMReadWritePage req;

         retval = HostIF_CopyFromUser(&req, ioarg, sizeof req);
         if (retval) {
            break;
         }
         retval = HostIF_WritePhysical(vm, MPN_2_MA(req.mpn), req.uAddr, FALSE,
                                       PAGE_SIZE);
#else
         retval = -EINVAL;
#endif
         break;
      }

   case IOCTL_VMX86_GET_REFERENCE_CLOCK_HZ: {
      uint64 refClockHz = HostIF_UptimeFrequency();

      retval = HostIF_CopyToUser(ioarg, &refClockHz, sizeof refClockHz);
      break;
   }

   case IOCTL_VMX86_INIT_PSEUDO_TSC: {
      PTSCInitParams params;

      retval = HostIF_CopyFromUser(&params, ioarg, sizeof params);
      if (retval != 0) {
         break;
      }
      Vmx86_InitPseudoTSC(&params);
      retval = HostIF_CopyToUser(ioarg, &params, sizeof params);
      break;
   }

   case IOCTL_VMX86_CHECK_PSEUDO_TSC: {
      PTSCCheckParams params;

      retval = HostIF_CopyFromUser(&params, ioarg, sizeof params);
      if (retval != 0) {
         break;
      }
      params.usingRefClock = Vmx86_CheckPseudoTSC(&params.lastTSC,
                                                  &params.lastRC);

      retval = HostIF_CopyToUser(ioarg, &params, sizeof params);
      break;
   }

   case IOCTL_VMX86_GET_PSEUDO_TSC: {
      uint64 ptsc = Vmx86_GetPseudoTSC();

      retval = HostIF_CopyToUser(ioarg, &ptsc, sizeof ptsc);
      break;
   }

   case IOCTL_VMX86_SYNC_GET_TSCS: {
      uint64 delta;

      if (LinuxDriverSyncReadTSCs(&delta)) {
         retval = HostIF_CopyToUser(ioarg, &delta, sizeof delta);
       } else {
         retval = -EBUSY;
      }
      break;
   }

   case IOCTL_VMX86_GET_UNAVAIL_PERF_CTRS: {
      uint64 ctrs = Vmx86_GetUnavailablePerfCtrs();
      retval = HostIF_CopyToUser(ioarg, &ctrs, sizeof ctrs);
      break;
   }

   case IOCTL_VMX86_GET_MONITOR_CONTEXT: {
      VMMonContext args;
      retval = HostIF_CopyFromUser(&args, ioarg, sizeof args);
      if (retval != 0) {
         break;
      }
      if (!Vmx86_GetMonitorContext(vm, args.vcpuid, &args.context)) {
         retval = -EINVAL;
      } else {
         retval = HostIF_CopyToUser(ioarg, &args, sizeof args);
      }
      break;
   }

   default:
      Warning("Unknown ioctl %d\n", iocmd);
      retval = -EINVAL;
   }

exit:
   if (needsWriteLock) {
      up_write(&device->vmDriverRWSema);
   } else {
      up_read(&device->vmDriverRWSema);
   }
   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxDriverQueue --
 *
 *      add the device to the global queue
 *
 * Results:
 *
 *      void
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
LinuxDriverQueue(Device *device)  // IN/OUT:
{
   /*
    * insert in global vm queue
    */

   HostIF_GlobalLock(12);

   device->next = linuxState.head;
   linuxState.head = device;

   HostIF_GlobalUnlock(12);
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxDriveDequeue --
 *
 *      remove from active list
 *
 * Results:
 *
 *      void
 * Side effects:
 *      printk if it is not in the list (error condition)
 *
 *----------------------------------------------------------------------
 */

static void
LinuxDriverDequeue(Device *device)  // IN/OUT:
{
   Device **p;

   HostIF_GlobalLock(13);
   for (p = &linuxState.head; *p != device; p = &(*p)->next) {
      ASSERT(*p != NULL);
   }
   *p = device->next;
   device->next = NULL;
   HostIF_GlobalUnlock(13);
}


/*
 *----------------------------------------------------------------------
 *
 * CheckPadding --
 *
 *      check for expected padding --
 *      this check currently fails on the egcs compiler
 *
 * Results:
 *
 *      TRUE if the check succeeds -- module will be loaded
 *
 *
 *
 * Side effects:
 *      output to kernel log on error
 *
 *----------------------------------------------------------------------
 */

static Bool
LinuxDriverCheckPadding(void)
{
   DTRWords32 dtr;
   uint16 *x;

   memset(&dtr, 0, sizeof dtr);
   dtr.dtr.limit = 0x1111;
   dtr.dtr.offset = 0x22223333;

   x = (uint16 *) &dtr;

   if (x[0] == 0x1111 && x[1] == 0x3333 && x[2] == 0x2222) {
   } else {
      Warning("DTR padding\n");
      goto error;
   }

   return TRUE;

error:
   printk("/dev/vmmon: Cannot load module. Use standard gcc compiler\n");

   return FALSE;
}


MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Virtual Machine Monitor.");
MODULE_LICENSE("GPL v2");
/*
 * Starting with SLE10sp2, Novell requires that IHVs sign a support agreement
 * with them and mark their kernel modules as externally supported via a
 * change to the module header. If this isn't done, the module will not load
 * by default (i.e., neither mkinitrd nor modprobe will accept it).
 */
MODULE_INFO(supported, "external");
