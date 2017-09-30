/*********************************************************
 * Copyright (C) 2004-2015 VMware, Inc. All rights reserved.
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

#ifndef _X86VTINSTR_H_
#define _X86VTINSTR_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMKDRIVERS
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "x86_basic_defs.h"
#include "community_source.h"

#if defined __cplusplus
extern "C" {
#endif


#define INVALID_VMCS_ADDR    ~0ULL

/*
 * All VMX operations set an exit status in EFLAGS_CF and EFLAGS_ZF.
 * If both flags are clear, the operation was successful.  If CF is set,
 * the operation failed and there was no valid current VMCS.  If ZF
 * is set, the operation failed and an error code was written to the
 * VM-instruction error field of the current VMCS.  Note that the other
 * four ALU flags are always cleared.
 *
 * Because of the VMX_FailValid behavior, we declare that "memory" is
 * modified by all of these operations.  This may be overly paranoid,
 * since the VM-instruction error field of the current VMCS should always
 * be accessed by a VMREAD, and never by a direct memory access.
 */

typedef enum {
   VMX_Success     = EFLAGS_SET,
   VMX_FailInvalid = EFLAGS_SET | EFLAGS_CF,
   VMX_FailValid   = EFLAGS_SET | EFLAGS_ZF
} VMXStatus;

static INLINE Bool
VMXStatus_Valid(VMXStatus status)
{
   return status == VMX_Success     ||
          status == VMX_FailInvalid ||
          status == VMX_FailValid;
}

#if defined __GNUC__

static INLINE VMXStatus
VMXON_2_STATUS(MA* vmxonRegion)
{
   VMXStatus status;
   __asm__ __volatile__("vmxon %1; lahf; movzbl %%ah, %0"
                        : "=a"(status)
                        : "m"(*vmxonRegion)
                        : "cc", "memory");
   ASSERT(VMXStatus_Valid(status));
   return status;
}

static INLINE void
VMXON_UNCHECKED(MA *vmxonRegion)
{
   __asm__ __volatile__("vmxon %0"
                        :
                        : "m"(*vmxonRegion)
                        : "cc", "memory");
}

static INLINE VMXStatus
VMXOFF_2_STATUS(void)
{
   VMXStatus status;
   __asm__ __volatile__("vmxoff; lahf; movzbl %%ah, %0"
                        : "=a"(status)
                        :
                        : "cc", "memory");
   ASSERT(VMXStatus_Valid(status));
   return status;
}

static INLINE void
VMXOFF_UNCHECKED(void)
{
   __asm__ __volatile__("vmxoff"
                        :
                        :
                        : "cc", "memory");
}

static INLINE VMXStatus
VMCLEAR_2_STATUS(MA* vmcs)
{
   VMXStatus status;
   __asm__ __volatile__("vmclear %1; lahf; movzbl %%ah, %0"
                        : "=a"(status)
                        : "m"(*vmcs)
                        : "cc", "memory");
   ASSERT(VMXStatus_Valid(status));
   return status;
}

static INLINE void
VMCLEAR_UNCHECKED(MA* vmcs)
{
   __asm__ __volatile__("vmclear %0"
                        :
                        : "m"(*vmcs)
                        : "cc", "memory");
}

static INLINE VMXStatus
VMPTRLD_2_STATUS(MA *vmcs)
{
   VMXStatus status;
   __asm__ __volatile__("vmptrld %1; lahf; movzbl %%ah, %0"
                        : "=a"(status)
                        : "m"(*vmcs)
                        : "cc", "memory");
   ASSERT(VMXStatus_Valid(status));
   return status;
}

static INLINE void
VMPTRLD_UNCHECKED(MA *vmcs)
{
   __asm__ __volatile__("vmptrld %0"
                        :
                        : "m"(*vmcs)
                        : "cc", "memory");
}

static INLINE VMXStatus
VMPTRST_2_STATUS(MA *vmcs)
{
   VMXStatus status;
   __asm__ __volatile__("vmptrst %1; lahf; movzbl %%ah, %0"
                        : "=a"(status)
                        : "m"(*vmcs)
                        : "cc", "memory");
   ASSERT(VMXStatus_Valid(status));
   return status;
}

static INLINE void
VMPTRST_UNCHECKED(MA *vmcs)
{
   __asm__ __volatile__("vmptrst %0"
                        :
                        : "m"(*vmcs)
                        : "cc", "memory");
}

