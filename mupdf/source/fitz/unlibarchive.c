// Copyright (C) 2023 Artifex Software, Inc.
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

#ifdef HAVE_LIBARCHIVE

#ifdef _WIN32
#include "libarchive/archive.h"
#include "libarchive/archive_entry.h"
#else
#include <archive.h>
#include <archive_entry.h>
#endif

typedef struct
{
	size_t len;
	uint8_t name[32];
} entry_t;

typedef struct
{
	fz_archive super;

	struct archive *archive;

	int current_entry_idx;

	int entries_max;
	int entries_len;
	entry_t **entries;

	fz_context *ctx; /* safe! */
	uint8_t block[4096];
} fz_libarchive_archive;

static la_ssize_t
libarchive_read(struct archive *a, void *client_data, const void **buf)
{
	fz_libarchive_archive *arch = (fz_libarchive_archive *)client_data;
	size_t z;
	uint8_t *p;
	size_t left;
	fz_context *ctx = arch->ctx;
	la_ssize_t ret = 0;

	fz_try(ctx)
	{
		z = fz_available(arch->ctx, arch->super.file, 1024);

		/* If we're at the EOF, can't read anything! */
		if (z == 0)
			break;

		/* If we have at least 1K, then just return the pointer to that
		 * directly. */
		if (z >= 1024)
		{
			*buf = arch->super.file->rp;
			arch->super.file->rp += z;
			ret = (la_ssize_t)z;
			break;
		}

		/* If not, let's pull a large enough lump out. */

		left = sizeof(arch->block);
		p = arch->block;
		do
		{
			memcpy(p, arch->super.file->rp, z);
			p += z;
			arch->super.file->rp += z;
			left -= z;
			if (left)
			{
				z = fz_available(arch->ctx, arch->super.file, left);
				if (z > left)
					z = left;
				if (z == 0)
					break;
			}
		}
		while (left != 0);

		ret = p - arch->block;
		*buf = arch->block;
	}
	fz_catch(ctx)
	{
		/* Ignore error */
		archive_set_error(a, ARCHIVE_FATAL, "%s", fz_convert_error(ctx, NULL));
		return -1;
	}

	return ret;
}

static la_int64_t
libarchive_skip(struct archive *a, void *client_data, la_int64_t skip)
{
	fz_libarchive_archive *arch = (fz_libarchive_archive *)client_data;
	int64_t pos;
	fz_context *ctx = arch->ctx;

	fz_try(ctx)
	{
		pos = fz_tell(arch->ctx, arch->super.file);
		fz_seek(arch->ctx, arch->super.file, pos + skip, SEEK_SET);
		pos = fz_tell(arch->ctx, arch->super.file) - pos;
	}
	fz_catch(ctx)
	{
		/* Ignore error */
		archive_set_error(a, ARCHIVE_FATAL, "%s", fz_convert_error(ctx, NULL));
		return -1;
	}

	return pos;
}

static la_int64_t
libarchive_seek(struct archive *a, void *client_data, la_int64_t offset, int whence)
{
	fz_libarchive_archive *arch = (fz_libarchive_archive *)client_data;
	fz_context *ctx = arch->ctx;
	int64_t pos;

	fz_try(ctx)
	{
		fz_seek(arch->ctx, arch->super.file, offset, whence);
		pos = fz_tell(arch->ctx, arch->super.file);
	}
	fz_catch(ctx)
	{
		/* Ignore error */
		archive_set_error(a, ARCHIVE_FATAL, "%s", fz_convert_error(ctx, NULL));
		return -1;
	}

	return pos;
}

static int
libarchive_close(struct archive *a, void *client_data)
{
	/* Nothing to do. Stream is dropped when the fz_archive is closed. */
	return ARCHIVE_OK;
}

static int
libarchive_open(fz_context *ctx, fz_libarchive_archive *arch)
{
	int r;

	arch->archive = archive_read_new();
	archive_read_support_filter_all(arch->archive);
	archive_read_support_format_all(arch->archive);

	arch->ctx = ctx;
	r = archive_read_set_seek_callback(arch->archive, libarchive_seek);
	if (r == ARCHIVE_OK)
		r = archive_read_open2(arch->archive, arch, NULL, libarchive_read, libarchive_skip, libarchive_close);
	arch->ctx = NULL;
	if (r != ARCHIVE_OK)
	{
		archive_read_free(arch->archive);
		arch->archive = NULL;
	}

	return r != ARCHIVE_OK;
}

static void
libarchive_reset(fz_context *ctx, fz_libarchive_archive *arch)
{
	if (arch->archive)
	{
		archive_read_free(arch->archive);
		arch->archive = NULL;
	}
	fz_seek(ctx, arch->super.file, 0, SEEK_SET);
	if (libarchive_open(ctx, arch))
		fz_throw(ctx, FZ_ERROR_LIBRARY, "Failed to restart archive traversal!");

	arch->current_entry_idx = 0;
}

