/*********************************************************
 * Copyright (C) 1998,2017-2018,2020 VMware, Inc. All rights reserved.
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
 * memtrack.h --
 *
 *    Utility module for tracking pinned memory, which allows later
 *    lookup by VPN and optionally by MPN.
 */


#ifndef _MEMTRACK_H_
#define _MEMTRACK_H_

#include "vmx86.h"

struct MemTrack;

typedef struct MemTrackEntry {
   VPN64                   vpn;
   MPN                     mpn;
   struct MemTrackEntry   *vpnChain;
   struct MemTrackEntry   *mpnChain;
} MemTrackEntry;

typedef void (MemTrackCleanupCb)(void *cData, MemTrackEntry *entry);

extern struct MemTrack *MemTrack_Init(VMDriver *vm);
extern PageCnt MemTrack_Cleanup(struct MemTrack *mt, MemTrackCleanupCb *cb,
                                void *cbData);
extern MemTrackEntry *MemTrack_Add(struct MemTrack *mt, VPN64 vpn, MPN mpn);
extern MemTrackEntry *MemTrack_LookupVPN(struct MemTrack *mt, VPN64 vpn);
extern MemTrackEntry *MemTrack_LookupMPN(struct MemTrack *mt, MPN mpn);

#endif // _MEMTRACK_H_
