/*-
 * Copyright (c) 2009-2011 Michihiro NAKAJIMA
 * Copyright (c) 2003-2007 Kees Zeelenberg
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
 *
 * $FreeBSD$
 */

/*
 * A set of compatibility glue for building libarchive on Windows platforms.
 *
 * Originally created as "libarchive-nonposix.c" by Kees Zeelenberg
 * for the GnuWin32 project, trimmed significantly by Tim Kientzle.
 *
 * Much of the original file was unnecessary for libarchive, because
 * many of the features it emulated were not strictly necessary for
 * libarchive.  I hope for this to shrink further as libarchive
 * internals are gradually reworked to sit more naturally on both
 * POSIX and Windows.  Any ideas for this are greatly appreciated.
 *
 * The biggest remaining issue is the dev/ino emulation; libarchive
 * has a couple of public APIs that rely on dev/ino uniquely
 * identifying a file.  This doesn't match well with Windows.  I'm
 * considering alternative APIs.
 */

#if defined(_WIN32) && !defined(__CYGWIN__)

#include "archive_platform.h"
#include "archive_private.h"
#include "archive_entry.h"
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#ifdef HAVE_SYS_UTIME_H
#include <sys/utime.h>
#endif
#include <sys/stat.h>
#include <locale.h>
#include <process.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <share.h>

#define EPOC_TIME ARCHIVE_LITERAL_ULL(116444736000000000)

#if defined(__LA_LSEEK_NEEDED)
static BOOL SetFilePointerEx_perso(HANDLE hFile,
				   LARGE_INTEGER liDistanceToMove,
				   PLARGE_INTEGER lpNewFilePointer,
				   DWORD dwMoveMethod)
{
	LARGE_INTEGER li;
	li.QuadPart = liDistanceToMove.QuadPart;
	li.LowPart = SetFilePointer(
		hFile, li.LowPart, &li.HighPart, dwMoveMethod);
	if(lpNewFilePointer) {
		lpNewFilePointer->QuadPart = li.QuadPart;
	}
	return li.LowPart != -1 || GetLastError() == NO_ERROR;
}
#endif

struct ustat {
	int64_t		st_atime;
	uint32_t	st_atime_nsec;
	int64_t		st_ctime;
	uint32_t	st_ctime_nsec;
	int64_t		st_mtime;
	uint32_t	st_mtime_nsec;
	gid_t		st_gid;
	/* 64bits ino */
	int64_t		st_ino;
	mode_t		st_mode;
	uint32_t	st_nlink;
	uint64_t	st_size;
	uid_t		st_uid;
	dev_t		st_dev;
	dev_t		st_rdev;
};

/* Transform 64-bits ino into 32-bits by hashing.
 * You do not forget that really unique number size is 64-bits.
 */
#define INOSIZE (8*sizeof(ino_t)) /* 32 */
static __inline ino_t
getino(struct ustat *ub)
{
	ULARGE_INTEGER ino64;
	ino64.QuadPart = ub->st_ino;
	/* I don't know this hashing is correct way */
	return ((ino_t)(ino64.LowPart ^ (ino64.LowPart >> INOSIZE)));
}

/*
 * Prepend "\\?\" to the path name and convert it to unicode to permit
 * an extended-length path for a maximum total path length of 32767
 * characters.
 * see also http://msdn.microsoft.com/en-us/library/aa365247.aspx
 */
wchar_t *
__la_win_permissive_name(const char *name)
{
	wchar_t *wn;
	wchar_t *ws;
	size_t ll;

	ll = strlen(name);
	wn = malloc((ll + 1) * sizeof(wchar_t));
	if (wn == NULL)
		return (NULL);
	ll = mbstowcs(wn, name, ll);
	if (ll == (size_t)-1) {
		free(wn);
		return (NULL);
	}
	wn[ll] = L'\0';
	ws = __la_win_permissive_name_w(wn);
	free(wn);
	return (ws);
}

