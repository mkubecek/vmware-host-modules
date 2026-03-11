/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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
 * smac_compat.h --
 *
 *      This file defines an abstraction layer to handling
 *      differences among the Linux kernel and avoiding
 *      symbol match issues.
 */

#ifndef _SMAC_COMPAT_H_
#define _SMAC_COMPAT_H_

#include "vm_basic_types.h"

#if defined(__x86_64__) 
#define SMACINT
#else
#define SMACINT __attribute__((cdecl, regparm(3)))
#endif

void   SMACINT SMACL_Memcpy(void *d, const void *s, size_t l);
int    SMACINT SMACL_Memcmp(const void *p1, const void *p2, size_t l);
void   SMACINT SMACL_Memset(void *p1, int val, size_t l);
void*  SMACINT SMACL_Alloc(size_t s);
void   SMACINT SMACL_Free(void *p);

unsigned long SMACINT SMACL_GetUptime(void);

void   SMACINT SMACL_InitSpinlock(void **s);
void   SMACINT SMACL_AcquireSpinlock(void **s, unsigned long *flags);
void   SMACINT SMACL_ReleaseSpinlock(void  **s, unsigned long *flags);


struct sk_buff* SMACINT SMACL_DupPacket(struct sk_buff *skb);
void*  SMACINT SMACL_PacketData(struct sk_buff *skb);
int    SMACINT SMACL_IsSkbHostBound(struct sk_buff *skb);
#ifdef DBG
void   SMACINT SMACL_Print(const char *m, ...);
void   SMACINT SMACL_PrintSkb(struct sk_buff *skb, char *type);
#endif /* DBG */

#endif /* _SMAC_COMPAT_H */

