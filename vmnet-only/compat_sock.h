/*********************************************************
 * Copyright (c) 2003-2025 Broadcom. All Rights Reserved.
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

#ifndef __COMPAT_SOCK_H__
#   define __COMPAT_SOCK_H__

#include <linux/stddef.h> /* for NULL */
#include <net/sock.h>

#define compat_sock_net(sk)            sock_net(sk)

#define compat_sk_receive_skb(sk, skb, nested) sk_receive_skb(sk, skb, nested)

#endif /* __COMPAT_SOCK_H__ */
