// Copyright (C) 2004-2024 Artifex Software, Inc.
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

#define _LARGEFILE_SOURCE
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include "mupdf/fitz.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <unistd.h>
#endif

int
fz_file_exists(fz_context *ctx, const char *path)
{
	FILE *file;
#ifdef _WIN32
	file = fz_fopen_utf8(path, "rb");
#else
	file = fopen(path, "rb");
#endif
	if (file)
		fclose(file);
	return !!file;
}

fz_stream *
fz_new_stream(fz_context *ctx, void *state, fz_stream_next_fn *next, fz_stream_drop_fn *drop)
{
	fz_stream *stm = NULL;

	fz_try(ctx)
	{
		stm = fz_malloc_struct(ctx, fz_stream);
	}
	fz_catch(ctx)
	{
		if (drop)
			drop(ctx, state);
		fz_rethrow(ctx);
	}

	stm->refs = 1;
	stm->error = 0;
	stm->eof = 0;
	stm->pos = 0;

	stm->bits = 0;
	stm->avail = 0;

	stm->rp = NULL;
	stm->wp = NULL;

	stm->state = state;
	stm->next = next;
	stm->drop = drop;
	stm->seek = NULL;

	return stm;
}

fz_stream *
fz_keep_stream(fz_context *ctx, fz_stream *stm)
{
	return fz_keep_imp(ctx, stm, &stm->refs);
}

void
fz_drop_stream(fz_context *ctx, fz_stream *stm)
{
	if (fz_drop_imp(ctx, stm, &stm->refs))
	{
		if (stm->drop)
			stm->drop(ctx, stm->state);
		fz_free(ctx, stm);
	}
}

/* File stream */

// TODO: WIN32: HANDLE CreateFileW(), etc.
// TODO: POSIX: int creat(), read(), write(), lseeko, etc.

typedef struct
{
	FILE *file;
	char *filename;
#ifdef _WIN32
	wchar_t *filename_w;
#endif
	int del_on_drop;
	unsigned char buffer[4096];
} fz_file_stream;

static int next_file(fz_context *ctx, fz_stream *stm, size_t n)
{
	fz_file_stream *state = stm->state;

	/* n is only a hint, that we can safely ignore */
	n = fread(state->buffer, 1, sizeof(state->buffer), state->file);
	if (n < sizeof(state->buffer) && ferror(state->file))
		fz_throw(ctx, FZ_ERROR_SYSTEM, "read error: %s", strerror(errno));
	stm->rp = state->buffer;
	stm->wp = state->buffer + n;
	stm->pos += (int64_t)n;

	if (n == 0)
		return EOF;
	return *stm->rp++;
}

static void seek_file(fz_context *ctx, fz_stream *stm, int64_t offset, int whence)
{
	fz_file_stream *state = stm->state;
#ifdef _WIN32
	int64_t n = _fseeki64(state->file, offset, whence);
#else
	int64_t n = fseeko(state->file, offset, whence);
#endif
	if (n < 0)
		fz_throw(ctx, FZ_ERROR_SYSTEM, "cannot seek: %s", strerror(errno));
#ifdef _WIN32
	stm->pos = _ftelli64(state->file);
#else
	stm->pos = ftello(state->file);
#endif
	stm->rp = state->buffer;
	stm->wp = state->buffer;
}

static void drop_file(fz_context *ctx, void *state_)
{
	fz_file_stream *state = state_;
	if (state->filename && state->del_on_drop)
	{
#ifdef _WIN32
		if (state->filename_w)
			_wunlink(state->filename_w);
		else
#endif
		unlink(state->filename);
	}
#ifdef _WIN32
	fz_free(ctx, state->filename_w);
#endif
	fz_free(ctx, state->filename);
	fz_free(ctx, state);
}

static void close_and_drop_file(fz_context *ctx, void *state_)
{
	fz_file_stream *state = state_;
	if (state)
	{
		int n = fclose(state->file);
		if (n < 0)
			fz_warn(ctx, "close error: %s", strerror(errno));
		drop_file(ctx, state_);
	}
}

static fz_stream *
fz_open_file_ptr(fz_context *ctx, FILE *file, const char *name, int wide, int del_on_drop)
{
	fz_stream *stm;
	fz_file_stream *state = NULL;

	fz_var(state);

#ifndef _WIN32
	assert(!wide);
#endif
	fz_try(ctx)
	{
		state = fz_malloc_struct(ctx, fz_file_stream);
		state->file = file;
#ifdef _WIN32
		if (wide)
		{
			size_t z = wcslen((const wchar_t *)name)+1;
			state->filename_w = fz_malloc(ctx, z*2);
			memcpy(state->filename_w, name, z*2);
			state->filename = fz_utf8_from_wchar(ctx, (const wchar_t *)name);
		}
		else
#endif
		state->filename = fz_strdup(ctx, name);
		state->del_on_drop = del_on_drop;

		stm = fz_new_stream(ctx, state, next_file, close_and_drop_file);
		stm->seek = seek_file;
	}
	fz_catch(ctx)
	{
		if (state == NULL && del_on_drop)
		{
			fclose(file);
#ifdef _WIN32
			if (wide)
				_wunlink((const wchar_t *)name);
			else
#endif
				unlink(name);
		}
		else
			close_and_drop_file(ctx, state);
		fz_rethrow(ctx);
	}

	return stm;
}

