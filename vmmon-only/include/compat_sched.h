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

#ifndef __COMPAT_SCHED_H__
#   define __COMPAT_SCHED_H__


#include <linux/sched.h>

/* CLONE_KERNEL available in 2.5.35 and higher. */
#ifndef CLONE_KERNEL
#define CLONE_KERNEL CLONE_FILES | CLONE_FS | CLONE_SIGHAND
#endif

/* TASK_COMM_LEN become available in 2.6.11. */
#ifndef TASK_COMM_LEN
#define TASK_COMM_LEN 16
#endif

#define compat_yield() yield()


/*
 * Since 2.5.34 there are two methods to enumerate tasks:
 * for_each_process(p) { ... } which enumerates only tasks and
 * do_each_thread(g,t) { ... } while_each_thread(g,t) which enumerates
 *     also threads even if they share same pid.
 */
#ifndef for_each_process
#   define for_each_process(p) for_each_task(p)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
#ifndef do_each_thread
#   define do_each_thread(g, t) for_each_task(g) { t = g; do
#   define while_each_thread(g, t) while (0) }
#endif
#endif


/*
 * Lock for signal mask is moving target...
 */
#define compat_sigmask_lock sighand->siglock
#define compat_dequeue_signal_current(siginfo_ptr) \
   dequeue_signal(current, &current->blocked, (siginfo_ptr))

#define compat_recalc_sigpending() recalc_sigpending()
#define compat_reparent_to_init() do {} while (0)
#define compat_flush_signals(task) flush_signals(task)
#define compat_allow_signal(signr) allow_signal(signr)
#define compat_daemonize(x...) daemonize(x)


/*
 * try to freeze a process. 
 * For kernels 2.6.20 and newer, we'll also need to include
 * freezer.h since the try_to_freeze definition was pulled out of sched.h.
 */
#include <linux/freezer.h>

#define compat_try_to_freeze() try_to_freeze()

#define compat_set_freezable() do { set_freezable(); } while (0)

/*
 * Around 2.6.27 kernel stopped sending signals to kernel
 * threads being frozen, instead threads have to check
 * freezing() or use wait_event_freezable(). Unfortunately
 * wait_event_freezable() completely hides the fact that
 * thread was frozen from calling code and sometimes we do
 * want to know that.
 */
#ifdef PF_FREEZER_NOSIG
#define compat_wait_check_freezing() freezing(current)
#else
#define compat_wait_check_freezing() (0)
#endif

typedef struct pid * compat_pid;
#define compat_find_get_pid(pid) find_get_pid(pid)
#define compat_put_pid(pid) put_pid(pid)
#define compat_kill_pid(pid, sig, flag) kill_pid(pid, sig, flag)


#endif /* __COMPAT_SCHED_H__ */