static void
drop_libarchive_archive(fz_context *ctx, fz_archive *arch_)
{
	fz_libarchive_archive *arch = (fz_libarchive_archive *)arch_;
	int i;

	archive_read_free(arch->archive);
	for (i = 0; i < arch->entries_len; ++i)
		fz_free(ctx, arch->entries[i]);
	fz_free(ctx, arch->entries);
	arch->archive = NULL;
}

int
fz_is_libarchive_archive(fz_context *ctx, fz_stream *file)
{
	fz_libarchive_archive arch;
	struct archive_entry *entry;
	int ret;

	arch.super.file = file;
	fz_seek(ctx, file, 0, SEEK_SET);

	/* Annoyingly, libarchive can say "sure, I can open this" only to
	 * then fail when we try to read from it. We therefore need to
	 * try to read at least 1 entry out to be sure. */
	ret = libarchive_open(ctx, &arch);
	if (ret == ARCHIVE_OK)
	{
		fz_var(ret);

		fz_try(ctx)
		{
			arch.ctx = ctx; /* safe */
			ret = archive_read_next_header(arch.archive, &entry);
		}
		fz_catch(ctx)
		{
			archive_read_free(arch.archive);
			fz_rethrow(ctx);
		}
	}

	archive_read_free(arch.archive);

	/* Do NOT return true if we get ARCHIVE_EOF. We will fail to recognise empty
	 * archives, but the alternative is false positives. */
	return ret == ARCHIVE_OK;
}

static int
lookup_archive_entry(fz_context *ctx, fz_libarchive_archive *arch, const char *name)
{
	int idx;

	for (idx = 0; idx < arch->entries_len; idx++)
	{
		if (!strcmp(name, (const char *)arch->entries[idx]->name))
			return idx;
	}

	return -1;
}

static int has_libarchive_entry(fz_context *ctx, fz_archive *arch_, const char *name)
{
	fz_libarchive_archive *arch = (fz_libarchive_archive *)arch_;
	return lookup_archive_entry(ctx, arch, name) != -1;
}

static const char *list_libarchive_entry(fz_context *ctx, fz_archive *arch_, int idx)
{
	fz_libarchive_archive *arch = (fz_libarchive_archive *)arch_;
	if (idx < 0 || idx >= arch->entries_len)
		return NULL;
	return (const char *)arch->entries[idx]->name;
}

static int count_libarchive_entries(fz_context *ctx, fz_archive *arch_)
{
	fz_libarchive_archive *arch = (fz_libarchive_archive *)arch_;
	return arch->entries_len;
}

static fz_buffer *
read_libarchive_entry(fz_context *ctx, fz_archive *arch_, const char *name)
{
	fz_libarchive_archive *arch = (fz_libarchive_archive *)arch_;
	fz_buffer *ubuf = NULL;
	int idx;
	struct archive_entry *entry;
	la_ssize_t ret;
	size_t size;

	idx = lookup_archive_entry(ctx, arch, name);
	if (idx < 0)
		return NULL;

	if (arch->current_entry_idx > idx)
		libarchive_reset(ctx, arch);

	fz_var(ubuf);

	arch->ctx = ctx;
	fz_try(ctx)
	{
		while (arch->current_entry_idx < idx)
		{
			int r = archive_read_next_header(arch->archive, &entry);
			if (r == ARCHIVE_OK)
				r = archive_read_data_skip(arch->archive);
			if (r != ARCHIVE_OK)
				fz_throw(ctx, FZ_ERROR_LIBRARY, "Failed to skip over archive entry");
			arch->current_entry_idx++;
		}

		/* This is the one we want. */
		if (archive_read_next_header(arch->archive, &entry) != ARCHIVE_OK)
			fz_throw(ctx, FZ_ERROR_LIBRARY, "Failed to read archive entry header");

		arch->current_entry_idx++;
		size = arch->entries[idx]->len;
		ubuf = fz_new_buffer(ctx, size);
		ubuf->len = size;

		ret = archive_read_data(arch->archive, ubuf->data, size);
		if (ret < 0)
			fz_throw(ctx, FZ_ERROR_LIBRARY, "Failed to read archive data");
		if ((size_t)ret != size)
			fz_warn(ctx, "Premature end of data reading archive entry data (%zu vs %zu)", (size_t)ubuf->len, (size_t)size);
	}
	fz_always(ctx)
		arch->ctx = NULL;
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, ubuf);
		fz_rethrow(ctx);
	}

	return ubuf;
}