static INLINE VMXStatus
VMREAD_2_STATUS(size_t encoding, size_t *retval)
{
   VMXStatus status;
   size_t value;
   __asm__ __volatile__("vmread %2, %1; lahf; movzbl %%ah, %0"
                        : "=a"(status), "=rm"(value)
                        : "r"(encoding)
                        : "cc", "memory");
   ASSERT(VMXStatus_Valid(status));
   *retval = value;
   return status;
}

static INLINE size_t
VMREAD_UNCHECKED(size_t encoding)
{
   size_t retval;
   __asm__ __volatile__("vmread %1, %0"
                        : "=rm"(retval)
                        : "r"(encoding)
                        : "cc", "memory");
   return retval;
}

static INLINE VMXStatus
VMWRITE_2_STATUS(size_t encoding, size_t val)
{
   VMXStatus status;
   __asm__ __volatile__("vmwrite %1, %2; lahf; movzbl %%ah, %0"
                        : "=a"(status)
                        : "rm"(val), "r"(encoding)
                        : "cc", "memory");
   ASSERT(VMXStatus_Valid(status));
   return status;
}

static INLINE void
VMWRITE_UNCHECKED(size_t encoding, size_t val)
{
   __asm__ __volatile__("vmwrite %0, %1"
                        :
                        : "rm"(val), "r"(encoding)
                        : "cc", "memory");
}

static INLINE void
VMWRITE(size_t encoding, size_t val)
{
   if (vmx86_debug) {
      VMXStatus status;
      status = VMWRITE_2_STATUS(encoding, val);
      ASSERT(status == VMX_Success);
   } else {
      VMWRITE_UNCHECKED(encoding, val);
   }
}

static INLINE VMXStatus
VMLAUNCH_2_STATUS(void)
{
   VMXStatus status;
   __asm__ __volatile__("vmlaunch; lahf; movzbl %%ah, %0"
                        : "=a"(status)
                        :
                        : "cc", "memory");
   ASSERT(VMXStatus_Valid(status));
   return status;
}

static INLINE void
VMLAUNCH_UNCHECKED(void)
{
   __asm__ __volatile__("vmlaunch"
                        :
                        :
                        : "cc", "memory");
}

static INLINE void
VMLAUNCH(void)
{
   if (vmx86_debug) {
      VMXStatus status;
      status = VMLAUNCH_2_STATUS();
      ASSERT(status == VMX_Success);
   } else {
      VMLAUNCH_UNCHECKED();
   }
}

static INLINE VMXStatus
VMRESUME_2_STATUS(void)
{
   VMXStatus status;
   __asm__ __volatile__("vmresume; lahf; movzbl %%ah, %0"
                        : "=a"(status)
                        :
                        : "cc", "memory");
   ASSERT(VMXStatus_Valid(status));
   return status;
}

static INLINE void
VMRESUME_UNCHECKED(void)
{
   __asm__ __volatile__("vmresume"
                        :
                        :
                        : "cc", "memory");
}

static INLINE void
VMRESUME(void)
{
   if (vmx86_debug) {
      VMXStatus status;
      status = VMRESUME_2_STATUS();
      ASSERT(status == VMX_Success);
   } else {
      VMRESUME_UNCHECKED();
   }
}

static INLINE VMXStatus
VMCALL_2_STATUS(void)
{
   VMXStatus status;
   __asm__ __volatile__("vmcall; lahf; movzbl %%ah, %0"
                        : "=a"(status)
                        :
                        : "cc", "memory");
   ASSERT(VMXStatus_Valid(status));
   return status;
}

static INLINE void
VMCALL_UNCHECKED(void)
{
   __asm__ __volatile__("vmcall"
                        :
                        :
                        : "cc", "memory");
}

static INLINE void
VMCALL(void)
{
   if (vmx86_debug) {
      VMXStatus status;
      status = VMCALL_2_STATUS();
      ASSERT(status == VMX_Success);
   } else {
      VMCALL_UNCHECKED();
   }
}

#define INVVPID_EXTENT_ADDR               0
#define INVVPID_EXTENT_VPID_CTX           1
#define INVVPID_EXTENT_ALL_CTX            2
#define INVVPID_EXTENT_VPID_CTX_LOCAL     3

typedef struct {
   uint64 vpid;
   uint64 la;
} InvvpidArg;

