/*********************************************************
 * Copyright (C) 2002,2018,2020 VMware, Inc. All rights reserved.
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
 * smac.c --
 *
 *      This file defines functionality that allows the
 *      bridge to be used across links that do
 *      not support promiscuous mode, or do not provide the
 *      ability to transmit ethernet frames whose MAC source
 *      address does not match the hardware's MAC address.
 *
 *      This code extension basically forces the bridge to
 *      use a single MAC, thus the name SMAC.
 */

/* platform-dependent includes */

#ifdef _WIN32

#define BINARY_COMPATIBLE 0 // NT-only driver (optimizes some NDIS calls)
#include <ndis.h>
#include <ntstrsafe.h>

#include "vnetInt.h"

#else /* _WIN32 */

#ifdef VMX86_DEVEL
#define DBG 1
#else
#undef DBG
#endif /* VMX86_DEVEL */

#ifdef __APPLE__
#include <sys/kpi_mbuf.h>
#include <libkern/libkern.h>
#endif

#include "smac_compat.h"

#endif /* _WIN32 */

/* platform-independent includes */
#include "smac.h"
#include "vm_basic_defs.h"
#include "vm_basic_asm.h"

#define SMAC_MODULE "SMAC: "
#define MODULE_NAME SMAC_MODULE

/* platform-dependent defines */
#ifdef _WIN32
#define ALLOCATEMEMORY(a,b) VNet_AllocateMemoryWithTag((a),(b))
#define FREEMEMORY(a)       VNet_FreeMemory((a))
#define MEMCPY(a,b,c)       NdisMoveMemory((a),(b),(c))
#define MEMSET(a,b,c)       NdisFillMemory((a),(c),(b))

#define SPINLOCKINIT()      do { } while (0)
#define INITSPINLOCK(a)     do { NdisAllocateSpinLock( (a) ); } while(0)
#define RAISEIRQL()         do { irql = KeRaiseIrqlToDpcLevel(); } while(0)
#define ACQUIRESPINLOCK(a)  do { ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL); \
                                 NdisDprAcquireSpinLock( (a) ); } while(0)
#define RELEASESPINLOCK(a)  do { NdisDprReleaseSpinLock( (a) ); } while(0)
#define LOWERIRQL()         do { KeLowerIrql(irql); } while(0)
#define FREESPINLOCK(a)     do { NdisFreeSpinLock( (a) ); } while(0)
#define ASSERTLOCKHELD()    ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL)
#define SNPRINTF(a)            (RtlStringCbPrintfA a)

#elif defined __linux__ || defined __APPLE__

#define ALLOCATEMEMORY(a,b) SMACL_Alloc((a))
#define MEMCPY(a,b,c)       SMACL_Memcpy((a),(b),(c))
#define MEMSET(a,b,c)       SMACL_Memset((a),(b),(c))

#define INITSPINLOCK(a)     SMACL_InitSpinlock( (a) )
#define RAISEIRQL()         do { } while (0)
#define LOWERIRQL()         do { } while (0)
#define ASSERTLOCKHELD()    do { } while (0)

#define UNREFERENCED_PARAMETER(a) { (a) = (a); }

/*
 * The following are defined to create OS dependent versions of
 * functionality available on Windows.
 */

#undef ASSERT

#ifdef DBG
#   if defined __APPLE__
#      define VNETKdPrint(a)         (Log a)
#   else // __linux__
#      define VNETKdPrint(a)         (SMACL_Print a)
#   endif
#   define ASSERT(a) do {if (!(a)) {VNETKdPrint(("ASSERT FAILED: "#a));}} while(0)
#else
#   define VNETKdPrint(a)         do { } while (0)
#   define ASSERT(a)              do { } while (0)
#endif

#define VNETKdPrintCall(a)     VNETKdPrint((MODULE_NAME "Calling    : %s\n", a))
#define VNETKdPrintReturn(a)   VNETKdPrint((MODULE_NAME "Returned   : %s\n", a))

#ifndef MAC_EQ
#define MAC_EQ(a,b)    (SMACL_Memcmp((a),(b),ETH_ALEN)==0)
#endif /* MAC_EQ */

#define IS_MULTICAST(_hdr)   ((_hdr)[0] & 0x1)
#define IS_BROADCAST(_a) \
   ((_a)[0] == 0xff && (_a)[1] == 0xff && (_a)[2] == 0xff && \
    (_a)[3] == 0xff && (_a)[4] == 0xff && (_a)[5] == 0xff)

#ifdef __linux__
#define FREEMEMORY(a)       SMACL_Free((a))
#define SPINLOCKINIT()      unsigned long flags
#define FREESPINLOCK(a)     SMACL_Free(*a)
#define ACQUIRESPINLOCK(a)  SMACL_AcquireSpinlock( (a), &flags)
#define RELEASESPINLOCK(a)  SMACL_ReleaseSpinlock( (a), &flags)
extern int VNetSnprintf(char *str, size_t size, const char *format, ...);
#define SNPRINTF(a)         (VNetSnprintf a)
#else /* __APPLE__ */
#define FREEMEMORY(a)       SMACL_Free((a), sizeof *(a))
#define SPINLOCKINIT()      do { } while (0)
#define FREESPINLOCK(a)     SMACL_FreeSpinlock( (a) )
#define ACQUIRESPINLOCK(a)  SMACL_AcquireSpinlock( *(a) )
#define RELEASESPINLOCK(a)  SMACL_ReleaseSpinlock( *(a) )
#define SNPRINTF(a)         (snprintf a)
#endif

#else
#error "unknown platform"
#endif /* _WIN32 */

/* Offsets/lengths for IPv4, UDP, and ARP headers. */
#define IP_HEADER_LEN               20
#define IP_HEADER_DEST_ADDR_OFFSET  16
#define IP_HEADER_SRC_ADDR_OFFSET   12
#define IP_HEADER_FLAGS_OFFSET	    6
#define IP_HEADER_PROTO_OFFSET	    9
#define UDP_HEADER_LEN              8
#define ARP_HEADER_LEN              28
#define ARP_SENDER_MAC_OFFSET       8
#define ARP_SENDER_IP_OFFSET        14
#define ARP_TARGET_MAC_OFFSET       18
#define ARP_TARGET_IP_OFFSET        24

#define IP_ADDR_BROADCAST           0xFFFFFFFF

#define IPv4                        4
#define IPv6                        6
#define IP_STRING_SIZE              40 /* To express IP in string format. */
#define MAC_STRING_SIZE             18 /* To express MAC in string format. */

/* Offsets/lengths for IPv6 headers. */
#define IPv6_HEADER_LEN               40
#define IPv6_HEADER_SRC_ADDR_OFFSET   8
#define IPv6_HEADER_DST_ADDR_OFFSET   24
#define IPv6_NEXT_HEADER_OFFSET       6
#define ICMPv6_TYPE_OFFSET            0  /* From start of ICMPv6 payload. */
#define ICMPv6_CHECKSUM_OFFSET        2  /* From start of ICMPv6 payload. */
#define ICMPv6_NDP_NBR_LEN            24 /*
                                          * Message length (not including
                                          * options) for neighbor solicitation
                                          * and advertisement messages.
                                          */
#define ICMPv6_NDP_RTR_SOL_LEN        8  /* For router solicitation. */
#define ICMPv6_NDP_RTR_ADV_LEN        16 /* For router advertisement. */
#define ICMPv6_TARGET_IP_OFFSET       8  /* From start of ICMPv6 payload. */
#define ICMPv6_NDP_OPTION_TYPE_OFFSET 0  /* Offset from start of option. */
#define ICMPv6_NDP_OPTION_LEN_OFFSET  1  /* Offset from start of option. */
#define ICMPv6_NDP_MAC_OFFSET         2  /* Offset from start of option. */
#define ICMPv6_NDP_OPTION_SRC_MAC     1  /* Source link-layer option. */
#define ICMPv6_NDP_OPTION_TARGET_MAC  2  /* Target link-layer option. */
#define ICMPv6_NDP_RTR_SOLICITATION   133 /* Router solicitation. */
#define ICMPv6_NDP_RTR_ADVERTISEMENT  134 /* Router advertisement. */
#define ICMPv6_NDP_NBR_SOLICITATION   135 /* Neighbor solicitation. */
#define ICMPv6_NDP_NBR_ADVERTISEMENT  136 /* Neighbor advertisement. */

/*
 * To limit the amount of kernel log information, define 
 * WIRELESS_BE_QUIET or WIRELESS_BE_VERY_QUIET.  The former
 * reduces the logging to a point where the host system isn't
 * bogged down with logging all the details of broadcast traffic
 * coming in from the company network.  The latter define will
 * completely turn off wireless logging.
 */

#define  WIRELESS_BE_VERY_QUIET
//#define  WIRELESS_BE_QUIET

#ifdef WIRELESS_BE_VERY_QUIET
#define WW_VNETKdPrint(a) 
#define WW_DEVEL_ONLY(a)
#define W_VNETKdPrint(a) 
#define W_DEVEL_ONLY(a)
#else // WIRELESS_BE_VERY_QUIET

#ifdef  WIRELESS_BE_QUIET
#define WW_VNETKdPrint(a)
#define WW_DEVEL_ONLY(a)
#define W_VNETKdPrint(a) VNETKdPrint(a)
#  ifdef DBG
#     define W_DEVEL_ONLY(a) a
#  else
#     define W_DEVEL_ONLY(a)
#endif
#else // WIRELESS_BE_QUIET
#define WW_VNETKdPrint(a) VNETKdPrint(a)
#  ifdef DBG
#     define WW_DEVEL_ONLY(a) a
#  else
#     define WW_DEVEL_ONLY(a)
#  endif
#define W_VNETKdPrint(a) VNETKdPrint(a)
#  ifdef DBG
#     define W_DEVEL_ONLY(a) a
#  else
#     define W_DEVEL_ONLY(a)
#  endif
#endif // WIRELESS_BE_QUIET

#endif // WIRELESS_BE_VERY_QUIET

/*
 * Host-to-Network / Network-to-Host byte-order routines
 */

#ifdef __APPLE__
#undef HTONL
#undef NTOHL
#undef HTONS
#undef NTOHS
#endif /* __APPLE__ */

/*
 * For use in case labels where compile-time consts are required (obj builds)
 */
#define CONST_NTOHL(i) \
   (((uint32)(i))>>24 | ((i)&0xff)<<24 | ((i)&0x00ff0000)>>8 | ((i)&0x0000ff00)<<8)

/*
 * All other uses should call inline BswapNN() for type checking
 */
#define HTONL(i) Bswap32(i)
#define NTOHL(i) Bswap32(i)
#define HTONS(i) Bswap16(i)
#define NTOHS(i) Bswap16(i)

/*
 * IPmacLookupEntry: defines entry in IP/MAC hash tables for finding which 
 * IP corresonds to which MAC
 */

#ifdef _WIN32
typedef uint64 SmacLastAccess;
#define LAST_ACCESS_FORMAT "%"FMT64"u" // format of lastAccess for printf()
#else
typedef unsigned long SmacLastAccess;
#define LAST_ACCESS_FORMAT "%lu"      // format of lastAccess for printf()
#endif /* _WIN32 */

typedef union IPAddrUnion {
   uint32 ipv4Addr;
   IPv6Addr ipv6Addr;
} IPAddrUnion;

typedef struct IPAddrContainer {
   IPAddrUnion addr;
   uint16 ver; /* IPv4 or IPv6 (uint16 pads better). */
} IPAddrContainer;

typedef struct IPmacLookupEntry {
   struct IPmacLookupEntry *ipNext;   // pointer to next item in bucket in IP hash table
   IPAddrContainer addrContainer;     // Struct holding the v4/v6 address
   uint8 mac[ETH_ALEN];               // ethernet MAC address
   SmacLastAccess lastAccess;         // estimated time of entry's last use
} IPmacLookupEntry;

/*
 * EthernetHeader: struct that corresponds with common ethernet header
 * (an ethernet frame that contians a VLAN header has different
 * format: 2 additional bytes after srcAddr)
 */

typedef struct EthernetHeader {
   uint8 destAddr[ETH_ALEN];   // destination MAC
   uint8 srcAddr[ETH_ALEN];    // source MAC
   uint16 lengthType;          // length/type field
} EthernetHeader;

/*
 * EthClass: used to classify the various ethernet media types
 * into a small group of classes.
 */

typedef enum {
   EthClassIllegal = 0x345,   // media type in an unrefined/reserved range
   EthClassCommon,	      // means known but no special handling needed
   EthClassUncommon,	      // like common, but should trigger more debug printouts
   EthClassUnknown,	      // not specifically known/handled), but a legal type
   EthClassIPv4,	      // IPv4 type
   EthClassARP,		      // one of the various ARP protocols
   EthClassVLAN,	      // VLAN type
   EthClassIPv6,              // IPv6 type
   EthClassEAPOL,             // 802.1x type (EAPOL)
} EthClass;

/*
 * SMACState: encapsulates all wireless state for a specific host adapter
 */

#define SMAC_HASH_TABLE_SIZE 256 // length of table, must be power of 2
#define SMAC_HASH_MASK       (SMAC_HASH_TABLE_SIZE - 1) // hash bits

typedef struct SMACState {
#ifdef _WIN32
   NDIS_SPIN_LOCK	 smacSpinLock;	     // spinlock that protects wireless state
#else /* _WIN32 */
   void            	 *smacSpinLock;       // spinlock that protects wireless state
#endif /* _WIN32 */
   SmacLastAccess	 lastUptimeRead;     // used to track uptime counter overflow
   struct IPmacLookupEntry * IPlookupTable[SMAC_HASH_TABLE_SIZE];  // IP hash table IP->MAC
   uint32 numberOfIPandMACEntries;	     // # of hash table entries
   IPAddrContainer lastIPadded;		     // last IP added to hash
   uint8  lastMACadded[ETH_ALEN];	     // last MAC added to hash
   struct IPmacLookupEntry * lastEntryAdded; // ptr to cache entry (to update timestamp)
   Bool	  smacForwardUnknownPackets;         // forward "all" packets? (typically doesn't)
   uint8  macAddress[ETH_ALEN];              // pointer to host MAC address
} SMACState;

/*
 * Function prototypes
 */

static INLINE uint32 SUM32(uint32 in);
static uint32 CalcChecksumDiff(uint32 sumBefore, uint32 sumAfter);
static uint16 UpdateSum(uint16 oricheck, uint32 sumDiff);

static INLINE IPmacLookupEntry *
LookupByIPNoAcquireLock(SMACState *state,
                        const IPAddrContainer *addrContainer);
static Bool LookupByIP(SMACState *state, const IPAddrContainer *addrContainer,
                       uint8 *macAddress);
static INLINE Bool LookupByIPv4(SMACState *state, uint32 ipv4Addr,
                                uint8 *macAddress);

static INLINE Bool RemoveIPfromHashTableNoAcquireLock(SMACState *state,
						      IPmacLookupEntry *entryToRemove);

static void TrimLookupTableIfNecessary(SMACState *state);

static INLINE void SetCacheEntry(SMACState *state, IPmacLookupEntry *entry);

static Bool AddIPandMACcombo(SMACState *state,
                             const IPAddrContainer *addrContainer, uint8 *mac);
static INLINE Bool AddIPv4andMACcombo(SMACState *state, uint32 ipv4Addr,
                                      uint8 *mac);
static INLINE Bool AddIPv6andMACcombo(SMACState *state,
                                      const IPv6Addr *ipv6Addr, uint8 *mac);

static void ProcessOutgoingIPv4Packet(SMACPacket *packet, uint32 ethHeaderLen);
static Bool ProcessOutgoingIPv6Packet(SMACPacket *packet, uint32 ethHeaderLen,
                                      const uint8 *smacAddress, Bool *toHost);
static Bool
PatchMacAddrFixChecksum(SMACPacket *packet, const uint32 packetLen,
                        const uint32 checksumOffset,
                        const uint32 patchMacOffset, const uint8 *macAddress,
                        const char *logPrefix);
#ifdef DBG
static void ProcessIncomingIPv4Packet(SMACPacket *packet, 
				      Bool knownMacForIp);
#endif
static SmacLastAccess GetSystemUptime(SMACState *state);

/* get information from packet */
static INLINE uint32 GetPacketLength(SMACPacket *packet);
static Bool GetPacketData(SMACPacket *packet, uint32 offset, 
			  uint32 length, void *data);

/* set information in packet */
static Bool SetPacketByte(SMACPacket *packet, uint32 offset, 
			  uint8 data);

/* clone source / write data to clone */
static Bool ClonePacket(SMACPackets *packets);
static Bool CopyDataToClonedPacket(SMACPackets *packets, const void *source,
				   uint32 offset, uint32 length);

/* write data to source (on Windows) / write data to clone (on Linux) */
static Bool CopyDataForPacketFromHost(SMACPackets *packets, uint32 changeNum,
				      uint32 offset, const uint8 *macAddress);

static EthClass LookupTypeClass(uint16 typeValue);
#ifdef DBG
#ifdef _WIN32
_At_(type, _In_z_bytecount_(typeLen))
#endif
static void LookupTypeName(uint16 typeValue, char *type, size_t typeLen);
#endif


/*
 * Various utility functions operating on IPv4/v6 addresses, address
 * containers, or IP/MAC lookup entries.
 */