wchar_t *
__la_win_permissive_name_w(const wchar_t *wname)
{
	wchar_t *wn, *wnp;
	wchar_t *ws, *wsp;
	DWORD l, len, slen;
	int unc;

	/* Get a full-pathname. */
	l = GetFullPathNameW(wname, 0, NULL, NULL);
	if (l == 0)
		return (NULL);
	/* NOTE: GetFullPathNameW has a bug that if the length of the file
	 * name is just 1 then it returns incomplete buffer size. Thus, we
	 * have to add three to the size to allocate a sufficient buffer
	 * size for the full-pathname of the file name. */
	l += 3;
	wnp = malloc(l * sizeof(wchar_t));
	if (wnp == NULL)
		return (NULL);
	len = GetFullPathNameW(wname, l, wnp, NULL);
	wn = wnp;

	if (wnp[0] == L'\\' && wnp[1] == L'\\' &&
	    wnp[2] == L'?' && wnp[3] == L'\\')
		/* We have already a permissive name. */
		return (wn);

	if (wnp[0] == L'\\' && wnp[1] == L'\\' &&
		wnp[2] == L'.' && wnp[3] == L'\\') {
		/* This is a device name */
		if (((wnp[4] >= L'a' && wnp[4] <= L'z') ||
		     (wnp[4] >= L'A' && wnp[4] <= L'Z')) &&
		    wnp[5] == L':' && wnp[6] == L'\\')
			wnp[2] = L'?';/* Not device name. */
		return (wn);
	}

	unc = 0;
	if (wnp[0] == L'\\' && wnp[1] == L'\\' && wnp[2] != L'\\') {
		wchar_t *p = &wnp[2];

		/* Skip server-name letters. */
		while (*p != L'\\' && *p != L'\0')
			++p;
		if (*p == L'\\') {
			wchar_t *rp = ++p;
			/* Skip share-name letters. */
			while (*p != L'\\' && *p != L'\0')
				++p;
			if (*p == L'\\' && p != rp) {
				/* Now, match patterns such as
				 * "\\server-name\share-name\" */
				wnp += 2;
				len -= 2;
				unc = 1;
			}
		}
	}

	slen = 4 + (unc * 4) + len + 1;
	ws = wsp = malloc(slen * sizeof(wchar_t));
	if (ws == NULL) {
		free(wn);
		return (NULL);
	}
	/* prepend "\\?\" */
	wcsncpy(wsp, L"\\\\?\\", 4);
	wsp += 4;
	slen -= 4;
	if (unc) {
		/* append "UNC\" ---> "\\?\UNC\" */
		wcsncpy(wsp, L"UNC\\", 4);
		wsp += 4;
		slen -= 4;
	}
	wcsncpy(wsp, wnp, slen);
	wsp[slen - 1] = L'\0'; /* Ensure null termination. */
	free(wn);
	return (ws);
}

/*
 * Create a file handle.
 * This can exceed MAX_PATH limitation.
 */
static HANDLE
la_CreateFile(const char *path, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	wchar_t *wpath;
	HANDLE handle;

	handle = CreateFileA(path, dwDesiredAccess, dwShareMode,
	    lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes,
	    hTemplateFile);
	if (handle != INVALID_HANDLE_VALUE)
		return (handle);
	if (GetLastError() != ERROR_PATH_NOT_FOUND)
		return (handle);
	wpath = __la_win_permissive_name(path);
	if (wpath == NULL)
		return (handle);
	handle = CreateFileW(wpath, dwDesiredAccess, dwShareMode,
	    lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes,
	    hTemplateFile);
	free(wpath);
	return (handle);
}

#if defined(__LA_LSEEK_NEEDED)
__int64
__la_lseek(int fd, __int64 offset, int whence)
{
	LARGE_INTEGER distance;
	LARGE_INTEGER newpointer;
	HANDLE handle;

	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	handle = (HANDLE)_get_osfhandle(fd);
	if (GetFileType(handle) != FILE_TYPE_DISK) {
		errno = EBADF;
		return (-1);
	}
	distance.QuadPart = offset;
	if (!SetFilePointerEx_perso(handle, distance, &newpointer, whence)) {
		DWORD lasterr;

		lasterr = GetLastError();
		if (lasterr == ERROR_BROKEN_PIPE)
			return (0);
		if (lasterr == ERROR_ACCESS_DENIED)
			errno = EBADF;
		else
			la_dosmaperr(lasterr);
		return (-1);
	}
	return (newpointer.QuadPart);
}
#endif