static INLINE VMXStatus
INVVPID_2_STATUS(InvvpidArg *v, size_t extent)
{
   VMXStatus status;
   __asm__ __volatile__(
                        "invvpid %1, %2;"
                        "lahf; movzbl %%ah, %0"
                        : "=a"(status)
                        : "m"(*v), "r"(extent)
                        : "cc", "memory");
   ASSERT(VMXStatus_Valid(status));
   return status;
}

static INLINE void
INVVPID_UNCHECKED(InvvpidArg *v, size_t extent)
{
   __asm__ __volatile__(
                        "invvpid %0, %1"
                        :
                        : "m"(*v), "r"(extent)
                        : "cc", "memory");
}

static INLINE void
INVVPID(InvvpidArg *v, size_t extent)
{
   if (vmx86_debug) {
      VMXStatus status;
      status = INVVPID_2_STATUS(v, extent);
      ASSERT(status == VMX_Success);
   } else {
      INVVPID_UNCHECKED(v, extent);
   }
}

static INLINE void
INVVPID_ADDR(uint16 vpid, LA lAddr)
{
   InvvpidArg v;
   v.vpid = vpid;
   v.la = lAddr;
   INVVPID(&v, INVVPID_EXTENT_ADDR);
}

static INLINE void
INVVPID_CTX(uint16 vpid, Bool global)
{
   InvvpidArg v;
   v.vpid = vpid;
   INVVPID(&v, global ? INVVPID_EXTENT_VPID_CTX
                      : INVVPID_EXTENT_VPID_CTX_LOCAL);
}

static INLINE void
INVVPID_ALL(void)
{
   InvvpidArg v;
   v.vpid = 0; // Bits 16-63 of the structure must be zero
   INVVPID(&v, INVVPID_EXTENT_ALL_CTX);
}


#define INVEPT_EXTENT_EPT_CTX       1
#define INVEPT_EXTENT_GLOBAL        2

typedef struct {
   uint64 eptp;
   uint64 rsvd;
} InveptArg;

static INLINE VMXStatus
INVEPT_2_STATUS(InveptArg *e, size_t extent)
{
   VMXStatus status;
   __asm__ __volatile__(
                        "invept %1, %2;"
                        "lahf;"
                        "movzbl %%ah, %0"
                        : "=a"(status)
                        : "m"(*e), "r"(extent)
                        : "cc", "memory");
   ASSERT(VMXStatus_Valid(status));
   return status;
}

static INLINE void
INVEPT_UNCHECKED(InveptArg *e, size_t extent)
{
   __asm__ __volatile__(
                        "invept %0, %1"
                        :
                        : "m" (*e), "r" (extent)
                        : "cc", "memory");
}

static INLINE void
INVEPT(InveptArg *e, size_t extent)
{
   if (vmx86_debug) {
      VMXStatus status;
      status = INVEPT_2_STATUS(e, extent);
      ASSERT(status == VMX_Success);
   } else {
      INVEPT_UNCHECKED(e, extent);
   }
}


static INLINE void
INVEPT_CTX(uint64 eptp)
{
   InveptArg e;
   e.eptp = eptp;
   e.rsvd = 0;
   INVEPT(&e, INVEPT_EXTENT_EPT_CTX);
}

static INLINE void
INVEPT_GLOBAL(void)
{
   InveptArg e;
   e.rsvd = 0;
   INVEPT(&e, INVEPT_EXTENT_GLOBAL);
}

static INLINE void
VMFUNC(unsigned num)
{
   __asm__ __volatile__ (".byte 0x0f, 0x01, 0xd4" : : "a" (num));
}

#elif defined _MSC_VER

#if !defined VM_X86_64
unsigned char __vmx_on(unsigned __int64 *);
void          __vmx_off(void);
int           __vmx_vmptrld(unsigned __int64 *);
void          __vmx_vmptrst(unsigned __int64 *);
#endif


static INLINE VMXStatus
VMXON_2_STATUS(MA *vmxonRegion)
{
   unsigned char mscStatus;
   static const VMXStatus MscToStatus[] =
      {VMX_Success, VMX_FailValid, VMX_FailInvalid};

   mscStatus = __vmx_on((unsigned __int64 *)vmxonRegion);
   ASSERT(mscStatus < ARRAYSIZE(MscToStatus));
   return MscToStatus[mscStatus];
}

static INLINE void
VMXON_UNCHECKED(MA *vmxonRegion)
{
   (void)__vmx_on((unsigned __int64 *)vmxonRegion);
}

