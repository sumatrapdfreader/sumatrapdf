#include "fitz.h"

char fz_errorbuf[150*20] = {0};
static int fz_errorlen = 0;
static int fz_errorclear = 1;

static void
fz_printerror(int type, const char *file, int line, const char *func, char *msg)
{
	char buf[150];
	int len;
	char *s;

	s = strrchr(file, '\\');
	if (s)
		file = s + 1;

	fprintf(stderr, "%c %s:%d: %s(): %s\n", type, file, line, func, msg);

	snprintf(buf, sizeof buf, "%s:%d: %s(): %s", file, line, func, msg);
	buf[sizeof(buf)-1] = 0;
	len = strlen(buf);

	if (fz_errorclear)
	{
		fz_errorclear = 0;
		fz_errorlen = 0;
		memset(fz_errorbuf, 0, sizeof fz_errorbuf);
	}

	if (fz_errorlen + len + 2 < sizeof fz_errorbuf)
	{
		memcpy(fz_errorbuf + fz_errorlen, buf, len);
		fz_errorlen += len;
		fz_errorbuf[fz_errorlen++] = '\n';
		fz_errorbuf[fz_errorlen] = 0;
	}
}

void fz_warn(char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "warning: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

fz_error fz_throwimp(const char *file, int line, const char *func, char *fmt, ...)
{
	char buf[150];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	buf[sizeof(buf)-1] = 0;
	fz_printerror('+', file, line, func, buf);
	return -1;
}

fz_error fz_rethrowimp(fz_error cause, const char *file, int line, const char *func, char *fmt, ...)
{
	char buf[150];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	buf[sizeof(buf)-1] = 0;
	fz_printerror('|', file, line, func, buf);
	return cause;
}

fz_error fz_catchimp(fz_error cause, const char *file, int line, const char *func, char *fmt, ...)
{
	char buf[150];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	buf[sizeof(buf)-1] = 0;
	fz_printerror('\\', file, line, func, buf);
	fz_errorclear = 1;
	return cause;
}

