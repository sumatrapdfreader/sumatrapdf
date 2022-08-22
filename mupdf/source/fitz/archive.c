// Copyright (C) 2004-2022 Artifex Software, Inc.
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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

#include "mupdf/fitz.h"

fz_stream *
fz_open_archive_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	if (arch == NULL || !arch->open_entry)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open archive entry");
	return arch->open_entry(ctx, arch, name);
}

fz_buffer *
fz_read_archive_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	if (arch == NULL || !arch->read_entry)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot read archive entry");
	return arch->read_entry(ctx, arch, name);
}

int
fz_has_archive_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	if (arch == NULL)
		return 0;
	if (!arch->has_entry)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot check if archive has entry");
	return arch->has_entry(ctx, arch, name);
}

const char *
fz_list_archive_entry(fz_context *ctx, fz_archive *arch, int idx)
{
	if (!arch->list_entry)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot list archive entries");
	return arch->list_entry(ctx, arch, idx);
}

int
fz_count_archive_entries(fz_context *ctx, fz_archive *arch)
{
	if (arch == NULL)
		return 0;
	if (!arch->count_entries)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot count archive entries");
	return arch->count_entries(ctx, arch);
}

const char *
fz_archive_format(fz_context *ctx, fz_archive *arch)
{
	if (arch == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot read format of non-existent archive");
	return arch->format;
}

fz_archive *
fz_new_archive_of_size(fz_context *ctx, fz_stream *file, int size)
{
	fz_archive *arch;
	arch = Memento_label(fz_calloc(ctx, 1, size), "fz_archive");
	arch->file = fz_keep_stream(ctx, file);
	return arch;
}

fz_archive *
fz_open_archive_with_stream(fz_context *ctx, fz_stream *file)
{
	fz_archive *arch = NULL;

	if (fz_is_zip_archive(ctx, file))
		arch = fz_open_zip_archive_with_stream(ctx, file);
	else if (fz_is_tar_archive(ctx, file))
		arch = fz_open_tar_archive_with_stream(ctx, file);
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot recognize archive");

	return arch;
}

fz_archive *
fz_open_archive(fz_context *ctx, const char *filename)
{
	fz_stream *file;
	fz_archive *arch = NULL;

	file = fz_open_file(ctx, filename);

	fz_try(ctx)
		arch = fz_open_archive_with_stream(ctx, file);
	fz_always(ctx)
		fz_drop_stream(ctx, file);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return arch;
}

void
fz_drop_archive(fz_context *ctx, fz_archive *arch)
{
	if (!arch)
		return;

	if (arch->drop_archive)
		arch->drop_archive(ctx, arch);
	fz_drop_stream(ctx, arch->file);
	fz_free(ctx, arch);
}

/* In-memory archive using a fz_tree holding fz_buffers */

typedef struct
{
	fz_archive super;
	fz_tree *tree;
} fz_tree_archive;

static int has_tree_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_tree *tree = ((fz_tree_archive*)arch)->tree;
	fz_buffer *ent = fz_tree_lookup(ctx, tree, name);
	return ent != NULL;
}

static fz_buffer *read_tree_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_tree *tree = ((fz_tree_archive*)arch)->tree;
	fz_buffer *ent = fz_tree_lookup(ctx, tree, name);
	return fz_keep_buffer(ctx, ent);
}

static fz_stream *open_tree_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_tree *tree = ((fz_tree_archive*)arch)->tree;
	fz_buffer *ent = fz_tree_lookup(ctx, tree, name);
	return fz_open_buffer(ctx, ent);
}

static void drop_tree_archive_entry(fz_context *ctx, void *ent)
{
	fz_drop_buffer(ctx, ent);
}

static void drop_tree_archive(fz_context *ctx, fz_archive *arch)
{
	fz_tree *tree = ((fz_tree_archive*)arch)->tree;
	fz_drop_tree(ctx, tree, drop_tree_archive_entry);
}

fz_archive *
fz_new_tree_archive(fz_context *ctx, fz_tree *tree)
{
	fz_tree_archive *arch;

	arch = fz_new_derived_archive(ctx, NULL, fz_tree_archive);
	arch->super.format = "tree";
	arch->super.has_entry = has_tree_entry;
	arch->super.read_entry = read_tree_entry;
	arch->super.open_entry = open_tree_entry;
	arch->super.drop_archive = drop_tree_archive;
	arch->tree = tree;

	return &arch->super;
}