/*
 *----------------------------------------------------------------------
 *
 * IPv4Hash --
 * IPv6Hash -- 
 *
 *      Returns a one byte hash of an IPv4 (IPv6) address by adding
 *      all the octets in the address.
 *
 * Results:
 *      One byte hash value.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint8
IPv4Hash(uint32 addr) // IN:
{
   return ((addr >> 24 & 0xff) + (addr >> 16 & 0xff) + (addr >> 8 & 0xff) +
           (addr & 0xff)) & SMAC_HASH_MASK;
}

static INLINE uint8
IPv6Hash(const IPv6Addr *addr) // IN:
{
   int i;
   uint8 hash = (uint8)(addr->addrLo & 0xff) + (uint8)(addr->addrHi & 0xff);

   for (i = 1; i < 4; i++) {
      hash += (uint8)(addr->addrLo >> (i * 8) & 0xff) +
              (uint8)(addr->addrHi >> (i * 8) & 0xff);
   }
   return hash & SMAC_HASH_MASK;
}


/*
 *----------------------------------------------------------------------
 *
 * IsIPAddrContainerV4 --
 * IsIPAddrContainerV6 -- 
 * IsLookupEntryV4 --
 * IsLookupEntryV6 --
 *
 *      Checks if an IP address container (lookup entry) is of type v4 (v6).
 *
 * Results:
 *      TRUE iff of type v4 (v6).
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
IsIPAddrContainerV4(const IPAddrContainer *addrContainer) // IN:
{
   return addrContainer->ver == IPv4;
}

static INLINE Bool
IsIPAddrContainerV6(const IPAddrContainer *addrContainer) // IN:
{
   return addrContainer->ver == IPv6;
}

static INLINE Bool
IsLookupEntryV4(const IPmacLookupEntry *entry) // IN:
{
   return IsIPAddrContainerV4(&entry->addrContainer);
}

static INLINE Bool
IsLookupEntryV6(const IPmacLookupEntry *entry) // IN:
{
   return IsIPAddrContainerV6(&entry->addrContainer);
}


/*
 *----------------------------------------------------------------------
 *
 * Container{Get,Set}IPv4Addr --
 * Container{Get,Set}IPv6Addr -- 
 * LookupEntry{Get,Set}IPv4Addr --
 * LookupEntry{Get,Set}IPv4Addr --
 *
 *      Returns/sets an IPv4 (IPv6) address from/to an IP address container
 *      (IP/MAC lookup entry) structure.  Assumes that the caller has checked
 *      the version type of the address prior to calling the Get function.
 *
 * Results:
 *      IP address (for the Get function), none for the Set function.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint32
ContainerGetIPv4Addr(const IPAddrContainer *addrContainer) // IN:
{
   ASSERT(IsIPAddrContainerV4(addrContainer));
   return addrContainer->addr.ipv4Addr;
}

static INLINE uint32
LookupEntryGetIPv4Addr(const IPmacLookupEntry *entry) // IN:
{
   ASSERT(IsLookupEntryV4(entry));
   return ContainerGetIPv4Addr(&entry->addrContainer);
}

static INLINE void
ContainerSetIPv4Addr(IPAddrContainer *addrContainer, // IN:
                     uint32 ipv4Addr)                // IN:
{
   addrContainer->ver = IPv4;
   addrContainer->addr.ipv4Addr = ipv4Addr;
}

static INLINE void
LookupEntrySetIPv4Addr(IPmacLookupEntry *entry, // IN:
                       uint32 ipv4Addr)         // IN:
{
   ContainerSetIPv4Addr(&entry->addrContainer, ipv4Addr);
}

static INLINE const IPv6Addr *
ContainerGetIPv6Addr(const IPAddrContainer *addrContainer) // IN:
{
   ASSERT(IsIPAddrContainerV6(addrContainer));
   return &addrContainer->addr.ipv6Addr;
}

static INLINE const IPv6Addr *
LookupEntryGetIPv6Addr(const IPmacLookupEntry *entry) // IN:
{
   ASSERT(IsLookupEntryV6(entry));
   return ContainerGetIPv6Addr(&entry->addrContainer);
}

static INLINE void
ContainerSetIPv6Addr(IPAddrContainer *addrContainer, // IN:
                     const IPv6Addr *ipv6Addr)       // IN:
{
   addrContainer->ver = IPv6;
   addrContainer->addr.ipv6Addr.addrHi = ipv6Addr->addrHi;
   addrContainer->addr.ipv6Addr.addrLo = ipv6Addr->addrLo;
}

static INLINE void
LookupEntrySetIPv6Addr(IPmacLookupEntry *entry,  // IN:
                       const IPv6Addr *ipv6Addr) // IN:
{
   ContainerSetIPv6Addr(&entry->addrContainer, ipv6Addr);
}


/*
 *----------------------------------------------------------------------
 *
 * LookupEntrySetIPAddrContainer --
 *
 *      Copies an IP address container to an IP/MAC lookup entry.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
LookupEntrySetIPAddrContainer(IPmacLookupEntry *entry,              // OUT:
                              const IPAddrContainer *addrContainer) // IN:
{
   if (IsIPAddrContainerV4(addrContainer)) {
      LookupEntrySetIPv4Addr(entry, ContainerGetIPv4Addr(addrContainer));
   } else {
      LookupEntrySetIPv6Addr(entry, ContainerGetIPv6Addr(addrContainer));
   }
}


/*
 *----------------------------------------------------------------------
 *
 * IPAddrContainerHash --
 * LookupEntryIPAddrHash --
 *
 *      Return the hash of a given IP address container (IP/MAC lookup
 *      entry) structure.
 *
 * Results:
 *      One byte hash of the IP address.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint8
IPAddrContainerHash(const IPAddrContainer *addrContainer) // IN:
{
   return IsIPAddrContainerV4(addrContainer) ?
      IPv4Hash(ContainerGetIPv4Addr(addrContainer)) :
      IPv6Hash(ContainerGetIPv6Addr(addrContainer));
}

static INLINE uint8
LookupEntryIPAddrHash(const IPmacLookupEntry *entry) // IN:
{
   return IPAddrContainerHash(&entry->addrContainer);
}


/*
 *----------------------------------------------------------------------
 *
 * IPv6AddrEquals --
 *
 *      Checks if two given IPv6 addresses match.
 *
 * Results:
 *      TRUE iff the addresses match.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
IPv6AddrEquals(const IPv6Addr *addr1, // IN:
               const IPv6Addr *addr2) // IN:
{
   return addr1->addrHi == addr2->addrHi && addr1->addrLo == addr2->addrLo;
}


/*
 *----------------------------------------------------------------------
 *
 * AddrContainersVersionsMatch --
 *
 *      Checks if two given IP address containers are of the same version.
 *
 * Results:
 *      TRUE iff the versions match.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
AddrContainersVersionsMatch(const IPAddrContainer *one, // IN:
                            const IPAddrContainer *two) // IN:
{
   return one->ver == two->ver;
}


/*
 *----------------------------------------------------------------------
 *
 * AddrContainersMatch --
 *
 *      Checks if two given IP address containers match (same version and
 *      same IP address).
 *
 * Results:
 *      TRUE iff the versions and the addresses match.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
AddrContainersMatch(const IPAddrContainer *one, // IN:
                    const IPAddrContainer *two) // IN:
{
   if (AddrContainersVersionsMatch(one, two)) {
      if (IsIPAddrContainerV4(one)) {
         return ContainerGetIPv4Addr(one) == ContainerGetIPv4Addr(two);
      } else {
         return IPv6AddrEquals(&one->addr.ipv6Addr, &two->addr.ipv6Addr);
      }
   }
   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * AddrContainerMatchIPv4Address --
 *
 *      Checks if a given IP address container matches a given IPv4 address.
 *
 * Results:
 *      TRUE iff the versions and the addresses match.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
AddrContainerMatchIPv4Address(const IPAddrContainer *addrContainer, // IN:
                              uint32 ipv4Addr)                      // IN:
{
   IPAddrContainer ipv4AddrContainer;

   ContainerSetIPv4Addr(&ipv4AddrContainer, ipv4Addr);
   return AddrContainersMatch(addrContainer, &ipv4AddrContainer);
}


#ifdef DBG
/*
 *----------------------------------------------------------------------
 *
 * ContainerPrintIPAddrToString --
 * LookupEntryPrintIPAddrToString --
 * PrintIPv4AddrToString --
 * PrintIPv6AddrToString --
 * PrintMACAddrToString
 *
 *      Uses platform specific snprintf() to convert an IP address (or
 *      MAC address) to string format and copies it to a given output
 *      string.
 *
 * Results:
 *      Pointer to the given string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE char *
ContainerPrintIPAddrToString(char *str,                            // OUT:
                             size_t size,                          // IN:
                             const IPAddrContainer *addrContainer) // IN:
{
   if (IsIPAddrContainerV4(addrContainer)) {
      uint32 addr = ContainerGetIPv4Addr(addrContainer);
      SNPRINTF((str, size, "%u.%u.%u.%u", addr & 0xff, addr >> 8 & 0xff,
                addr >> 16 & 0xff, addr >> 24 & 0xff));
   } else {
      const IPv6Addr *addr = ContainerGetIPv6Addr(addrContainer);

      SNPRINTF((str, size, "%x:%x:%x:%x:%x:%x:%x:%x",
                NTOHS((uint16)(addr->addrHi & 0xffff)),
                NTOHS((uint16)(addr->addrHi >> 16 & 0xffff)),
                NTOHS((uint16)(addr->addrHi >> 32 & 0xffff)),
                NTOHS((uint16)(addr->addrHi >> 48 & 0xffff)),
                NTOHS((uint16)(addr->addrLo & 0xffff)),
                NTOHS((uint16)(addr->addrLo >> 16 & 0xffff)),
                NTOHS((uint16)(addr->addrLo >> 32 & 0xffff)),
                NTOHS((uint16)(addr->addrLo >> 48 & 0xffff))));
   }
   return str;
}

static INLINE char *
LookupEntryPrintIPAddrToString(char *str,                     // OUT:
                               size_t size,                   // IN:
                               const IPmacLookupEntry *entry) // IN:
{
   return ContainerPrintIPAddrToString(str, size, &entry->addrContainer);
}

static INLINE void
PrintIPv4AddrToString(char *str,       // OUT:
                      size_t size,     // IN:
                      uint32 ipv4Addr) // IN:
{
   IPAddrContainer addrContainer;

   ContainerSetIPv4Addr(&addrContainer, ipv4Addr);
   ContainerPrintIPAddrToString(str, size, &addrContainer);
}

static INLINE char *
PrintIPv6AddrToString(char *str,                // OUT:
                      size_t size,              // IN:
                      const IPv6Addr *ipv6Addr) // IN:
{
   IPAddrContainer addrContainer;

   ContainerSetIPv6Addr(&addrContainer, ipv6Addr);
   return ContainerPrintIPAddrToString(str, size, &addrContainer);
}

static INLINE char *
PrintMACAddrToString(char *str,        // OUT:
                     size_t size,      // IN:
                     const uint8 *mac) // IN:
{
   SNPRINTF((str, size, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]));
   return str;
}
#endif /* DBG */


/*
 * IP hash table routines: the following routines pertain to operations
 * on the IP hash table. SMACState->smacSpinLock should be held when reading or
 * writing data in the hash table.  A read/write lock might be better
 * but the locks are usually held for a brief period of time.  
 *
 * 'lastIPadded' and 'lastMACadded' are used to cache the last entry that
 * was added to the hash table.  For most packets we attempt to add IP/MAC
 * information from that packet to the hash table.  In most cases (especially
 * during file transfers) the entry will already be added to the table so we
 * cache the last addition to minimize overhead.  The cache information is
 * not used for lookups, only to make adds more efficient.
 */


/*
 *----------------------------------------------------------------------
 *
 * LookupByIP --
 * LookupByIPNoAcquireLock -- 
 *
 *      Lookup entry or MAC address that corresponds to the 
 *      specified IP address.   Locking and non-locking versions
 *      are provided.  The non-locking version returns the actual
 *      table entry, while the locking version only returns the 
 *      actual MAC address (to avoid reference counting).
 *
 * Results:
 *      Nonlocking: pointer to entry (if found), otherwise NULL
 *      Locking: TRUE if MAC found, otherwise FALSE
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE IPmacLookupEntry *
LookupByIPNoAcquireLock(SMACState *state,                     // IN: state
                        const IPAddrContainer *addrContainer) // IN: v4/v6 addr
{
   uint8 hash = IPAddrContainerHash(addrContainer);
   IPmacLookupEntry *curr;

   ASSERT(SMAC_HASH_TABLE_SIZE == 256);

   /*
    * Search thru bucket for match.
    */

   for (curr = state->IPlookupTable[hash]; curr; curr = curr->ipNext) {
      if (AddrContainersMatch(&curr->addrContainer, addrContainer)) {
         break;
      }
   }
   return curr;
}


