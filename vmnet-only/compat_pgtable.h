#ifndef __COMPAT_PGTABLE_H__
#define __COMPAT_PGTABLE_H__

static inline
long compat_get_user_pages(unsigned long start, unsigned long nr_pages,
			   unsigned int gup_flags, struct page **pages)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
	return get_user_pages(start, nr_pages, gup_flags, pages);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
	return get_user_pages(start, nr_pages, gup_flags, pages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
	return get_user_pages(start, nr_pages, !!(gup_flags & FOLL_WRITE),
			      !!(gup_flags & FOLL_FORCE), pages, NULL);
#else
	return get_user_pages(current, current->mm,
			      start, nr_pages, !!(gup_flags & FOLL_WRITE),
			      !!(gup_flags & FOLL_FORCE), pages, NULL);
#endif
}

#endif /* __COMPAT_PGTABLE_H__ */