static fz_stream *
open_libarchive_entry(fz_context *ctx, fz_archive *arch_, const char *name)
{
	fz_buffer *buf = read_libarchive_entry(ctx, arch_, name);
	fz_stream *stm = NULL;

	fz_try(ctx)
		stm = fz_open_buffer(ctx, buf);
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return stm;
}

fz_archive *
fz_open_libarchive_archive_with_stream(fz_context *ctx, fz_stream *file)
{
	fz_libarchive_archive *arch = fz_new_derived_archive(ctx, file, fz_libarchive_archive);
	int r;

	fz_seek(ctx, file, 0, SEEK_SET);

	if (libarchive_open(ctx, arch) != ARCHIVE_OK)
	{
		fz_drop_archive(ctx, &arch->super);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "cannot recognize libarchive archive");
	}

	arch->super.format = "libarchive";
	arch->super.count_entries = count_libarchive_entries;
	arch->super.list_entry = list_libarchive_entry;
	arch->super.has_entry = has_libarchive_entry;
	arch->super.read_entry = read_libarchive_entry;
	arch->super.open_entry = open_libarchive_entry;
	arch->super.drop_archive = drop_libarchive_archive;

	fz_try(ctx)
	{
		arch->ctx = ctx;
		/* Count the archive entries */
		do
		{
			struct archive_entry *entry;
			const char *path;
			size_t z;

			r = archive_read_next_header(arch->archive, &entry);
			if (r == ARCHIVE_EOF)
				break;

			if (r != ARCHIVE_OK)
				fz_throw(ctx, FZ_ERROR_LIBRARY, "Corrupt archive");

			path = archive_entry_pathname_utf8(entry);
			if (!path)
				continue;

			if (arch->entries_len == arch->entries_max)
			{
				int new_max = arch->entries_max * 2;
				if (new_max == 0)
					new_max = 32;

				arch->entries = fz_realloc(ctx, arch->entries, sizeof(arch->entries[0]) * new_max);
				arch->entries_max = new_max;
			}

			z = strlen(path);
			arch->entries[arch->entries_len] = fz_malloc(ctx, sizeof(entry_t) - 32 + z + 1);
			memcpy(&arch->entries[arch->entries_len]->name[0], path, z+1);
			arch->entries[arch->entries_len]->len = archive_entry_size(entry);

			arch->entries_len++;
		}
		while (r != ARCHIVE_EOF && r != ARCHIVE_FATAL);

		libarchive_reset(ctx, arch);
	}
	fz_catch(ctx)
	{
		arch->ctx = NULL;
		fz_drop_archive(ctx, &arch->super);
		fz_rethrow(ctx);
	}

	return &arch->super;
}

fz_archive *
fz_open_libarchive_archive(fz_context *ctx, const char *filename)
{
	fz_archive *tar = NULL;
	fz_stream *file;

	file = fz_open_file(ctx, filename);

	fz_try(ctx)
		tar = fz_open_libarchive_archive_with_stream(ctx, file);
	fz_always(ctx)
		fz_drop_stream(ctx, file);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return tar;
}


/* Universal decomp stream */

typedef struct
{
	fz_stream *chain;
	fz_context *ctx; /* Safe as not persistent. */
	struct archive *archive;
	struct archive_entry *entry;
	uint8_t block[4096];
} fz_libarchived_state;

static la_ssize_t
libarchived_read(struct archive *a, void *client_data, const void **buf)
{
	fz_libarchived_state *state = (fz_libarchived_state *)client_data;
	size_t z;
	uint8_t *p;
	size_t left;
	fz_context *ctx = state->ctx;
	la_ssize_t ret = 0;

	fz_try(ctx)
	{
		z = fz_available(ctx, state->chain, 1024);

		/* If we're at the EOF, can't read anything! */
		if (z == 0)
			break;

		/* If we have at least 1K, then just return the pointer to that
		 * directly. */
		if (z >= 1024)
		{
			*buf = state->chain->rp;
			state->chain->rp += z;
			ret = (la_ssize_t)z;
			break;
		}

		/* If not, let's pull a large enough lump out. */

		left = sizeof(state->block);
		p = state->block;
		do
		{
			memcpy(p, state->chain->rp, z);
			p += z;
			state->chain->rp += z;
			left -= z;
			if (left)
			{
				z = fz_available(ctx, state->chain, left);
				if (z > left)
					z = left;
				if (z == 0)
					break;
			}
		}
		while (left != 0);

		ret = p - state->block;
		*buf = state->block;
	}
	fz_catch(ctx)
	{
		/* Ignore error */
		archive_set_error(a, ARCHIVE_FATAL, "%s", fz_convert_error(ctx, NULL));
		return -1;
	}

	return ret;
}

