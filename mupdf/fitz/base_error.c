#include "fitz_base.h"

char fz_errorbuf[100*20] = {0};
static int fz_errorlen = 0;
static int fz_errorclear = 1;

static void
fz_printerror(int type, const char *file, int line, const char *func, char *msg)
{
	char buf[100];
	int len;

	snprintf(buf, sizeof buf, "%c %s:%d: %s(): %s", type, file, line, func, msg);
	len = strlen(buf);

	fputs(buf, stderr);
	putc('\n', stderr);

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
	char buf[100];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	fz_printerror('+', file, line, func, buf);
	return -1;
}

fz_error fz_rethrowimp(fz_error cause, const char *file, int line, const char *func, char *fmt, ...)
{
	char buf[100];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	fz_printerror('|', file, line, func, buf);
	return cause;
}

fz_error fz_catchimp(fz_error cause, const char *file, int line, const char *func, char *fmt, ...)
{
	char buf[100];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	fz_printerror('\\', file, line, func, buf);
	fz_errorclear = 1;
	return cause;
}

