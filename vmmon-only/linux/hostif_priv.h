/*********************************************************
 * Copyright (C) 2009-2015,2018 VMware, Inc. All rights reserved.
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
 * hostif_priv.h --
 *
 *      Defines several Linux-only additions to the HostIF API.
 */

#ifndef _HOSTIF_PRIV_H_
#define _HOSTIF_PRIV_H_

/* Uptime-related functions. */
EXTERN void   HostIF_InitUptime(void);
EXTERN void   HostIF_CleanupUptime(void);

/* Miscellaneous functions. */
EXTERN void   HostIF_InitGlobalLock(void);
EXTERN Bool   HostIF_GetAllCpuInfo(CPUIDQuery *query);

#endif // ifdef _HOSTIF_PRIV_H_
