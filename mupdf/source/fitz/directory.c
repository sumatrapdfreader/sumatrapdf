// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"

#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#include <windows.h>
#include <errno.h>
#define stat _stat
#else
#include <dirent.h>
#endif

typedef struct
{
	fz_archive super;

	char *path;

	int max_entries;
	int num_entries;
	char **entries;
} fz_directory;

static void drop_directory(fz_context *ctx, fz_archive *arch)
{
	fz_directory *dir = (fz_directory *) arch;
	int i;

	fz_free(ctx, dir->path);
	for (i = 0; i < dir->num_entries; i++)
		fz_free(ctx, dir->entries[i]);
	fz_free(ctx, dir->entries);
}

static fz_stream *open_dir_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_directory *dir = (fz_directory *) arch;
	char path[2048];
	fz_strlcpy(path, dir->path, sizeof path);
	fz_strlcat(path, "/", sizeof path);
	fz_strlcat(path, name, sizeof path);
	return fz_try_open_file(ctx, path);
}

static fz_buffer *read_dir_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_directory *dir = (fz_directory *) arch;
	char path[2048];
	fz_strlcpy(path, dir->path, sizeof path);
	fz_strlcat(path, "/", sizeof path);
	fz_strlcat(path, name, sizeof path);
	return fz_try_read_file(ctx, path);
}

static int has_dir_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_directory *dir = (fz_directory *) arch;
	char path[2048];
	fz_strlcpy(path, dir->path, sizeof path);
	fz_strlcat(path, "/", sizeof path);
	fz_strlcat(path, name, sizeof path);
	return fz_file_exists(ctx, path);
}

int
fz_is_directory(fz_context *ctx, const char *path)
{
	struct stat info;

	if (stat(path, &info) < 0)
		return 0;

	return S_ISDIR(info.st_mode);
}

static int
count_dir_entries(fz_context *ctx, fz_archive *arch)
{
	fz_directory *dir = (fz_directory *) arch;

	return dir->num_entries;
}

const char *
list_dir_entry(fz_context *ctx, fz_archive *arch, int n)
{
	fz_directory *dir = (fz_directory *) arch;

	if (n < 0 || n >= dir->num_entries)
		return NULL;

	return dir->entries[n];
}

fz_archive *
fz_open_directory(fz_context *ctx, const char *path)
{
	fz_directory *dir;
#ifdef _MSC_VER
	WCHAR *wpath = NULL;
	size_t z = 3;
	HANDLE h = NULL;
	WIN32_FIND_DATAW dw;

	fz_var(wpath);
	fz_var(h);
#else
	DIR *dp;
	struct dirent *ep;

	fz_var(dp);
#endif

	if (!fz_is_directory(ctx, path))
		fz_throw(ctx, FZ_ERROR_FORMAT, "'%s' is not a directory", path);

	dir = fz_new_derived_archive(ctx, NULL, fz_directory);
	dir->super.format = "dir";
	dir->super.count_entries = count_dir_entries;
	dir->super.list_entry = list_dir_entry;
	dir->super.has_entry = has_dir_entry;
	dir->super.read_entry = read_dir_entry;
	dir->super.open_entry = open_dir_entry;
	dir->super.drop_archive = drop_directory;

	fz_try(ctx)
	{
#ifdef _MSC_VER
		char const *p = path;
		WCHAR *w;
		while (*p)
		{
			int rune;
			p += fz_chartorune(&rune, p);
			if (rune >= 0x10000 || rune < 0)
			{
				errno = EINVAL;
				fz_throw(ctx, FZ_ERROR_SYSTEM, "Unrepresentable UTF-8 char in directory name");
			}
			z++;
		}
		w = wpath = fz_malloc(ctx, z * sizeof(WCHAR));
		p = path;
		while (*p)
		{
			int rune;
			p += fz_chartorune(&rune, p);
			*w++ = rune;
		}
		w[0] = '\\';
		w[1] = '*';
		w[2] = 0;

		/* Now enumerate the paths. */
		h = FindFirstFileW(wpath, &dw);
		if (h == INVALID_HANDLE_VALUE)
			break;

		do
		{
			char *u;

			if (dir->max_entries == dir->num_entries)
			{
				int newmax = dir->max_entries * 2;
				if (newmax == 0)
					newmax = 32;

				dir->entries = fz_realloc(ctx, dir->entries, sizeof(*dir->entries) * newmax);
				dir->max_entries = newmax;
			}

			/* Count the len as utf-8. */
			w = dw.cFileName;
			z = 1;
			while (*w)
				z += fz_runelen(*w++);

			u = dir->entries[dir->num_entries] = fz_malloc(ctx, z);
			dir->num_entries++;

			/* Copy the name across. */
			w = dw.cFileName;
			while (*w)
				u += fz_runetochar(u, *w++);
			*u = 0;
		}
		while (FindNextFileW(h, &dw));
#else
		dp = opendir(path);
		if (dp == NULL)
			break;

		while ((ep = readdir(dp)) != NULL)
		{
			if (dir->max_entries == dir->num_entries)
			{
				int newmax = dir->max_entries * 2;
				if (newmax == 0)
					newmax = 32;

				dir->entries = fz_realloc(ctx, dir->entries, sizeof(*dir->entries) * newmax);
				dir->max_entries = newmax;
			}

			dir->entries[dir->num_entries] = fz_strdup(ctx, ep->d_name);
			dir->num_entries++;
		}
#endif
		dir->path = fz_strdup(ctx, path);
	}
	fz_always(ctx)
	{
#ifdef _MSC_VER
		fz_free(ctx, wpath);
		if (h)
			(void)FindClose(h);
#else
		if (dp)
			(void)closedir(dp);
#endif
	}
	fz_catch(ctx)
	{
		fz_drop_archive(ctx, &dir->super);
		fz_rethrow(ctx);
	}

	return &dir->super;
}
