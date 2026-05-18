/*********************************************************
 * Copyright (c) 2007-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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

#ifndef __COMPAT_SKBUFF_H__
#   define __COMPAT_SKBUFF_H__

#include <linux/skbuff.h>

#define compat_skb_mac_header(skb)         skb_mac_header(skb)
#define compat_skb_network_header(skb)     skb_network_header(skb)
#define compat_skb_network_offset(skb)     skb_network_offset(skb)
#define compat_skb_transport_header(skb)   skb_transport_header(skb)
#define compat_skb_transport_offset(skb)   skb_transport_offset(skb)
#define compat_skb_network_header_len(skb) skb_network_header_len(skb)
#define compat_skb_tail_pointer(skb)       skb_tail_pointer(skb)
#define compat_skb_end_pointer(skb)        skb_end_pointer(skb)
#define compat_skb_ip_header(skb)       ip_hdr(skb)
#define compat_skb_ipv6_header(skb)     ipv6_hdr(skb)
#define compat_skb_tcp_header(skb)      tcp_hdr(skb)
#define compat_skb_reset_mac_header(skb)          skb_reset_mac_header(skb)
#define compat_skb_reset_network_header(skb)      skb_reset_network_header(skb)
#define compat_skb_reset_transport_header(skb)    skb_reset_transport_header(skb)
#define compat_skb_set_network_header(skb, off)   skb_set_network_header(skb, off)
#define compat_skb_set_transport_header(skb, off) skb_set_transport_header(skb, off)
#define compat_skb_linearize(skb) skb_linearize((skb))
#define compat_skb_csum_offset(skb)        (skb)->csum_offset

/*
 * Note that compat_skb_csum_start() has semantic different from kernel's csum_start:
 * kernel's skb->csum_start is offset between start of checksummed area and start of
 * complete skb buffer, while our compat_skb_csum_start(skb) is offset from start
 * of packet itself.
 */
#define compat_skb_csum_start(skb)         ((skb)->csum_start - skb_headroom(skb))

#if defined(NETIF_F_GSO) /* 2.6.18 and upwards */
#define compat_skb_mss(skb) (skb_shinfo(skb)->gso_size)
#else
#define compat_skb_mss(skb) (skb_shinfo(skb)->tso_size)
#endif

/* used by both received pkts and outgoing ones */
#define VM_CHECKSUM_UNNECESSARY CHECKSUM_UNNECESSARY

/* csum status of received pkts */
#if defined(CHECKSUM_COMPLETE)
#   define VM_RX_CHECKSUM_PARTIAL     CHECKSUM_COMPLETE
#else
#   define VM_RX_CHECKSUM_PARTIAL     CHECKSUM_PARTIAL
#endif

/* csum status of outgoing pkts */
#define VM_TX_CHECKSUM_PARTIAL      CHECKSUM_PARTIAL

#define compat_kfree_skb(skb, type) kfree_skb(skb)
#define compat_dev_kfree_skb(skb, type) dev_kfree_skb(skb)
#define compat_dev_kfree_skb_any(skb, type) dev_kfree_skb_any(skb)
#define compat_dev_kfree_skb_irq(skb, type) dev_kfree_skb_irq(skb)

#ifndef NET_IP_ALIGN
#   define COMPAT_NET_IP_ALIGN  2
#else
#   define COMPAT_NET_IP_ALIGN  NET_IP_ALIGN 
#endif

#define compat_skb_headlen(skb)         skb_headlen(skb)
#define compat_pskb_may_pull(skb, len)  pskb_may_pull(skb, len)
#define compat_skb_is_nonlinear(skb)    skb_is_nonlinear(skb)
#define compat_skb_header_cloned(skb)   skb_header_cloned(skb)

#endif /* __COMPAT_SKBUFF_H__ */