/* This can exceed MAX_PATH limitation. */
int
__la_open(const char *path, int flags, ...)
{
	va_list ap;
	wchar_t *ws;
	int r, pmode;
	DWORD attr;

	va_start(ap, flags);
	pmode = va_arg(ap, int);
	va_end(ap);
	ws = NULL;
	if ((flags & ~O_BINARY) == O_RDONLY) {
		/*
		 * When we open a directory, _open function returns 
		 * "Permission denied" error.
		 */
		attr = GetFileAttributesA(path);
		if (attr == (DWORD)-1 && GetLastError() == ERROR_PATH_NOT_FOUND) {
			ws = __la_win_permissive_name(path);
			if (ws == NULL) {
				errno = EINVAL;
				return (-1);
			}
			attr = GetFileAttributesW(ws);
		}
		if (attr == (DWORD)-1) {
			la_dosmaperr(GetLastError());
			free(ws);
			return (-1);
		}
		if (attr & FILE_ATTRIBUTE_DIRECTORY) {
			HANDLE handle;

			if (ws != NULL)
				handle = CreateFileW(ws, 0, 0, NULL,
				    OPEN_EXISTING,
				    FILE_FLAG_BACKUP_SEMANTICS |
				    FILE_ATTRIBUTE_READONLY,
					NULL);
			else
				handle = CreateFileA(path, 0, 0, NULL,
				    OPEN_EXISTING,
				    FILE_FLAG_BACKUP_SEMANTICS |
				    FILE_ATTRIBUTE_READONLY,
					NULL);
			free(ws);
			if (handle == INVALID_HANDLE_VALUE) {
				la_dosmaperr(GetLastError());
				return (-1);
			}
			r = _open_osfhandle((intptr_t)handle, _O_RDONLY);
			return (r);
		}
	}
	if (ws == NULL) {
#if defined(__BORLANDC__)
		/* Borland has no mode argument.
		   TODO: Fix mode of new file.  */
		r = _open(path, flags);
#else
		r = _open(path, flags, pmode);
#endif
		if (r < 0 && errno == EACCES && (flags & O_CREAT) != 0) {
			/* Simulate other POSIX system action to pass our test suite. */
			attr = GetFileAttributesA(path);
			if (attr == (DWORD)-1)
				la_dosmaperr(GetLastError());
			else if (attr & FILE_ATTRIBUTE_DIRECTORY)
				errno = EISDIR;
			else
				errno = EACCES;
			return (-1);
		}
		if (r >= 0 || errno != ENOENT)
			return (r);
		ws = __la_win_permissive_name(path);
		if (ws == NULL) {
			errno = EINVAL;
			return (-1);
		}
	}
	r = _wopen(ws, flags, pmode);
	if (r < 0 && errno == EACCES && (flags & O_CREAT) != 0) {
		/* Simulate other POSIX system action to pass our test suite. */
		attr = GetFileAttributesW(ws);
		if (attr == (DWORD)-1)
			la_dosmaperr(GetLastError());
		else if (attr & FILE_ATTRIBUTE_DIRECTORY)
			errno = EISDIR;
		else
			errno = EACCES;
	}
	free(ws);
	return (r);
}

