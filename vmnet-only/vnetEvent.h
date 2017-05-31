/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * vnetEvent.h --
 */

#ifndef _VNETEVENT_H_
#define _VNETEVENT_H_

#include "vm_basic_types.h"
#include "vnet.h"

typedef struct VNetEvent_Mechanism VNetEvent_Mechanism;

typedef struct VNetEvent_Sender VNetEvent_Sender;

typedef struct VNetEvent_Listener VNetEvent_Listener;

typedef void (*VNetEvent_Handler)(void *data, VNet_EventHeader *e);

int VNetEvent_CreateMechanism(VNetEvent_Mechanism **m);
int VNetEvent_DestroyMechanism(VNetEvent_Mechanism *m);

int VNetEvent_CreateSender(VNetEvent_Mechanism *m, VNetEvent_Sender **s);
int VNetEvent_DestroySender(VNetEvent_Sender *s);
int VNetEvent_Send(VNetEvent_Sender *s, VNet_EventHeader *e);
int VNetEvent_GetSenderId(const VNetEvent_Sender *s, uint32 *senderId);

int VNetEvent_CreateListener(VNetEvent_Mechanism *m, VNetEvent_Handler h,
                             void *data, uint32 classMask,
                             VNetEvent_Listener **l);
int VNetEvent_DestroyListener(VNetEvent_Listener *l);

#endif // _VNETEVENT_H_
