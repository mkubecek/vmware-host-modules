/*********************************************************
 * Copyright (C) 1998, 2021 VMware, Inc. All rights reserved.
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

#ifndef _VM_OUI_H_
#define _VM_OUI_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vm_basic_asm.h"

#if defined __cplusplus
extern "C" {
#endif

/*
 * Our own OUIs given by IEEE.
 */

/*
 * This OUI was previously used for generated MAC addresses on ESX.
 * Don't reuse.
 */
#define VMX86_LEGACY_OUI      0x000569

/* This OUI is used for static MAC addresses. */
#define VMX86_STATIC_OUI      0x005056

/* This OUI is used for generated MAC addresses. */
#define VMX86_GENERATED_OUI   0x000C29

/* Entire OUI is reserved and should not be used for any purpose. */
#define VMX86_FUTURE_OUI      0x001C14

#define VMX86_OUI_SIZE	3

#define VMX86_OUI_BYTE0(x) ((uint8) (((x) >> (VMX86_OUI_SIZE - 1) * 8) & 0xFF))
#define VMX86_OUI_BYTE1(x) ((uint8) (((x) >> (VMX86_OUI_SIZE - 2) * 8) & 0xFF))
#define VMX86_OUI_BYTE2(x) ((uint8) (((x) >> (VMX86_OUI_SIZE - 3) * 8) & 0xFF))

#define VMX86_LEGACY_OUI0 VMX86_OUI_BYTE0(VMX86_LEGACY_OUI)
#define VMX86_LEGACY_OUI1 VMX86_OUI_BYTE1(VMX86_LEGACY_OUI)
#define VMX86_LEGACY_OUI2 VMX86_OUI_BYTE2(VMX86_LEGACY_OUI)

#define VMX86_STATIC_OUI0 VMX86_OUI_BYTE0(VMX86_STATIC_OUI)
#define VMX86_STATIC_OUI1 VMX86_OUI_BYTE1(VMX86_STATIC_OUI)
#define VMX86_STATIC_OUI2 VMX86_OUI_BYTE2(VMX86_STATIC_OUI)

#define VMX86_GEN_OUI0    VMX86_OUI_BYTE0(VMX86_GENERATED_OUI)
#define VMX86_GEN_OUI1    VMX86_OUI_BYTE1(VMX86_GENERATED_OUI)
#define VMX86_GEN_OUI2    VMX86_OUI_BYTE2(VMX86_GENERATED_OUI)

#define VMX86_FUTURE_OUI0 VMX86_OUI_BYTE0(VMX86_FUTURE_OUI)
#define VMX86_FUTURE_OUI1 VMX86_OUI_BYTE1(VMX86_FUTURE_OUI)
#define VMX86_FUTURE_OUI2 VMX86_OUI_BYTE2(VMX86_FUTURE_OUI)

#define VMX86_LEGACY_MIN_MAC  (((uint64)VMX86_LEGACY_OUI) << (VMX86_OUI_SIZE) * 8)
#define VMX86_LEGACY_MAX_MAC  ((((uint64)VMX86_LEGACY_OUI) << (VMX86_OUI_SIZE) * 8) | 0xFFFFFF)

#define VMX86_STATIC_MIN_MAC  (((uint64)VMX86_STATIC_OUI) << (VMX86_OUI_SIZE) * 8)
#define VMX86_STATIC_MAX_MAC  ((((uint64)VMX86_STATIC_OUI) << (VMX86_OUI_SIZE) * 8) | 0xFFFFFF)

#define VMX86_STATIC_NON_VPX_RANGE1_MIN_MAC  VMX86_STATIC_MIN_MAC
#define VMX86_STATIC_NON_VPX_RANGE1_MAX_MAC  ((((uint64)VMX86_STATIC_OUI) << (VMX86_OUI_SIZE) * 8) | 0x7FFFFF)

#define VMX86_STATIC_NON_VPX_RANGE2_MIN_MAC  ((((uint64)VMX86_STATIC_OUI) << (VMX86_OUI_SIZE) * 8) | 0xC00000)
#define VMX86_STATIC_NON_VPX_RANGE2_MAX_MAC  VMX86_STATIC_MAX_MAC

#define VMX86_GENERATED_MIN_MAC  (((uint64)VMX86_GENERATED_OUI) << (VMX86_OUI_SIZE) * 8)
#define VMX86_GENERATED_MAX_MAC  ((((uint64)VMX86_GENERATED_OUI) << (VMX86_OUI_SIZE) * 8) | 0xFFFFFF)


/* This OUI is used for generated WWN addresses. */
/* What exactly is a WWN address, anyway? */
#define VMX86_STATIC_WWN_OUI   0x000C29

#define VMX86_WWN_OUI_SIZE	3

#define VMX86_STATIC_WWN_OUI0 ((uint8) (VMX86_STATIC_WWN_OUI >> (VMX86_WWN_OUI_SIZE - 1) * 8))
#define VMX86_STATIC_WWN_OUI1 ((uint8) (VMX86_STATIC_WWN_OUI >> (VMX86_WWN_OUI_SIZE - 2) * 8))
#define VMX86_STATIC_WWN_OUI2 ((uint8) (VMX86_STATIC_WWN_OUI >> (VMX86_WWN_OUI_SIZE - 3) * 8))

