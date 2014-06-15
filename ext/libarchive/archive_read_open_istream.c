/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef _WIN32
#error archive_read_open_istream is only supported for Win32
#endif
#define COBJMACROS

#include "archive_platform.h"

#include "archive.h"
#include "archive_private.h"

struct read_istream_data {
	IStream	*stream;
	ULONG	 block_size;
	char	 buffer[1];
};

static ssize_t
istream_read(struct archive *a, void *client_data, const void **buff)
{
	struct read_istream_data *mine = client_data;
	ULONG cbRead = mine->block_size;
	HRESULT res;

	res = IStream_Read(mine->stream, mine->buffer, mine->block_size, &cbRead);
	if (FAILED(res)) {
		archive_set_error(a, EIO, "IStream read error: %x", res);
		return ARCHIVE_FATAL;
	}

	*buff = mine->buffer;
	return cbRead;
}

static int64_t
istream_seek(struct archive *a, void *client_data, int64_t request, int whence)
{
	struct read_istream_data *mine = client_data;
	LARGE_INTEGER off;
	ULARGE_INTEGER n;
	HRESULT res;

	off.QuadPart = request;
	res = IStream_Seek(mine->stream, off, whence, &n);
	if (FAILED(res)) {
		archive_set_error(a, EIO, "IStream seek error: %x", res);
		return ARCHIVE_FATAL;
	}

	return n.QuadPart;
}

static int64_t
istream_skip(struct archive *a, void *client_data, int64_t request)
{
	return istream_seek(a, client_data, request, SEEK_CUR);
}

static int
istream_close(struct archive *a, void *client_data)
{
	struct read_istream_data *mine = client_data;

	IUnknown_Release(mine->stream);
	free(mine);

	return ARCHIVE_OK;
}

int
archive_read_open_istream(struct archive *a, IStream *stream, ULONG block_size)
{
	struct read_istream_data *mine;
	LARGE_INTEGER zero = { 0 };
	HRESULT res;

	res = IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
	if (FAILED(res)) {
		archive_set_error(a, EINVAL, "IStream isn't seekable");
		return ARCHIVE_FATAL;
	}

	mine = calloc(1, sizeof(*mine) + block_size);
	if (!mine) {
		archive_set_error(a, ENOMEM, "No memory");
		return ARCHIVE_FATAL;
	}

	mine->block_size = block_size;
	mine->stream = stream;
	IUnknown_AddRef(stream);

	if (archive_read_append_callback_data(a, mine) != ARCHIVE_OK) {
		istream_close(a, mine);
		return ARCHIVE_FATAL;
	}
	archive_read_set_read_callback(a, istream_read);
	archive_read_set_seek_callback(a, istream_seek);
	archive_read_set_skip_callback(a, istream_skip);
	archive_read_set_close_callback(a, istream_close);

	return archive_read_open1(a);
}