ssize_t
__la_read(int fd, void *buf, size_t nbytes)
{
	HANDLE handle;
	DWORD bytes_read, lasterr;
	int r;

#ifdef _WIN64
	if (nbytes > UINT32_MAX)
		nbytes = UINT32_MAX;
#endif
	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	/* Do not pass 0 to third parameter of ReadFile(), read bytes.
	 * This will not return to application side. */
	if (nbytes == 0)
		return (0);
	handle = (HANDLE)_get_osfhandle(fd);
	r = ReadFile(handle, buf, (uint32_t)nbytes,
	    &bytes_read, NULL);
	if (r == 0) {
		lasterr = GetLastError();
		if (lasterr == ERROR_NO_DATA) {
			errno = EAGAIN;
			return (-1);
		}
		if (lasterr == ERROR_BROKEN_PIPE)
			return (0);
		if (lasterr == ERROR_ACCESS_DENIED)
			errno = EBADF;
		else
			la_dosmaperr(lasterr);
		return (-1);
	}
	return ((ssize_t)bytes_read);
}

/* Convert Windows FILETIME to UTC */
__inline static void
fileTimeToUTC(const FILETIME *filetime, time_t *t, long *ns)
{
	ULARGE_INTEGER utc;

	utc.HighPart = filetime->dwHighDateTime;
	utc.LowPart  = filetime->dwLowDateTime;
	if (utc.QuadPart >= EPOC_TIME) {
		utc.QuadPart -= EPOC_TIME;
		*t = (time_t)(utc.QuadPart / 10000000);	/* milli seconds base */
		*ns = (long)(utc.QuadPart % 10000000) * 100;/* nano seconds base */
	} else {
		*t = 0;
		*ns = 0;
	}
}

/* Stat by handle
 * Windows' stat() does not accept the path added "\\?\" especially "?"
 * character.
 * It means we cannot access the long name path longer than MAX_PATH.
 * So I've implemented simular Windows' stat() to access the long name path.
 * And I've added some feature.
 * 1. set st_ino by nFileIndexHigh and nFileIndexLow of
 *    BY_HANDLE_FILE_INFORMATION.
 * 2. set st_nlink by nNumberOfLinks of BY_HANDLE_FILE_INFORMATION.
 * 3. set st_dev by dwVolumeSerialNumber by BY_HANDLE_FILE_INFORMATION.
 */
