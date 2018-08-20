#ifndef __COMPAT_POLL_H__
#define __COMPAT_POLL_H__

#include <linux/poll.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0))

#ifndef __poll_t
typedef unsigned int __poll_t;
#endif

static inline __poll_t compat_vfs_poll(struct file *file,
				       struct poll_table_struct *pt)
{
	if (unlikely(!file->f_op->poll))
		return DEFAULT_POLLMASK;
	return file->f_op->poll(file, pt);
}

#else

static inline __poll_t compat_vfs_poll(struct file *file,
				       struct poll_table_struct *pt)
{
	return vfs_poll(file, pt);
}

#endif

#endif /* __COMPAT_POLL_H__ */
