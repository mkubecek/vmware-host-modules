/*********************************************************
 * Copyright (c) 2002-2025 Broadcom. All Rights Reserved.
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

#ifndef __COMPAT_TIMER_H__
#   define __COMPAT_TIMER_H__

#define compat_del_timer_sync(timer) del_timer_sync(timer)

#include <linux/delay.h>
#define compat_msleep_interruptible(msecs) msleep_interruptible(msecs)
#define compat_msleep(msecs) msleep(msecs)

#define compat_init_timer_deferrable(timer) init_timer_deferrable(timer)

#define compat_setup_timer(timer, function, data) \
       setup_timer(timer, function, data)

#if TIMER_DELETE_SYNC_MISSING
static inline int timer_delete_sync(struct timer_list *timer)
{
   return del_timer_sync(timer);
}
#endif


#endif /* __COMPAT_TIMER_H__ */
