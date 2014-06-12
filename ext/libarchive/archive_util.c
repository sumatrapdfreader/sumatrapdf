/*-
 * Copyright (c) 2009-2012 Michihiro NAKAJIMA
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"
__FBSDID("$FreeBSD: head/lib/libarchive/archive_util.c 201098 2009-12-28 02:58:14Z kientzle $");

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#if defined(HAVE_WINCRYPT_H) && !defined(__CYGWIN__)
#include <wincrypt.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_string.h"

#ifndef O_CLOEXEC
#define O_CLOEXEC	0
#endif

/* Generic initialization of 'struct archive' objects. */
int
__archive_clean(struct archive *a)
{
	archive_string_conversion_free(a);
	return (ARCHIVE_OK);
}

int
archive_version_number(void)
{
	return (ARCHIVE_VERSION_NUMBER);
}

const char *
archive_version_string(void)
{
	return (ARCHIVE_VERSION_STRING);
}

int
archive_errno(struct archive *a)
{
	return (a->archive_error_number);
}

const char *
archive_error_string(struct archive *a)
{

	if (a->error != NULL  &&  *a->error != '\0')
		return (a->error);
	else
		return (NULL);
}

int
archive_file_count(struct archive *a)
{
	return (a->file_count);
}

int
archive_format(struct archive *a)
{
	return (a->archive_format);
}

const char *
archive_format_name(struct archive *a)
{
	return (a->archive_format_name);
}


int
archive_compression(struct archive *a)
{
	return archive_filter_code(a, 0);
}

const char *
archive_compression_name(struct archive *a)
{
	return archive_filter_name(a, 0);
}


/*
 * Return a count of the number of compressed bytes processed.
 */
int64_t
archive_position_compressed(struct archive *a)
{
	return archive_filter_bytes(a, -1);
}

/*
 * Return a count of the number of uncompressed bytes processed.
 */
int64_t
archive_position_uncompressed(struct archive *a)
{
	return archive_filter_bytes(a, 0);
}

void
archive_clear_error(struct archive *a)
{
	archive_string_empty(&a->error_string);
	a->error = NULL;
	a->archive_error_number = 0;
}

void
archive_set_error(struct archive *a, int error_number, const char *fmt, ...)
{
	va_list ap;

	a->archive_error_number = error_number;
	if (fmt == NULL) {
		a->error = NULL;
		return;
	}

	archive_string_empty(&(a->error_string));
	va_start(ap, fmt);
	archive_string_vsprintf(&(a->error_string), fmt, ap);
	va_end(ap);
	a->error = a->error_string.s;
}

void
archive_copy_error(struct archive *dest, struct archive *src)
{
	dest->archive_error_number = src->archive_error_number;

	archive_string_copy(&dest->error_string, &src->error_string);
	dest->error = dest->error_string.s;
}

void
__archive_errx(int retvalue, const char *msg)
{
	static const char *msg1 = "Fatal Internal Error in libarchive: ";
	size_t s;

	s = write(2, msg1, strlen(msg1));
	(void)s; /* UNUSED */
	s = write(2, msg, strlen(msg));
	(void)s; /* UNUSED */
	s = write(2, "\n", 1);
	(void)s; /* UNUSED */
	exit(retvalue);
}

/*
 * Create a temporary file
 */
#if defined(_WIN32) && !defined(__CYGWIN__)

/*
 * Do not use Windows tmpfile() function.
 * It will make a temporary file under the root directory
 * and it'll cause permission error if a user who is
 * non-Administrator creates temporary files.
 * Also Windows version of mktemp family including _mktemp_s
 * are not secure.
 */