/*
 * Top 2 bits of byte 3 of MAC address
 */

#define VMX86_MAC_PREFIX      0xc0
#define VMX86_MAC_RESERVED    0xc0  // reserved private MAC range.
#define VMX86_MAC_VPX         0x80  // VPX MAC range (old IP-based)
#define VMX86_MAC_STATIC      0x00  // reserved static MAC range.
#define VMX86_MAC_ESX         0x40  // standalone ESX VNIC MAC range.
#define VMX86_MAC_VMWARE_OUI_VPX_PREFIX_MIN 0x80 // VPX MAC range begin (old IP-based)
#define VMX86_MAC_VMWARE_OUI_VPX_PREFIX_MAX 0xBF // VPX MAC range end

/*
 * Bits left for MAC address assignment
 *
 * The explicit casts shut the compiler up.
 */

#define VMX86_MAC_BITS		22

#define VMX86_IS_LEGACY_OUI(addr) \
   ((addr)[0] == VMX86_LEGACY_OUI0 && \
    (addr)[1] == VMX86_LEGACY_OUI1 && \
    (addr)[2] == VMX86_LEGACY_OUI2)

#define VMX86_IS_STATIC_OUI(addr) \
   ((addr)[0] == VMX86_STATIC_OUI0 && \
    (addr)[1] == VMX86_STATIC_OUI1 && \
    (addr)[2] == VMX86_STATIC_OUI2)

#define VMX86_IS_GENERATED_OUI(addr) \
   ((addr)[0] == VMX86_GEN_OUI0 && \
    (addr)[1] == VMX86_GEN_OUI1 && \
    (addr)[2] == VMX86_GEN_OUI2)

#define VMX86_IS_FUTURE_OUI(addr) \
   ((addr)[0] == VMX86_FUTURE_OUI0 && \
    (addr)[1] == VMX86_FUTURE_OUI1 && \
    (addr)[2] == VMX86_FUTURE_OUI2)

#define VMX86_IS_RESERVED_MAC(addr) \
   (VMX86_IS_STATIC_OUI(addr) && \
    ((addr)[3] & VMX86_MAC_PREFIX) == VMX86_MAC_RESERVED)

#define VMX86_IS_STATIC_MAC(addr) \
   (VMX86_IS_STATIC_OUI(addr) && \
    ((addr)[3] & VMX86_MAC_PREFIX) == VMX86_MAC_STATIC)

#define VMX86_IS_VMWARE_OUI_VPX_MAC(addr) \
   (VMX86_IS_STATIC_OUI(addr) && \
    (((addr)[3] & VMX86_MAC_PREFIX) >= VMX86_MAC_VMWARE_OUI_VPX_PREFIX_MIN) && \
    (((addr)[3] & VMX86_MAC_PREFIX) <= VMX86_MAC_VMWARE_OUI_VPX_PREFIX_MAX))

/*
 * MAC addresses reserved for hostonly adapters.
 */
#define VMX86_IS_VIRT_ADAPTER_MAC(addr) \
   (VMX86_IS_RESERVED_MAC(addr) && \
    ((addr)[3] & ~VMX86_MAC_PREFIX) == 0x00 && \
    (addr)[4] == 0x00)

#define VMX86_BUILD_MAC(addr, suffix) do {                        \
   (addr)[0] = VMX86_STATIC_OUI0;                                 \
   (addr)[1] = VMX86_STATIC_OUI1;                                 \
   (addr)[2] = VMX86_STATIC_OUI2;                                 \
   (addr)[3] = (uint8) (VMX86_MAC_RESERVED                        \
                      | (((suffix) >> 16) & ~VMX86_MAC_PREFIX));  \
   (addr)[4] = (uint8) ((suffix) >> 8);                           \
   (addr)[5] = (uint8) (suffix);                                  \
} while (0)

/*
 * Generate a random static MAC usable by devices that are not
 * virtual host adapters.
 *
 * XXX - 0 if non-x86 and not ARM
 */

static INLINE void
VMX86_GENERATE_RANDOM_MAC(uint8 mac[6])
{
   uint32 offset, r = 0;

   /*
    * We use the offset to only generate addresses in the range
    * 0xe0:00:00-0xff:ff:ff instead of 0xc0:00:00-0xff:ff:ff.
    * We reserve the lower range for other purposes that may come
    * later.
    * E.g. virtual host adapters use the range c0:00:00-c0:00:ff.
    */
   offset = 0x200000;
   /* Randomize bits 20-0 and make them unique on this machine. */
#if defined(__i386__) || defined(__x86_64__) || defined(VM_ARM_64)
   r = (uint32)RDTSC();
#endif
   VMX86_BUILD_MAC(mac, r | offset);
}


static INLINE void
VMX86_GENERATE_LEGACY_MAC(uint8 mac[6],  //OUT:
			  uint32 suffix) //IN: Only 3 lower bytes are used.
{
   mac[0] = VMX86_LEGACY_OUI0;
   mac[1] = VMX86_LEGACY_OUI1;
   mac[2] = VMX86_LEGACY_OUI2;
   mac[3] = (suffix >> 16) & 0xff;
   mac[4] = (suffix >> 8) & 0xff;
   mac[5] = (suffix) & 0xff;
}

#if defined __cplusplus
} // extern "C"
#endif

#endif
