#include "fitz.h"

void *
fz_malloc(int size)
{
	void *p = malloc(size);
	if (!p)
	{
		fprintf(stderr, "fatal error: out of memory\n");
		abort();
	}
	return p;
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
		abort();
	}

	p = malloc(count * size);
	if (!p)
	{
		fprintf(stderr, "fatal error: out of memory\n");
		abort();
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
		abort();
	}

	np = realloc(p, count * size);
	if (np == NULL)
	{
		fprintf(stderr, "fatal error: out of memory\n");
		abort();
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