static int
__hstat(HANDLE handle, struct ustat *st)
{
	BY_HANDLE_FILE_INFORMATION info;
	ULARGE_INTEGER ino64;
	DWORD ftype;
	mode_t mode;
	time_t t;
	long ns;

	switch (ftype = GetFileType(handle)) {
	case FILE_TYPE_UNKNOWN:
		errno = EBADF;
		return (-1);
	case FILE_TYPE_CHAR:
	case FILE_TYPE_PIPE:
		if (ftype == FILE_TYPE_CHAR) {
			st->st_mode = S_IFCHR;
			st->st_size = 0;
		} else {
			DWORD avail;

			st->st_mode = S_IFIFO;
			if (PeekNamedPipe(handle, NULL, 0, NULL, &avail, NULL))
				st->st_size = avail;
			else
				st->st_size = 0;
		}
		st->st_atime = 0;
		st->st_atime_nsec = 0;
		st->st_mtime = 0;
		st->st_mtime_nsec = 0;
		st->st_ctime = 0;
		st->st_ctime_nsec = 0;
		st->st_ino = 0;
		st->st_nlink = 1;
		st->st_uid = 0;
		st->st_gid = 0;
		st->st_rdev = 0;
		st->st_dev = 0;
		return (0);
	case FILE_TYPE_DISK:
		break;
	default:
		/* This ftype is undocumented type. */
		la_dosmaperr(GetLastError());
		return (-1);
	}

	ZeroMemory(&info, sizeof(info));
	if (!GetFileInformationByHandle (handle, &info)) {
		la_dosmaperr(GetLastError());
		return (-1);
	}

	mode = S_IRUSR | S_IRGRP | S_IROTH;
	if ((info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == 0)
		mode |= S_IWUSR | S_IWGRP | S_IWOTH;
	if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
	else
		mode |= S_IFREG;
	st->st_mode = mode;
	
	fileTimeToUTC(&info.ftLastAccessTime, &t, &ns);
	st->st_atime = t; 
	st->st_atime_nsec = ns;
	fileTimeToUTC(&info.ftLastWriteTime, &t, &ns);
	st->st_mtime = t;
	st->st_mtime_nsec = ns;
	fileTimeToUTC(&info.ftCreationTime, &t, &ns);
	st->st_ctime = t;
	st->st_ctime_nsec = ns;
	st->st_size = 
	    ((int64_t)(info.nFileSizeHigh) * ((int64_t)MAXDWORD + 1))
		+ (int64_t)(info.nFileSizeLow);
#ifdef SIMULATE_WIN_STAT
	st->st_ino = 0;
	st->st_nlink = 1;
	st->st_dev = 0;
#else
	/* Getting FileIndex as i-node. We should remove a sequence which
	 * is high-16-bits of nFileIndexHigh. */
	ino64.HighPart = info.nFileIndexHigh & 0x0000FFFFUL;
	ino64.LowPart  = info.nFileIndexLow;
	st->st_ino = ino64.QuadPart;
	st->st_nlink = info.nNumberOfLinks;
	if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		++st->st_nlink;/* Add parent directory. */
	st->st_dev = info.dwVolumeSerialNumber;
#endif
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_rdev = 0;
	return (0);
}

static void
copy_stat(struct stat *st, struct ustat *us)
{
	st->st_atime = us->st_atime;
	st->st_ctime = us->st_ctime;
	st->st_mtime = us->st_mtime;
	st->st_gid = us->st_gid;
	st->st_ino = getino(us);
	st->st_mode = us->st_mode;
	st->st_nlink = us->st_nlink;
	st->st_size = (off_t)us->st_size;
	st->st_uid = us->st_uid;
	st->st_dev = us->st_dev;
	st->st_rdev = us->st_rdev;
}

/*
 * TODO: Remove a use of __la_fstat and __la_stat.
 * We should use GetFileInformationByHandle in place
 * where We still use the *stat functions.
 */
int
__la_fstat(int fd, struct stat *st)
{
	struct ustat u;
	int ret;

	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	ret = __hstat((HANDLE)_get_osfhandle(fd), &u);
	if (ret >= 0) {
		copy_stat(st, &u);
		if (u.st_mode & (S_IFCHR | S_IFIFO)) {
			st->st_dev = fd;
			st->st_rdev = fd;
		}
	}
	return (ret);
}

/* This can exceed MAX_PATH limitation. */
int
__la_stat(const char *path, struct stat *st)
{
	HANDLE handle;
	struct ustat u;
	int ret;

	handle = la_CreateFile(path, 0, 0, NULL, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		la_dosmaperr(GetLastError());
		return (-1);
	}
	ret = __hstat(handle, &u);
	CloseHandle(handle);
	if (ret >= 0) {
		char *p;

		copy_stat(st, &u);
		p = strrchr(path, '.');
		if (p != NULL && strlen(p) == 4) {
			char exttype[4];

			++ p;
			exttype[0] = toupper(*p++);
			exttype[1] = toupper(*p++);
			exttype[2] = toupper(*p++);
			exttype[3] = '\0';
			if (!strcmp(exttype, "EXE") || !strcmp(exttype, "CMD") ||
				!strcmp(exttype, "BAT") || !strcmp(exttype, "COM"))
				st->st_mode |= S_IXUSR | S_IXGRP | S_IXOTH;
		}
	}
	return (ret);
}

/*
 * This waitpid is limited implementation.
 */
pid_t
__la_waitpid(HANDLE child, int *status, int option)
{
	DWORD cs;

	(void)option;/* UNUSED */
	do {
		if (GetExitCodeProcess(child, &cs) == 0) {
			CloseHandle(child);
			la_dosmaperr(GetLastError());
			*status = 0;
			return (-1);
		}
	} while (cs == STILL_ACTIVE);

	*status = (int)(cs & 0xff);
	return (0);
}

ssize_t
__la_write(int fd, const void *buf, size_t nbytes)
{
	DWORD bytes_written;

#ifdef _WIN64
	if (nbytes > UINT32_MAX)
		nbytes = UINT32_MAX;
#endif
	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	if (!WriteFile((HANDLE)_get_osfhandle(fd), buf, (uint32_t)nbytes,
	    &bytes_written, NULL)) {
		DWORD lasterr;

		lasterr = GetLastError();
		if (lasterr == ERROR_ACCESS_DENIED)
			errno = EBADF;
		else
			la_dosmaperr(lasterr);
		return (-1);
	}
	return (bytes_written);
}

/*
 * Replace the Windows path separator '\' with '/'.
 */
static int
replace_pathseparator(struct archive_wstring *ws, const wchar_t *wp)
{
	wchar_t *w;
	size_t path_length;

	if (wp == NULL)
		return(0);
	if (wcschr(wp, L'\\') == NULL)
		return(0);
	path_length = wcslen(wp);
	if (archive_wstring_ensure(ws, path_length) == NULL)
		return(-1);
	archive_wstrncpy(ws, wp, path_length);
	for (w = ws->s; *w; w++) {
		if (*w == L'\\')
			*w = L'/';
	}
	return(1);
}

static int
fix_pathseparator(struct archive_entry *entry)
{
	struct archive_wstring ws;
	const wchar_t *wp;
	int ret = ARCHIVE_OK;

	archive_string_init(&ws);
	wp = archive_entry_pathname_w(entry);
	switch (replace_pathseparator(&ws, wp)) {
	case 0: /* Not replaced. */
		break;
	case 1: /* Replaced. */
		archive_entry_copy_pathname_w(entry, ws.s);
		break;
	default:
		ret = ARCHIVE_FAILED;
	}
	wp = archive_entry_hardlink_w(entry);
	switch (replace_pathseparator(&ws, wp)) {
	case 0: /* Not replaced. */
		break;
	case 1: /* Replaced. */
		archive_entry_copy_hardlink_w(entry, ws.s);
		break;
	default:
		ret = ARCHIVE_FAILED;
	}
	wp = archive_entry_symlink_w(entry);
	switch (replace_pathseparator(&ws, wp)) {
	case 0: /* Not replaced. */
		break;
	case 1: /* Replaced. */
		archive_entry_copy_symlink_w(entry, ws.s);
		break;
	default:
		ret = ARCHIVE_FAILED;
	}
	archive_wstring_free(&ws);
	return(ret);
}

struct archive_entry *
__la_win_entry_in_posix_pathseparator(struct archive_entry *entry)
{
	struct archive_entry *entry_main;
	const wchar_t *wp;
	int has_backslash = 0;
	int ret;

	wp = archive_entry_pathname_w(entry);
	if (wp != NULL && wcschr(wp, L'\\') != NULL)
		has_backslash = 1;
	if (!has_backslash) {
		wp = archive_entry_hardlink_w(entry);
		if (wp != NULL && wcschr(wp, L'\\') != NULL)
			has_backslash = 1;
	}
	if (!has_backslash) {
		wp = archive_entry_symlink_w(entry);
		if (wp != NULL && wcschr(wp, L'\\') != NULL)
			has_backslash = 1;
	}
	/*
	 * If there is no backslach chars, return the original.
	 */
	if (!has_backslash)
		return (entry);

	/* Copy entry so we can modify it as needed. */
	entry_main = archive_entry_clone(entry);
	if (entry_main == NULL)
		return (NULL);
	/* Replace the Windows path-separator '\' with '/'. */
	ret = fix_pathseparator(entry_main);
	if (ret < ARCHIVE_WARN) {
		archive_entry_free(entry_main);
		return (NULL);
	}
	return (entry_main);
}


/*
 * The following function was modified from PostgreSQL sources and is
 * subject to the copyright below.
 */
/*-------------------------------------------------------------------------
 *
 * win32error.c
 *	  Map win32 error codes to errno values
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/port/win32error.c,v 1.4 2008/01/01 19:46:00 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
/*
PostgreSQL Database Management System
(formerly known as Postgres, then as Postgres95)

Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group

Portions Copyright (c) 1994, The Regents of the University of California

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose, without fee, and without a written agreement
is hereby granted, provided that the above copyright notice and this
paragraph and the following two paragraphs appear in all copies.

IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATIONS TO
PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/

static const struct {
	DWORD		winerr;
	int		doserr;
} doserrors[] =
{
	{	ERROR_INVALID_FUNCTION, EINVAL	},
	{	ERROR_FILE_NOT_FOUND, ENOENT	},
	{	ERROR_PATH_NOT_FOUND, ENOENT	},
	{	ERROR_TOO_MANY_OPEN_FILES, EMFILE	},
	{	ERROR_ACCESS_DENIED, EACCES	},
	{	ERROR_INVALID_HANDLE, EBADF	},
	{	ERROR_ARENA_TRASHED, ENOMEM	},
	{	ERROR_NOT_ENOUGH_MEMORY, ENOMEM	},
	{	ERROR_INVALID_BLOCK, ENOMEM	},
	{	ERROR_BAD_ENVIRONMENT, E2BIG	},
	{	ERROR_BAD_FORMAT, ENOEXEC	},
	{	ERROR_INVALID_ACCESS, EINVAL	},
	{	ERROR_INVALID_DATA, EINVAL	},
	{	ERROR_INVALID_DRIVE, ENOENT	},
	{	ERROR_CURRENT_DIRECTORY, EACCES	},
	{	ERROR_NOT_SAME_DEVICE, EXDEV	},
	{	ERROR_NO_MORE_FILES, ENOENT	},
	{	ERROR_LOCK_VIOLATION, EACCES	},
	{	ERROR_SHARING_VIOLATION, EACCES	},
	{	ERROR_BAD_NETPATH, ENOENT	},
	{	ERROR_NETWORK_ACCESS_DENIED, EACCES	},
	{	ERROR_BAD_NET_NAME, ENOENT	},
	{	ERROR_FILE_EXISTS, EEXIST	},
	{	ERROR_CANNOT_MAKE, EACCES	},
	{	ERROR_FAIL_I24, EACCES	},
	{	ERROR_INVALID_PARAMETER, EINVAL	},
	{	ERROR_NO_PROC_SLOTS, EAGAIN	},
	{	ERROR_DRIVE_LOCKED, EACCES	},
	{	ERROR_BROKEN_PIPE, EPIPE	},
	{	ERROR_DISK_FULL, ENOSPC	},
	{	ERROR_INVALID_TARGET_HANDLE, EBADF	},
	{	ERROR_INVALID_HANDLE, EINVAL	},
	{	ERROR_WAIT_NO_CHILDREN, ECHILD	},
	{	ERROR_CHILD_NOT_COMPLETE, ECHILD	},
	{	ERROR_DIRECT_ACCESS_HANDLE, EBADF	},
	{	ERROR_NEGATIVE_SEEK, EINVAL	},
	{	ERROR_SEEK_ON_DEVICE, EACCES	},
	{	ERROR_DIR_NOT_EMPTY, ENOTEMPTY	},
	{	ERROR_NOT_LOCKED, EACCES	},
	{	ERROR_BAD_PATHNAME, ENOENT	},
	{	ERROR_MAX_THRDS_REACHED, EAGAIN	},
	{	ERROR_LOCK_FAILED, EACCES	},
	{	ERROR_ALREADY_EXISTS, EEXIST	},
	{	ERROR_FILENAME_EXCED_RANGE, ENOENT	},
	{	ERROR_NESTING_NOT_ALLOWED, EAGAIN	},
	{	ERROR_NOT_ENOUGH_QUOTA, ENOMEM	}
};

void
__la_dosmaperr(unsigned long e)
{
	int			i;

	if (e == 0)
	{
		errno = 0;
		return;
	}

	for (i = 0; i < (int)sizeof(doserrors); i++)
	{
		if (doserrors[i].winerr == e)
		{
			errno = doserrors[i].doserr;
			return;
		}
	}

	/* fprintf(stderr, "unrecognized win32 error code: %lu", e); */
	errno = EINVAL;
	return;
}

#endif /* _WIN32 && !__CYGWIN__ */