static la_int64_t
libarchived_skip(struct archive *a, void *client_data, la_int64_t skip)
{
	fz_libarchived_state *state = (fz_libarchived_state *)client_data;
	int64_t pos;
	fz_context *ctx = state->ctx;

	fz_try(ctx)
	{
		pos = fz_tell(state->ctx, state->chain);
		fz_seek(state->ctx, state->chain, pos + skip, SEEK_SET);
		pos = fz_tell(state->ctx, state->chain) - pos;
	}
	fz_catch(ctx)
	{
		/* Ignore error */
		archive_set_error(a, ARCHIVE_FATAL, "%s", fz_convert_error(ctx, NULL));
		return -1;
	}

	return pos;
}

static la_int64_t
libarchived_seek(struct archive *a, void *client_data, la_int64_t offset, int whence)
{
	fz_libarchived_state *state = (fz_libarchived_state *)client_data;
	fz_context *ctx = state->ctx;
	int64_t pos;

	fz_try(ctx)
	{
		fz_seek(ctx, state->chain, offset, whence);
		pos = fz_tell(ctx, state->chain);
	}
	fz_catch(ctx)
	{
		/* Ignore error */
		archive_set_error(a, ARCHIVE_FATAL, "%s", fz_convert_error(ctx, NULL));
		return -1;
	}

	return pos;
}

static int
libarchived_close(struct archive *a, void *client_data)
{
	/* Nothing to do. Stream is dropped when the fz_stream is dropped. */
	return ARCHIVE_OK;
}

static int
next_libarchived(fz_context *ctx, fz_stream *stm, size_t required)
{
	fz_libarchived_state *state = stm->state;
	la_ssize_t z;

	if (stm->eof)
		return EOF;

	z = archive_read_data(state->archive, state->block, sizeof(state->block));
	if (z < 0)
		fz_throw(ctx, FZ_ERROR_LIBRARY, "Failed to read compressed data");
	if (z == 0)
	{
		stm->eof = 1;
		return EOF;
	}

	stm->rp = state->block;
	stm->wp = state->block + z;

	return *stm->rp++;
}

static void
close_libarchived(fz_context *ctx, void *state_)
{
	fz_libarchived_state *state = (fz_libarchived_state *)state_;
	int code;

	state->ctx = ctx;
	code = archive_read_free(state->archive);
	state->ctx = NULL;
	if (code != ARCHIVE_OK)
		fz_warn(ctx, "libarchive error: archive_read_free: %d", code);

	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state);
}

fz_stream *
fz_open_libarchived(fz_context *ctx, fz_stream *chain)
{
	fz_libarchived_state *state;
	int r;

	state = fz_malloc_struct(ctx, fz_libarchived_state);

	state->chain = fz_keep_stream(ctx, chain);
	state->archive = archive_read_new();
	archive_read_support_filter_all(state->archive);
	archive_read_support_format_raw(state->archive);

	state->ctx = ctx;
	r = archive_read_set_seek_callback(state->archive, libarchived_seek);
	if (r == ARCHIVE_OK)
		r = archive_read_open2(state->archive, state, NULL, libarchived_read, libarchived_skip, libarchived_close);
	if (r != ARCHIVE_OK)
	{
		archive_read_free(state->archive);
		state->ctx = NULL;
		fz_drop_stream(ctx, state->chain);
		fz_free(ctx, state);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "Failed to open archive");
	}

	r = archive_filter_code(state->archive, 0);
	if (r == ARCHIVE_FILTER_NONE)
	{
		archive_read_free(state->archive);
		state->ctx = NULL;
		fz_drop_stream(ctx, state->chain);
		fz_free(ctx, state);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "Failed to open archive");
	}

	/* This is the one we want. */
	r = archive_read_next_header(state->archive, &state->entry);
	if (r != ARCHIVE_OK)
	{
		archive_read_free(state->archive);
		state->ctx = NULL;
		fz_drop_stream(ctx, state->chain);
		fz_free(ctx, state);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "Failed to open archive");
	}

	return fz_new_stream(ctx, state, next_libarchived, close_libarchived);
}

#else

int
fz_is_libarchive_archive(fz_context *ctx, fz_stream *file)
{
	static int warned = 0;

	if (!warned)
	{
		warned = 1;
		fz_warn(ctx, "libarchive support not included");
	}

	return 0;
}

fz_archive *
fz_open_libarchive_archive_with_stream(fz_context *ctx, fz_stream *file)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "libarchive support not included");

	return NULL;
}

fz_archive *
fz_open_libarchive_archive(fz_context *ctx, const char *filename)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "libarchive support not included");

	return NULL;
}

fz_stream *
fz_open_libarchived(fz_context *ctx, fz_stream *chain)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "libarchive support not included");

	return NULL;
}

#endif
