/*********************************************************
 * Copyright (C) 2009-2018 VMware, Inc. All rights reserved.
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
 * pcip_defs.h --
 *
 *      PCI passthru definitions shared by the vmx, monitor, vmkernel, and
 *      vmmon. Not all PCI passthru definitions are found here: the shared
 *      bits mainly pertain to interrupt proxying.
 */

#ifndef _PCIP_DEFS_H
#define _PCIP_DEFS_H

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMMON
#include "includeCheck.h"

#include "monitorAction_exported.h"

#include "bitvector.h"

#define MAX_INTERRUPTS                 256 // max interrupts a device could use
#define PCIP_MAX_MSIX_VECTORS          128
#define PCIP_MAX_VECTORS               (PCIP_MAX_MSIX_VECTORS + 2)

typedef struct PCIPVecBV {
   BitVector bv;
   uint32 reserved[PCIP_MAX_VECTORS / sizeof (uint32) - 1];
} PCIPVecBV;

typedef enum PCIPassthruVectorIndex {
   PCIP_INDEX_IOAPIC,
   PCIP_INDEX_MSI,
   PCIP_INDEX_MSIXOFF,
   PCIP_INDEX_INVALID = PCIP_INDEX_MSIXOFF + PCIP_MAX_MSIX_VECTORS,
} PCIPassthruVectorIndex;

typedef enum PCIPassthru_IntrType {
   PCIPASSTHRU_INTR_NONE   = 0x00,
   PCIPASSTHRU_INTR_IOAPIC = 0x01,
   PCIPASSTHRU_INTR_MSI    = 0x02,
   PCIPASSTHRU_INTR_MSIX   = 0x04,
} PCIPassthru_IntrType;

typedef struct FPTIntrProxyInfo {
   uint32            adapterIndex;
   uint32            vectorIndex;
   MonitorIdemAction actionID;
} FPTIntrProxyInfo;

typedef struct UPTIntrProxyInfo {
   uint32            adapterIndex;
   MonitorIdemAction actionID;
} UPTIntrProxyInfo;

typedef union PCIPassthru_IntrProxyInfo {
   FPTIntrProxyInfo fpt;
   UPTIntrProxyInfo upt;
} PCIPassthru_IntrProxyInfo;

typedef enum PCIPassthru_ErrorType {
   PCIPASSTHRU_ERROR_NONE       = 0x00,
   PCIPASSTHRU_ERROR_AER        = 0x01,
   PCIPASSTHRU_ERROR_PAGE_FAULT = 0x02,
} PCIPassthru_ErrorType;

typedef struct PCIPassthru_PageFaultInfo {
   uint64 ioAddr;
   uint64 machAddr;
   uint8  faultReason;
   uint8  unused[7];
} PCIPassthru_PageFaultInfo;

typedef struct PCIPassthru_AERInfo {
   uint64 count;
} PCIPassthru_AERInfo;

typedef union PCIPassthru_ErrorInfo {
   PCIPassthru_PageFaultInfo pageFaultInfo;
   PCIPassthru_AERInfo aerInfo;
} PCIPassthru_ErrorInfo;

typedef 
#include "vmware_pack_begin.h"
struct PCIPassthru_ErrorMsg {
   uint32                 sbdf;
   PCIPassthru_ErrorType  errorType;
   PCIPassthru_ErrorInfo  errorInfo;
}
#include "vmware_pack_end.h"
PCIPassthru_ErrorMsg;

#endif // _PCIP_DEFS_H
