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
#define stat _stat
#endif

typedef struct
{
	fz_archive super;

	char *path;
} fz_directory;

static void drop_directory(fz_context *ctx, fz_archive *arch)
{
	fz_directory *dir = (fz_directory *) arch;

	fz_free(ctx, dir->path);
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

fz_archive *
fz_open_directory(fz_context *ctx, const char *path)
{
	fz_directory *dir;

	if (!fz_is_directory(ctx, path))
		fz_throw(ctx, FZ_ERROR_FORMAT, "'%s' is not a directory", path);

	dir = fz_new_derived_archive(ctx, NULL, fz_directory);
	dir->super.format = "dir";
	dir->super.has_entry = has_dir_entry;
	dir->super.read_entry = read_dir_entry;
	dir->super.open_entry = open_dir_entry;
	dir->super.drop_archive = drop_directory;

	fz_try(ctx)
	{
		dir->path = fz_strdup(ctx, path);
	}
	fz_catch(ctx)
	{
		fz_drop_archive(ctx, &dir->super);
		fz_rethrow(ctx);
	}

	return &dir->super;
}
