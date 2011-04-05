#include "fitz.h"

enum { LINE_LEN = 160, LINE_COUNT = 25 };

static char warn_message[LINE_LEN] = "";
static int warn_count = 0;

void fz_flush_warnings(void)
{
	if (warn_count > 1)
		fprintf(stderr, "warning: ... repeated %d times ...\n", warn_count);
	warn_message[0] = 0;
	warn_count = 0;
}

void fz_warn(char *fmt, ...)
{
	va_list ap;
	char buf[LINE_LEN];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	if (!strcmp(buf, warn_message))
	{
		warn_count++;
	}
	else
	{
		fz_flush_warnings();
		fprintf(stderr, "warning: %s\n", buf);
		fz_strlcpy(warn_message, buf, sizeof warn_message);
		warn_count = 1;
	}
}

static char error_message[LINE_COUNT][LINE_LEN];
static int error_count = 0;

static void
fz_emit_error(char what, char *location, char *message)
{
	fz_flush_warnings();

	fprintf(stderr, "%c %s%s\n", what, location, message);

	if (error_count < LINE_COUNT)
	{
		fz_strlcpy(error_message[error_count], location, LINE_LEN);
		fz_strlcat(error_message[error_count], message, LINE_LEN);
		error_count++;
	}
}

int
fz_get_error_count(void)
{
	return error_count;
}

char *
fz_get_error_line(int n)
{
	return error_message[n];
}

fz_error
fz_throw_imp(const char *file, int line, const char *func, char *fmt, ...)
{
	va_list ap;
	char one[LINE_LEN], two[LINE_LEN];

	error_count = 0;

	snprintf(one, sizeof one, "%s:%d: %s(): ", file, line, func);
	va_start(ap, fmt);
	vsnprintf(two, sizeof two, fmt, ap);
	va_end(ap);

	fz_emit_error('+', one, two);

	return -1;
}

fz_error
fz_rethrow_imp(const char *file, int line, const char *func, fz_error cause, char *fmt, ...)
{
	va_list ap;
	char one[LINE_LEN], two[LINE_LEN];

	snprintf(one, sizeof one, "%s:%d: %s(): ", file, line, func);
	va_start(ap, fmt);
	vsnprintf(two, sizeof two, fmt, ap);
	va_end(ap);

	fz_emit_error('|', one, two);

	return cause;
}

void
fz_catch_imp(const char *file, int line, const char *func, fz_error cause, char *fmt, ...)
{
	va_list ap;
	char one[LINE_LEN], two[LINE_LEN];

	snprintf(one, sizeof one, "%s:%d: %s(): ", file, line, func);
	va_start(ap, fmt);
	vsnprintf(two, sizeof two, fmt, ap);
	va_end(ap);

	fz_emit_error('\\', one, two);
}

fz_error
fz_throw_impx(char *fmt, ...)
{
	va_list ap;
	char buf[LINE_LEN];

	error_count = 0;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	fz_emit_error('+', "", buf);

	return -1;
}

fz_error
fz_rethrow_impx(fz_error cause, char *fmt, ...)
{
	va_list ap;
	char buf[LINE_LEN];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	fz_emit_error('|', "", buf);

	return cause;
}

void
fz_catch_impx(fz_error cause, char *fmt, ...)
{
	va_list ap;
	char buf[LINE_LEN];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	fz_emit_error('\\', "", buf);
}