fz_stream *fz_open_file_ptr_no_close(fz_context *ctx, FILE *file)
{
	fz_stream *stm;
	fz_file_stream *state = fz_malloc_struct(ctx, fz_file_stream);
	state->file = file;

	/* We don't own the file ptr. Ensure we don't close it */
	stm = fz_new_stream(ctx, state, next_file, drop_file);
	stm->seek = seek_file;

	return stm;
}

fz_stream *
fz_open_file(fz_context *ctx, const char *name)
{
	FILE *file;
#ifdef _WIN32
	file = fz_fopen_utf8(name, "rb");
#else
	file = fopen(name, "rb");
#endif
	if (file == NULL)
		fz_throw(ctx, FZ_ERROR_SYSTEM, "cannot open %s: %s", name, strerror(errno));
	return fz_open_file_ptr(ctx, file, name, 0, 0);
}

fz_stream *
fz_open_file_autodelete(fz_context *ctx, const char *name)
{
	FILE *file;
#ifdef _WIN32
	file = fz_fopen_utf8(name, "rb");
#else
	file = fopen(name, "rb");
#endif
	if (file == NULL)
		fz_throw(ctx, FZ_ERROR_SYSTEM, "cannot open %s: %s", name, strerror(errno));
	return fz_open_file_ptr(ctx, file, name, 0, 1);
}

fz_stream *
fz_try_open_file(fz_context *ctx, const char *name)
{
	FILE *file;
#ifdef _WIN32
	file = fz_fopen_utf8(name, "rb");
#else
	file = fopen(name, "rb");
#endif
	if (file == NULL)
		return NULL;
	return fz_open_file_ptr(ctx, file, name, 0, 0);
}

#ifdef _WIN32
fz_stream *
fz_open_file_w(fz_context *ctx, const wchar_t *name)
{
	FILE *file = _wfopen(name, L"rb");
	if (file == NULL)
		fz_throw(ctx, FZ_ERROR_SYSTEM, "cannot open file %ls: %s", name, strerror(errno));

	return fz_open_file_ptr(ctx, file, (const char *)name, 1, 0);
}
#endif

const char *
fz_stream_filename(fz_context *ctx, fz_stream *stm)
{
	if (!stm || stm->next != next_file)
		return NULL;

	return ((fz_file_stream *)stm->state)->filename;
}

/* Memory stream */

static int next_buffer(fz_context *ctx, fz_stream *stm, size_t max)
{
	return EOF;
}

static void seek_buffer(fz_context *ctx, fz_stream *stm, int64_t offset, int whence)
{
	int64_t pos = stm->pos - (stm->wp - stm->rp);
	/* Convert to absolute pos */
	if (whence == 1)
	{
		offset += pos; /* Was relative to current pos */
	}
	else if (whence == 2)
	{
		offset += stm->pos; /* Was relative to end */
	}

	if (offset < 0)
		offset = 0;
	if (offset > stm->pos)
		offset = stm->pos;
	stm->rp += (int)(offset - pos);
}

static void drop_buffer(fz_context *ctx, void *state_)
{
	fz_buffer *state = (fz_buffer *)state_;
	fz_drop_buffer(ctx, state);
}

fz_stream *
fz_open_buffer(fz_context *ctx, fz_buffer *buf)
{
	fz_stream *stm;

	if (buf == NULL)
		return NULL;

	fz_keep_buffer(ctx, buf);
	stm = fz_new_stream(ctx, buf, next_buffer, drop_buffer);
	stm->seek = seek_buffer;

	stm->rp = buf->data;
	stm->wp = buf->data + buf->len;

	stm->pos = (int64_t)buf->len;

	return stm;
}

fz_stream *
fz_open_memory(fz_context *ctx, const unsigned char *data, size_t len)
{
	fz_stream *stm;

	stm = fz_new_stream(ctx, NULL, next_buffer, NULL);
	stm->seek = seek_buffer;

	stm->rp = (unsigned char *)data;
	stm->wp = (unsigned char *)data + len;

	stm->pos = (int64_t)len;

	return stm;
}
