#include <time.h>
#include "fitz.h"

#ifdef _WIN32
#ifndef METRO
#include <winsock2.h>
#endif
#include <windows.h>

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS 11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS 11644473600000000ULL
#endif

#ifndef _WINRT

struct timeval;

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	FILETIME ft;
	unsigned __int64 tmpres = 0;

	if (tv)
	{
		GetSystemTimeAsFileTime(&ft);

		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		tmpres /= 10; /*convert into microseconds*/
		/*converting file time to unix epoch*/
		tmpres -= DELTA_EPOCH_IN_MICROSECS;
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}

	return 0;
}

#else /* !_WINRT */

void fz_gettimeofday_dummy() { }

#endif /* !_WINRT) */

FILE *fopen_utf8(const char *name, const char *mode)
{
	wchar_t *wname, *wmode, *d;
	const char *s;
	int c;
	FILE *file;

	/* SumatraPDF: prefer ANSI to UTF-8 for reading for consistency with remaining API */
#undef fopen
	if (strchr(mode, 'r') && (file = fopen(name, mode)) != NULL)
		return file;

	d = wname = (wchar_t*) malloc((strlen(name)+1) * sizeof(wchar_t));
	if (d == NULL)
		return NULL;
	s = name;
	while (*s) {
		s += fz_chartorune(&c, s);
		*d++ = c;
	}
	*d = 0;
	d = wmode = (wchar_t*) malloc((strlen(mode)+1) * sizeof(wchar_t));
	if (d == NULL)
	{
		free(wname);
		return NULL;
	}
	s = mode;
	while (*s) {
		s += fz_chartorune(&c, s);
		*d++ = c;
	}
	*d = 0;
	file = _wfopen(wname, wmode);
	free(wname);
	free(wmode);
	return file;
}

#endif /* _WIN32 */
