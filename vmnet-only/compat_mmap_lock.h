#ifndef __COMPAT_MMAP_LOCK_H__
#define __COMPAT_MMAP_LOCK_H__

#include <linux/mm.h>

/*
 * In 5.8-rc1, mmap locking was reworked to use wrappers around mmap_sem
 * (which was also renamed to mmap_lock). All code is now supposed to use
 * these wrappers as the internal implementation of the lock may change in
 * the future.
 *
 * Check also _LINUX_MMAP_LOCK_H to handle possible backports to distribution
 * pre-5.8 kernel. This macro is defined in <linux/mmap_lock.h> which is also
 * included in <linux/mm.h> since the commit introducing the wrappers so that
 * we should have it defined in any kernel providing the new API.
 */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)) || defined(_LINUX_MMAP_LOCK_H)
#include <linux/mmap_lock.h>
#else

static inline void mmap_read_lock(struct mm_struct *mm)
{
	down_read(&mm->mmap_sem);
}

static inline void mmap_read_unlock(struct mm_struct *mm)
{
	up_read(&mm->mmap_sem);
}

#endif /* 5.8.0 */

#endif /* __COMPAT_MMAP_LOCK_H__ */