int
__archive_mktemp(const char *tmpdir)
{
	static const wchar_t num[] = {
		L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7',
		L'8', L'9', L'A', L'B', L'C', L'D', L'E', L'F',
		L'G', L'H', L'I', L'J', L'K', L'L', L'M', L'N',
		L'O', L'P', L'Q', L'R', L'S', L'T', L'U', L'V',
		L'W', L'X', L'Y', L'Z', L'a', L'b', L'c', L'd',
		L'e', L'f', L'g', L'h', L'i', L'j', L'k', L'l',
		L'm', L'n', L'o', L'p', L'q', L'r', L's', L't',
		L'u', L'v', L'w', L'x', L'y', L'z'
	};
	HCRYPTPROV hProv;
	struct archive_wstring temp_name;
	wchar_t *ws;
	DWORD attr;
	wchar_t *xp, *ep;
	int fd;

	hProv = (HCRYPTPROV)NULL;
	fd = -1;
	ws = NULL;
	archive_string_init(&temp_name);

	/* Get a temporary directory. */
	if (tmpdir == NULL) {
		size_t l;
		wchar_t *tmp;

		l = GetTempPathW(0, NULL);
		if (l == 0) {
			la_dosmaperr(GetLastError());
			goto exit_tmpfile;
		}
		tmp = malloc(l*sizeof(wchar_t));
		if (tmp == NULL) {
			errno = ENOMEM;
			goto exit_tmpfile;
		}
		GetTempPathW((DWORD)l, tmp);
		archive_wstrcpy(&temp_name, tmp);
		free(tmp);
	} else {
		if (archive_wstring_append_from_mbs(&temp_name, tmpdir,
		    strlen(tmpdir)) < 0)
			goto exit_tmpfile;
		if (temp_name.s[temp_name.length-1] != L'/')
			archive_wstrappend_wchar(&temp_name, L'/');
	}

	/* Check if temp_name is a directory. */
	attr = GetFileAttributesW(temp_name.s);
	if (attr == (DWORD)-1) {
		if (GetLastError() != ERROR_FILE_NOT_FOUND) {
			la_dosmaperr(GetLastError());
			goto exit_tmpfile;
		}
		ws = __la_win_permissive_name_w(temp_name.s);
		if (ws == NULL) {
			errno = EINVAL;
			goto exit_tmpfile;
		}
		attr = GetFileAttributesW(ws);
		if (attr == (DWORD)-1) {
			la_dosmaperr(GetLastError());
			goto exit_tmpfile;
		}
	}
	if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
		errno = ENOTDIR;
		goto exit_tmpfile;
	}

	/*
	 * Create a temporary file.
	 */
	archive_wstrcat(&temp_name, L"libarchive_");
	xp = temp_name.s + archive_strlen(&temp_name);
	archive_wstrcat(&temp_name, L"XXXXXXXXXX");
	ep = temp_name.s + archive_strlen(&temp_name);

	if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL,
		CRYPT_VERIFYCONTEXT)) {
		la_dosmaperr(GetLastError());
		goto exit_tmpfile;
	}

	for (;;) {
		wchar_t *p;
		HANDLE h;

		/* Generate a random file name through CryptGenRandom(). */
		p = xp;
		if (!CryptGenRandom(hProv, (DWORD)(ep - p)*sizeof(wchar_t),
		    (BYTE*)p)) {
			la_dosmaperr(GetLastError());
			goto exit_tmpfile;
		}
		for (; p < ep; p++)
			*p = num[((DWORD)*p) % (sizeof(num)/sizeof(num[0]))];

		free(ws);
		ws = __la_win_permissive_name_w(temp_name.s);
		if (ws == NULL) {
			errno = EINVAL;
			goto exit_tmpfile;
		}
		/* Specifies FILE_FLAG_DELETE_ON_CLOSE flag is to
		 * delete this temporary file immediately when this
		 * file closed. */
		h = CreateFileW(ws,
		    GENERIC_READ | GENERIC_WRITE | DELETE,
		    0,/* Not share */
		    NULL,
		    CREATE_NEW,/* Create a new file only */
		    FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
		    NULL);
		if (h == INVALID_HANDLE_VALUE) {
			/* The same file already exists. retry with
			 * a new filename. */
			if (GetLastError() == ERROR_FILE_EXISTS)
				continue;
			/* Otherwise, fail creation temporary file. */
			la_dosmaperr(GetLastError());
			goto exit_tmpfile;
		}
		fd = _open_osfhandle((intptr_t)h, _O_BINARY | _O_RDWR);
		if (fd == -1) {
			CloseHandle(h);
			goto exit_tmpfile;
		} else
			break;/* success! */
	}
exit_tmpfile:
	if (hProv != (HCRYPTPROV)NULL)
		CryptReleaseContext(hProv, 0);
	free(ws);
	archive_wstring_free(&temp_name);
	return (fd);
}

#else

