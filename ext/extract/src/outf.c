#include "memento.h"
#include "outf.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int extract_outf_verbose = 0;

void extract_outf_verbose_set(int verbose)
{
	extract_outf_verbose = verbose;
}

void (extract_outf)(
		int         level,
		const char *file,
		int         line,
		const char *fn,
		int         ln,
		const char *format,
		...
		)
{
	va_list va;
	if (level > extract_outf_verbose) {
		return;
	}

	if (ln) {
		fprintf(stderr, "%s:%i:%s: ", file, line, fn);
	}
	va_start(va, format);
	vfprintf(stderr, format, va);
	va_end(va);
	if (ln) {
		size_t len = strlen(format);
		if (len == 0 || format[len-1] != '\n') {
			fprintf(stderr, "\n");
		}
	}
}
