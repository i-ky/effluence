#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

void	efflu_strappf(char **old, size_t *old_size, size_t *old_offset, const char *format, ...)
{
	char	*new = *old;
	size_t	new_size = *old_size, new_offset = *old_offset;
	va_list	args;

	if (NULL == new && NULL == (new = malloc(new_size = 128)))
		return;

	va_start(args, format);
	new_offset += vsnprintf(new + new_offset, new_size - new_offset, format, args);
	va_end(args);

	if (new_offset >= new_size)
	{
		new_size = new_offset + 1;

		if (NULL != (new = realloc(new, new_size)))
		{
			va_start(args, format);
			vsnprintf(new + *old_offset, new_size - *old_offset, format, args);
			va_end(args);
		}
		else
			new_size = new_offset = 0;
	}

	*old = new;
	*old_size = new_size;
	*old_offset = new_offset;
}
