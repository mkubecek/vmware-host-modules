/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * compat_timer.h - Timer API compatibility for Linux kernel 6.10+
 *
 * Kernel 6.10+ renamed del_timer_sync() to timer_delete_sync()
 *********************************************************/

#ifndef __COMPAT_TIMER_H__
#define __COMPAT_TIMER_H__

/* Include after other kernel headers (need PAGE_SIZE etc) */
#include <linux/timer.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
#define del_timer_sync  timer_delete_sync
#endif

#endif /* __COMPAT_TIMER_H__ */
