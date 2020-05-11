#ifndef EFFLU_UTILS_H
#define EFFLU_UTILS_H

#include <stddef.h>

#ifndef __GNUC__
#	warning "Expect to get less warnings because your compiler does not support attributes"
#	define __attribute__(x)
#endif

void	efflu_strappf(char **old, size_t *old_size, size_t *old_offset, const char *format, ...)
	__attribute__((nonnull(1, 2, 3), format(printf, 4, 5)))
;

#endif	/* EFFLU_UTILS_H */