static INLINE VMXStatus
VMXOFF_2_STATUS(void)
{
   (void)__vmx_off();
   return VMX_Success;
}

static INLINE void
VMXOFF_UNCHECKED(void)
{
   (void)__vmx_off();
}

static INLINE void
VMCLEAR_UNCHECKED(MA *vmcs)
{
   (void)__vmx_vmclear((unsigned __int64 *)vmcs);
}

static INLINE size_t
VMREAD_UNCHECKED(size_t encoding)
{
   size_t retval;
   (void)__vmx_vmread(encoding, &retval);
   return retval;
}

static INLINE VMXStatus
VMCLEAR_2_STATUS(MA *vmcs)
{
   unsigned char mscStatus;
   static const VMXStatus MscToStatus[] =
      {VMX_Success, VMX_FailValid, VMX_FailInvalid};

   mscStatus = __vmx_vmclear((unsigned __int64 *)vmcs);
   ASSERT(mscStatus < ARRAYSIZE(MscToStatus));
   return MscToStatus[mscStatus];
}
 
static INLINE VMXStatus
VMREAD_2_STATUS(size_t encoding, size_t *retval)
{
   unsigned char mscStatus;
   static const VMXStatus MscToStatus[] =
      {VMX_Success, VMX_FailValid, VMX_FailInvalid};
 
   mscStatus = __vmx_vmread(encoding, retval);
   ASSERT(mscStatus < ARRAYSIZE(MscToStatus));
   return MscToStatus[mscStatus];
}


static INLINE VMXStatus
VMPTRLD_2_STATUS(MA *vmcs)
{
   unsigned char mscStatus;
   static const VMXStatus MscToStatus[] =
      {VMX_Success, VMX_FailValid, VMX_FailInvalid};

   mscStatus = __vmx_vmptrld((unsigned __int64 *)vmcs);
   ASSERT(mscStatus < ARRAYSIZE(MscToStatus));
   return MscToStatus[mscStatus];
}

static INLINE void
VMPTRLD_UNCHECKED(MA *vmcs)
{
   (void)__vmx_vmptrld((unsigned __int64 *)vmcs);
}

static INLINE VMXStatus
VMPTRST_2_STATUS(MA *vmcs)
{
   (void)__vmx_vmptrst((unsigned __int64 *)vmcs);
   return VMX_Success;
}

static INLINE void
VMPTRST_UNCHECKED(MA *vmcs)
{
   (void)__vmx_vmptrst((unsigned __int64 *)vmcs);
}


#endif


static INLINE void
VMXON(MA *vmxonRegion)
{
   if (vmx86_debug) {
      VMXStatus status;
      status = VMXON_2_STATUS(vmxonRegion);
      ASSERT(status == VMX_Success);
   } else {
      VMXON_UNCHECKED(vmxonRegion); 
   }
}

static INLINE void
VMXOFF(void)
{
   if (vmx86_debug) {
      VMXStatus status;
      status = VMXOFF_2_STATUS();
      ASSERT(status == VMX_Success);
   } else {
      VMXOFF_UNCHECKED();
   }
}

static INLINE void
VMPTRLD(MA *vmcs)
{
   if (vmx86_debug) {
      VMXStatus status;
      status = VMPTRLD_2_STATUS(vmcs);
      ASSERT(status == VMX_Success);
   } else {
      VMPTRLD_UNCHECKED(vmcs);
   }
}

static INLINE void
VMPTRST(MA *vmcs)
{
   if (vmx86_debug) {
      VMXStatus status;
      status = VMPTRST_2_STATUS(vmcs);
      ASSERT(status == VMX_Success);
   } else {
      VMPTRST_UNCHECKED(vmcs);
   }
}


static INLINE void
VMCLEAR(MA* vmcs)
{
   if (vmx86_debug) {
      VMXStatus status;
      status = VMCLEAR_2_STATUS(vmcs);
      ASSERT(status == VMX_Success);
   } else {
      VMCLEAR_UNCHECKED(vmcs);
   }
}

static INLINE size_t
VMREAD(size_t encoding)
{
   size_t retval;
   if (vmx86_debug) {
      VMXStatus status;
      status = VMREAD_2_STATUS(encoding, &retval);
      ASSERT(status == VMX_Success);
   } else {
      retval = VMREAD_UNCHECKED(encoding);
   }
   return retval;
}


#if defined __cplusplus
}  // extern "C"
#endif

#endif /* _X86VTINSTR_H_ */
