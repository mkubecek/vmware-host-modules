/*********************************************************
 * Copyright (C) 2002 VMware, Inc. All rights reserved.
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
 * smac.h --
 *
 *      This file declares functionality that allows the
 *      bridge to be used across links that do
 *      not support promiscuous mode, nor provide the
 *      ability to transmit ethernet frames whose MAC source
 *      address does not match the hardware's MAC address.
 */

#ifndef _SMAC_H_
#define _SMAC_H_

#ifdef _WIN32
#include "vnetInt.h"
#else /* _WIN32 */

#include "vm_basic_types.h"

/* linux header files include too much garbage, so just define if needed */
#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif /* ETH_ALEN */

#ifndef ETH_HLEN
#define ETH_HLEN 14
#endif /* ETH_HLEN */

#endif /* _WIN32 */

#if defined __linux__ && !defined __x86_64__
#define SMACINT __attribute__((cdecl, regparm(3)))
#else
#define SMACINT
#endif

typedef enum {
   PacketStatusTooShort = 0x4546, // insuficient data to process packet
   PacketStatusDropPacket,        // bridge should drop packet
   PacketStatusForwardPacket      // bridge should accept/process/forward packet
} PacketStatus;

typedef struct IPv6Addr {
   uint64 addrHi; /* High order 64 bits of the address. */
   uint64 addrLo; /* Low order 64 bits of the address. */
} IPv6Addr;

struct SMACState;

#if defined(_WIN32) && NDIS_SUPPORT_NDIS6
Bool BridgeIPv6MatchAddrMAC(const IPv6Addr *addr, const uint8 *mac);
Bool BridgeIPv4MatchAddrMAC(const ULONG ipAddr, const uint8 *mac);
#endif
void SMACINT
SMAC_InitState(struct SMACState **ptr);           // IN: state to alloc/init
void SMACINT
SMAC_SetMac(struct SMACState *state, const uint8 *mac); // IN: state, and host MAC
void SMACINT
SMAC_CleanupState(struct SMACState **ptr);        // IN: state to cleanup/dealloc

/* 
 * Structure is used to separate out differences 
 * between packets on different OSes.
 */

#ifdef _WIN32
/* defines Windows versions of SMACPacket and SMACPackets */
#include "smac_win.h"
#else /* _WIN32 */
/* non-WIN32 versions of these structs */
typedef struct SMACPacket {
#ifdef __linux__
   struct sk_buff *skb;  // packet
   void *startOfData;    // handles non-uniform start of data in sk_buff
   unsigned int len;     // compensates for ethernet header for inbound packets
#else
   mbuf_t m;             // packet
#endif
} SMACPacket;

typedef struct SMACPackets {
   SMACPacket orig;  // IN: packet
   SMACPacket clone; // OUT: packet
} SMACPackets;
#endif /* _WIN32 */

PacketStatus SMACINT
SMAC_CheckPacketFromHost(struct SMACState *state,  // IN: pointer to smac state
			 SMACPackets    *packets); // IN/OUT: packet(s) to process

PacketStatus SMACINT
SMAC_CheckPacketToHost(struct SMACState *state,  // IN: pointer to smac state
		       SMACPackets *packets);	 // IN/OUT: packet(s) to process

void SMACINT
SMAC_SetForwardUnknownPackets(struct SMACState *state, // IN: pointer to smac state
			      Bool forwardUnknown);    // IN: T/F to forward

#endif // _SMAC_H_


