#ifndef _VMW_COMPAT_ENTROPY_H_
#define _VMW_COMPAT_ENTROPY_H_

/* Prototype only: timex.h (and other kernel headers) may call
 * random_get_entropy_fallback() — ensure a prototype is visible.
 */
#ifndef HAVE_RANDOM_GET_ENTROPY_FALLBACK
unsigned long random_get_entropy_fallback(void);
#endif

#endif /* _VMW_COMPAT_ENTROPY_H_ */
