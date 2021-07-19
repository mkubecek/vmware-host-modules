/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __COMPAT_COMPILER_H__
#define __COMPAT_COMPILER_H__

#ifndef fallthrough
#ifndef __has_attribute
	#define fallthrough do {} while (0)
#elif __has_attribute(__fallthrough__)
	#define fallthrough __attribute__((__fallthrough__))
#else
	#define fallthrough do {} while (0)
#endif /* __has_attribute */
#endif /* fallthrough */

#endif /* __COMPAT_COMPILER_H__ */
