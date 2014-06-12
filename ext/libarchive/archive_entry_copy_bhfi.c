/*-
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
__FBSDID("$FreeBSD$");

#include "archive_private.h"
#include "archive_entry.h"

#if defined(_WIN32) && !defined(__CYGWIN__)

#define EPOC_TIME ARCHIVE_LITERAL_ULL(116444736000000000)

__inline static void
fileTimeToUtc(const FILETIME *filetime, time_t *t, long *ns)
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

void
archive_entry_copy_bhfi(struct archive_entry *entry,
			BY_HANDLE_FILE_INFORMATION *bhfi)
{
	time_t secs;
	long nsecs;

	fileTimeToUtc(&bhfi->ftLastAccessTime, &secs, &nsecs);
	archive_entry_set_atime(entry, secs, nsecs);
	fileTimeToUtc(&bhfi->ftLastWriteTime, &secs, &nsecs);
	archive_entry_set_mtime(entry, secs, nsecs);
	fileTimeToUtc(&bhfi->ftCreationTime, &secs, &nsecs);
	archive_entry_set_birthtime(entry, secs, nsecs);
	archive_entry_set_ctime(entry, secs, nsecs);
	archive_entry_set_dev(entry, bhfi->dwVolumeSerialNumber);
	archive_entry_set_ino64(entry, (((int64_t)bhfi->nFileIndexHigh) << 32)
		+ bhfi->nFileIndexLow);
	archive_entry_set_nlink(entry, bhfi->nNumberOfLinks);
	archive_entry_set_size(entry, (((int64_t)bhfi->nFileSizeHigh) << 32)
		+ bhfi->nFileSizeLow);
	/* archive_entry_set_mode(entry, st->st_mode); */
}
#endif