static int
get_tempdir(struct archive_string *temppath)
{
	const char *tmp;

	tmp = getenv("TMPDIR");
	if (tmp == NULL)
#ifdef _PATH_TMP
		tmp = _PATH_TMP;
#else
                tmp = "/tmp";
#endif
	archive_strcpy(temppath, tmp);
	if (temppath->s[temppath->length-1] != '/')
		archive_strappend_char(temppath, '/');
	return (ARCHIVE_OK);
}

#if defined(HAVE_MKSTEMP)

/*
 * We can use mkstemp().
 */

int
__archive_mktemp(const char *tmpdir)
{
	struct archive_string temp_name;
	int fd = -1;

	archive_string_init(&temp_name);
	if (tmpdir == NULL) {
		if (get_tempdir(&temp_name) != ARCHIVE_OK)
			goto exit_tmpfile;
	} else {
		archive_strcpy(&temp_name, tmpdir);
		if (temp_name.s[temp_name.length-1] != '/')
			archive_strappend_char(&temp_name, '/');
	}
	archive_strcat(&temp_name, "libarchive_XXXXXX");
	fd = mkstemp(temp_name.s);
	if (fd < 0)
		goto exit_tmpfile;
	__archive_ensure_cloexec_flag(fd);
	unlink(temp_name.s);
exit_tmpfile:
	archive_string_free(&temp_name);
	return (fd);
}

#else

/*
 * We use a private routine.
 */

int
__archive_mktemp(const char *tmpdir)
{
        static const char num[] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
		'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
		'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
		'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
		'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
		'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
		'u', 'v', 'w', 'x', 'y', 'z'
        };
	struct archive_string temp_name;
	struct stat st;
	int fd;
	char *tp, *ep;
	unsigned seed;

	fd = -1;
	archive_string_init(&temp_name);
	if (tmpdir == NULL) {
		if (get_tempdir(&temp_name) != ARCHIVE_OK)
			goto exit_tmpfile;
	} else
		archive_strcpy(&temp_name, tmpdir);
	if (temp_name.s[temp_name.length-1] == '/') {
		temp_name.s[temp_name.length-1] = '\0';
		temp_name.length --;
	}
	if (stat(temp_name.s, &st) < 0)
		goto exit_tmpfile;
	if (!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		goto exit_tmpfile;
	}
	archive_strcat(&temp_name, "/libarchive_");
	tp = temp_name.s + archive_strlen(&temp_name);
	archive_strcat(&temp_name, "XXXXXXXXXX");
	ep = temp_name.s + archive_strlen(&temp_name);

	fd = open("/dev/random", O_RDONLY | O_CLOEXEC);
	__archive_ensure_cloexec_flag(fd);
	if (fd < 0)
		seed = time(NULL);
	else {
		if (read(fd, &seed, sizeof(seed)) < 0)
			seed = time(NULL);
		close(fd);
	}
	do {
		char *p;

		p = tp;
		while (p < ep)
			*p++ = num[((unsigned)rand_r(&seed)) % sizeof(num)];
		fd = open(temp_name.s, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC,
			  0600);
	} while (fd < 0 && errno == EEXIST);
	if (fd < 0)
		goto exit_tmpfile;
	__archive_ensure_cloexec_flag(fd);
	unlink(temp_name.s);
exit_tmpfile:
	archive_string_free(&temp_name);
	return (fd);
}

#endif /* HAVE_MKSTEMP */
#endif /* !_WIN32 || __CYGWIN__ */

/*
 * Set FD_CLOEXEC flag to a file descriptor if it is not set.
 * We have to set the flag if the platform does not provide O_CLOEXEC
 * or F_DUPFD_CLOEXEC flags.
 *
 * Note: This function is absolutely called after creating a new file
 * descriptor even if the platform seemingly provides O_CLOEXEC or
 * F_DUPFD_CLOEXEC macros because it is possible that the platform
 * merely declares those macros, especially Linux 2.6.18 - 2.6.24 do it.
 */
void
__archive_ensure_cloexec_flag(int fd)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	(void)fd; /* UNSED */
#else
	int flags;

	if (fd >= 0) {
		flags = fcntl(fd, F_GETFD);
		if (flags != -1 && (flags & FD_CLOEXEC) == 0)
			fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
	}
#endif
}
