#ifndef __COMPAT_TIMEKEEPING32_H__
#define __COMPAT_TIMEKEEPING32_H__

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
static inline void do_gettimeofday(struct timeval *tv)
{
	struct timespec64 now;

	ktime_get_real_ts64(&now);
	tv->tv_sec = now.tv_sec;
	tv->tv_usec = now.tv_nsec / 1000;
}
#endif

#endif /* __COMPAT_TIMEKEEPING32_H__ */