static Bool
LookupByIP(SMACState *state,                     // IN: adapter 
           const IPAddrContainer *addrContainer, // IN: IP to lookup
           uint8 *macAddress)                    // OUT: (optional) MAC of IP
                                                 //      (uint8[ETH_ALEN])
{
   IPmacLookupEntry *entry;
   WW_DEVEL_ONLY(char ipStr[IP_STRING_SIZE];)
   SPINLOCKINIT();

   WW_VNETKdPrint((MODULE_NAME "LookupByIP: told to find %s\n",
                   ContainerPrintIPAddrToString(ipStr, sizeof ipStr,
                                                addrContainer)));

   ACQUIRESPINLOCK(&state->smacSpinLock);
   entry = LookupByIPNoAcquireLock(state, addrContainer);
   if (entry != NULL && macAddress != NULL) {
      MEMCPY(macAddress, entry->mac, ETH_ALEN);
   }
   RELEASESPINLOCK(&state->smacSpinLock);
   return entry != NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * LookupByIPv4 --
 *
 *      Helper wrapper function for LookupByIP() for IPv4 cases.
 *
 * Results:
 *      As in LookupByIP().
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
LookupByIPv4(SMACState *state,  // IN:
             uint32 ipv4Addr,   // IN:
             uint8 *macAddress) // OUT:
{
   IPAddrContainer addrContainer;

   ContainerSetIPv4Addr(&addrContainer, ipv4Addr);
   return LookupByIP(state, &addrContainer, macAddress);
}


/*
 *----------------------------------------------------------------------
 *
 * RemoveIPfromHashTable --
 * RemoveIPfromHashTableNoAcquireLock --
 *
 *      Removed specified entry from the IP hash table.  Function
 *      presumes that specified table entry still contains the IP 
 *      address that was used to add the entry to the hash table.
 *      Locking and no-locking version of function are provided
 *
 *      This function doesn't check whether the cached entry is
 *      being removed (and thus won't reset the cached entry). This
 *      code is primarily used to remove the oldest entry, and by
 *      definition the cached entry is the newest (i.e., it's never
 *      the oldest and thus won't be removed).
 *
 * Results:
 *      TRUE if entry removed, FALSE otherwise.
 *
 * Side effects:
 *      May remove entry from IP hash table.  Actual entry is not
 *      modified nor deallocated by this function.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
RemoveIPfromHashTableNoAcquireLock(SMACState *state,                 // IN: state
				   IPmacLookupEntry * entryToRemove) // IN: packet
{
   uint8 ipHashToRemove;
   IPmacLookupEntry * prev = NULL, *entry;

   ASSERT(entryToRemove);
   ASSERT(SMAC_HASH_TABLE_SIZE == 256);

   ipHashToRemove = LookupEntryIPAddrHash(entryToRemove);
   entry = state->IPlookupTable[ipHashToRemove]; // get bucket
      
   /*
    * locate and remove old IP entry from bucket
    */

   while (entry) {
      if (entry == entryToRemove) {
	 if (prev) {
	    W_VNETKdPrint((MODULE_NAME "RemoveIPfromHashTable: removed IP "
			   "entry from middle of bucket\n"));
	    prev->ipNext = entry->ipNext;
	 } else {
	    W_VNETKdPrint((MODULE_NAME "RemoveIPfromHashTable: removed IP "
			   "entry from front of bucket\n"));
	    state->IPlookupTable[ipHashToRemove] = entry->ipNext;
	 }
	 return TRUE;
      } else {
	 prev = entry;
	 entry = entry->ipNext;
      }
   }
   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * TrimLookupTableIfNecessary --
 *
 *      If the number of entries in the IP and MAC tables exceeds a
 *      specified value (currently 20), then we remove and deallocate
 *      an entry--ideally the entry which has been used least recently
 *      is removed.  The code presumes that this function will be called
 *      anytime a new entry is added, thus we should never need to
 *      remove more than one entry per function call.
 *
 *      Function presumes that state lock is held while this function
 *      is called.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May remove and deallocates an entry from the IP and MAC hash
 *      tables.
 *
 *----------------------------------------------------------------------
 */

static void
TrimLookupTableIfNecessary(SMACState *state) // IN: smac state
{
   IPmacLookupEntry * oldestEntry = NULL;      // oldest entry found
   SmacLastAccess oldestUpdate = (uint64)~0;	  // age of oldest entry
   SmacLastAccess currentUptime = 0;           // time since the system was booted
   int i;

   VNETKdPrintCall(("TrimLookupTableIfNecessary"));
   ASSERT(state);
   ASSERT(SMAC_HASH_TABLE_SIZE == 256);
   ASSERTLOCKHELD();

   if (state->numberOfIPandMACEntries <= 20) { // if not too many entries
      VNETKdPrint((MODULE_NAME "TrimLookupTableIfNeccessary: number of "
                   "entries is small: %u.\n", state->numberOfIPandMACEntries));
      return;
   }

   VNETKdPrint((MODULE_NAME "TrimLookupTableIfNecessary: "
		 "reducing # of entries\n"));
   
   currentUptime = GetSystemUptime(state); /* must be called with lock held */
   
   VNETKdPrint((MODULE_NAME "TrimLookupTableIfNecessary: current uptime is "
		LAST_ACCESS_FORMAT "\n", currentUptime));

   /*
    * NOTE: this code presumes that no system will ever be up long enough
    * for the uptime to wrap.  To get around this assumption we could try to
    * determine "oldest" by computing which entry has the largest difference
    * from the current updtime.  However, there are no guarantees that this
    * metric is any more accurate.  Given that we never expect more than 20
    * entries to ever exist, I won't implement a more sophisiticated 
    * mechanism at this time.
    */

   /*
    * Search thru entire table to find oldest uptime, and remove it
    */

   for (i = 0; i < SMAC_HASH_TABLE_SIZE; ++i) {
      IPmacLookupEntry *currentEntry = state->IPlookupTable[i];
      while (currentEntry) {
	 if (currentEntry->lastAccess < oldestUpdate) { // if older than candidate

	    /* 
	     * Skip cached entry since the entry was used most recently, but its
	     * timestamp might be old since the timestamp is only updated after
	     * the entry is no longer cached (i.e., something else is now the newest)
	     *
	     * This code will only be executed if at least 20 entries exist, so there
	     * must be some much better candidates elsewhere in the table.
	     */

	    if (currentEntry == state->lastEntryAdded) {
	       VNETKdPrint((MODULE_NAME "TrimLookupTableIfNecessary: oldest "
		  "candidate is the cached entry, so skipping\n"));
	    } else {
	       DEVEL_ONLY(char ipStr[IP_STRING_SIZE];)

	       VNETKdPrint((MODULE_NAME "TrimLookupTableIfNecessary: current "
	                    "oldest candidate: %s time " LAST_ACCESS_FORMAT "\n",
	                    LookupEntryPrintIPAddrToString(ipStr, sizeof ipStr,
	                                                   currentEntry),
	                    currentEntry->lastAccess));
	       oldestEntry = currentEntry;
	       oldestUpdate = currentEntry->lastAccess;
	    }
	 }
	 if (currentEntry->lastAccess > currentUptime) {
	    VNETKdPrint((MODULE_NAME "TrimLookupTableIfNecessary: "
			 "ERROR: last access " LAST_ACCESS_FORMAT 
			 " > current uptime " LAST_ACCESS_FORMAT "\n", 
			 currentEntry->lastAccess, currentUptime));
	 }
	 
	 currentEntry = currentEntry->ipNext;
      }
   }

   if (oldestEntry) {
      DEVEL_ONLY(char ipStr[IP_STRING_SIZE];)

      VNETKdPrint((MODULE_NAME "TrimLookupTableIfNecessary: found oldest "
                   "candidate: %s time " LAST_ACCESS_FORMAT "\n",
                   LookupEntryPrintIPAddrToString(ipStr, sizeof ipStr,
                                                  oldestEntry),
                   oldestEntry->lastAccess));
      if (!RemoveIPfromHashTableNoAcquireLock(state, oldestEntry)) {
	 VNETKdPrint((MODULE_NAME "TrimLookupTableIfNecessary: could not "
		      "find entry in IP table\n"));
	 ASSERT(0); // should never occur
      } else {
	 FREEMEMORY(oldestEntry);
	 --state->numberOfIPandMACEntries;
      }
   }
   else {
      VNETKdPrint((MODULE_NAME "TrimLookupTableIfNecessary: "
		   "found no entry to remove!!\n"));
   }

   VNETKdPrintReturn(("TrimLookupTableIfNecessary"));   
   return;
}


/*
 *----------------------------------------------------------------------
 *
 * SetCacheEntry --
 *
 *      Sets the cached MAC/IP entry for an adapter, and updates the
 *      access time for the previous cache entry (if any).  The cache
 *      is used to avoid the overhead of checking for the existance of,
 *      for the purposes of adding, a MAC/IP entry that has already
 *      been added recently.
 *
 *      Function is called with state->smacSpinLock held.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Modified the cached entry for the adapter, and updates the
 *      access time for the previous cache entry (if any)
 *
 *----------------------------------------------------------------------
 */

static INLINE void
SetCacheEntry(SMACState *state,         // IN: smac state
	      IPmacLookupEntry *entry)	// IN: entry to cache
{
   ASSERT(state);
   ASSERT(entry);

   /* 
    * Set new cached entry, but first set the current cache entry's
    * access time to the current time
    */

   if (state->lastEntryAdded) {
      state->lastEntryAdded->lastAccess = GetSystemUptime(state);
   }

   state->lastIPadded = entry->addrContainer;
   MEMCPY(state->lastMACadded, entry->mac, ETH_ALEN);
   state->lastEntryAdded = entry;
   entry->lastAccess = GetSystemUptime(state);
}


/*
 *----------------------------------------------------------------------
 *
 * AddIPandMACcombo --
 *
 *      Adds an entry for a paired MAC/IP{v4,v6} into the IP and MAC
 *      hash tables.
 *
 * Results:
 *      TRUE if added, updated, or already present, FALSE on error.
 *
 * Side effects:
 *      Allocates a new table entry and adds it to the IP and MAC
 *      hash tables.
 *
 *----------------------------------------------------------------------
 */

static Bool
AddIPandMACcombo(SMACState *state,                     // IN: smac state
                 const IPAddrContainer *addrContainer, // IN: IPv4/v6 address to add
                 uint8 *mac)                           // IN: ethernet MAC to add
{
   Bool result = TRUE;
   IPmacLookupEntry *entryIP = NULL;
   DEVEL_ONLY(char ipStr[IP_STRING_SIZE];)
   DEVEL_ONLY(char macStr[MAC_STRING_SIZE];)
   SPINLOCKINIT();

   /*
    * If the current IP/MAC is the same as the immediately prior add, then
    * return and don't bother to process this request.
    */

   VNETKdPrint((MODULE_NAME "AddIPMAC:  told to add %s %s\n", 
                ContainerPrintIPAddrToString(ipStr, sizeof ipStr,
                                             addrContainer),
                PrintMACAddrToString(macStr, sizeof macStr, mac)));

   ASSERTLOCKHELD();
   ASSERT(SMAC_HASH_TABLE_SIZE == 256);

   if (AddrContainersMatch(&state->lastIPadded, addrContainer) &&
       MAC_EQ(mac, state->lastMACadded)) {
      VNETKdPrint((MODULE_NAME "AddIPMAC: cache says already present\n"));
      return TRUE;
   }

   if (IsIPAddrContainerV4(addrContainer)) {
      uint32 addr = ContainerGetIPv4Addr(addrContainer);

      /*
       * Don't allow an IP of 0.0.0.0 or 255.255.255.255 to be added.  In fact,
       * consider deleting any existing MAC entry that was provided, since the
       * IP is apparently no longer in use.
       */

      if (!addr || addr == IP_ADDR_BROADCAST) {
         VNETKdPrint((MODULE_NAME "AddIPMAC: trying to add IP 0.0.0.0 or "
                      "255.255.255.255, disallowing add & removing MAC "
                      "entry.\n"));
         return TRUE;
      }
   } else {
      const IPv6Addr *addr = ContainerGetIPv6Addr(addrContainer);

      /*
       * Don't allow unspecified IPv6 addresses.  There is no IPv6 broadcast
       * address to check here.
       */

      if (addr->addrHi == CONST64U(0) && addr->addrLo == CONST64U(0)) {
         VNETKdPrint((MODULE_NAME "AddIPMAC:  trying to add unspecified IPv6 "
                     "address  ::/128, disallowing adding of MAC entry.\n"));
         return TRUE;
      }
   }

   ACQUIRESPINLOCK(&state->smacSpinLock);

   /* 
    * Lookup table entry for specified IP addr and ethernet MAC.
    */

   entryIP = LookupByIPNoAcquireLock(state, addrContainer);

   /*
    * If an entry for the specified IP addr was found, and the MACs match,
    * then we don't need to add a new entry nor modify any existing entries 
    * and can return immediately.
    */

   if (entryIP && MAC_EQ(entryIP->mac, mac)) {
      VNETKdPrint((MODULE_NAME "AddIPMAC: entry already exists, "
	           "and matches current IP/MAC\n"));

      /* 
       * Update the cached entry to this new request.
       */

      SetCacheEntry(state, entryIP);
      goto exit;
   }

   /*
    * If no table entry was found for the IP, then this is a completely new add 
    * (also, no changes need to made to pre-existing table entries).
    */

   if (!entryIP) {
      uint8 ipHash = IPAddrContainerHash(addrContainer);
      
      IPmacLookupEntry *entry = ALLOCATEMEMORY(sizeof *entry,
                                               REORDER_TAG('SMle'));
      VNETKdPrint((MODULE_NAME "AddIPMACnew:  neither MAC or IP is in table, "
                   "so adding new entry for %s %s\n",
                   ContainerPrintIPAddrToString(ipStr, sizeof ipStr,
                                                addrContainer),
                   PrintMACAddrToString(macStr, sizeof macStr, mac)));

      if (!entry) {
	 // entry allocation error
	 VNETKdPrint((MODULE_NAME "AddIPMACnew: Failed to allocate "
		      " MAC/IP entry\n"));
	 result = FALSE;
	 goto exit;
      }

      ++state->numberOfIPandMACEntries;

      // initialize the contents of the table entry
      LookupEntrySetIPAddrContainer(entry, addrContainer);
      entry->lastAccess = 0; /* initialize to 0 for sanity, although it's not vital */

      MEMCPY(entry->mac, mac, ETH_ALEN);

      // add entry to IP hash table
      entry->ipNext = state->IPlookupTable[ipHash];
      state->IPlookupTable[ipHash] = entry;

      VNETKdPrint((MODULE_NAME "AddIPMACnew: entry allocated, and added\n"));
      SetCacheEntry(state, entry);
      TrimLookupTableIfNecessary(state);

   } else {

      /*
       * If table entry was found for IP, but the MACs don't match, then this means
       * that a new/different ethernet device/MAC is using the IP address.  We need
       * to update the contents of the table entry to specify the new MAC
       */

      VNETKdPrint((MODULE_NAME "AddIPMACmacmod: IP has changed from known "
	           "MAC %02x:%02x:%02x:%02x:%02x:%02x to new unknown "
		   "MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
		   entryIP->mac[0]&0xff, entryIP->mac[1]&0xff, 
		   entryIP->mac[2]&0xff, entryIP->mac[3]&0xff, 
		   entryIP->mac[4]&0xff, entryIP->mac[5]&0xff,
		   mac[0]&0xff, mac[1]&0xff, mac[2]&0xff, mac[3]&0xff, 
		   mac[4]&0xff, mac[5]&0xff));

      // update MAC in the table entry
      MEMCPY(entryIP->mac, mac, ETH_ALEN);
      
      // update which was the last IP/MAC combo to be added
      SetCacheEntry(state, entryIP);
      // no new entry added, so no need to call TrimLookupTableIfNecessary()
   }

exit:

   RELEASESPINLOCK(&state->smacSpinLock);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * AddIPv4andMACcombo --
 * AddIPv6andMACcombo --
 *
 *      Helper wrapper functions for AddIPandMACcombo() for the IPv4/v6
 *      cases.
 *
 * Results:
 *      As in AddIPandMACcombo().
 *
 * Side effects:
 *      As in AddIPandMACcombo().
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
AddIPv4andMACcombo(SMACState *state, // IN:
                   uint32 ipv4Addr,  // IN:  IPv4 address to add
                   uint8 *mac)       // IN:  ethernet MAC to add
{
   IPAddrContainer addrContainer;

   ContainerSetIPv4Addr(&addrContainer, ipv4Addr);
   return AddIPandMACcombo(state, &addrContainer, mac);
}

static INLINE Bool
AddIPv6andMACcombo(SMACState *state,
                   const IPv6Addr *ipv6Addr, // IN:  IPv6 address to add
                   uint8 *mac)               // IN:  ethernet MAC to add
{
   IPAddrContainer addrContainer;

   ContainerSetIPv6Addr(&addrContainer, ipv6Addr);
   return AddIPandMACcombo(state, &addrContainer, mac);
}


/*
 * Overview of functions:
 *
 * "LookupTypeClass" and "LookupTypeName" are used to identify an 
 * ethernet frame's type, esentially IP, ARP, or neither.
 * "LookupTypeName" is used for debugging purposes, since it also
 * returns a string representing the name of the type.
 *
 * When the bridge wishes to send a packet to the host/network, it
 * calls "SMAC_CheckPacketToHost", which essentially handles the
 * link layer and ARP.  If the packet is IPv4 then it calls 
 * ProcessOutgoingIPv4Packet to analyze (and potentially modify)
 * the packet.  Currently ProcessOutgoingIPv4Packet only turns on
 * the broadcast flag for DHCP client packets.
 *
 * When the bridge receives a packet from the host/network, it
 * calls "SMAC_CheckPacketFromHost", which essentially handles the
 * link layer and ARP.  If the packet is IPv4 then it calls 
 * ProcessIncomingIPv4Packet to analyze (and potentially modify)
 * the packet.
 *
 * On Linux, the packet modifications are made to a private clone
 * of the network packet.  We don't want to modify a packet from
 * the host, nor do we want to make modifications in a way that's
 * visible to VMs on the same subnet.
 *
 * On Windows, we only clone packets that are headed towards the host.
 * Packets from the host can be modified since they're already a private
 * copy.  However, we might not have the whole packet and thus need to
 * store the changes in a separate table until the whole packet is
 * received (at which time the modifications can then be made).
 *
 * On Mac OS, the packet modifications are made to the original network packet,
 * since the packet passed to us is already pre-cloned and private and can be
 * modified here. So cloning is faked by simply setting the clone to be a
 * pointer to the original packet.
 */


/*
 *----------------------------------------------------------------------
 *
 * SMAC_CheckPacketFromHost --
 *
 *      Examines the contents of a packet that has been received from
 *      the network.  On Windows the function will provide suggestions 
 *      (via table) for where MAC substitution should occur.
 *      Function supports IP, ARP, RARP, IARP, and DHCP.  On Linux the
 *      function will clone the packet and make modifications to the
 *      clone (the caller is responsible for freeing the original and
 *      the cloned packets).
 *
 *      NOTE: On Windows this function presumes it is called at DISPATCH_LEVEL
 *
 * Results:
 *      Returns 'PacketStatusForwardPacket' if packet should/can be 
 *      received, 'PacketStatusDropPacket' if packet should be filtered 
 *      and not received, PacketStatusTooShort if insufficient data 
 *      provided to process packet (suggested action it to receive 
 *      packet in its entirety, and call this function
 *      again on the whole packet)
 *
 * Side effects:
 *      May add/modify the adapter's MAC/IP hash tables.  May clone a
 *      packet if function returns PacketStatusForwardPacket.
 *
 *----------------------------------------------------------------------
 */

PacketStatus SMACINT
SMAC_CheckPacketFromHost(SMACState *state,       // IN: pointer to state
			 SMACPackets *packets)   // IN/OUT: packet to process
{
   EthernetHeader eh;
   EthClass typeClass;
   SMACPacket *packet = NULL; // original packet from host

   ASSERT(state);
   ASSERT(packets);

   WW_VNETKdPrint((MODULE_NAME "FromHost: Called\n"));

   ASSERTLOCKHELD();

#ifdef _WIN32
   {
      MacReplacementTable *macTable = packets->table;
      ASSERT(macTable);
      macTable->numOfOffsets = 0;
   }
#endif /* _WIN32 */

   packet = &(packets->orig);

   /*
    * Read in the Ethernet header, and return failure if this
    * is a runt packet that doesn't have a whole header
    */

   ASSERT(sizeof eh == ETH_HLEN);
   if (!GetPacketData(packet, 0, sizeof eh, &eh)) {
      VNETKdPrint((MODULE_NAME "FromHost: Packet missing eth header\n"));
      return PacketStatusDropPacket; // instruct bridge to drop this runt packet
   }

#ifdef __linux__
   /* 
    * Reject the duplicate packet (occurs only in Infrastructure mode)
    *
    * When the vm is communicating with the host, the host arp table 
    * would have the vm's mac address same as the physical hw address.
    * so when SMAC_PacketFromHost is called it will create a duplicate of 
    * the packet and let the original pass as is, this original packet 
    * would then be transmitted on the network, where the AP would return
    * it back to us because it matches the hardware address , this is the 
    * duplicate packet we are talking about.
    */
   if (SMACL_IsSkbHostBound(packet->skb) && 
       MAC_EQ(state->macAddress, eh.srcAddr)) {
      W_VNETKdPrint((MODULE_NAME "FromHostIP: incoming request has "
                     "same mac as destination, so blackholing\n"));
      return PacketStatusDropPacket;
   }
#endif

   /*
    * Lookup the ethernet media type of the packet
    */

   typeClass = LookupTypeClass(NTOHS(eh.lengthType));

   /*
    * For reference, the VLAN support was removed since it was determined that
    * for Windows, such information would be present only in the OOB area of a
    * NDIS packet, and thus no specific support/handling for VLAN is required
    * for that OS.
    *
    * For any other OS, we need to add support to the vmnet driver for that OS
    * to allow VLAN tagged frames.
    */

#if 0

   /*
    * If broadcast, then allow packet with no further checks.  We might be
    * able to actually enable this code, depending on how we want to handle
    * DHCP replies.  Currently DHCP replies don't need to be processed, so
    * it's probably okay to enable this.  Currently it's disabled so that
    * the debug statements can continue to give us information about each 
    * packet.
    */
   
   if (IS_MULTICAST(eh.destAddr) || IS_BROADCAST(eh.destAddr)) { 
      return PacketStatusForwardPacket;
   }

#endif /* 0 */

   /*
    * DEBUG: if not IP & not ARP & not EAPOL
    */

   if (typeClass != EthClassIPv4 && typeClass != EthClassIPv6 &&
       typeClass != EthClassARP && typeClass != EthClassEAPOL) {

      /* 
       * If not a common/known media type, then print a status 
       * message and return
       */

#ifdef DBG
      if (typeClass != EthClassCommon) { // print only if not a common type
         char type[50] = ""; // holds textual name of type
         LookupTypeName(NTOHS(eh.lengthType), type, sizeof type); // lookup ethernet type
         VNETKdPrint((MODULE_NAME "FromHost: non-IP & non-ARP %s "
                      "%02x:%02x:%02x:%02x:%02x:%02x -> "
                      "%02x:%02x:%02x:%02x:%02x:%02x %s\n",
                      (IS_MULTICAST(eh.destAddr) || IS_BROADCAST(eh.destAddr))?
                      "(b|m)cast":"ucast",
                      eh.srcAddr[0], eh.srcAddr[1], eh.srcAddr[2],
                      eh.srcAddr[3], eh.srcAddr[4], eh.srcAddr[5],
                      eh.destAddr[0], eh.destAddr[1], eh.destAddr[2],
                      eh.destAddr[3], eh.destAddr[4], eh.destAddr[5], type));
      }
#endif /* DBG */

      /*
       * Let these unrecognized packets through only if they are broadcast or
       * multicast. Drop unicast packets because it's easier to debug a lack
       * of traffic than damaged traffic.
       */

      if (IS_MULTICAST(eh.destAddr) || IS_BROADCAST(eh.destAddr)) {
         VNETKdPrint((MODULE_NAME "FromHost: Forward unrecognized "
                      "non-arp/ip b/mcast\n"));
         return PacketStatusForwardPacket;
      } else {
#ifdef DBG
         char type[50] = ""; // holds textual name of type
         LookupTypeName(NTOHS(eh.lengthType), type, sizeof type);
         VNETKdPrint((MODULE_NAME "FromHost: Dropping unrecognized "
                      "unicast non-IP & non-ARP unicast packet: %s\n", type));
         VNETKdPrint((MODULE_NAME "FromHost: the non-IP & non-ARP is "
                      "%02x:%02x:%02x:%02x:%02x:%02x -> "
                      "%02x:%02x:%02x:%02x:%02x:%02x\n",
                      eh.srcAddr[0], eh.srcAddr[1], eh.srcAddr[2],
                      eh.srcAddr[3], eh.srcAddr[4], eh.srcAddr[5],
                      eh.destAddr[0], eh.destAddr[1], eh.destAddr[2],
                      eh.destAddr[3], eh.destAddr[4], eh.destAddr[5]));
#endif
         /*
          * Drop non-IP/non-ARP unicast packets, unless we've been
          * requested to forward unknown/unrecognized packets.
          */

         if (state->smacForwardUnknownPackets) {
            return PacketStatusForwardPacket;
         } else {
            return PacketStatusDropPacket;
         }
      }
   }

   /*
    * If IP packet, then lookup ethernet MAC based on dest IP and replace 
    * dest ethernet MAC with that of the lookup table entry
    */

   if (typeClass == EthClassIPv4 || typeClass == EthClassIPv6) { // IP packet
      IPAddrContainer addrContainer;

      /*
       * First, we do some IP version dependent parsing (checking basic fields
       * in the packet, extracting destination IP address, etc.).  Then we
       * perform the common processing for IPv4/IPv6 packets (finding
       * destination MAC address from SMAC hash table, replacing destination
       * MAC in the packet --- the wireless PNIC's MAC address --- with the MAC
       * address of the VM's VNIC.
       */

      if (typeClass == EthClassIPv4) { // IPv4
         uint8  ipHeader[IP_HEADER_LEN];   // IP header
         uint32 ipVer;                     // IP header ver
         uint32 ipHeaderLen;               // IP header length 

         /* Verify that we have at least a whole, minimal IP header */
         if (!GetPacketData(packet, ETH_HLEN, sizeof ipHeader, ipHeader)) {
            VNETKdPrint((MODULE_NAME "FromHostIP: Got type IP, "
                         "but only have partial IP header\n"));
            return PacketStatusTooShort;
         }

         ipVer = ipHeader[0] >> 4;              // IP header ver
         ipHeaderLen = 4 * (ipHeader[0] & 0xf); // IP header length

         /*
          * Verify basic fields in IP header
          */

         if (ipVer != 4 || ipHeaderLen < 20 ||
             GetPacketLength(packet) - ETH_HLEN < ipHeaderLen) {
            VNETKdPrint((MODULE_NAME "FromHostIP: got an IP version %u, "
                         "or len %u < reported len %u\n", ipVer,
                         GetPacketLength(packet) - ETH_HLEN, ipHeaderLen));
            if ((GetPacketLength(packet) - ETH_HLEN) < ipHeaderLen) {

               /*
                * insufficient data -- process anyway since we have the necessary
                * amount of information (first 20 bytes) to process this packet?
                */

               VNETKdPrint((MODULE_NAME "FromHostIP: got an IP "
                                 "that's too short\n"));
               return PacketStatusTooShort; 
            } else {
               VNETKdPrint((MODULE_NAME "FromHostIP: got an IP "
                            "that's unrecognised\n"));
               return PacketStatusDropPacket; // invalid/unrecognized data
            }
         }

         /*
          * Extract the IPv4 destination address.
          */

         ContainerSetIPv4Addr(&addrContainer,
                              *((uint32 *)(ipHeader +
                                           IP_HEADER_DEST_ADDR_OFFSET)));
      } else { // IPv6
         uint8 ipv6Header[IPv6_HEADER_LEN];
         uint8 ipv6Ver;
         IPv6Addr ipv6DstAddr;

         if (!GetPacketData(packet, ETH_HLEN, sizeof ipv6Header, ipv6Header)) {
            VNETKdPrint((MODULE_NAME "FromHostIP: Got type IPv6, but only have "
                         "partial IPv6 header.\n"));
            return PacketStatusTooShort;
         }

         ipv6Ver = ipv6Header[0] >> 4;
         if (ipv6Ver != IPv6 || GetPacketLength(packet) < ETH_HLEN +
                                                          IPv6_HEADER_LEN) {
            VNETKdPrint((MODULE_NAME "FromHostIP:  got an IP version %u, or "
                         "length %u less than minimum length %d.\n", ipv6Ver,
                         GetPacketLength(packet), ETH_HLEN + IPv6_HEADER_LEN));
            return PacketStatusDropPacket;
         }

         /*
          * Extract the IPv6 destination address.
          */

         ipv6DstAddr.addrHi = *((uint64 *)(ipv6Header +
                                           IPv6_HEADER_DST_ADDR_OFFSET));
         ipv6DstAddr.addrLo = *((uint64 *)(ipv6Header +
                                           IPv6_HEADER_DST_ADDR_OFFSET +
                                           sizeof ipv6DstAddr.addrHi));
         ContainerSetIPv6Addr(&addrContainer, &ipv6DstAddr);
      }

      /*
       * Broadcast/multicast processing: don't modify dest MAC but check if the 
       * payload should be modified.
       *
       * Unicast processing: modify MAC (if packet destined for VM) and check if
       * the payload should be modified
       */

      if (IS_MULTICAST(eh.destAddr) || IS_BROADCAST(eh.destAddr)) {
    
         /*
          * Check if payload should be modified.
          */

         /* 
          * NOTE: ProcessIncomingIPv4Packet doesn't currently modify packets,
          * so in an actual product release we can remove this call.
          *
          * TODO: investigate if there is work to be done here for IPv6 case.
          */

#ifdef DBG
         if (typeClass == EthClassIPv4) {
            ProcessIncomingIPv4Packet(packet, FALSE); 
         }
#endif
         W_VNETKdPrint((MODULE_NAME "FromHostIP: forward b/mcast IP\n"));
         return PacketStatusForwardPacket;
      } else { // unicast ethernet
         Bool foundMac;
         uint8 vmMacAddress[ETH_ALEN];
         DEVEL_ONLY(char ipStr[IP_STRING_SIZE];)

         /*
          * For unicast IP processing: lookup MAC based on IP addr
          */

         foundMac = LookupByIP(state, &addrContainer, vmMacAddress);

         if (!foundMac && 
             AddrContainerMatchIPv4Address(&addrContainer, IP_ADDR_BROADCAST)) {
            /*
             * If IPv4 destination address is the IP limited broadcast address
             * '255.255.255.255', then SMAC the unicast ethernet MAC address into
             * the ethernet broadcast MAC address 'ff:ff:ff:ff:ff:ff' and forward
             * the packet.
             *
             * Certain DHCP Servers/Relay Agents choose to ignore the
             * recommendations of RFC 1542 "Clarifications and Extensions for the
             * Bootstrap Protocol" section 4.1.2, and send DHCP Offers/ACKs to a
             * unicast ethernet address in response to DHCP Discovers/Requests
             * with the "Broadcast" flag set. See PRs 224129, 172947.
             *
             * Other than such packets, there shouldn't be any other IPv4 packets
             * sent to the IP limited broadcast address but not to the ethernet
             * broadcast address, but even if there are, we'll forward such
             * packets onto the VNet and let the guests decide what to do about
             * them.
             */

            ASSERT(typeClass == EthClassIPv4);
            MEMSET(vmMacAddress, 0xFF, ETH_ALEN);
            foundMac = TRUE;
         }

         if (foundMac) { // if IP is known on VNet, then substitute MAC
            W_VNETKdPrint((MODULE_NAME "FromHostIP: would assign MAC"
                           " of IP %s to packet\n",
                           ContainerPrintIPAddrToString(ipStr, sizeof ipStr,
                                                        &addrContainer)));

            /*
             * Eth dest MAC needs to be corrected.
             */

            if (!CopyDataForPacketFromHost(packets, 0, 0, vmMacAddress)) {
               VNETKdPrint((MODULE_NAME "FromHostIP: couldn't clone packet\n"));
               return PacketStatusDropPacket;
            }

            /*
             * NOTE: ProcessIncomingIPv4Packet doesn't currently modify packets,
             * so in an actual product release we can remove this call.
             */

#ifdef DBG
            if (typeClass == EthClassIPv4) {
               ProcessIncomingIPv4Packet(packet, foundMac);
            }
#endif /* DBG */
            W_VNETKdPrint((MODULE_NAME "FromHostIP: forward IP\n"));
            return PacketStatusForwardPacket;
         } else { // IP is unknown on VNet
            VNETKdPrint((MODULE_NAME "FromHostIP: could not find IP "
                         "%s in lookup.\n",
                         ContainerPrintIPAddrToString(ipStr, sizeof ipStr,
                                                      &addrContainer)));
            if (state->smacForwardUnknownPackets) {
               return PacketStatusForwardPacket;
            } else {
               return PacketStatusDropPacket; // drop packet
            }
         } // end IP addr lookup
      } // end unicast
   } // end IP

   /*
    * If ARP packet, then lookup ethernet MAC based on dest IP and replace 
    * dest ethernet MAC (and potentially ARP MAC) with that of the lookup table entry
    */

   else if (typeClass == EthClassARP) { // ARP packet
      uint32 arpHeaderWord1; // first word in ARP header
      uint32 arpHeaderWord2; // second word in ARP header

      if (GetPacketLength(packet) < ETH_HLEN + ARP_HEADER_LEN) {
         VNETKdPrint((MODULE_NAME "FromHostARP: ARP packet is insufficient "
                     "length of IPv4 and Ethernet, expected %d got %u\n",
                     ETH_HLEN + ARP_HEADER_LEN, GetPacketLength(packet)));
         return PacketStatusTooShort;
      }

      /*
       * Verify the first word of ARP header (hardcoded for ethernet and IPv4)
       *
       * I recently added IEEE802 support.  These types of ARP requests were
       * observed on the company network, so someone uses them.  As long as
       * the address lengths are the same then I imagine that the processing
       * is identical and we can handle them (lengths are checked as part
       * of processing the second word of ARP header).
       */
      
      if (!GetPacketData(packet, ETH_HLEN, sizeof arpHeaderWord1, 
                         &arpHeaderWord1)) {
         VNETKdPrint((MODULE_NAME "FromHostARP: couldn't read "
                      "ARP header #1\n"));
         return PacketStatusTooShort;
      }

      if (arpHeaderWord1 != HTONL(0x00010800) /* ethernet */ &&
         arpHeaderWord1 != HTONL(0x00060800) /* ieee802  */ ) {
         VNETKdPrint((MODULE_NAME "FromHostARP: ARP header1 appears "
                      "wrong, got %08x\n", arpHeaderWord1));
         return PacketStatusDropPacket;
      }

      /*
       * Perform action based on opcode in second word of ARP header.
       */

      if (!GetPacketData(packet, ETH_HLEN + sizeof arpHeaderWord1,
          sizeof arpHeaderWord2, &arpHeaderWord2)) {
         VNETKdPrint((MODULE_NAME "FromHostARP: couldn't read "
                      "ARP header #2\n"));
         return PacketStatusTooShort;
      }

#ifdef DBG

      /*
       * DEBUG: print general information about the packet
       */

      switch (arpHeaderWord2) {
	 case CONST_NTOHL(0x06040001):
	    WW_VNETKdPrint((MODULE_NAME "FromHostARP: "	       
			  "ARP header2 indicates ARP request\n"));
	    break;
	 case CONST_NTOHL(0x06040002):
	    W_VNETKdPrint((MODULE_NAME "FromHostARP: "
			 "ARP header2 indicates ARP reply\n"));
	    break;
	 case CONST_NTOHL(0x06040003):
	    VNETKdPrint((MODULE_NAME "FromHostARP: "
			 "ARP header2 indicates RARP request\n"));
	    break;
	 case CONST_NTOHL(0x06040004):
	    VNETKdPrint((MODULE_NAME "FromHostARP: "
			 "ARP header2 indicates RARP reply\n"));
	    break;
	 case CONST_NTOHL(0x06040008):
	    VNETKdPrint((MODULE_NAME "FromHostARP: "
			 "ARP header2 indicates IARP request\n"));
	    break;
	 case CONST_NTOHL(0x06040009):
	    VNETKdPrint((MODULE_NAME "FromHostARP: "
			 "ARP header2 indicates IARP reply\n"));
	    break;
	 default:
	    VNETKdPrint((MODULE_NAME "FromHostARP: "
			 "ARP header2 indicates unknown opcode\n"));
	    break;
      }

      {
	 uint8 data[ETH_ALEN + 4] = {0}; // MAC and IP in one contiguous buffer
	 if (GetPacketData(packet, ETH_HLEN + ARP_SENDER_MAC_OFFSET, 
			   sizeof data, data)) {
	    WW_VNETKdPrint((MODULE_NAME "FromHostARP: sender MAC is "
	       "%02x:%02x:%02x:%02x:%02x:%02x IP is %d.%d.%d.%d\n",
	       ((unsigned char*)data)[0]&0xff, ((unsigned char*)data)[1]&0xff,
	       ((unsigned char*)data)[2]&0xff, ((unsigned char*)data)[3]&0xff,
	       ((unsigned char*)data)[4]&0xff, ((unsigned char*)data)[5]&0xff, 
	       ((unsigned char*)data)[6]&0xff, ((unsigned char*)data)[7]&0xff, 
	       ((unsigned char*)data)[8]&0xff, ((unsigned char*)data)[9]&0xff));
	 } else {
	    VNETKdPrint((MODULE_NAME "FromHostARP: couldn't read "
			  "sender MAC/IP\n"));
	 }
      
	 if (GetPacketData(packet, ETH_HLEN + ARP_TARGET_MAC_OFFSET, 
			   sizeof data, data)) {
	    WW_VNETKdPrint((MODULE_NAME "FromHostARP: target MAC is "
	       "%02x:%02x:%02x:%02x:%02x:%02x IP is %d.%d.%d.%d\n",
	       ((unsigned char*)data)[0]&0xff, ((unsigned char*)data)[1]&0xff, 
	       ((unsigned char*)data)[2]&0xff, ((unsigned char*)data)[3]&0xff, 
	       ((unsigned char*)data)[4]&0xff, ((unsigned char*)data)[5]&0xff, 
	       ((unsigned char*)data)[6]&0xff, ((unsigned char*)data)[7]&0xff, 
	       ((unsigned char*)data)[8]&0xff, ((unsigned char*)data)[9]&0xff));
	 } else {
	    VNETKdPrint((MODULE_NAME "FromHostARP: couldn't read "
			 "target MAC/IP\n"));
	 }
      }

#endif

      /*
       * ARP handling for *incoming traffic*
       *
       * ARP: host wants to know the MAC that corresponds to a particular IP
       * 1 ARP request:  <srcMAC, srcIP, 0,      dstIP>
       *   ALLOW CONDITIONALLY: if dest eth mac broadcast then nothing to modify,
       *                        otherwise need to patch ucast dest eth mac.
       * 2 ARP reply:    <srcMAC, srcIP, dstMAC, dstIP>
       *   ALLOW IF BROADCAST: broadcast so nothing to modify (except ARP dest MAC?)
       *   ALLOW IF LOOKUP: lookup dstIP and mofify dstMAC (&ethDestMAC) to match VM
       *
       * RARP: host knows its MAC and wants to find out which IP it is assigned
       * 3 RARP request: <srcMAC, 0    , srcMAC, 0    >  can't store <MAC,IP>, 
       *   ALLOW CONDITIONALLY: if dest eth mac broadcast then nothing to modify,
       *                        otherwise need to patch ucast dest eth mac.
       * 4 RARP reply:   <srcMAC, srcIP, dstMAC, dstIP>
       *   ALLOW IF BROADCAST: broadcast so nothing to modify (except ARP dest MAC?)
       *   ALLOW IF LOOKUP: lookup dstIP and modify dstMAC (&ethDestMAC) to match VM
       *
       * IARP: host knows a peer's MAC and wants to determine its IP address
       * 8 IARP request: <srcMAC, srcIP, dstMAC, 0    >
       *   ALLOW CONDITIONALLY: if dest eth mac broadcast then nothing to modify,
       *                        otherwise need to patch ucast dest eth mac.
       * 9 IARP reply:   <srcMAC, srcIP, dstMAC, dstIP>
       *   ALLOW IF BROADCAST: broadcast so nothing to modify (except ARP dest MAC?)
       *   ALLOW IF LOOKUP: lookup dstIP and modify dstMAC (&ethDestMAC) to match VM
       */

      if (arpHeaderWord2 == HTONL(0x06040001) ||  // If ARP request
	  arpHeaderWord2 == HTONL(0x06040003) ||  // If RARP request
	  arpHeaderWord2 == HTONL(0x06040008)) {  // If IARP request

	 uint32 targetAddr;
	 uint32 sourceAddr;
	 Bool foundIP;

	 if (!GetPacketData(packet, ETH_HLEN + ARP_TARGET_IP_OFFSET, 
			    sizeof targetAddr, &targetAddr) ||
	     !GetPacketData(packet, ETH_HLEN + ARP_SENDER_IP_OFFSET, 
			    sizeof sourceAddr, &sourceAddr)) {
	    VNETKdPrint((MODULE_NAME "FromHostARP: "
			 "couldn't read target and/or sourceAddr\n"));
	    return PacketStatusTooShort;
	 }

	 /*
	  * Some host configurations will require allowing packet loopback, meaning
	  * that we'll see the packets that we're transmitting.  Since we're performing
	  * MAC tricks, the guest could see its own ARP request--this echoed ARP request
	  * will contain the host's wireless MAC, and thus the guest OS might think that
	  * there is a address conflict.  Thus, if we receive an ARP request whose source
	  * IP address is a known VM (or 0.0.0.0, as commonly used by Vista-and-later),
	  * and the MAC is the wireless hardware, then the ARP request is dropped.
	  * This will allow address conflicts to be detected when a conflict
	  * exists with another device, but it prevents conflict detection when
	  * the conflict is present on/within the host.
	  */

	 foundIP = sourceAddr == 0 || LookupByIPv4(state, sourceAddr, NULL);
	 if (foundIP) { // if known IP is contained within request
	    uint8 packetMac[ETH_ALEN];
	    VNETKdPrint((MODULE_NAME "FromHostARP: observed a "
	                 "incoming request from an IP we know about\n"));

	    if (!GetPacketData(packet, ETH_HLEN + ARP_SENDER_MAC_OFFSET, 
			       sizeof packetMac, packetMac)) {
	       VNETKdPrint((MODULE_NAME "FromHostARP: "
			    "couldn't read MAC\n"));
	       return PacketStatusTooShort;
	    }

	    /*
	     * Check if the sender MAC is using the wireless MAC
	     */

	    if (MAC_EQ(state->macAddress, packetMac)) {

	       /*
	        * Sender is using wireless MAC, so drop packet 
		* -- the reasons for this are stated above in the large comment block.
	        */

	       VNETKdPrint((MODULE_NAME "FromHostARP: incoming request using "
	                    "wireless hardware addr, so blackholing\n"));
	       return PacketStatusDropPacket;
	    }
	    /*
	     * Requester IP matches a VM IP, but the source MAC is different.
	     * This is apparently a true conflict with another network peer, so
	     * we'll forward the packet 
	     */
	    VNETKdPrint((MODULE_NAME "FromHostARP: incoming request using "
			 "non-wireless-hardware-addr, may conflict--allowing\n"));
	 } else {
	    /*
	     * We don't have information about the source (typical case), 
             * so forward the packet.
	     */
	    WW_VNETKdPrint((MODULE_NAME "FromHostARP: got ARP, no record of "
			    "source so forward\n"));
	 }

	 if (IS_MULTICAST(eh.destAddr) || IS_BROADCAST(eh.destAddr)) {
	    /* dest mac is broacast, so can just forward */
	    return PacketStatusForwardPacket;
	 } else if (!MAC_EQ(state->macAddress, eh.destAddr)) {
	    VNETKdPrint((MODULE_NAME "FromHostARP: incoming request using "
			 "non-wireless-hardware-addr eth dest MAC, dropping\n"));
	    return PacketStatusDropPacket;
	 } else {
	    Bool  foundMac;
	    uint8 vmMacAddress[ETH_ALEN];

	    /* 
	     * Someone sent request to VM with the host's destination MAC,
	     * so need to patch this with the VM's MAC so the VM can reply.
	     */
	    
	    /*
	     * Unicast destination--lookup MAC in table.  If entry exists then 
	     * do MAC replacement and return.  Otherwise, drop the packet
	     */
	    
	    foundMac = LookupByIPv4(state, targetAddr, vmMacAddress);
	    if (!foundMac) {
	       W_VNETKdPrint((MODULE_NAME "FromHostARP: target IP in request ARP was"
			      " NOT found in table, presuming for another peer\n"));
	       return PacketStatusDropPacket;
	    }
	    
	    VNETKdPrint((MODULE_NAME "FromHostARP: target IP in "
			 "request ARP was found in table\n"));
	    
#ifdef DBG
	    {
	       uint8 packetMac[ETH_ALEN];
	       if (!GetPacketData(packet, ETH_HLEN + ARP_TARGET_MAC_OFFSET, 
				  sizeof packetMac, packetMac)) {
		  VNETKdPrint((MODULE_NAME "FromHostARP: "
			       "couldn't read MAC\n"));
		  return PacketStatusTooShort;
	       }
	       W_VNETKdPrint((MODULE_NAME "FromHostARP: will modify request "
			      "ARP ETH MAC %02x.%02x.%02x.%02x.%02x.%02x "
			      "dest address and ARP target address "
			      "%02x.%02x.%02x.%02x.%02x.%02x to "
			      "match VM %02x.%02x.%02x.%02x.%02x.%02x\n",
			      eh.destAddr[0], eh.destAddr[1], eh.destAddr[2], 
			      eh.destAddr[3], eh.destAddr[4], eh.destAddr[5],
			      packetMac[0], packetMac[1], packetMac[2], 
			      packetMac[3], packetMac[4], packetMac[5],  
			      vmMacAddress[0], vmMacAddress[1], vmMacAddress[2], 
			      vmMacAddress[3], vmMacAddress[4], vmMacAddress[5]));
	    }
#endif
	    /*
	     * Copy in the MAC that will be written to eth dest MAC.
	     */
	    
	    if (!CopyDataForPacketFromHost(packets, 0, 0, vmMacAddress)) {
	       W_VNETKdPrint((MODULE_NAME "FromHostIP: couldn't clone request packet\n"));
	       return PacketStatusDropPacket;
	    }
	    return PacketStatusForwardPacket;
	 }
      }

      if (arpHeaderWord2 == HTONL(0x06040002) ||  // If ARP reply
	  arpHeaderWord2 == HTONL(0x06040004) ||  // If RARP reply
	  arpHeaderWord2 == HTONL(0x06040009)) {  // If IARP reply
	 uint32 targetAddr;
	 Bool foundMac;
	 uint8 vmMacAddress[ETH_ALEN];

	 if (!GetPacketData(packet, ETH_HLEN + ARP_TARGET_IP_OFFSET, 
			    sizeof targetAddr, &targetAddr)) {
	    VNETKdPrint((MODULE_NAME "FromHostARP: couldn't target IP\n"));
	    return PacketStatusTooShort;
	 }

	 /*
	  * If broadcast then return because we don't need to modify anything
	  */

	 if (IS_MULTICAST(eh.destAddr) || IS_BROADCAST(eh.destAddr)) {
	    VNETKdPrint((MODULE_NAME "FromHostARP: observed a "
	                 "broadcast ARP, RARP, or IARP reply packet\n"));

	    /* 
	     * Don't need to make changes since target MAC is the broadcast addr
	     */

	    /*
	     * Ethernet MAC is broadcast, but the dest ARP address might
	     * be unicast (in fact, it likely is), so we should patch the
	     * destination ARP MAC address??
	     */

	    foundMac = LookupByIPv4(state, targetAddr, vmMacAddress);
	    if (foundMac) {
	       uint8 packetMac[ETH_ALEN];

	       VNETKdPrint((MODULE_NAME "FromHostARP: target IP in "
	                    "broadcast reply ARP was found in table\n"));
	       
	       if (!GetPacketData(packet, ETH_HLEN + ARP_TARGET_MAC_OFFSET, 
				  sizeof packetMac, packetMac)) {
		  VNETKdPrint((MODULE_NAME "FromHostARP: "
			       "couldn't read MAC\n"));
		  return PacketStatusTooShort;
	       }

	       W_VNETKdPrint((MODULE_NAME "FromHostARP: will modify reply "
			      "ARP target address %02x.%02x.%02x.%02x.%02x.%02x to "
			      "match VM %02x.%02x.%02x.%02x.%02x.%02x\n",
			      packetMac[0], packetMac[1], packetMac[2], 
			      packetMac[3], packetMac[4], packetMac[5], 
			      vmMacAddress[0], vmMacAddress[1], vmMacAddress[2], 
			      vmMacAddress[3], vmMacAddress[4], vmMacAddress[5]));
	       
               /*
		* Copy in the MAC that will be written to ARP dest MAC
	        */

	       if (!CopyDataForPacketFromHost(packets, 0, 
				              ETH_HLEN + ARP_TARGET_MAC_OFFSET,
                                              vmMacAddress)) {
		  VNETKdPrint((MODULE_NAME "FromHostIP: couldn't "
			       "clone packet\n"));
		  return PacketStatusDropPacket;
	       }
	       return PacketStatusForwardPacket;
	    } else {
	       W_VNETKdPrint((MODULE_NAME "FromHostARP: target IP in broadcast "
			      "reply ARP was NOT found in table\n"));
	       return PacketStatusForwardPacket; 
	    }
	 }

	 W_VNETKdPrint((MODULE_NAME "FromHostARP: target IP is %d.%d.%d.%d\n", 
			(targetAddr)&0xff, (targetAddr>>8)&0xff, 
			(targetAddr>>16)&0xff, (targetAddr>>24)&0xff));
#ifdef DBG
	 {
	    uint32 senderAddr;
	    if (!GetPacketData(packet, ETH_HLEN + ARP_SENDER_IP_OFFSET, 
			       sizeof senderAddr, &senderAddr)) {
	       VNETKdPrint((MODULE_NAME "FromHostARP: couldn't "
			    "read IP addr\n"));
	       return PacketStatusTooShort;
	    }
	    W_VNETKdPrint((MODULE_NAME "FromHostARP: sender IP %d.%d.%d.%d\n", 
	                   (senderAddr&0xff), (senderAddr>>8)&0xff, 
	                   (senderAddr>>16)&0xff, (senderAddr>>24)&0xff));
	 }
#endif

	 /*
	  * Unicast destination--lookup MAC in table.  If entry exists then 
	  * do MAC replacement and return.  Otherwise, drop the packet
	  */

	 // see if the reply is for a host that we're aware of, and modify as necessary
	 foundMac = LookupByIPv4(state, targetAddr, vmMacAddress);
	 if (foundMac) {
	    VNETKdPrint((MODULE_NAME "FromHostARP: target IP in "
	                 "reply ARP was found in table\n"));

#ifdef DBG
	    {
	       uint8 packetMac[ETH_ALEN];
	       if (!GetPacketData(packet, ETH_HLEN + ARP_TARGET_MAC_OFFSET, 
				  sizeof packetMac, packetMac)) {
		  VNETKdPrint((MODULE_NAME "FromHostARP: "
			       "couldn't read MAC\n"));
		  return PacketStatusTooShort;
	       }
	       W_VNETKdPrint((MODULE_NAME "FromHostARP: will modify reply "
			      "ARP ETH MAC %02x.%02x.%02x.%02x.%02x.%02x "
			      "dest address and ARP target address "
			      "%02x.%02x.%02x.%02x.%02x.%02x to "
			      "match VM %02x.%02x.%02x.%02x.%02x.%02x\n",
			      eh.destAddr[0], eh.destAddr[1], eh.destAddr[2], 
			      eh.destAddr[3], eh.destAddr[4], eh.destAddr[5],
			      packetMac[0], packetMac[1], packetMac[2], 
			      packetMac[3], packetMac[4], packetMac[5],  
			      vmMacAddress[0], vmMacAddress[1], vmMacAddress[2], 
			      vmMacAddress[3], vmMacAddress[4], vmMacAddress[5]));
	    }
#endif
	    /*
	     * Copy in the MAC that will be written to eth 
	     * dest MAC and ARP dest MAC
	     */

	    if (!CopyDataForPacketFromHost(packets, 0, 0, vmMacAddress)) {
	       W_VNETKdPrint((MODULE_NAME "FromHostIP: couldn't "
			      "clone packet\n"));
	       return PacketStatusDropPacket;
	    }
	    if (!CopyDataForPacketFromHost(packets, 1, 
		                           ETH_HLEN + ARP_TARGET_MAC_OFFSET, 
                                           vmMacAddress)) {
	       W_VNETKdPrint((MODULE_NAME "FromHostIP: "
			      "couldn't clone packet #2\n"));
	       return PacketStatusDropPacket;
	    }

	    return PacketStatusForwardPacket;      
	 } else {
	    W_VNETKdPrint((MODULE_NAME "FromHostARP: target IP in reply ARP was"
			   " NOT found in table, presuming for another peer\n"));
	    return PacketStatusDropPacket;
	 }
      }
      VNETKdPrint((MODULE_NAME "FromHostARP: unrecognized ARP type %08x\n",
		   arpHeaderWord2));
      return PacketStatusDropPacket;
   } else { // if EAPOL packet: typeClass == EthClassEAPOL

      /*
       * Allow incoming EAPOL packets to proceed unmolested provided the
       * destination address matches the hardware address.
       */

      if (!MAC_EQ(state->macAddress, eh.destAddr)) {
         VNETKdPrint((MODULE_NAME "FromHostEAPOL: incoming request using "
                      "non-wireless-hardware-addr eth dest MAC, dropping\n"));
         return PacketStatusDropPacket;
      }
      return PacketStatusForwardPacket;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * SMAC_CheckPacketToHost --
 *
 *      Modifies packet contents to be suitable for transmission over
 *      a wireless network.  Function supports IP, ARP, RARP, IARP.
 *      Function will support DHCP in the future.  This function clones
 *      the source packet, modifies the clone packet in situations
 *      where the packet should be forwarded to the host.
 *
 *      NOTE: On Windows this function presumes it is called at DISPATCH_LEVEL
 *
 * Results:
 *      Returns 'PacketStatusTooShort' if insufficient data to process
 *      packet (suggest packet drop), 'PacketStatusDropPacket' if the
 *      packet should be dropped, or 'PacketStatusForwardPacket' if
 *      the packet should be forwarded.
 *
 * Side effects:
 *      Modifies contents of network packet, if nessary.
 *
 *----------------------------------------------------------------------
 */

PacketStatus SMACINT
SMAC_CheckPacketToHost(SMACState *state,     // IN: pointer to state
		       SMACPackets *packets) // IN/OUT: packet to process; cloned/modified packet
{
   uint8 buf[sizeof(EthernetHeader) + 4]; /* allocate extra 4 for VLAN headers */
   EthernetHeader *eh = (EthernetHeader*)buf;
   uint32 ethHeaderLen = ETH_HLEN; // tracks length of ethernet header
   SMACPacket *packet = NULL;
   EthClass typeClass; // classification of ethernet frame's type

   ASSERT(state);

   W_VNETKdPrint((MODULE_NAME "ToHost: Called\n"));
   ASSERTLOCKHELD();
   ASSERT(sizeof(EthernetHeader) == ETH_HLEN);

   packet = &(packets->orig);
   ASSERT(packet);

   /*
    * Verify that we have at least a minimal packet
    */

   if (!GetPacketData(packet, 0, ethHeaderLen, eh)) {
      VNETKdPrint((MODULE_NAME "ToHost: Packet missing eth header\n"));
      return PacketStatusTooShort;
   }

   /*
    * Lookup the ethernet media type of the packet
    */

   typeClass = LookupTypeClass(NTOHS(eh->lengthType));

   /*
    * If packet type is a VLAN header, then adjust pointers and 
    * decode the "real" media type which follows the VLAN header
    */

   if (typeClass == EthClassVLAN) {
      uint16 actualType = 0;

      ethHeaderLen +=4;
      if (!GetPacketData(packet, 0, ethHeaderLen, eh)) {
	 VNETKdPrint((MODULE_NAME "  ToHost: Packet is using VLAN header, "
	              "but supplied header is too small\n"));
	 return PacketStatusTooShort;
      }

      actualType = *(uint16*)(((uint8*)&(eh->lengthType)) + 4);
      
      VNETKdPrint((MODULE_NAME "  ToHost: Taking special measures for VLAN\n"));
      typeClass = LookupTypeClass(NTOHS(actualType));
   }

   /*
    * If the packet is not type IP, ARP or EAPOL, then drop the packet
    * unless it is a broadcast packet (broadcast packets don't
    * require much manipulating, so they should be safe to let thru).
    */

   if (typeClass != EthClassIPv4 && typeClass != EthClassIPv6 &&
       typeClass != EthClassARP && typeClass != EthClassEAPOL) {

      /*
       * DEBUG: if not a common/known media type, then print a status message
       */

#ifdef DBG
      if (typeClass != EthClassCommon) {
         char type[50] = ""; // holds textual name of type
         if (ethHeaderLen == ETH_HLEN) { // if not a vlan packet
            LookupTypeName(NTOHS(eh->lengthType), type, sizeof type);
         } else { // if a vlan packet
            uint16 actualType = *(uint16*)(((uint8*)&(eh->lengthType)) + 4);
            LookupTypeName(NTOHS(actualType), type, sizeof type);
         }
         VNETKdPrint((MODULE_NAME "  ToHost: non-IP & non-ARP "
		     "%02x:%02x:%02x:%02x:%02x:%02x -> "
		     "%02x:%02x:%02x:%02x:%02x:%02x %s\n",
             eh->srcAddr[0]&0xff, eh->srcAddr[1]&0xff,
             eh->srcAddr[2]&0xff, eh->srcAddr[3]&0xff,
             eh->srcAddr[4]&0xff, eh->srcAddr[5]&0xff,
             eh->destAddr[0]&0xff, eh->destAddr[1]&0xff,
             eh->destAddr[2]&0xff, eh->destAddr[3]&0xff,
             eh->destAddr[4]&0xff, eh->destAddr[5]&0xff, type));
      }
#endif

      /*
       * Let these unrecognized packets through only if they are broadcast or
       * multicast, and drop unicast packets (it's easier/debug loss of traffic
       * versus corrupted/invalid traffic).
       */

      if (IS_MULTICAST(eh->destAddr) || IS_BROADCAST(eh->destAddr)) {

         /*
          * Modify the source address to be that of the wireless hardware, so
          * that this packet can be transmitted.  First we need to duplicate
          * the packet so that other VMs don't get confused about MACs that
          * arbitrarily change between the VM's MAC and the host's MAC.
          */

         if (!ClonePacket(packets)) {
            VNETKdPrint((MODULE_NAME "  ToHost: couldn't clone packet\n"));
            return PacketStatusDropPacket;
         }

         CopyDataToClonedPacket(packets, state->macAddress, 
                                ETH_ALEN /* offset for source MAC */, 
                                ETH_ALEN /* length */);
         return PacketStatusForwardPacket;
      } else {

         /* 
          * Drop the packet, but first display a status message indicating
          * that we're dropping this packet (largely so we know what kind/type
          * of traffic that we are dropping
          */

#ifdef DBG
         char type[50] = ""; // holds textual name of type
         if (ethHeaderLen == ETH_HLEN) { // if not a vlan packet
            LookupTypeName(NTOHS(eh->lengthType), type, sizeof type);
         } else { // if a vlan packet
            uint16 actualType = *(uint16*)(((uint8*) & (eh->lengthType)) + 4);
            LookupTypeName(NTOHS(actualType), type, sizeof type);
         }
         VNETKdPrint((MODULE_NAME "  ToHost: Dropping unrecognized "
                      "unicast non-IP & non-ARP unicast packet: %s\n", type));
         VNETKdPrint((MODULE_NAME "  ToHost: the non-IP & non-ARP is "
                      "%02x:%02x:%02x:%02x:%02x:%02x -> "
                      "%02x:%02x:%02x:%02x:%02x:%02x\n",
                      eh->srcAddr[0]&0xff, eh->srcAddr[1]&0xff, 
                      eh->srcAddr[2]&0xff, eh->srcAddr[3]&0xff, 
                      eh->srcAddr[4]&0xff, eh->srcAddr[5]&0xff, 
                      eh->destAddr[0]&0xff, eh->destAddr[1]&0xff, 
                      eh->destAddr[2]&0xff, eh->destAddr[3]&0xff, 
                      eh->destAddr[4]&0xff, eh->destAddr[5]&0xff));   
#endif
         return PacketStatusDropPacket;
      }
   }

   /*
    * If IP packet, then store <source IP address, source MAC address> in lookup 
    * table and replace source ethernet MAC with that of the wireless hardware.
    */

   if (typeClass == EthClassIPv4 || typeClass == EthClassIPv6) {
      W_DEVEL_ONLY(char macStr1[MAC_STRING_SIZE];)
      W_DEVEL_ONLY(char macStr2[MAC_STRING_SIZE];)

      if (typeClass == EthClassIPv4) {
         uint8 ipHeader[IP_HEADER_LEN];
         uint32 ipVer;       // version of IP header
         uint32 ipHeaderLen; // reported length of IPv4 header
         
         uint16 ipLen;       // reported length of IPv4 packet
         uint32 ipSrcAddr;   // source address

         /*
          * Read in the IPv4 header.
          */

         if (!GetPacketData(packet, ethHeaderLen, sizeof ipHeader, ipHeader)) {
            VNETKdPrint((MODULE_NAME "  ToHostIP: got an IP type, "
                         "but incomplete header\n"));
            return PacketStatusTooShort;
         }


         ipVer = ipHeader[0] >> 4;
         ipHeaderLen = 4 * (ipHeader[0]&0xf); // reported length of IP header
         
         ipLen = NTOHS(*(uint16*)(ipHeader + 2)); // packet len
         ipSrcAddr = *((uint32*)(ipHeader + IP_HEADER_SRC_ADDR_OFFSET));

         /*
          * Verify basic fields in IPv4 header.
          */
      
         if (ipVer != IPv4 || ipHeaderLen < IP_HEADER_LEN ||
             GetPacketLength(packet) < ethHeaderLen + ipHeaderLen) {
            VNETKdPrint((MODULE_NAME "ToHostIP:  IPv4 packet's ver = %u, "
                         "reported length = %u, reported length = %u.\n",
                         ipVer, GetPacketLength(packet),
                         ethHeaderLen + ipHeaderLen));
            return PacketStatusDropPacket;
         }

         /*
          * Store IPv4/MAC combo in lookup table, that way we can replace
          * the MAC when any replies to this IP are received.
          */
      
         if (!AddIPv4andMACcombo(state, ipSrcAddr, eh->srcAddr)) {
            return PacketStatusDropPacket;
         }
      } else {
         uint8 ipv6Header[IPv6_HEADER_LEN];
         uint8 ipv6Ver;
         IPv6Addr ipv6SrcAddr;

         /*
          * Read the IPv6 header.
          */

         if (!GetPacketData(packet, ethHeaderLen, sizeof ipv6Header,
             ipv6Header)) {
            VNETKdPrint((MODULE_NAME "ToHostIP:  got incomplete IPv6 "
                         "header.\n"));
            return PacketStatusTooShort;
         }

         ipv6Ver = ipv6Header[0] >> 4;
         if (ipv6Ver != IPv6 || GetPacketLength(packet) < ethHeaderLen +
                                                          IPv6_HEADER_LEN) {
            VNETKdPrint((MODULE_NAME "  ToHostIP: got an IP version %u, or "
                         "length %u less than minimum length %u.\n", ipv6Ver,
                         GetPacketLength(packet), ethHeaderLen +
                         IPv6_HEADER_LEN));
            return PacketStatusDropPacket;
         }

         ipv6SrcAddr.addrHi = *((uint64 *)(ipv6Header +
                                           IPv6_HEADER_SRC_ADDR_OFFSET));
         ipv6SrcAddr.addrLo = *((uint64 *)(ipv6Header +
                                           IPv6_HEADER_SRC_ADDR_OFFSET +
                                           sizeof ipv6SrcAddr.addrHi));

         /*
          * Store IPv6/MAC combo in lookup table, that way we can replace
          * the MAC when any replies to this IP are received.
          */
      
         if (!AddIPv6andMACcombo(state, &ipv6SrcAddr, eh->srcAddr)) {
            return PacketStatusDropPacket;
         }
      }

      /*
       * Now the code common to IPv4 and IPv6 cases start.
       *
       * First, replace source MAC with wireless hardware MAC.
       */

      W_VNETKdPrint((MODULE_NAME "  ToHostIP: modifying ETH MAC %s source "
                     "address to match wireless hardware MAC %s\n",
                     PrintMACAddrToString(macStr1, sizeof macStr1,
                                          eh->srcAddr),
                     PrintMACAddrToString(macStr2, sizeof macStr2,
                                          state->macAddress)));

      /* 
       * Duplicate the packet so that other VMs don't get confused about 
       * MACs that arbitrarily change between the VM's MAC and the host's 
       * MAC.
       */

      if (!ClonePacket(packets)) {
         VNETKdPrint((MODULE_NAME "  ToHostIP: couldn't clone packet\n"));
         return PacketStatusDropPacket;
      }
       
      /*
       * Make any necessary modifications to packet payload.
       */

      if (typeClass == EthClassIPv4) {
         ProcessOutgoingIPv4Packet(&packets->clone, ethHeaderLen);
      } else {
         Bool toHost = FALSE; /* Updated only on Windows Vista and later. */

         if (!ProcessOutgoingIPv6Packet(&packets->clone, ethHeaderLen,
                                        state->macAddress, &toHost)) {
            VNETKdPrint((MODULE_NAME "  ToHostIP: error in processing "
                         "outgoing IPv6 packet.\n"));
            return PacketStatusDropPacket;
         }

         if (UNLIKELY(toHost)) {
            /*
             * Don't replace the ethernet source MAC address in the packet with
             * state->macAddress.
             */

            return PacketStatusForwardPacket;
         }
      }
      CopyDataToClonedPacket(packets, state->macAddress, 
                             ETH_ALEN /* offset for source MAC */, 
                             ETH_ALEN /* length */);
      return PacketStatusForwardPacket;
   } 

   /*
    * If ARP packet, then store <source IP address, source MAC address> in 
    * lookup table (if possible) and replace source ethernet MAC (and source 
    * ARP MAC, if appropriate) with that of the wireless hardware
    */

   else if (typeClass == EthClassARP) { // ARP packet
      uint32 arpHeaderWord1; // first word of ARP header
      uint32 arpHeaderWord2; // second word of ARP header

      /*
       * Verify that packet meets minimum length expectations, the ARP header 
       * is 7 words for ethernet and IPv4, but the length may actually be 60 
       * due to ethernet's minimum length requirements.
       */

      if (GetPacketLength(packet) < ethHeaderLen + ARP_HEADER_LEN) {
         VNETKdPrint((MODULE_NAME "  ToHostARP: ARP packet is insufficient "
                      "length of IPv4 and Ethernet, expected %u got %u\n",
                      ethHeaderLen + ARP_HEADER_LEN, GetPacketLength(packet)));
         return PacketStatusDropPacket;
      }

      /*
       * Verify the first word of the ARP header (hardcoded for ethernet and IPv4)
       *
       * I recently added IEEE802 support.  These types of ARP requests were
       * observed on the company network, so someone uses them.  As long as
       * the address lengths are the same then I imagine that the processing
       * is identical and we can handle them (lengths are checked as part
       * of processing the second word of ARP header).
       */

      if (!GetPacketData(packet, ethHeaderLen, sizeof arpHeaderWord1,
          &arpHeaderWord1) ||
          !GetPacketData(packet, ethHeaderLen + sizeof arpHeaderWord1,
          sizeof arpHeaderWord2, &arpHeaderWord2)) {
         VNETKdPrint((MODULE_NAME "ToHostARP: ARP header couldnt be loaded\n"));
         return PacketStatusDropPacket;
      }

      if (arpHeaderWord1 != HTONL(0x00010800) /* ethernet */ &&
         arpHeaderWord1 != HTONL(0x00060800) /* ieee802  */ ) {
         VNETKdPrint((MODULE_NAME "  ToHostARP: ARP header appears wrong, "
                      "got %08x\n", arpHeaderWord1));
         return PacketStatusDropPacket;
      }

      /*
       * Perform action based on opcode in second word of ARP header.
       * DEBUG: print general information about the packet
       */

#ifdef DBG
      switch (arpHeaderWord2) {
	 case CONST_NTOHL(0x06040001):
	    W_VNETKdPrint((MODULE_NAME "  ToHostARP: "
	                 "ARP header2 indicates ARP request\n"));
	    break;
	 case CONST_NTOHL(0x06040002):
	    W_VNETKdPrint((MODULE_NAME "  ToHostARP: "
	                 "ARP header2 indicates ARP reply\n"));
	    break;
	 case CONST_NTOHL(0x06040003):
	    VNETKdPrint((MODULE_NAME "  ToHostARP: "
	                 "ARP header2 indicates RARP request\n"));
	    break;
	 case CONST_NTOHL(0x06040004):
	    VNETKdPrint((MODULE_NAME "  ToHostARP: "
	                 "ARP header2 indicates RARP reply\n"));
	    break;
	 case CONST_NTOHL(0x06040008):
	    VNETKdPrint((MODULE_NAME "  ToHostARP: "
	                 "ARP header2 indicates IARP request\n"));
	    break;
	 case CONST_NTOHL(0x06040009):
	    VNETKdPrint((MODULE_NAME "  ToHostARP: "
	                 "ARP header2 indicates IARP reply\n"));
	    break;
	 default:
	    VNETKdPrint((MODULE_NAME "  ToHostARP: "
	                 "ARP header2 indicates unknown opcode\n"));
	    break;
      }

      {
	 uint8 packetMac[ETH_ALEN];
	 uint8 packetIP[4];

	 if (!GetPacketData(packet, ethHeaderLen + ARP_SENDER_MAC_OFFSET, 
			    sizeof packetMac, packetMac)) {
	    VNETKdPrint((MODULE_NAME "  ToHostARP: couldn't read MAC\n"));
	    return PacketStatusTooShort;
	 }
	 if (!GetPacketData(packet, ethHeaderLen + ARP_SENDER_IP_OFFSET, 
			    sizeof packetIP, packetIP)) {
	    VNETKdPrint((MODULE_NAME "  ToHostARP: couldn't read IP\n"));
	    return PacketStatusTooShort;
	 }

	 W_VNETKdPrint((MODULE_NAME "  ToHostARP: sender MAC is "
			"%02x:%02x:%02x:%02x:%02x:%02x IP is %d.%d.%d.%d\n",
			packetMac[0], packetMac[1], packetMac[2], 
			packetMac[3], packetMac[4], packetMac[5], 
			packetIP[0], packetIP[1],  packetIP[2], packetIP[3]));

	 if (!GetPacketData(packet, ethHeaderLen + ARP_TARGET_MAC_OFFSET, 
			    sizeof packetMac, packetMac)) {
	    VNETKdPrint((MODULE_NAME "  ToHostARP: couldn't read MAC\n"));
	    return PacketStatusTooShort;
	 }
	 if (!GetPacketData(packet, ethHeaderLen + ARP_TARGET_IP_OFFSET, 
			    sizeof packetIP, packetIP)) {
	    VNETKdPrint((MODULE_NAME "  ToHostARP: couldn't read IP\n"));
	    return PacketStatusTooShort;
	 }

	 W_VNETKdPrint((MODULE_NAME "  ToHostARP: target MAC is "
			"%02x:%02x:%02x:%02x:%02x:%02x IP is %d.%d.%d.%d\n",
			packetMac[0], packetMac[1], packetMac[2], 
			packetMac[3], packetMac[4], packetMac[5], 
			packetIP[0], packetIP[1],  packetIP[2], packetIP[3]));
      }
#endif

      /*
       * ARP handling for *outgoing traffic*
       *
       * ARP: host wants to know the MAC that corresponds to a particular IP
       * 1 ARP request:  <srcMAC, srcIP, 0,      dstIP> store source <MAC,IP>, 
       *   modify source MAC in eth & ARP
       * 2 ARP reply:    <srcMAC, srcIP, dstMAC, dstIP> store source <MAC,IP>, 
       *   modify source MAC in eth & ARP
       *
       * RARP: host knows its MAC and wants to find out which IP it is 
       *   assigned (simple form of DHCP)
       * 3 RARP request: <srcMAC, 0    , srcMAC, 0    >  can't store <MAC,IP>, 
       *   should modify srcMAC but response would be MAC/IP of host 
       *   I'm tempted to blackhole this packet, but for now I will just modify 
       *   the eth MAC and allow it to be sent
       *   -- for now, presuming that lookup done on second srcMAC, but packet
       *      sent to first srcMAC (in other words, like IARP).  This may
       *      be more correct / interoperable, but we still won't be able to
       *      handle RARP replies properly unless they are broadcasted.
       * 4 RARP reply:   <srcMAC, srcIP, dstMAC, dstIP> store source <MAC,IP>, 
       *   modify source MAC in eth & ARP
       *
       * IARP: host knows a peer's MAC and wants to determine its IP address
       * 8 IARP request: <srcMAC, srcIP, dstMAC, 0    > store source <MAC,IP>, 
       *   modify source MAC in eth & ARP
       * 9 IARP reply:   <srcMAC, srcIP, dstMAC, dstIP> store source <MAC,IP>, 
       *   modify source MAC in eth & ARP
       */

      if (arpHeaderWord2 == HTONL(0x06040001) ||
	  arpHeaderWord2 == HTONL(0x06040002) ||
	  arpHeaderWord2 == HTONL(0x06040003) ||
	  arpHeaderWord2 == HTONL(0x06040004) ||
	  arpHeaderWord2 == HTONL(0x06040008) ||
	  arpHeaderWord2 == HTONL(0x06040009)) {

	 if (arpHeaderWord2 != HTONL(0x06040003)) { // don't store MAC/IP for RARP req.
	    uint32 IPaddr;
	    uint8 packetMac[ETH_ALEN];

	    W_VNETKdPrint((MODULE_NAME "  ToHostARP: adding MAC/IP "
			   "combo to lookup table\n"));

	    /* read IP */
	    if (!GetPacketData(packet, ethHeaderLen + ARP_SENDER_IP_OFFSET, 
			       sizeof IPaddr, &IPaddr)) {
	       VNETKdPrint((MODULE_NAME "  ToHostARP: couldn't read IP\n"));
	       return PacketStatusTooShort;
	    }

	    /* read MAC */
	    if (!GetPacketData(packet, ethHeaderLen + ARP_SENDER_MAC_OFFSET, 
			       sizeof packetMac, packetMac)) {
	       VNETKdPrint((MODULE_NAME "  ToHostARP: couldn't read MAC\n"));
	       return PacketStatusTooShort;
	    }
	    
	    /*
	     * Store <IP,MAC> combo in table.  For MAC we could also use 
	     * (char*)(packet)+ETH_ALEN, but it's more consistent with the ARP
	     * protocol to use the MAC located within the packet
	     */
	    
	    if (!AddIPv4andMACcombo(state, IPaddr, packetMac)) { 
	       return PacketStatusDropPacket;
	    }
	 }

	 /*
	  * First we need to duplicate the packet so that other VMs don't 
	  * get confused about MACs that arbitrarily change between the 
	  * VM's MAC and the host's MAC.
	  */

	 {
            uint32  offset = ethHeaderLen + ARP_SENDER_MAC_OFFSET;

	    if (!ClonePacket(packets)) {
	       VNETKdPrint((MODULE_NAME "  ToHostARP: couldn't "
                            "clone packet\n"));
	       return PacketStatusDropPacket;
	    }

	    /*
	     * Substitute sender ethernet MAC with the wireless hardware's MAC
	     */

	    W_VNETKdPrint((MODULE_NAME "  ToHostARP: modifying ETH MAC "
			"%02x.%02x.%02x.%02x.%02x.%02x source address to match"
			" wireless hardware %02x.%02x.%02x.%02x.%02x.%02x \n",
			eh->srcAddr[0]&0xff, eh->srcAddr[1]&0xff, 
                        eh->srcAddr[2]&0xff, eh->srcAddr[3]&0xff, 
                        eh->srcAddr[4]&0xff, eh->srcAddr[5]&0xff, 
			state->macAddress[0]&0xff, state->macAddress[1]&0xff, 
			state->macAddress[2]&0xff, state->macAddress[3]&0xff, 
			state->macAddress[4]&0xff, state->macAddress[5]&0xff));
	 
	    CopyDataToClonedPacket(packets, state->macAddress, ETH_ALEN /* offset */,
				   ETH_ALEN /* length */);

	    /* 
	     * Modify ARP source MAC
	     */

#if defined(_WIN32) && NDIS_SUPPORT_NDIS6
            /*
             *  In normal case, we will substitute ARP source MAC with the
             *  wireless hardware's MAC in the ARP payload.
             *
             * However if the target IP address is host IP address,
             *  the arp protocol payload should not be touched. Otherwise
             * the Windows TCP/IP stack will not respond for the request.
             * (This behavior is seen in Vista and after.)
             * Since the ARP payload is not touched, We should change the
             * destination MAC in the MAC header to wireless hardware MAC
             * so that no other host will receive and process the ARP packet
             */
            {
               ULONG ipAddr;
               if (!GetPacketData(packet, ethHeaderLen + ARP_TARGET_IP_OFFSET,
                                  sizeof ipAddr, &ipAddr)) {
                  VNETKdPrint((MODULE_NAME " ToHostARP: couldn't get target "
                               "IP address\n"));
                  return PacketStatusTooShort;
               }
               if (BridgeIPv4MatchAddrMAC(ipAddr, state->macAddress)) {
                  offset = 0;
               }
            }
#endif 


#ifdef DBG
            {
	       uint8 macAddr[ETH_ALEN];
	       if (!GetPacketData(packet, offset, sizeof macAddr, macAddr)) {
		  VNETKdPrint((MODULE_NAME "  ToHostARP: "
			       "couldn't read data at offset %u\n", offset));
		  return PacketStatusTooShort;
	       }

	       W_VNETKdPrint((MODULE_NAME "  ToHostARP: modifying %s from "
                              "%02x.%02x.%02x.%02x.%02x.%02x to "
			      "wireless hardware address "
                              "%02x.%02x.%02x.%02x.%02x.%02x \n",
                              (offset == 0) ? "destination address of MAC header"
                              : "ARP payload source MAC address",
			      macAddr[0], macAddr[1], macAddr[2],
			      macAddr[3], macAddr[4], macAddr[5],
			      state->macAddress[0], state->macAddress[1],
			      state->macAddress[2], state->macAddress[3],
			      state->macAddress[4], state->macAddress[5]));
            }
#endif
	    CopyDataToClonedPacket(packets, state->macAddress, offset, ETH_ALEN);
	    return PacketStatusForwardPacket;
	 }
      }

      VNETKdPrint((MODULE_NAME "  ToHostARP: unrecognized ARP type %08x\n",
		   arpHeaderWord2));
      return PacketStatusDropPacket;
   } else { // if EAPOL packet: typeClass == EthClassEAPOL
      if (!ClonePacket(packets)) {
         VNETKdPrint((MODULE_NAME "  ToHostEapol: couldn't clone packet\n"));
         return PacketStatusDropPacket;
      }

      /* For wireless, send EAPOL packets to host side. */
      CopyDataToClonedPacket(packets, state->macAddress,
                             ETH_ALEN /* offset for source MAC */,
                             ETH_ALEN /* length */);

      return PacketStatusForwardPacket;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * ProcessOutgoingIPv4Packet --
 *
 *      This function is used to examine IPv4 packets, and make any
 *      adjustments (if necessary) to the IP header and/or payload.
 *      The caller should have determined that the packet is IPv4
 *      before calling this function.
 *
 *      Currently this function just makes sure that the broadcast
 *      bit is set on outgoing client DHCP packets that are being
 *      sent to a server.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May modify the contents of the packet.  Length should be 
 *      unchanged.
 *
 *----------------------------------------------------------------------
 */

static void
ProcessOutgoingIPv4Packet(SMACPacket *packet,  // IN: cloned packet to process
			  uint32 ethHeaderLen) // IN: length of ethernet header
{
   uint8  proto;   // protocol within ip
   uint16 ipFlags; // flags and offset

   /*
    * Should certain checks, e.g., IP version field, be checked here,
    * or assume that caller has already performed this check?
    */

   // check should have been performed by caller
   ASSERT(GetPacketLength(packet) >= IP_HEADER_LEN + ethHeaderLen); 

   if (!GetPacketData(packet, ethHeaderLen + IP_HEADER_PROTO_OFFSET, 
		      sizeof proto, &proto)) {
      VNETKdPrint((MODULE_NAME "ProcessOutgoing: couldn't get protocol\n"));
      ASSERT(0); // should never occur
      return;      
   }

   if (!GetPacketData(packet, ethHeaderLen + IP_HEADER_FLAGS_OFFSET,
		      sizeof ipFlags, &ipFlags)) {
      VNETKdPrint((MODULE_NAME "ProcessOutgoing: couldn't get flags\n"));
      ASSERT(0); // should never occur
      return;      
   }

   /*
    * Verify offset=0 and M=0: mask off "fragment" flag, 
    * all others should be zero.  This is to ensure that we're processing
    * only the front of the actual packet (i.e., validate that what we
    * think if offset 0 is really offset 0).
    */

   if (ipFlags & HTONS((uint16)(0xbfff))) { 
      VNETKdPrint((MODULE_NAME "ProcessOutgoing: got a fragmented IP "
	           "(ipFlags %04x), so not performing higher-level processing\n",
		   ipFlags));
      return;
   }

   /*
    * NOTE: the following switch statement can be replaced with a single
    * check for proto==17, since UDP is the only non-debug case
    */

   switch (proto) {
   case 1: { // ICMP
#ifdef DBG
      uint8  ipHeader[IP_HEADER_LEN];
      uint32 ipHeaderLen = 0;
      uint32 ipVer = 0;
      uint16 ipLen = 0;
      uint8  typeField = 0;
      uint8  codeField = 0;

      /* Verify that we have at least a whole, minimal IP header */
      if (!GetPacketData(packet, ethHeaderLen, sizeof ipHeader, ipHeader)) {
         VNETKdPrint((MODULE_NAME "ProcessOutgoing: couldn't get IP\n"));
         return;
      }

      ipHeaderLen = 4 * (ipHeader[0] & 0xf); // version of IP header
      ipVer = ipHeader[0] >> 4; // length of IP header
      ipLen = NTOHS(*(uint16*)(ipHeader + 2)); // total datagram len

      if (ipVer == 4 && GetPacketLength(packet) >=
          ipHeaderLen + ethHeaderLen + 2 * 4 &&
          GetPacketData(packet, ethHeaderLen + ipHeaderLen,
                        sizeof typeField, &typeField) &&
          GetPacketData(packet, ethHeaderLen + ipHeaderLen + 1,
                        sizeof codeField, &codeField)) {
         VNETKdPrint((MODULE_NAME "ProcessOutgoing: got ICMP, "
                      "type = %u, code = %u,\n", typeField, codeField));
      } else {
         VNETKdPrint((MODULE_NAME "ProcessOutgoing: got ICMP, "
                      "but couldn't process\n"));
      }
#endif /* DBG */
      return;
   }
   case 2: // IGMP
      VNETKdPrint((MODULE_NAME "ProcessOutgoing: got IGMP\n"));
      return;
   case 4: // IP in IP
      VNETKdPrint((MODULE_NAME "ProcessOutgoing: got IP in IP\n"));
      return;
   case 6: // TCP
      VNETKdPrint((MODULE_NAME "ProcessOutgoing: got TCP\n"));
      return;
   case 17: { // UDP
      uint8 ipHeaderFirstByte;
      uint32 ipHeaderLen; // length of IP header
      uint16 srcPort;     // UDP source port
      uint16 destPort;    // UDP dest port

      if (!GetPacketData(packet, ethHeaderLen, 
			 sizeof ipHeaderFirstByte, &ipHeaderFirstByte)) {
	 VNETKdPrint((MODULE_NAME "FromHostIP: Failed to get IP length\n"));
	 return;
      }
     
      ipHeaderLen = 4 * (ipHeaderFirstByte & 0xf); // version of IP header

      W_VNETKdPrint((MODULE_NAME "ProcessOutgoing: got UDP\n"));

      /*
       * Verify that sufficient data for UDP packet header is present
       */
      
      if (GetPacketLength(packet) < ethHeaderLen + ipHeaderLen + UDP_HEADER_LEN) {
	 VNETKdPrint((MODULE_NAME "ProcessOutgoing: UDP header not present\n"));
	 return;
      }

      /*
       * decode the source and destination ports in UDP header
       */

      if (!GetPacketData(packet, ethHeaderLen + ipHeaderLen, 
			 sizeof srcPort, &srcPort) ||
	  !GetPacketData(packet, ethHeaderLen + ipHeaderLen + 2, 
			 sizeof destPort, &destPort)) {
	 VNETKdPrint((MODULE_NAME "FromHostIP: Failed to get UDP ports\n"));
	 return;
      }

      srcPort  = NTOHS(srcPort);
      destPort = NTOHS(destPort);

#ifdef DBG
      {
	 uint8 srcAddr[4];
	 uint8 destAddr[4];

	 if (!GetPacketData(packet, ethHeaderLen + IP_HEADER_SRC_ADDR_OFFSET, 
			    sizeof srcAddr, srcAddr) ||
	     !GetPacketData(packet, ethHeaderLen + IP_HEADER_DEST_ADDR_OFFSET, 
			    sizeof destAddr, destAddr)) {
	    VNETKdPrint((MODULE_NAME "FromHostIP: Failed to get IP addrs\n"));
	    ASSERT(0); // should never happen
	 } else {
	    W_VNETKdPrint((MODULE_NAME "ProcessOutgoing: got UDP, "
		      "src %d.%d.%d.%d %d dest %d.%d.%d.%d %d\n", 
		      srcAddr[0], srcAddr[1], srcAddr[2], srcAddr[3], srcPort, 
		      destAddr[0], destAddr[1], destAddr[2], destAddr[3],
                      destPort));
	 }
      }
#endif
      
      if (destPort == 67) { // if destined for DHCP server port
	 uint32 firstDHCPword;
	 uint16 dhcpFlags;

	 VNETKdPrint((MODULE_NAME "ProcessOutgoing: got client "
	              "DHCP packet destined for a server\n"));

	 /*
	  * The minimum length of a DHCP packet must be 243 bytes:-
	  * 240 (DHCP header including magic cookie) + 3 (message type option)
	  * RFC 2131 mandates having the 'message type' option in every DHCP packet.
	  */

	 if (GetPacketLength(packet) < ethHeaderLen + ipHeaderLen +
               UDP_HEADER_LEN + 243) {
	    VNETKdPrint((MODULE_NAME "ProcessOutgoing: packet "
	                 "too small for DHCP\n"));
	    return;
	 }

	 /*
	  * Verify that the first word in the DHCP header matches
	  * out expectations
	  */

	 if (!GetPacketData(packet, ethHeaderLen + ipHeaderLen + 
                  UDP_HEADER_LEN, sizeof firstDHCPword, &firstDHCPword)) {
	    VNETKdPrint((MODULE_NAME "ProcessOutgoing: couldn't get first "
	                 "DHCP word\n"));
	    return;
	 }

	 firstDHCPword = NTOHL(firstDHCPword);
	 if ((firstDHCPword & 0xffffff00) != 0x01010600) {
	    VNETKdPrint((MODULE_NAME "ProcessOutgoing: DHCP "
	                 "first word appears incorrect\n"));
	    return;
	 }

	 /*
	  * Turn on the broadcast flag, just in case.  This
	  * instructs/requests that the server broadcast the reply
	  * back, which is a nice way to avoid the issue about
	  * whether to set chaddr to the VM MAC or the wireless MAC.
	  * 
	  * If chaddr is set to wireless MAC then we'd receive the reply,
	  * but the DHCP server might confuse us with the host and send
	  * us a copy of the host's current IP address assignment.  This
	  * confusion should be occur if all OSes used the client identifier
	  * extension, but we can't rely on this.
	  *
	  * If chaddr is set to VM MAC then no alias confusion should occur,
	  * but the reply might be unicast to the VM's MAC (in which case
	  * we might not , or will not, receive the reply).  To avoid the
	  * receive issue, it's easiest to just set the broadcast packet
	  * if it's not already set.
	  *
	  * One potential downside is that the VM might be expecting a unicast
	  * reply, and it's unclear whether it can or will handling a 
	  * broadcast reply properly.  We can special case this support, if
	  * necessary.
	  */

	 if (!GetPacketData(packet, ethHeaderLen + ipHeaderLen + 
                            UDP_HEADER_LEN + 10, sizeof dhcpFlags, &dhcpFlags)) {
	    VNETKdPrint((MODULE_NAME "ProcessOutgoing: couldn't get "
	                 "DHCP flags\n"));
	    return;
	 }

	 dhcpFlags = NTOHS(dhcpFlags);
	 if (dhcpFlags & 0x8000) { // if flag already set, then no work to do
	    VNETKdPrint((MODULE_NAME "ProcessOutgoing: DHCP broadcast "
	                 "bit is already set\n"));
	 } else {

	    /*
	     * Get the original checksum
	     */

	    uint16 oriChecksum;

	    if (!GetPacketData(packet, ethHeaderLen + ipHeaderLen + 6, 
			       sizeof oriChecksum, &oriChecksum)) {
	       VNETKdPrint((MODULE_NAME "ProcessOutgoing: couldn't get "
			    "UDP checksum\n"));
	       return;
	    }
	    oriChecksum = NTOHS(oriChecksum);

            VNETKdPrint((MODULE_NAME "ProcessOutgoing: DHCP broadcast bit "
	                 "NOT set, need to do so\n"));

	    if (oriChecksum) { // if checksum specified

	       /*
	        * Compute the checksum over the word that we're about to change
		*/

	       uint32 sumBefore;   // checksum before
	       uint32 sumAfter;    // checksum after changes
	       uint32 sumDiff;     // difference in checksum
	       uint16 newChecksum; // new checksum
	       
	       /* checksum bytes 8-11 */

	       if (!GetPacketData(packet, ethHeaderLen + ipHeaderLen + 
                                  UDP_HEADER_LEN + 8,
				  sizeof sumBefore, &sumBefore)) {
		  VNETKdPrint((MODULE_NAME "ProcessOutgoing: couldn't get "
			       "UDP checksum before\n"));
		  return;
	       }

	       sumBefore = SUM32(sumBefore); // checksum before

	       VNETKdPrint((MODULE_NAME "ProcessOutgoing: DHCP UDP checksum "
		            "is present, changing flags and checksum\n"));

	       /*
	        * Set the actual flag in bytes 10 and 11
		*/

	       dhcpFlags |= 0x8000;

	       if (!SetPacketByte(packet, ethHeaderLen + ipHeaderLen + 
                                  UDP_HEADER_LEN + 11, dhcpFlags & 0xff) || 
		   !SetPacketByte(packet, ethHeaderLen + ipHeaderLen + 
                                  UDP_HEADER_LEN + 10, dhcpFlags >> 8)) {
		  VNETKdPrint((MODULE_NAME "ProcessOutgoing: couldn't set "
		               "new UDP flags\n"));
		  return;
	       }
	       
	       /*
	        * Compute the checksum over the word that we have just changed
		*/

	       /* checksum (again) bytes 8-11 */

	       if (!GetPacketData(packet, ethHeaderLen + ipHeaderLen + 
                                  UDP_HEADER_LEN + 8,
				  sizeof sumAfter, &sumAfter)) {
		  VNETKdPrint((MODULE_NAME "ProcessOutgoing: couldn't get "
			       "UDP checksum after\n"));
		  ASSERT(0); // should never occur
		  return;
	       }

	       sumAfter = SUM32(sumAfter);

	       /*
	        * Compute difference/delta between old and new checksums
		*/

   	       sumDiff = CalcChecksumDiff(sumBefore, sumAfter);
    
	       /*
	        * Compute new checksum, based on delta from existing checksum
		*/

	       newChecksum = UpdateSum(oriChecksum, sumDiff);

	       /*
	        * Verify that the new checksum isn't 0 (in which case make it ~0)
		*/

	       if (!newChecksum) {
		  newChecksum = 0xffff;
	       }

	       /*
	        * Write back the new checksum
		*/

	       if (!SetPacketByte(packet, ethHeaderLen + ipHeaderLen + 6, 
				  newChecksum >> 8) ||
		   !SetPacketByte(packet, ethHeaderLen + ipHeaderLen + 7, 
				  newChecksum & 0xff)) {
		  VNETKdPrint((MODULE_NAME "ProcessOutgoing: couldn't set "
		               "new UDP checksum\n"));
		  return;
	       }
	    } else {
	       VNETKdPrint((MODULE_NAME "ProcessOutgoing: DHCP UDP "
		            "checksum is NOT present, changing only flags\n"));
	       /*
	        * Set the actual flag without modifying checksum
		*/

	       dhcpFlags |= 0x8000;
	       if (!SetPacketByte(packet, ethHeaderLen + ipHeaderLen + 
                                  UDP_HEADER_LEN + 11, dhcpFlags & 0xff) || 
		   !SetPacketByte(packet, ethHeaderLen + ipHeaderLen + 
                                  UDP_HEADER_LEN + 10, dhcpFlags >> 8)) {
		  VNETKdPrint((MODULE_NAME "ProcessOutgoing: couldn't set "
		               "new UDP flags, non-checksum case\n"));
		  return;
	       }
	    }
	 }

#ifdef DBG
	 {
	    uint32 IPDestAddr;
	    if (!GetPacketData(packet, ethHeaderLen + IP_HEADER_DEST_ADDR_OFFSET, 
			       sizeof IPDestAddr, &IPDestAddr)) {
	       VNETKdPrint((MODULE_NAME "ProcessOutgoing: couldn't get "
			    "IP dest addr\n"));
	       return;
	    }
	    
	    if (IPDestAddr == IP_ADDR_BROADCAST) {
	       VNETKdPrint((MODULE_NAME "ProcessOutgoing: server-bound DHCP "
			    "packet is broadcast\n"));
	    } else {
	       VNETKdPrint((MODULE_NAME "ProcessOutgoing: server-bound DHCP "
			    "packet is unicast\n"));
	    }
	 }
#endif
	 return;
      } else if (destPort == 68) { // if packet for destined DHCP client port
#ifdef DBG
	 uint32 IPDestAddr = 0;
	 if (!GetPacketData(packet, ethHeaderLen + IP_HEADER_DEST_ADDR_OFFSET, 
			    sizeof IPDestAddr, &IPDestAddr)) {
	    VNETKdPrint((MODULE_NAME "ProcessOutgoing: couldn't get "
			 "IP dest addr\n"));
	    return;
	 }

	 VNETKdPrint((MODULE_NAME "ProcessOutgoing: got server "
	              "DHCP packet destined for a client\n"));

	 if (IPDestAddr == IP_ADDR_BROADCAST) {
	    VNETKdPrint((MODULE_NAME "ProcessOutgoing: client-bound "
	                 "DHCP packet is broadcast\n"));
	 } else {
	    VNETKdPrint((MODULE_NAME "ProcessOutgoing: client-bound "
	                 "DHCP packet is unicast\n"));
	 }
#endif /* DBG */
	 return;
      } else {
	 W_VNETKdPrint((MODULE_NAME "ProcessOutgoing: got UDP srcPort "
	              "%d destPort %d\n", srcPort, destPort));
	 return;
      }
   }
   case 27: // RDP
      VNETKdPrint((MODULE_NAME "ProcessOutgoing: got RDP\n"));
      return;
   case 41: // IPv6
      VNETKdPrint((MODULE_NAME "ProcessOutgoing: got IPv6\n"));
      return;
   case 51: // Authentication Header
      VNETKdPrint((MODULE_NAME "ProcessOutgoing: got Authentication Header\n"));
      return;
   case 55: // Mobile IP
      VNETKdPrint((MODULE_NAME "ProcessOutgoing: got Mobile IP\n"));
      return;
   case 103: // PIM
      VNETKdPrint((MODULE_NAME "ProcessOutgoing: got PIM\n"));
      return;
   case 111: // IPX in IP
      VNETKdPrint((MODULE_NAME "ProcessOutgoing: got IPX in IP\n"));
      return;
   default:
      VNETKdPrint((MODULE_NAME "ProcessOutgoing: Unknown/unhandled "
	           "service reported by IP packet\n"));
      return;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * PatchMacAddrFixChecksum --
 *
 *      This function takes in a packet and patches the MAC address at
 *      a given offset in the packet with a given MAC address.  It also
 *      updates the 2byte checksum field in the packet at a given offset.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
PatchMacAddrFixChecksum(SMACPacket *packet,          // IN:
                        const uint32 packetLen,      // IN:
                        const uint32 checksumOffset, // IN:
                        const uint32 patchMacOffset, // IN:
                        const uint8 *macAddress,     // IN:
                        const char *logPrefix)       // IN:
{
   uint16 oriChecksum;
   uint32 sumBefore[2];
   uint32 sumAfter[2];
   uint32 sumDiff;
   uint16 newChecksum;
   int i;

   UNREFERENCED_PARAMETER(logPrefix);
   if (!packet || checksumOffset + sizeof oriChecksum > packetLen ||
       patchMacOffset + ETH_ALEN > packetLen) {
      VNETKdPrint((MODULE_NAME "%s:  patching MAC address got invalid input: "
                   "packet = %p, packetLen = %u, checksumOffset = %u, "
                   "patchMacOffset = %u.\n", logPrefix, packet, packetLen,
                   checksumOffset, patchMacOffset));
      return FALSE;
   }

   if (!GetPacketData(packet, checksumOffset, sizeof oriChecksum,
                      &oriChecksum)) {
      VNETKdPrint((MODULE_NAME "%s:  couldn't get original checksum value.\n",
                   logPrefix));
      return FALSE;
   }

   oriChecksum = NTOHS(oriChecksum);

   sumBefore[1] = 0; /* Clear out any garbage. */
   if (!GetPacketData(packet, patchMacOffset, sizeof sumBefore[0],
                      &sumBefore[0]) ||
       !GetPacketData(packet, patchMacOffset + sizeof sumBefore[0], ETH_ALEN -
                      sizeof sumBefore[0], &sumBefore[1])) {
      VNETKdPrint((MODULE_NAME "%s:  couldn't get original MAC address in "
                   "packet\n", logPrefix));
      return FALSE;
   }

   sumAfter[0] = *(uint32 *)macAddress;
   sumAfter[1] = *(uint16 *)(macAddress + sizeof sumAfter[0]);

   sumBefore[0] = SUM32(sumBefore[0]);
   sumAfter[0] = SUM32(sumAfter[0]);

   sumDiff = CalcChecksumDiff(sumBefore[0], sumAfter[0]);
   newChecksum = UpdateSum(oriChecksum, sumDiff);
   sumDiff = CalcChecksumDiff(sumBefore[1], sumAfter[1]);
   newChecksum = UpdateSum(newChecksum, sumDiff);

   for (i = 0; i < ETH_ALEN; i++) {
      if (!SetPacketByte(packet, patchMacOffset + i, macAddress[i])) {
         VNETKdPrint((MODULE_NAME "%s:  couldn't patch MAC address.\n",
                      logPrefix));
         return FALSE;
      }
   }

   if (!SetPacketByte(packet, checksumOffset, newChecksum >> 8) ||
       !SetPacketByte(packet, checksumOffset + 1, newChecksum & 0xff)) {
      VNETKdPrint((MODULE_NAME "%s:  couldn't set new checksum in packet.\n",
                   logPrefix));
      return FALSE;
   }

   return TRUE;
}


#if defined(_WIN32) && NDIS_SUPPORT_NDIS6


/*
 *----------------------------------------------------------------------
 *
 * IPv6IsMulticast --
 *
 *      Determines if a given IPv6 address is a multicast address or not.
 *
 * Results:
 *      TRUE/FALSE.
 *
 * Side effects:
 *       None.
 *
 *----------------------------------------------------------------------
 */

static INLINE_SINGLE_CALLER Bool
IPv6IsMulticast(const IPv6Addr *ipv6Addr) // IN:
{
   return *(uint8 *)ipv6Addr == 0xff;
}


/*
 *----------------------------------------------------------------------
 *
 * IsPacketDestinationTheHost --
 *
 *      Given an IPv6 packet determines if it is destined to a unicast IPv6
 *      address associated with the host interface.
 *
 * Results:
 *      TRUE/FALSE.
 *
 * Side effects:
 *       None.
 *
 *----------------------------------------------------------------------
 */

static INLINE_SINGLE_CALLER Bool
IsPacketDestinationTheHost(const uint8 *dstMACAddr,  // IN:
                           const uint8 *smacAddress, // IN:
                           const IPv6Addr *ipv6Addr) // IN:
{
   if (IS_MULTICAST(dstMACAddr)) { /* Multicast or broadcast. */
      return IPv6IsMulticast(ipv6Addr) ? FALSE :
                                         BridgeIPv6MatchAddrMAC(ipv6Addr,
                                                                smacAddress);
   } else { /* Unicast. */
      return MAC_EQ(dstMACAddr, smacAddress);
   }
}

#endif

#define NEXTHDR_HOP        0
#define NEXTHDR_ROUTING    43
#define NEXTHDR_AUTH       51
#define NEXTHDR_DEST       60
#define NEXTHDR_MOBILITY   135


/*
 *----------------------------------------------------------------------
 *
 * SmacWalkIPv6ExtensionHeaders --
 *
 *      Walks (some) IPv6 extension headers in a given packet.  The packet's
 *      length and offset within packet of start of IPv6 header & payload
 *      is input to this function.  There are two outputs: the next header
 *      type of the PDU contained (when it does not match any of the
 *      extension headers that we walk or a higher level PDU type such as
 *      TCP or UDP), and the offset of this PDU within the packet.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
SmacWalkIPv6ExtensionHeaders(SMACPacket *packet,        // IN:
                             const uint32 packetLen,    // IN:
                             const uint32 ethHeaderLen, // IN:
                             uint8 *nextHeaderPtr,      // OUT:
                             uint32 *nextHeaderOffset)  // OUT:
{
   uint8 nextHeader;
   uint32 offset; /* Bytes inspected. */
   uint32 headerLen; /* Length of current header. */

   ASSERT(packet && packetLen >= IPv6_HEADER_LEN + ethHeaderLen &&
          nextHeaderPtr && nextHeaderOffset);

   if (!GetPacketData(packet, ethHeaderLen + IPv6_NEXT_HEADER_OFFSET,
                      sizeof nextHeader, &nextHeader)) {
      return FALSE;
   }

   headerLen = IPv6_HEADER_LEN;
   offset = ethHeaderLen + headerLen;
   while (offset < packetLen - 1) { /* Subtract 1 b/c we lookahead 2 bytes. */
      switch (nextHeader) {
      case NEXTHDR_HOP:
      case NEXTHDR_ROUTING:
      case NEXTHDR_AUTH:
      case NEXTHDR_DEST:
      case NEXTHDR_MOBILITY:
         if (!GetPacketData(packet, offset + 1, sizeof(uint8), &headerLen)) {
            return FALSE;
         }
         if (nextHeader == NEXTHDR_AUTH) {
            headerLen = (headerLen + 2) << 2;
         } else {
            headerLen = (headerLen + 1) << 3;
         }
         if (!GetPacketData(packet, offset, sizeof nextHeader, &nextHeader)) {
            return FALSE;
         }
         offset += headerLen;
         break;

      default:
         goto out;
      }
   }

out:
   *nextHeaderOffset = offset;
   *nextHeaderPtr = nextHeader;
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcessOutgoingIPv6Packet --
 *
 *      This function is used to examine IPv6 packets, and make any
 *      modifications (if necessary).  Currently it assumes no IPv6
 *      extension options are present, and processes ICMPv6 payloads
 *      only.  If the source link layer option in an NDP (ICMPv6) packet
 *      is present, this function replaces that MAC address with the
 *      MAC address of the wireless PNIC so that replies (neighbour
 *      advertisements) would be addressed to the host.  The ICMPv6
 *      checksums are updated accordingly.
 *      The caller should have determined that the packet is IPv6
 *      before calling this function.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      May modify the contents of the packet.
 *
 *----------------------------------------------------------------------
 */

static Bool
ProcessOutgoingIPv6Packet(SMACPacket *packet,       // IN:  cloned packet
                          uint32 ethHeaderLen,      // IN:
                          const uint8 *smacAddress, // IN:  wireless PNIC's MAC
                          Bool *toHost)             // OUT: destined to host?
{
   uint8 nextHeader = 0; /* Silence compiler warnings. */
   uint32 nextHeaderOffset = 0; /* Silence compiler warnings. */
   const uint32 packetLen = GetPacketLength(packet);
#if defined(_WIN32) && NDIS_SUPPORT_NDIS6
   uint8 dstMACAddr[ETH_ALEN];
#endif

   UNREFERENCED_PARAMETER(toHost);

   /*
    * Assume caller has done certain checks.
    */

   ASSERT(packetLen >= IPv6_HEADER_LEN + ethHeaderLen);
   ASSERT(toHost && !*toHost);

#if defined(_WIN32) && NDIS_SUPPORT_NDIS6
   {
      IPv6Addr dstAddr;

      if (!GetPacketData(packet, 0, sizeof dstMACAddr, &dstMACAddr)) {
         VNETKdPrint((MODULE_NAME "ProcessOutgoingIPv6: couldn't get "
                      "destination MAC address.\n"));
         return FALSE;
      }
      if (!GetPacketData(packet, ethHeaderLen + IPv6_HEADER_DST_ADDR_OFFSET,
                         sizeof dstAddr, &dstAddr)) {
         VNETKdPrint((MODULE_NAME "ProcessOutgoingIPv6: couldn't get "
                      "destination IPv6 address.\n"));
         return FALSE;
      }

      *toHost = IsPacketDestinationTheHost(dstMACAddr, smacAddress, &dstAddr);
      if (*toHost) {
         return TRUE;
      }
   }
#endif

   if (!SmacWalkIPv6ExtensionHeaders(packet, packetLen, ethHeaderLen,
                                     &nextHeader, &nextHeaderOffset)) {
      VNETKdPrint((MODULE_NAME "ProcessOutgoingIPv6:  couldn't get "
                   "next header.\n"));
      return FALSE;
   }

   switch (nextHeader) {
   case 58: { /* ICMPv6. */
      uint8 type;
      uint8 optionToFind;
      uint32 ndpMsgLen;
      const uint32 icmpv6ChecksumOffset = nextHeaderOffset +
                                          ICMPv6_CHECKSUM_OFFSET;

      if (!GetPacketData(packet, nextHeaderOffset + ICMPv6_TYPE_OFFSET,
                         sizeof type, &type)) {
         VNETKdPrint((MODULE_NAME "ProcessOutgoingIPv6:  couldn't get ICMPv6 "
                      "type value.\n"));
         return FALSE;
      }

      switch (type) {
      case ICMPv6_NDP_RTR_SOLICITATION:
         optionToFind = ICMPv6_NDP_OPTION_SRC_MAC;
         ndpMsgLen = ICMPv6_NDP_RTR_SOL_LEN;
         break;

      case ICMPv6_NDP_RTR_ADVERTISEMENT:
         optionToFind = ICMPv6_NDP_OPTION_SRC_MAC;
         ndpMsgLen = ICMPv6_NDP_RTR_ADV_LEN;
         break;

      case ICMPv6_NDP_NBR_SOLICITATION:
         optionToFind = ICMPv6_NDP_OPTION_SRC_MAC;
         ndpMsgLen = ICMPv6_NDP_NBR_LEN;
         break;

      case ICMPv6_NDP_NBR_ADVERTISEMENT:
         optionToFind = ICMPv6_NDP_OPTION_TARGET_MAC;
         ndpMsgLen = ICMPv6_NDP_NBR_LEN;
         break;

      default: /* Nothing left to do. */
         return TRUE;
      }

#if defined(_WIN32) && NDIS_SUPPORT_NDIS6
      if (type == ICMPv6_NDP_NBR_SOLICITATION) {
         IPv6Addr targetAddr;

         if (!GetPacketData(packet, nextHeaderOffset + ICMPv6_TARGET_IP_OFFSET,
                            sizeof targetAddr, &targetAddr)) {
            VNETKdPrint((MODULE_NAME "ProcessOutgoingIPv6:  couldn't get "
                         "target IPv6 address in ICMPv6 NDP packet.\n"));
            return FALSE;
         }

         *toHost = IsPacketDestinationTheHost(dstMACAddr, smacAddress,
                                              &targetAddr);
         if (*toHost) {
            return TRUE;
         }
      }
#endif

      nextHeaderOffset += ndpMsgLen; /* Start of NDP options. */

      /*
       * Walk through all NDP options searching for the option we are
       * interested in.
       */

      while (nextHeaderOffset < packetLen) {
         uint8 option;
         uint8 optionLen;

         if (!GetPacketData(packet, nextHeaderOffset +
                            ICMPv6_NDP_OPTION_TYPE_OFFSET, sizeof option,
                            &option)) {
            VNETKdPrint((MODULE_NAME "ProcessOutgoingIPv6:  couldn't get "
                         "ICMPv6 NDP option type value.\n"));
            return FALSE;
         }

         /*
          * Replace the source/target MAC address option with the MAC address
          * of the wireless PNIC when we find the option we are looking for.
          */

         if (option == optionToFind) {
            return PatchMacAddrFixChecksum(packet, packetLen,
                                           icmpv6ChecksumOffset,
                                           nextHeaderOffset +
                                           ICMPv6_NDP_MAC_OFFSET, smacAddress,
                                           "Outgoing IPv6 packet");
         }

         if (!GetPacketData(packet, nextHeaderOffset +
                            ICMPv6_NDP_OPTION_LEN_OFFSET, sizeof optionLen,
                            &optionLen)) {
            VNETKdPrint((MODULE_NAME "ProcessOutgoingIPv6:  couldn't get "
                         "ICMPv6 NDP option length value.\n"));
            return FALSE;
         }

         /*
          * Option length is in units of 8 bytes.  Option length of 0 is
          * invalid and such packets must be discarded.
          */

         if (UNLIKELY(!optionLen)) {
            VNETKdPrint((MODULE_NAME "ProcessOutgoingIPv6:  got invalid "
                         "option length (0).\n"));
            return FALSE;
         }
         nextHeaderOffset += (uint32)optionLen << 3;
      }

      break;
   }

   default:
      VNETKdPrint((MODULE_NAME "ProcessOutgoingIPv6:  Unknown/unhandled "
                   "next header type %u in IPv6 packet.\n", nextHeader));
      break;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcessIncomingIPv4Packet --
 *
 *      This function is used to examine IPv4 packets, and make any
 *      adjustments (if necessary) to the IP header and/or payload.
 *      The third paramter is optional (specify NULL if not provided).
 *
 *      Currently **this function does nothing** but print out debug
 *      information about the contents of the IP packet
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May modify the contents of the packet.  Length should be 
 *      unchanged.
 *
 *----------------------------------------------------------------------
 */

/* 
 * NOTE: this abstraction doesn't match reality because we may only have
 * a partial packet received so far.  But, since we're not modifying the
 * packet (and only trying to ID/classify it) then this shouldn't matter.
 */

#ifdef DBG

static void
ProcessIncomingIPv4Packet(SMACPacket *packet, // IN/OUT: incoming packet
			  Bool knownMacForIp) // IN: IP is present in table?
{
   uint8 proto;    // protocol within ip
   uint16 ipFlags; // offset and flags

   /*
    * Should certain checks, e.g., IP version field, be checked here,
    * or assume that caller has already performed this check?
    */

   /*
    * Presuming that caller has performed checks, and that IP header
    * comes exactly ETH_HLEN bytes into the packet 
    */

   if (GetPacketLength(packet) < ETH_HLEN + IP_HEADER_LEN) {
      return;
   }

   if (!GetPacketData(packet, ETH_HLEN + IP_HEADER_PROTO_OFFSET, sizeof proto, 
                      &proto)) {
      VNETKdPrint((MODULE_NAME "ProcessIncoming: couldn't get protocol\n"));
      return;
   }

   if (!GetPacketData(packet, ETH_HLEN + IP_HEADER_FLAGS_OFFSET, sizeof ipFlags,
                      &ipFlags)) {
      VNETKdPrint((MODULE_NAME "ProcessIncoming: couldn't get ip flags\n"));
      return;
   }

   /*
    * Verify offset=0 and M=0: mask off "fragment" flag,
    * all others should be zero
    */

   if (ipFlags & HTONS((uint16)~(0x4000))) {
      VNETKdPrint((MODULE_NAME "ProcessIncoming: got a fragmented IP "
		   "(ipFlags %04x), so not performing higher-level processing\n",
		   ipFlags));
      return;
   }

   switch (proto) {
   case 1: { // ICMP
      uint32 ipHeaderLen = 0; // length of IP header
      uint32 ipVer = 0;       // version of IP header
      uint16 ipLen = 0;       // total length of datagram
      uint8 firstByte = '\0';  // used to safely read from packet

      if (!GetPacketData(packet, ETH_HLEN, sizeof firstByte, &firstByte)) {
	 VNETKdPrint((MODULE_NAME "ProcessIncoming: ICMP read ver error\n"));
	 return;
      }
      ipHeaderLen = 4 * (firstByte & 0xf); // length of IP header
      ipVer = firstByte>>4; // version of IP header

      if (!GetPacketData(packet, ETH_HLEN + 2, sizeof ipLen, &ipLen)) {
	 VNETKdPrint((MODULE_NAME "ProcessIncoming: ICMP read len error\n"));
	 return;
      }
      ipLen = NTOHS(ipLen);

      if (ipVer == 4 && GetPacketLength(packet) >= 
          ipHeaderLen + 2 * 4 + ETH_HLEN) {
	 uint8 typeField = 0;
	 uint8 codeField = 0;
	 if (!GetPacketData(packet, ETH_HLEN + ipHeaderLen, 
			    sizeof typeField, &typeField) ||
	     !GetPacketData(packet, ETH_HLEN + ipHeaderLen + 1, 
			    sizeof codeField, &codeField)) {
	    VNETKdPrint((MODULE_NAME "ProcessIncoming: ICMP "
			 "read ICMP error\n"));
	    return;
	 }

	 W_VNETKdPrint((MODULE_NAME "ProcessIncoming: got ICMP, "
	                "type = %d, code = %d,\n", typeField, codeField));
      } else {
	 VNETKdPrint((MODULE_NAME "ProcessIncoming: got ICMP, "
	              "but couldn't process\n"));
      }
      return;
   }
   case 2: // IGMP
      W_VNETKdPrint((MODULE_NAME "ProcessIncoming: got IGMP\n"));
      return;
   case 4: // IP in IP
      VNETKdPrint((MODULE_NAME "ProcessIncoming: got IP in IP\n"));
      return;
   case 6: // TCP
      W_VNETKdPrint((MODULE_NAME "ProcessIncoming: got TCP\n"));
      return;
   case 17: { // UDP
      uint32 ipHeaderLen = 0;  // length of IP header
      uint32 ipVer = 0;        // version of IP header
      uint16 ipLen = 0;        // total length of datagram
      uint8  firstByte;         // used to safely read from packet

      if (!GetPacketData(packet, ETH_HLEN, sizeof firstByte, &firstByte)) {
	 VNETKdPrint((MODULE_NAME "ProcessIncoming: ICMP read ver error\n"));
	 ASSERT(0);
	 return;
      }
      ipHeaderLen = 4 * (firstByte&0xf); // length of IP header
      ipVer = firstByte >> 4; // version of IP header
      
      /*
       * verifify that IP fragment offset is 0
       */

      if (!GetPacketData(packet, ETH_HLEN + 2, sizeof ipLen, &ipLen)) {
	 VNETKdPrint((MODULE_NAME "ProcessIncoming: ICMP read len error\n"));
	 ASSERT(0);
	 return;
      }
      ipLen = NTOHS(ipLen);
      
      W_VNETKdPrint((MODULE_NAME "ProcessIncoming: got UDP, "
	 	    "IP ver %d, header len %d,"
	            "overall len %d\n", ipVer, ipHeaderLen, ipLen));
      
      if (ipVer == 4 && GetPacketLength(packet) >= 
            ipHeaderLen + 2 * 4 + ETH_HLEN) {
	 uint16 srcPort  = 0;
	 uint16 destPort = 0;
	 uint8 srcAddr[4];
	 uint8 destAddr[4];

	 if (!GetPacketData(packet, ipHeaderLen + ETH_HLEN, 
			    sizeof srcPort, &srcPort) || 
	     !GetPacketData(packet, ipHeaderLen + ETH_HLEN + 2, 
			    sizeof destPort, &destPort)) {
	    VNETKdPrint((MODULE_NAME "ProcessIncoming: read "
	 	    	 "UDP header error\n"));
	    ASSERT(0);
	    return;
	 }

	 srcPort  = NTOHS(srcPort);
	 destPort = NTOHS(destPort);

	 if (!GetPacketData(packet, ETH_HLEN + IP_HEADER_SRC_ADDR_OFFSET, 
			    sizeof srcAddr, srcAddr) ||
	     !GetPacketData(packet, ETH_HLEN + IP_HEADER_DEST_ADDR_OFFSET, 
			    sizeof destAddr, destAddr)) {
	    VNETKdPrint((MODULE_NAME "ProcessIncoming: couldn't read "
			 "source and/or dest IP addr\n"));
	    return;
	 }
	 
	 W_VNETKdPrint((MODULE_NAME "ProcessIncoming: got UDP, src "
			"%d.%d.%d.%d %d dest %d.%d.%d.%d %d\n", 
			srcAddr[0], srcAddr[1], srcAddr[2], srcAddr[3], 
			srcPort, destAddr[0], destAddr[1], destAddr[2], 
			destAddr[3], destPort));
	 
	 if (destPort == 67) {

	    if (knownMacForIp) {
	       W_VNETKdPrint((MODULE_NAME "ProcessIncoming: got client DHCP "
		              "packet destined for THIS server\n"));
	    } else {
	       W_VNETKdPrint((MODULE_NAME "ProcessIncoming: got client DHCP "
		              "packet destined for a server\n"));
	    }

	 } else if (destPort == 68) {

	    if (knownMacForIp) {
	       W_VNETKdPrint((MODULE_NAME "ProcessIncoming: got server DHCP "
		              "packet destined for THIS client\n"));
	    } else {
	       W_VNETKdPrint((MODULE_NAME "ProcessIncoming: got server DHCP "
		              "packet destined for a client\n"));
	    }

	 }
      }
      return;	    
    }
   case 27: // RDP
      VNETKdPrint((MODULE_NAME "ProcessIncoming: got RDP\n"));
      return;
   case 41: // IPv6
      VNETKdPrint((MODULE_NAME "ProcessIncoming: got IPv6\n"));
      return;
   case 51: // Authentication Header
      VNETKdPrint((MODULE_NAME "ProcessIncoming: got Authentication Header\n"));
      return;
   case 55: // Mobile IP
      VNETKdPrint((MODULE_NAME "ProcessIncoming: got Mobile IP\n"));
      return;
   case 103: // PIM
      VNETKdPrint((MODULE_NAME "ProcessIncoming: got PIM\n"));
      return;
   case 111: // IPX in IP
      VNETKdPrint((MODULE_NAME "ProcessIncoming: got IPX in IP\n"));
      return;
   default:
      VNETKdPrint((MODULE_NAME "ProcessIncoming: Unknown/unhandled service "
	           "reported by IP packet\n"));
      return;
   }
}

#endif /* DBG */

/*
 *----------------------------------------------------------------------
 *
 * SMAC_InitState --
 *
 *      Initialize adapter SMAC state.  Presumes that the 
 *      supplied adapter object has already been initialized to zero.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Initializes adapter SMAC state.
 *
 *----------------------------------------------------------------------
 */

void SMACINT
SMAC_InitState(SMACState **ptr) // OUT: pointer to alloced/inited state
{
   SMACState * state;
   VNETKdPrintCall(("SMAC_InitState"));
   ASSERT(ptr);

   state = ALLOCATEMEMORY(sizeof *state, REORDER_TAG('SMAC'));
   if (state == NULL) {
      *ptr = NULL;
      return;
   }

   MEMSET(state, 0, sizeof *state);

   VNETKdPrint((MODULE_NAME "SMAC_InitState: state %p\n", state));

   INITSPINLOCK(&(state->smacSpinLock));
#ifndef _WIN32
   if (state->smacSpinLock == NULL) {
      VNETKdPrint((MODULE_NAME "SMAC_InitState: coudln't initialize spinlock."
                   "Freeing state.\n"));
      FREEMEMORY(state);
      state = NULL;
   }
#endif
   VNETKdPrintReturn(("SMAC_InitState"));
   *ptr = state;
}


/*
 *----------------------------------------------------------------------
 *
 * SMAC_SetMac --
 *
 *      Set MAC stored in SMAC state.  Presumes that the 
 *      supplied SMAC state has already been initialized.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Sets MAC in SMAC state.
 *
 *----------------------------------------------------------------------
 */

void SMACINT
SMAC_SetMac(SMACState *state,  // IN: state to update
	    const uint8 *mac)  // IN: pointer to host adapter's MAC
{
   VNETKdPrintCall(("SMAC_SetMac"));
   ASSERT(state);

   VNETKdPrint((MODULE_NAME "SMAC_SetMac: state %p mac %p\n", 
		state, mac));
#ifdef DBG
   if (mac) {
      VNETKdPrint((MODULE_NAME 
		   "mac 0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x\n", 
		   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]));
   }
#endif

   /* 
    * There's likely some atomicity issues here, but grabbing a lock
    * here won't help since the readers won't be grabbing a lock.
    * The only time an update can occur with traffic being processed
    * is on Linux, which I don't see as a big deal given the lack
    * of demand for this feature on that OS.
    */

   if (mac) {
      MEMCPY(state->macAddress, mac, ETH_ALEN);
   } else {
      MEMSET(state->macAddress, 0, ETH_ALEN);
   }
   VNETKdPrintReturn(("SMAC_SetMac"));
}


/*
 *----------------------------------------------------------------------
 *
 * SMAC_SetForwardUnknownPackets --
 *
 *      Initialize adapter SMAC state.  Presumes that the 
 *      supplied adapter object has already been initialized to zero.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Initializes adapter SMAC state.
 *
 *----------------------------------------------------------------------
 */

void SMACINT
SMAC_SetForwardUnknownPackets(SMACState *state,    // IN: pointer to smac state
			      Bool forwardUnknown) // IN: T/F to forward
{
   VNETKdPrintCall(("SMAC_SetForwardUnknownPackets"));
   ASSERT(state);
   state->smacForwardUnknownPackets = forwardUnknown;
   VNETKdPrintReturn(("SMAC_SetForwardUnknownPackets"));
}


/*
 *----------------------------------------------------------------------
 *
 * SMAC_CleanupState --
 *
 *      Deallocates adapter SMAC state.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Deallocates adapter SMAC state.
 *
 *----------------------------------------------------------------------
 */

void SMACINT
SMAC_CleanupState(SMACState **ptr) // IN: state to dealloc
{
   uint32 i = 0;
   SMACState *state;
#ifdef _WIN32
   KIRQL irql;
#endif
   SPINLOCKINIT();

   VNETKdPrintCall(("SMAC_CleanupState"));
   ASSERT(ptr);

   state = *ptr;
   if (state == NULL) {
      return;
   }
   *ptr = NULL;

   RAISEIRQL();
   ACQUIRESPINLOCK(&state->smacSpinLock);

   for (i = 0; i < SMAC_HASH_TABLE_SIZE; ++i) {
      IPmacLookupEntry * entry = state->IPlookupTable[i];
      while (entry) {
	 IPmacLookupEntry * next = entry->ipNext;
	 VNETKdPrintCall(("--deleted entry\n"));
	 FREEMEMORY(entry);
	 --state->numberOfIPandMACEntries;
	 entry = next;
      }
   }
   if (state->numberOfIPandMACEntries != 0) {
      VNETKdPrint((MODULE_NAME "SMAC_CleanupState: "
                   "entry count is non-zero: %u\n",
                   state->numberOfIPandMACEntries));
      ASSERT(state->numberOfIPandMACEntries == 0);
   }

   RELEASESPINLOCK(&state->smacSpinLock);
   FREESPINLOCK(&state->smacSpinLock);
   LOWERIRQL();
   FREEMEMORY(state);

   VNETKdPrintReturn(("SMAC_CleanupState"));
}


/*
 *----------------------------------------------------------------------
 *
 * LookupTypeClass --
 *
 *      Examines and classifies the protocol type of an ethernet frame
 *
 * Results:
 *       Returns the appropriate EthClass classification for the 
 *       specified ethernet media type.
 *       
 *
 * Side effects:
 *      May modify the contents of the packet.  Length should be 
 *      unchanged.
 *
 *----------------------------------------------------------------------
 */

EthClass
LookupTypeClass(unsigned short typeValue) // IN: ethernet type 
{
   if (typeValue <=1500) {
      return EthClassCommon;
   }
   if (typeValue >= 0x600) {
      switch (typeValue) {
      case 0x0800: // IPv4
	 return EthClassIPv4;
      case 0x0806: // ARP
	 return EthClassARP;
      case 0x0BAD: // Banyan Vines
	 return EthClassUncommon;
      case 0x2000: // Cisco CDP
	 return EthClassCommon;
      case 0x6002: // DEC MOP Remote Console
      case 0x6558: // Trans Ether Bridging [RFC1701]
      case 0x6559: // Raw Frame Relay [RFC1701]
	 return EthClassUncommon;
      case 0x8035: // Reverse ARP
	 return EthClassARP;
      case 0x809B: // AppleTalk
      case 0x80F3: // AppleTalk AARP
	 return EthClassUncommon;
      case 0x8100: // VLAN special type
	 return EthClassVLAN;
      case 0x8137: // Novell 8137
      case 0x8138: // Novell 8138
         return EthClassUncommon;
      case 0x86DD: // IPv6
         return EthClassIPv6;
      case 0x876B: // TCP/IP Compression [RFC1144]
	 return EthClassUncommon;
      case 0x886f: // Microsoft 886f
	 return EthClassCommon;
      case 0x888e: // 802.1x (aka EAPOL)
      case 0x88c7: // 802.11i pre-authentication (treated as EAPOL)
         return EthClassEAPOL;
      default:
	 return EthClassUnknown;
      }
   } else {
      return EthClassUnknown;
   }
}


#ifdef DBG

/*
 *----------------------------------------------------------------------
 *
 * LookupTypeName --
 *
 *      Examines and classifies the protocol type of an ethernet frame
 *
 * Results:
 *       Provides textual name of type in 'type' pointer.
 *
 *
 * Side effects:
 *      May modify the contents of the packet.  Length should be
 *      unchanged.
 *
 *----------------------------------------------------------------------
 */

#ifdef _WIN32
_Use_decl_annotations_
#endif
void
LookupTypeName(unsigned short typeValue, // IN: ethernet type
               char * type,              // IN/OUT: string to store type name
               size_t typeLen)           // IN: size of out buffer
{
   if (!type) {
      return;
   }

   if (typeValue <=1500) {
      SNPRINTF((type, typeLen, "length %u", typeValue));
      return;
   }
#ifdef _WIN32
#define STRCPY_S(a,b,c) RtlStringCbCopyA(a,b,c)
#else
#define STRCPY_S(a,b,c) MEMCPY(a, c, sizeof c)
#endif
   if (typeValue >= 0x600) {
      switch (typeValue) {
      case 0x0800:
         STRCPY_S(type, typeLen, "IPv4");
         return;
      case 0x0806:
         STRCPY_S(type, typeLen, "ARP");
         return;
      case 0x0BAD:
         STRCPY_S(type, typeLen, "Banyan VINES");
         return;
      case 0x2000:
         STRCPY_S(type, typeLen, "Cisco CDP");
         return;
      case 0x6002:
         STRCPY_S(type, typeLen, "DEC MOP Remote Console");
         return;
      case 0x6558:
         STRCPY_S(type, typeLen, "Trans Ether Bridging [RFC1701]");
         return;
      case 0x6559:
         STRCPY_S(type, typeLen, "Raw Frame Relay [RFC1701]");
         return;
      case 0x8035:
         STRCPY_S(type, typeLen, "Reverse ARP");
         return;
      case 0x809B:
         STRCPY_S(type, typeLen, "AppleTalk");
         return;
      case 0x80F3:
         STRCPY_S(type, typeLen, "AppleTalk AARP");
         return;
      case 0x8100:
         STRCPY_S(type, typeLen, "VLAN special type");
         return;
      case 0x8137:
         STRCPY_S(type, typeLen, "Novell 8137");
         return;
      case 0x8138:
         STRCPY_S(type, typeLen, "Novell 8138");
         return;
      case 0x86DD:
         STRCPY_S(type, typeLen, "IPv6");
         return;
      case 0x876B:
         STRCPY_S(type, typeLen, "TCP/IP Compression [RFC1144]");
         return;
      case 0x886f:
         STRCPY_S(type, typeLen, "Microsoft 886f");
         return;
      case 0x888e:
         STRCPY_S(type, typeLen, "EAPOL");
         return;
      case 0x88c7:
         STRCPY_S(type, typeLen, "802.11i pre-auth");
         return;
      default:
         SNPRINTF((type, typeLen, "unknown type 0x%04x", typeValue));
         return;
      }
   } else {
      SNPRINTF((type, typeLen, "invalid value 0x%04x", typeValue));
      return;
   }
}
#endif

/*
 * Checksum-related functions.  In certain cases the payload of a UDP
 * packet needs to be modified.  The following functions are used to
 * calculate the new checksum based on the old checksum and an offset
 * of the changes.
 */


/*
 *----------------------------------------------------------------------
 *
 * SUM32 --
 *
 *      performs 2's complement sum of the high bits and low bits
 *      in a 32-bit word, resulting in a 16-bit number (with
 *      potentially additional bits containing overflow)
 *
 * Results:
 *      Returns result described above
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint32
SUM32(uint32 in) { // IN: 32-bit number
   return (in & 0xffff) + (in >> 16);
}


/*
 *----------------------------------------------------------------------
 *
 * CalcChecksumDiff --
 *
 *      Computes the differences between two checksums
 *
 * Results:
 *      Returns result described above
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static uint32
CalcChecksumDiff(uint32 sumBefore, // IN: checksum before modification 
		 uint32  sumAfter) // IN: checksum after modification
{
   uint32 diff; // stores resume to return
   
   sumBefore = SUM32(sumBefore); // Convert sum to 16-bit + overflow
   sumBefore = SUM32(sumBefore); // Convert sum to 16-bit
   sumAfter = SUM32(sumAfter);   // Convert sum to 16-bit + overflow
   sumAfter = SUM32(sumAfter);   // Convert sum to 16-bit
   
   /*
    * 2's complement versus 1's complement arthmetic requires the
    * following piece of code.  I don't completely understand
    * why it's needed, but my sources (and my own testing) say it 
    * needs to be here.
    */

   if (sumBefore > sumAfter) { 
      --sumAfter;
   }

   diff = sumAfter - sumBefore; // subtrace to get delta
   diff = SUM32(diff);          // incorporate overflow to get 16-bits
   return diff;
}


/*
 *----------------------------------------------------------------------
 *
 * UpdateSum --
 *
 *      Computes a new internet checksum by using an existing
 *      checksum and a delta of changes to that checksum.
 *
 * Results:
 *      Returns new checksum
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static uint16
UpdateSum(uint16 oricheck, // IN: old checksum
	  uint32 sumDiff)  // IN: delta of changes
{
   uint32 sum;
   uint16 sumShort;
   if (sumDiff == 0) {
      return oricheck;
   }
   
   sum = ( ~NTOHS(oricheck) & 0xffff); // undo net order & bit complement
   sum += sumDiff;   // add in difference
   sum = SUM32(sum); // wrap any overflow
   sum = SUM32(sum); // wrap any overflow (again; last wrap may cause overflow)
   sumShort = ~(uint16)(sum); // truncate to 16-bits and complement bits
   return HTONS(sumShort); // return result in network order
}


/*
 *----------------------------------------------------------------------
 *
 * GetSystemUptime --
 *
 *      Return current uptime of system, this is essentially a wrapper
 *      that checks for overflow.  When 
 *      overflow occurs the existing last used times reduced by half.
 *      As part of the solution, the returned value always has the 
 *      highest bit turned on.
 *
 *      Previously, this was a wrapper for NdisGetSystemUptime().
 *      Recent updates to the DDK/WDK have confused this function - there
 *      is now a depricated 32-bit version and a 64-bit version only 
 *      available on newer version of the OS.  Rather than trying to be
 *      clever, we will use KeQueryTickCount instead.
 *
 *      This function should only be called with the lock held.
 *
 * Results:
 *      Returns uptime of system (skewed so always in upper half 
 *      of numeric range for 32-bits)
 *
 * Side effects:
 *      Updates the "last uptime" stored in adapter struct, and may
 *      modify the uptimes of all entries in the hash table.
 *
 *----------------------------------------------------------------------
 */

static SmacLastAccess 
GetSystemUptime(SMACState *state) // IN: smac state
{
   SmacLastAccess currentUptime;

#ifdef _WIN32
   {
      LARGE_INTEGER arg;
      KeQueryTickCount(&arg);
      currentUptime = arg.QuadPart;
   }
#else
   currentUptime = SMACL_GetUptime();
#endif
   /*
    * Force on the highest bit.  This basically means that recent 
    * values have the high bit turned on, while values that were 
    * obtained prior to the last overflow will have the highest bit
    * (and potentially other neighboring bits) turned off.
    */

   currentUptime |= (SmacLastAccess)1 << (sizeof currentUptime * 8 - 1);

   /* If overflow occurred, then reduce all existing values by half */
   if (currentUptime < state->lastUptimeRead) {
      uint32 i;
      VNETKdPrint((MODULE_NAME "GetSystemUptime: overflow detected, "
		   "adjusting counters\n"));
      for (i = 0; i < SMAC_HASH_TABLE_SIZE; ++i) {
	 IPmacLookupEntry *entry = state->IPlookupTable[i];
	 while (entry) {
	    entry->lastAccess >>= 1; /* reduce value by half */
	    entry = entry->ipNext;
	 }
      }
   }
   state->lastUptimeRead = currentUptime;
   return currentUptime;
}


/*
 *----------------------------------------------------------------------
 *
 * GetPacketLength --
 *
 *      Returns the total length of data in a packet
 *
 * Results:
 *      returns length of packet
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint32 
GetPacketLength(SMACPacket *packet) // IN: packet
{
   ASSERT(packet);
#ifdef _WIN32
   return packet->buf1Len + packet->buf2Len;
#elif defined __linux__
   ASSERT(packet->skb);
   return (uint32)packet->len;
#else /* __APPLE__ */
   ASSERT(packet->m);
   return SMACL_PacketLength(packet->m);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * GetPacketData --
 *
 *      Copies select portion of packet
 *
 * Results:
 *      TRUE if data was safely copied, otherwise FALSE (e.g., offset
 *      too large, packet too small, etc).
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool 
GetPacketData(SMACPacket *packet, // IN: packet to copy from
	      uint32 offset,	  // IN: offset in packet to copy from
	      uint32 length,	  // IN: length to copy
	      void *data)	  // OUT: data (caller must pass in buffer >= length)
{
#ifdef _WIN32
   uint32 copyOffset = 0;
   uint32 copyLength = 0;
#endif

   ASSERT(packet);
   ASSERT(data);
   ASSERT(length > 0);

   /* check length, be sure to handle case where offset = -1, length > 0 */
   if (length == 0 || offset + length > GetPacketLength(packet) || 
       offset + length < offset) {
      /* packet not long enough for data */
      return FALSE;
   }

#ifdef _WIN32
   /* if offset starts in the first buffer, then copy from first buffer */   
   if (offset < packet->buf1Len) {
      copyOffset = offset;
      copyLength = (offset + length > packet->buf1Len)?
			(packet->buf1Len - copyOffset):(length);

      MEMCPY(data, ((uint8*)packet->buf1) + copyOffset, copyLength);
      offset = packet->buf1Len; /* advance offset to start of second buffer */

      data = ((uint8*)data) + copyLength;
      length -= copyLength;
   }
   /* copy any remaining data from second buffer */   
   if (length) {
      copyOffset = offset - packet->buf1Len;
      copyLength = length;
      MEMCPY(data, ((uint8*)packet->buf2) + copyOffset, copyLength);
   }   

#elif __linux__

   MEMCPY(data, packet->startOfData + offset, length);

#else /* __APPLE__ */

   SMACL_CopyDataFromPkt(packet->m, offset, data, length);

#endif /* _WIN32 */

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * ClonePacket --
 *
 *      Makes a private copy of the incoming packet.  This modified
 *      copy is private and can be modified at will.  Caller is
 *      responsible for freeing the cloned packet.
 *
 * Results:
 *      TRUE if clone was successful, otherwise FALSE.
 *
 * Side effects:
 *      Duplicates a packet.
 *
 *----------------------------------------------------------------------
 */

static Bool 
ClonePacket(SMACPackets *packets)   // IN: struct in which to clone packet
{
#ifdef _WIN32
   VNetPacket * packetClone = NULL;

   ASSERT(packets);

   packetClone = VNet_PacketAllocate(packets->orig.buf1Len + 
                                     packets->orig.buf2Len, 
				     "SMAC_CheckPacketToHost");
   if (!packetClone) {
      VNETKdPrint((MODULE_NAME "  ToHost: couldn't clone packet\n"));
      return FALSE;
   }

   if (packets->orig.buf1Len) {
      MEMCPY(packetClone->data, packets->orig.buf1, packets->orig.buf1Len);
   }
   if (packets->orig.buf2Len) {
      MEMCPY(packetClone->data + packets->orig.buf1Len, packets->orig.buf2, 
	     packets->orig.buf2Len);
   }

   packets->clone.buf1 = packetClone->data;
   packets->clone.buf1Len = packetClone->len;
   packets->clone.buf2 = NULL;
   packets->clone.buf2Len = 0;
   packets->clonedPacket = packetClone;
   return TRUE;

#elif defined __linux__

   packets->clone.skb = SMACL_DupPacket(packets->orig.skb);
   if (packets->clone.skb) {
      packets->clone.startOfData = (packets->orig.startOfData - 
				    SMACL_PacketData(packets->orig.skb)) + 
 	                            SMACL_PacketData(packets->clone.skb);
      packets->clone.len = packets->orig.len;
   }
   return packets->clone.skb != NULL;

#else /* __APPLE__ */

   /*
    * Don't need to clone packet again. We could even get rid of "clone" from
    * the SMACPackets struct, but this minimizes differences from other OSes.
    */
   packets->clone.m = packets->orig.m;
   return packets->clone.m != NULL;

#endif /* _WIN32 */
}


/*
 *----------------------------------------------------------------------
 *
 * CopyDataToClonedPacket --
 *
 *      Makes changes to the private clone copy of a packet.
 *
 * Results:
 *      TRUE if modification was successful, otherwise FALSE.
 *
 * Side effects:
 *      Modifies a packet.
 *
 *----------------------------------------------------------------------
 */

static Bool 
CopyDataToClonedPacket(SMACPackets *packets, // IN: packets
		       const void * source,  // IN: data to copy to packet
		       uint32 offset,	     // IN: dest offset at which to copy
		       uint32 length)	     // IN: length of data to copy
{
#ifdef _WIN32
   ASSERT(packets);
   ASSERT(packets->clone.buf1);
   ASSERT(packets->clone.buf1Len > offset + length);
   ASSERT(packets->clone.buf2 == NULL);
   ASSERT(packets->clone.buf2Len == 0);

   /*
    * For windows this code presumes that all of the clone packet's data is
    * in the first buffer.
    */

   if (!packets || !packets->clone.buf1 || packets->clone.buf1Len 
         <= offset + length) {
      ASSERT(0); // should never occur
      return FALSE;
   }
   MEMCPY((uint8 *)(packets->clone.buf1) + offset, source, length);

#elif defined __linux__

   ASSERT(packets);
   ASSERT(packets->clone.skb);

   MEMCPY((uint8 *)(packets->clone.startOfData) + offset, 
	  source, length);

#else /* __APPLE __ */

   ASSERT(packets);
   ASSERT(packets->clone.m);
   SMACL_CopyDataToPkt(packets->clone.m, offset, source, length);

#endif

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * SetPacketByte --
 *
 *      Makes changes to a packet, either public or private
 *
 * Results:
 *      TRUE if modification was successful, otherwise FALSE.
 *
 * Side effects:
 *      Modifies a packet.
 *
 *----------------------------------------------------------------------
 */

static Bool 
SetPacketByte(SMACPacket *packet, // IN: packet
	      uint32 offset,	  // IN: offset to change
	      uint8 data)	  // IN: data to set
{
   ASSERT(packet);

#ifdef _WIN32
   /* check length, be sure to handle case where offset = -1, length > 0 */
   if (offset > GetPacketLength(packet)) {
      /* packet not long enough for data */
      return FALSE;
   }

   /* if offset starts in the first buffer, then copy from first buffer */   
   if (offset < packet->buf1Len) {
      ((uint8*)packet->buf1)[offset] = data;
   } else {
      offset -= packet->buf1Len;
      ((uint8*)packet->buf2)[offset] = data;
   }

#elif __linux__

   ASSERT(packet);
   ASSERT(packet->skb);

   ((uint8*)packet->startOfData)[offset] = data;

#else

   ASSERT(packet);
   ASSERT(packet->m);
   SMACL_CopyDataToPkt(packet->m, offset, &data, 1);

#endif /* _WIN32 */

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * CopyDataForPacketFromHost --
 *
 *      When receiving data from the host, this function is used
 *      to make changes to the packet.  Since the only changes we
 *      ever make are to replace one Ethernet MAC with another, this
 *      function currently just does MAC replacement.
 *
 * Results:
 *      TRUE if modification was successful, otherwise FALSE.
 *
 * Side effects:
 *      Modifies a packet.
 *
 *----------------------------------------------------------------------
 */

Bool
CopyDataForPacketFromHost(SMACPackets *packets,    // IN/OUT: packets struct
			  uint32 changeNum,        // IN: serialized # of change
			  uint32 offset,           // IN: byte offset for change
			  const uint8 *macAddress) // IN: new MAC to add to packet
{
#ifdef _WIN32
   MacReplacementTable *table = NULL;
   ASSERT(packets);
   ASSERT(macAddress);

   table = packets->table;
   ASSERT(table);
   ASSERT(changeNum == table->numOfOffsets);
   UNREFERENCED_PARAMETER(changeNum);

   if (table->numOfOffsets == 0) {
      MEMCPY(table->mac, macAddress, ETH_ALEN);
   }
   table->offsets[table->numOfOffsets++] = offset;

#else /* _WIN32 */

   /* clone packet if this is the first change */
   if (changeNum == 0 && !ClonePacket(packets)) {
      VNETKdPrint((MODULE_NAME "FromHostIP: couldn't clone packet\n"));
      return FALSE;
   }	     
   CopyDataToClonedPacket(packets, macAddress, offset, ETH_ALEN); 
#endif /* _WIN32 */

   return TRUE;
}
