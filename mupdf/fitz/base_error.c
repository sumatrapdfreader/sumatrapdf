#include "fitz.h"

enum { LINELEN = 160, LINECOUNT = 25 };

static char warnmessage[LINELEN] = "";
static int warncount = 0;

void fz_flushwarnings(void)
{
	if (warncount > 1)
		fprintf(stderr, "warning: ... repeated %d times ...\n", warncount);
	warnmessage[0] = 0;
	warncount = 0;
}

void fz_warn(char *fmt, ...)
{
	va_list ap;
	char buf[LINELEN];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	if (!strcmp(buf, warnmessage))
	{
		warncount++;
	}
	else
	{
		fz_flushwarnings();
		fprintf(stderr, "warning: %s\n", buf);
		fz_strlcpy(warnmessage, buf, sizeof warnmessage);
		warncount = 1;
	}
}

static char errormessage[LINECOUNT][LINELEN];
static int errorcount = 0;

static void
fz_emiterror(char what, char *location, char *message)
{
	fz_flushwarnings();

	fprintf(stderr, "%c %s%s\n", what, location, message);

	if (errorcount < LINECOUNT)
	{
		fz_strlcpy(errormessage[errorcount], location, LINELEN);
		fz_strlcat(errormessage[errorcount], message, LINELEN);
		errorcount++;
	}
}

int
fz_geterrorcount(void)
{
	return errorcount;
}

char *
fz_geterrorline(int n)
{
	return errormessage[n];
}

fz_error
fz_throwimp(const char *file, int line, const char *func, char *fmt, ...)
{
	va_list ap;
	char one[LINELEN], two[LINELEN];

	errorcount = 0;

	snprintf(one, sizeof one, "%s:%d: %s(): ", file, line, func);
	va_start(ap, fmt);
	vsnprintf(two, sizeof two, fmt, ap);
	va_end(ap);

	fz_emiterror('+', one, two);

	return -1;
}

fz_error
fz_rethrowimp(const char *file, int line, const char *func, fz_error cause, char *fmt, ...)
{
	va_list ap;
	char one[LINELEN], two[LINELEN];

	snprintf(one, sizeof one, "%s:%d: %s(): ", file, line, func);
	va_start(ap, fmt);
	vsnprintf(two, sizeof two, fmt, ap);
	va_end(ap);

	fz_emiterror('|', one, two);

	return cause;
}

void
fz_catchimp(const char *file, int line, const char *func, fz_error cause, char *fmt, ...)
{
	va_list ap;
	char one[LINELEN], two[LINELEN];

	snprintf(one, sizeof one, "%s:%d: %s(): ", file, line, func);
	va_start(ap, fmt);
	vsnprintf(two, sizeof two, fmt, ap);
	va_end(ap);

	fz_emiterror('\\', one, two);
}

fz_error
fz_throwimpx(char *fmt, ...)
{
	va_list ap;
	char buf[LINELEN];

	errorcount = 0;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	fz_emiterror('+', "", buf);

	return -1;
}

fz_error
fz_rethrowimpx(fz_error cause, char *fmt, ...)
{
	va_list ap;
	char buf[LINELEN];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	fz_emiterror('|', "", buf);

	return cause;
}

void
fz_catchimpx(fz_error cause, char *fmt, ...)
{
	va_list ap;
	char buf[LINELEN];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	fz_emiterror('\\', "", buf);
}
