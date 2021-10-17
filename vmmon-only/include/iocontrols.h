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
 * iocontrols.h
 *
 *        The driver io controls.
 */

#ifndef _IOCONTROLS_H_
#define _IOCONTROLS_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#ifdef VMX86_SERVER
#error iocontrols.h is for hosted vmmon, do not use on visor
#endif

#include "x86segdescrs.h"
#include "rateconv.h"
#include "overheadmem_types.h"
#include "pageLock_defs.h"
#include "contextinfo.h"
#include "vcpuid.h"
#include "sharedAreaType.h"

#if defined __cplusplus
extern "C" {
#endif


/*
 * Maximum number of shared regions that can be passed to MonLoader.
 */
#define ML_SHARED_REGIONS_MAX 3

/*
 *-----------------------------------------------------------------------------
 *
 * VA64ToPtr --
 *
 *      Convert a VA64 to a pointer.
 *
 *      Usage of this function is strictly limited to these 2 cases:
 *
 *      1) In a VMX function which does an ioctl to vmmon, and receives a VMX
 *         pointer as a result.
 *
 *      2) In the vmmon code, for the functions which have a VA64 and need
 *         to call kernel APIs which take pointers.
 *
 * Results:
 *      Virtual address.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void *
VA64ToPtr(VA64 va64) // IN
{
#ifdef VM_64BIT
   ASSERT_ON_COMPILE(sizeof (void *) == 8);
#else
   ASSERT_ON_COMPILE(sizeof (void *) == 4);
   // Check that nothing of value will be lost.
   ASSERT(!(va64 >> 32));
#endif
   return (void *)(uintptr_t)va64;
}


/*
 *-----------------------------------------------------------------------------
 *
 * PtrToVA64 --
 *
 *      Convert a pointer to a VA64.
 *
 *      Usage of this function is strictly limited to these 2 cases:
 *
 *      1) In a VMX function which does an ioctl to vmmon, and passes in a VMX
 *         pointer.
 *
 *      2) In the vmmon code, for the functions which need to pass in a kernel
 *         pointer to functions which can take either a user or a kernel
 *         pointer in the same parameter.
 *
 * Results:
 *      Virtual address.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE VA64
PtrToVA64(void const *ptr) // IN
{
   ASSERT_ON_COMPILE(sizeof ptr <= sizeof (VA64));
   return (VA64)(uintptr_t)ptr;
}


/*
 * Driver version.
 *
 * Increment major version when you make an incompatible change.
 * Compatibility goes both ways (old driver with new executable
 * as well as new driver with old executable).
 *
 * Note: Vmcore compatibility is different from driver versioning.
 * For vmcore puposes, the bora tree is conceptually split in two:
 * vmcore, and rest-of-bora. The vmmon driver is largely outside
 * vmcore and vmcore imports functionality from vmmon. Addition,
 * deletion or modification of an iocontrol used only by rest-of-bora
 * does not break vmcore compatibility.
 *
 * See bora/doc/vmcore details.
 *
 * If you increment VMMON_VERSION, you must also increment
 * the NT specific VMX86_DRIVER_VERSION.
 */

#define VMMON_VERSION           (410 << 16 | 0)
#define VMMON_VERSION_MAJOR(v)  ((uint32) (v) >> 16)
#define VMMON_VERSION_MINOR(v)  ((uint16) (v))


/*
 * ENOMEM returned after MAX_VMS virtual machines created
 */

#define MAX_VMS 64

/*
 * MsgWaitForMultipleObjects doesn't scale well enough on Win32.
 * Allocate with MAX_VMS so static buffers are large, but do
 * admissions control with this value on Win32 until we check
 * scalability (probably in authd).
 */
#ifdef _WIN32
#define MAX_VMS_WIN32 64
#endif


#if !__linux__
/*
 * On platforms other than Linux, IOCTLCMD_foo values are just numbers, and
 * we build the IOCTL_VMX86_foo values around these using platform-specific
 * format for encoding arguments and sizes.
 */
#  define IOCTLCMD(_cmd) IOCTLCMD_ ## _cmd
#else // if __linux__
/*
 * Linux defines _IO* macros, but the core kernel code ignore the encoded
 * ioctl value. It is up to individual drivers to decode the value (for
 * example to look at the size of a structure to determine which version
 * of a specific command should be used) or not (which is what we
 * currently do, so right now the ioctl value for a given command is the
 * command itself).
 *
 * Hence, we just define the IOCTL_VMX86_foo values directly, with no
 * intermediate IOCTLCMD_ representation.
 */
#  define IOCTLCMD(_cmd) IOCTL_VMX86_ ## _cmd
#endif


enum IOCTLCmd {
   /*
    * We need to bracket the range of values used for ioctls, because x86_64
    * Linux forces us to explicitly register ioctl handlers by value for
    * handling 32 bit ioctl syscalls.  Hence FIRST and LAST.  FIRST must be
    * 2001 so that VERSION is 2001 for backwards compatibility.
    */
#if defined __linux__ || defined _WIN32
   /* Start at 2001 because legacy code did. */
   IOCTLCMD(FIRST) = 2001,
#else
   /* Start at 0. */
   IOCTLCMD(FIRST),
#endif
   IOCTLCMD(VERSION) = IOCTLCMD(FIRST),
   IOCTLCMD(CREATE_VM),
   IOCTLCMD(PROCESS_BOOTSTRAP),
   IOCTLCMD(REGISTER_SHARED),
   IOCTLCMD(REGISTER_STATVARS),
   IOCTLCMD(RELEASE_VM),
   IOCTLCMD(GET_NUM_VMS),
   IOCTLCMD(RUN_VM),
   IOCTLCMD(LOOK_UP_MPN),
   IOCTLCMD(GET_VMM_PAGE_ROOT),
   IOCTLCMD(LOCK_PAGE),
   IOCTLCMD(UNLOCK_PAGE),
   IOCTLCMD(APIC_INIT),
   IOCTLCMD(SET_HARD_LIMIT),
   IOCTLCMD(GET_MEM_INFO),
   IOCTLCMD(ADMIT),
   IOCTLCMD(UPDATE_MEM_INFO),
   IOCTLCMD(READMIT),
   IOCTLCMD(GET_KHZ_ESTIMATE),
   IOCTLCMD(SET_HOST_CLOCK_RATE),
   IOCTLCMD(READ_PAGE),
   IOCTLCMD(WRITE_PAGE),
   IOCTLCMD(LOCK_PAGE_NEW),
   IOCTLCMD(UNLOCK_PAGE_BY_MPN),
    /* AWE calls */
   IOCTLCMD(ALLOC_LOCKED_PAGES),
   IOCTLCMD(GET_NEXT_ANON_PAGE),
   IOCTLCMD(GET_NUM_ANON_PAGES),

   IOCTLCMD(GET_ALL_MSRS),

   IOCTLCMD(GET_REFERENCE_CLOCK_HZ),
   IOCTLCMD(INIT_PSEUDO_TSC),
   IOCTLCMD(CHECK_PSEUDO_TSC),
   IOCTLCMD(GET_PSEUDO_TSC),

   IOCTLCMD(SYNC_GET_TSCS),

   IOCTLCMD(GET_IPI_VECTORS),
   IOCTLCMD(SEND_IPI),
   IOCTLCMD(SEND_ONE_IPI),
   IOCTLCMD(GET_SWITCH_ERROR_ADDR),

   /*
    * Keep host-specific calls at the end so they can be undefined
    * without renumbering the common calls.
    */

#if defined __linux__
   IOCTLCMD(SET_UID),		// VMX86_DEVEL only
   IOCTLCMD(GET_ALL_CPUID),
#endif

#if defined _WIN32
   IOCTLCMD(READ_DISASM_PROC_BINARY),
   IOCTLCMD(CHECK_CANDIDATE_VA64),
   IOCTLCMD(SET_MEMORY_PARAMS),
   IOCTLCMD(REMEMBER_KHZ_ESTIMATE),
   IOCTLCMD(REMAP_SCATTER_LIST),
   IOCTLCMD(REMAP_SCATTER_LIST_RO),     // map the list as read-only
   IOCTLCMD(UNMAP_SCATTER_LIST),
#endif

#if defined __APPLE__
   IOCTLCMD(GET_NUM_RESPONDING_CPUS),
   IOCTLCMD(INIT_DRIVER),
   IOCTLCMD(BLUEPILL),
#endif

   IOCTLCMD(GET_UNAVAIL_PERF_CTRS),
   IOCTLCMD(GET_MONITOR_CONTEXT),
   // Must be last.
   IOCTLCMD(LAST)
};


#if defined _WIN32
/*
 * Windows ioctl definitions.
 *
 * We use the IRP Information field for the return value
 * of IOCTLCMD_RUN_VM, to be faster since it is used a lot.
 */

#define FILE_DEVICE_VMX86        0x8101
#define VMX86_IOCTL_BASE_INDEX   0x801
#define VMIOCTL_BUFFERED(name) \
     CTL_CODE(FILE_DEVICE_VMX86, \
	       VMX86_IOCTL_BASE_INDEX + IOCTLCMD_ ## name, \
	       METHOD_BUFFERED, \
	       FILE_ANY_ACCESS)
#define VMIOCTL_NEITHER(name) \
      CTL_CODE(FILE_DEVICE_VMX86, \
	       VMX86_IOCTL_BASE_INDEX + IOCTLCMD_ ## name, \
	       METHOD_NEITHER, \
	       FILE_ANY_ACCESS)

#define IOCTL_VMX86_VERSION             VMIOCTL_BUFFERED(VERSION)
#define IOCTL_VMX86_CREATE_VM           VMIOCTL_BUFFERED(CREATE_VM)
#define IOCTL_VMX86_RELEASE_VM          VMIOCTL_BUFFERED(RELEASE_VM)
#define IOCTL_VMX86_PROCESS_BOOTSTRAP   VMIOCTL_BUFFERED(PROCESS_BOOTSTRAP)
#define IOCTL_VMX86_REGISTER_SHARED     VMIOCTL_BUFFERED(REGISTER_SHARED)
#define IOCTL_VMX86_REGISTER_STATVARS   VMIOCTL_BUFFERED(REGISTER_STATVARS)
#define IOCTL_VMX86_GET_NUM_VMS         VMIOCTL_BUFFERED(GET_NUM_VMS)
#define IOCTL_VMX86_RUN_VM              VMIOCTL_NEITHER(RUN_VM)
#define IOCTL_VMX86_SEND_IPI            VMIOCTL_NEITHER(SEND_IPI)
#define IOCTL_VMX86_SEND_ONE_IPI        VMIOCTL_BUFFERED(SEND_ONE_IPI)
#define IOCTL_VMX86_GET_IPI_VECTORS     VMIOCTL_BUFFERED(GET_IPI_VECTORS)
#define IOCTL_VMX86_GET_SWITCH_ERROR_ADDR VMIOCTL_BUFFERED(GET_SWITCH_ERROR_ADDR)
#define IOCTL_VMX86_LOOK_UP_MPN         VMIOCTL_BUFFERED(LOOK_UP_MPN)
#define IOCTL_VMX86_GET_VMM_PAGE_ROOT   VMIOCTL_BUFFERED(GET_VMM_PAGE_ROOT)
#define IOCTL_VMX86_LOCK_PAGE           VMIOCTL_BUFFERED(LOCK_PAGE)
#define IOCTL_VMX86_UNLOCK_PAGE         VMIOCTL_BUFFERED(UNLOCK_PAGE)
#define IOCTL_VMX86_APIC_INIT           VMIOCTL_BUFFERED(APIC_INIT)
#define IOCTL_VMX86_SET_HARD_LIMIT      VMIOCTL_BUFFERED(SET_HARD_LIMIT)
#define IOCTL_VMX86_GET_MEM_INFO        VMIOCTL_BUFFERED(GET_MEM_INFO)
#define IOCTL_VMX86_ADMIT               VMIOCTL_BUFFERED(ADMIT)
#define IOCTL_VMX86_READMIT             VMIOCTL_BUFFERED(READMIT)
#define IOCTL_VMX86_UPDATE_MEM_INFO     VMIOCTL_BUFFERED(UPDATE_MEM_INFO)

#define IOCTL_VMX86_GET_TOTAL_MEM_USAGE	VMIOCTL_BUFFERED(GET_TOTAL_MEM_USAGE)
#define IOCTL_VMX86_GET_KHZ_ESTIMATE    VMIOCTL_BUFFERED(GET_KHZ_ESTIMATE)
#define IOCTL_VMX86_SET_HOST_CLOCK_RATE VMIOCTL_BUFFERED(SET_HOST_CLOCK_RATE)
#define IOCTL_VMX86_SYNC_GET_TSCS       VMIOCTL_BUFFERED(SYNC_GET_TSCS)
#define IOCTL_VMX86_READ_PAGE           VMIOCTL_BUFFERED(READ_PAGE)
#define IOCTL_VMX86_WRITE_PAGE          VMIOCTL_BUFFERED(WRITE_PAGE)
#define IOCTL_VMX86_LOCK_PAGE_NEW       VMIOCTL_BUFFERED(LOCK_PAGE_NEW)
#define IOCTL_VMX86_UNLOCK_PAGE_BY_MPN  VMIOCTL_BUFFERED(UNLOCK_PAGE_BY_MPN)
#define IOCTL_VMX86_ALLOC_LOCKED_PAGES  VMIOCTL_BUFFERED(ALLOC_LOCKED_PAGES)
#define IOCTL_VMX86_GET_NEXT_ANON_PAGE  VMIOCTL_BUFFERED(GET_NEXT_ANON_PAGE)
#define IOCTL_VMX86_GET_NUM_ANON_PAGES  VMIOCTL_BUFFERED(GET_NUM_ANON_PAGES)

#define IOCTL_VMX86_READ_DISASM_PROC_BINARY \
                                      VMIOCTL_BUFFERED(READ_DISASM_PROC_BINARY)
#define IOCTL_VMX86_CHECK_CANDIDATE_VA64 VMIOCTL_BUFFERED(CHECK_CANDIDATE_VA64)

#define IOCTL_VMX86_SET_MEMORY_PARAMS   VMIOCTL_BUFFERED(SET_MEMORY_PARAMS)

#define IOCTL_VMX86_REMEMBER_KHZ_ESTIMATE VMIOCTL_BUFFERED(REMEMBER_KHZ_ESTIMATE)

#define IOCTL_VMX86_GET_ALL_MSRS        VMIOCTL_BUFFERED(GET_ALL_MSRS)

#define IOCTL_VMX86_GET_REFERENCE_CLOCK_HZ   VMIOCTL_BUFFERED(GET_REFERENCE_CLOCK_HZ)
#define IOCTL_VMX86_INIT_PSEUDO_TSC          VMIOCTL_BUFFERED(INIT_PSEUDO_TSC)
#define IOCTL_VMX86_CHECK_PSEUDO_TSC         VMIOCTL_BUFFERED(CHECK_PSEUDO_TSC)
#define IOCTL_VMX86_GET_PSEUDO_TSC           VMIOCTL_NEITHER(GET_PSEUDO_TSC)
#define IOCTL_VMX86_GET_UNAVAIL_PERF_CTRS    VMIOCTL_NEITHER(GET_UNAVAIL_PERF_CTRS)
#define IOCTL_VMX86_GET_MONITOR_CONTEXT      VMIOCTL_BUFFERED(GET_MONITOR_CONTEXT)
#define IOCTL_VMX86_REMAP_SCATTER_LIST    VMIOCTL_BUFFERED(REMAP_SCATTER_LIST)
#define IOCTL_VMX86_REMAP_SCATTER_LIST_RO VMIOCTL_BUFFERED(REMAP_SCATTER_LIST_RO)
#define IOCTL_VMX86_UNMAP_SCATTER_LIST    VMIOCTL_BUFFERED(UNMAP_SCATTER_LIST)
#endif


#define INIT_BLOCK_MAGIC     (0x1789 + 14)


#pragma pack(push, 1)
typedef struct VMLockPageRet {
   MPN   mpn;      // OUT: MPN
   int32 status;   // OUT: PAGE_* status code
} VMLockPageRet;
#pragma pack(pop)

#pragma pack(push, 1)
typedef union {
   VA64 uAddr;        // IN: user address
   VMLockPageRet ret; // OUT: status code and MPN
} VMLockPage;
#pragma pack(pop)

#pragma pack(push, 1)
typedef union {
   Vcpuid vcpuid; // IN: VCPU
   MPN pageRoot;  // OUT: MPN of the VCPU's page root
} VcpuPageRoot;
#pragma pack(pop)

#define VMX86_DRIVER_VCPUID_OFFSET 1000


/*
 * We keep track of 3 different limits on the number of pages we can lock.
 * The host limit is determined at driver load time (in windows only) to
 * make sure we do not starve the host by locking too many pages.
 * The static limit is user defined in the UI and the dynamic limit is
 * set by authd's hardLimitMonitor code (windows only), which queries
 * host load and adjusts the limit accordingly.  We lock the minimum of
 * all these values at any given time.
 */
typedef struct LockedPageLimit {
   PageCnt host;        // driver calculated maximum for this host
   PageCnt configured;  // user defined maximum pages to lock
} LockedPageLimit;

/*
 * Sentinel VA for IOCTL_VMX86_SET_MEMORY_PARAMS, indicates
 * NtQuerySystemInformation should be used to determine the host
 * LockedPageLimit.
 */
#define MEMORY_PARAM_USE_SYSINFO_FOR_LOCKED_PAGE_LIMIT   ((VA64)(int64)-1)

/*
 * Data structures for the GET_MEM_INFO and ADMIT ioctls.
 *
 * Be careful adding structs and fields to VMMemInfoArgs and its
 * substructures. These are compiled into both the 32-bit and 64-bit
 * vmmon drivers and the 32-bit and 64-bit vmx's and need to have
 * the same size and layout in all four combinations. Note the
 * use of padding below to ensure that this happens.
 */

typedef struct VMMemMgmtInfo {
   PageCnt         minAllocation;   // minimum pages for vm
   PageCnt         maxAllocation;   // maximum pages the vm could lock
   PageCnt         nonpaged;        // overhead memory (guest, mmap)
   PageCnt         paged;           // vmx memory (malloc, statics)
   PageCnt         anonymous;       // vmm memory
   PageCnt         mainMemSize;     // guest main memory size
   PageCnt         locked;          // number of pages locked by this vm
   PageCnt         perVMOverhead;   // memory for vmx/vmmon overheads
   uint32          shares;          // proportional sharing weight
   Percent         touchedPct;      // % of guest memory being touched
   Percent         dirtiedPct;      // % of guest memory being dirtied
   Bool            admitted;        // admission control
   uint8           _pad;            // for alignment of 64-bit fields
   uint64          hugePageBytes;   // number of bytes occupied by huge pages
   uint64          timestamp;       // most recent poll of get mem info time
} VMMemMgmtInfo;

typedef struct VMMemMgmtInfoPatch {
   Percent         touchedPct;      // % of guest memory being touched
   Percent         dirtiedPct;      // % of guest memory being dirtied
   uint8           _pad[6];
   uint64          hugePageBytes;
} VMMemMgmtInfoPatch;

/*
 * See comment on padding and size/layout constraints above when
 * when modifying VMMemInfoArgs or its components.
 */

typedef struct VMMemInfoArgs {
   uint64          currentTime;        // Host time in secs of the call.
   PageCnt         globalMinAllocation;// pages that must fit in maxLockedPages
   PageCnt         numLockedPages;     // total locked pages by all vms
   LockedPageLimit lockedPageLimit;    // set of locked page limits
   PageCnt         maxLockedPages;     // effective limit on locked pages
   uint32          callerIndex;        // this vm's index memInfo array
   uint32          numVMs;             // number of running VMs
   Percent         minVmMemPct;        // % of vm that must fit in memory
   uint8           _pad[7];
   VMMemMgmtInfo   memInfo[1];
} VMMemInfoArgs;

#define VM_GET_MEM_INFO_SIZE(numVMs) \
   (sizeof(VMMemInfoArgs) - sizeof(VMMemMgmtInfo) + (numVMs) * sizeof(VMMemMgmtInfo))

typedef struct VMMPNNext {
   MPN         inMPN;   // IN
   MPN         outMPN;  // OUT
} VMMPNNext;

typedef struct VMMPNList {
   PageCnt   mpnCount;   // IN (and OUT on Mac OS)
   Bool      ignoreLimits;
   uint8     _pad[7];
   VA64      mpnList;    // IN: User VA of an array of 64-bit MPNs.
} VMMPNList;

typedef struct VMMUnlockPageByMPN {
   MPN       mpn;
   VA64      uAddr;         /* IN: User VA of the page (optional). */
} VMMUnlockPageByMPN;

typedef struct VMMReadWritePage {
   MPN          mpn;   // IN
   VA64         uAddr; // IN: User VA of a PAGE_SIZE-large buffer.
} VMMReadWritePage;

/*
 * Data structure for the INIT_PSEUDO_TSC and CHECK_PSEUDO_TSC.
 */

typedef struct PTSCInitParams {
   RateConv_Params refClockToPTSC;
   uint64          tscHz;
   uint64          initialPTSC;
   int64           tscOffset;
   Bool            forceRefClock;
   Bool            forceTSC;
   Bool            hwTSCsSynced;
   uint8           _pad[5];
} PTSCInitParams;

typedef struct PTSCCheckParams {
   uint64 lastTSC;
   uint64 lastRC;
   Bool   usingRefClock;
   uint8  _pad[7];
} PTSCCheckParams;

typedef struct IPIVectors {
   /*
    * Vectors we have allocated or stolen for the monitor interrupts.
    */
   uint8 monitorIPIVector;
   uint8 hvIPIVector;
} IPIVectors;

/*
 * Arguments and return value for VM creation.
 */
typedef struct VMCreateBlock {
   VA64               bsBlob;        // IN: User VA of the VMM bootstrap blob.
   VA64               vmmonData;     // IN: User VA of a userlevel scratch area
                                     //     required by the Linux vmmon
   uint32             bsBlobSize;    // IN: Size of VMM bootstrap blob.
   uint32             numVCPUs;      // IN: Number of VCPUs.
   uint16             vmid;          // OUT: VM ID for the created VM.
} VMCreateBlock;

/*
 * Information about a shared region.
 */
typedef struct VMSharedRegion {
   SharedAreaType index;
   VPN            baseVpn;
   uint32         numPages;
} VMSharedRegion;

/*
 * Arguments for VMM shared area registration.
 */
typedef struct VMSharedAreaRegistrationBlock {
   Vcpuid vcpu;            // IN: VCPU being registered.
   VMSharedRegion region;  // IN: Shared region being registered.
} VMSharedAreaRegistrationBlock;

/*
 * Information about a VM's statvars.
 */
typedef struct VMStatVarsRegistrationBlock {
   VPN     baseVpn;
   PageCnt numPages;
   Vcpuid  vcpu;
} VMStatVarsRegistrationBlock;

typedef struct {
   VA64   ptRoot;    // IN: User VA of VCPU L4 page table root.
} PerVcpuPages;

/*
 * Arguments for VMM bootstrap processing.
 */
typedef struct VMProcessBootstrapBlock {
   VA64           bsBlobAddr;    // IN: User VA of the VMM bootstrap blob.
   uint32         numBytes;      // IN: Size of VMM bootstrap blob.
   uint32         headerOffset;  // IN: Offset of header in blob.
   uint16         numVCPUs;      // IN: Number of VCPUs.
   VMSharedRegion shRegions[ML_SHARED_REGIONS_MAX]; // IN: Shared regions.
   PerVcpuPages   perVcpuPages[0];
} VMProcessBootstrapBlock;

/*
 * Arguments for retrieving the switch error address.
 */
typedef struct VMSwitchErrorArgs {
   Vcpuid vcpuid;  // IN: The VCPU of interest.
   uint64 addr;    // OUT: The code address that a failure was detected at, or 0
                   //      if no failure has occurred.
} VMSwitchErrorArgs;

static INLINE size_t
GetVMProcessBootstrapBlockSize(unsigned numVCPUs)
{
   VMProcessBootstrapBlock *args = NULL;
   return sizeof *args + numVCPUs * sizeof args->perVcpuPages[0];
}

/*
 * Arguments for VMM context retrieval.
 */
typedef union {
   Vcpuid vcpuid;     // IN
   Context64 context; // OUT
} VMMonContext;

#if defined __APPLE__
#   include "iocontrolsMacos.h"
#endif

/* Clean up helper macros */
#undef IOCTLCMD


#if defined __cplusplus
} // extern "C"
#endif

#endif // ifndef _IOCONTROLS_H_
