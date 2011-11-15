#include "fitz.h"

/* SumatraPDF: force crash so that we get crash report */
static void
fz_crash_abort(int total_size)
{
	char *p = (char *)total_size;
	// first try to crash on an address that is equal to total_size.
	// this is a way to easily know the amount memory that was requested
	// from crash report
	*p = 0;
	// if that address was writeable, crash for sure writing to address 0
	p = NULL;
	*p = 0;
}

void *
fz_malloc(int size)
{
	void *p = malloc(size);
	if (!p)
	{
		fprintf(stderr, "fatal error: out of memory\n");
		fz_crash_abort(size);
	}
	return p;
}

/* SumatraPDF: allow to failibly allocate memory */
void *
fz_calloc_no_abort(int count, int size)
{
	if (count == 0 || size == 0)
		return NULL;

	if (count < 0 || size < 0 || count > INT_MAX / size)
		return NULL;

	return malloc(count * size);
}

void *
fz_calloc(int count, int size)
{
	void *p;

	if (count == 0 || size == 0)
		return 0;

	if (count < 0 || size < 0 || count > INT_MAX / size)
	{
		fprintf(stderr, "fatal error: out of memory (integer overflow)\n");
		fz_crash_abort(count * size);
	}

	p = malloc(count * size);
	if (!p)
	{
		fprintf(stderr, "fatal error: out of memory\n");
		fz_crash_abort(count *size);
	}
	return p;
}

void *
fz_realloc(void *p, int count, int size)
{
	void *np;

	if (count == 0 || size == 0)
	{
		fz_free(p);
		return 0;
	}

	if (count < 0 || size < 0 || count > INT_MAX / size)
	{
		fprintf(stderr, "fatal error: out of memory (integer overflow)\n");
		fz_crash_abort(count * size);
	}

	np = realloc(p, count * size);
	if (np == NULL)
	{
		fprintf(stderr, "fatal error: out of memory\n");
		fz_crash_abort(count * size);
	}
	return np;
}

void
fz_free(void *p)
{
	free(p);
}

char *
fz_strdup(char *s)
{
	int len = strlen(s) + 1;
	char *ns = fz_malloc(len);
	memcpy(ns, s, len);
	return ns;
}
