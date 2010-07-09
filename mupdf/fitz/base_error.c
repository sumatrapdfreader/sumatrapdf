#include "fitz.h"

void fz_warn(char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "warning: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

fz_error
fz_throwimp(const char *file, int line, const char *func, char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "+ %s:%d: %s(): ", file, line, func);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	return -1;
}

fz_error
fz_rethrowimp(const char *file, int line, const char *func, fz_error cause, char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "| %s:%d: %s(): ", file, line, func);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	return cause;
}

void
fz_catchimp(const char *file, int line, const char *func, fz_error cause, char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "\\ %s:%d: %s(): ", file, line, func);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

fz_error
fz_throwimpx(char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "+ ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	return -1;
}

fz_error
fz_rethrowimpx(fz_error cause, char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "| ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	return cause;
}

void
fz_catchimpx(fz_error cause, char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "\\ ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}
