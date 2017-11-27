#ifndef __COMPAT_TIMER_H__
#   define __COMPAT_TIMER_H__

#include <linux/timer.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)) && !defined(timer_setup)

typedef unsigned long compat_timer_arg_t;

static inline void compat_timer_setup(struct timer_list *timer,
				      void (*func)(compat_timer_arg_t),
				      unsigned int flags)
{
	init_timer(timer);
	timer->function = func;
	timer->data = 0;
	timer->flags = flags;
}

#else /* new timer interface since 4.15 */

typedef struct timer_list *compat_timer_arg_t;

static inline void compat_timer_setup(struct timer_list *timer,
				      void (*func)(compat_timer_arg_t),
				      unsigned int flags)
{
	timer_setup(timer, func, flags);
}

#endif /* new timer interface since 4.15 */

#endif /* __COMPAT_TIMER_H__ */
