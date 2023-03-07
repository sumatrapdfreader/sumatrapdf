#include "extract/alloc.h"

#include "mem.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "compat_va_copy.h"


void extract_bzero(void *b, size_t len)
{
	memset(b, 0, len);
}

int extract_vasprintf(extract_alloc_t *alloc, char **out, const char *format, va_list va)
{
	int n;
	int ret;
	va_list va2;

	va_copy(va2, va);
	n = vsnprintf(NULL, 0, format, va);
	if (n < 0)
	{
		ret = n;
		goto end;
	}
	if (extract_malloc(alloc, out, n + 1))
	{
		ret = -1;
		goto end;
	}
	vsnprintf(*out, n + 1, format, va2);

	ret = 0;
end:

	va_end(va2);

	return ret;
}


int extract_asprintf(extract_alloc_t *alloc, char **out, const char *format, ...)
{
	va_list va;
	int     ret;

	va_start(va, format);
	ret = extract_vasprintf(alloc, out, format, va);
	va_end(va);

	return ret;
}

int extract_strdup(extract_alloc_t *alloc, const char *s, char **o_out)
{
	size_t l = strlen(s) + 1;

	if (extract_malloc(alloc, o_out, l)) return -1;
	memcpy(*o_out, s, l);

	return 0;
}
