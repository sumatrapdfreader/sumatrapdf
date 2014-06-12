/*-
 * Copyright (c) 2008 Anselm Strauss
 * Copyright (c) 2009 Joerg Sonnenberger
 * Copyright (c) 2011-2012 Michihiro NAKAJIMA
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

/*
 * Development supported by Google Summer of Code 2008.
 */

/*
 * The current implementation is very limited:
 *
 *   - No encryption support.
 *   - No ZIP64 support.
 *   - No support for splitting and spanning.
 *   - Only supports regular file and folder entries.
 *
 * Note that generally data in ZIP files is little-endian encoded,
 * with some exceptions.
 *
 * TODO: Since Libarchive is generally 64bit oriented, but this implementation
 * does not yet support sizes exceeding 32bit, it is highly fragile for
 * big archives. This should change when ZIP64 is finally implemented, otherwise
 * some serious checking has to be done.
 *
 */

#include "archive_platform.h"
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_set_format_zip.c 201168 2009-12-29 06:15:32Z kientzle $");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "archive.h"
#include "archive_endian.h"
#include "archive_entry.h"
#include "archive_entry_locale.h"
#include "archive_private.h"
#include "archive_write_private.h"

#ifndef HAVE_ZLIB_H
#include "archive_crc32.h"
#endif

#define ZIP_SIGNATURE_LOCAL_FILE_HEADER 0x04034b50
#define ZIP_SIGNATURE_DATA_DESCRIPTOR 0x08074b50
#define ZIP_SIGNATURE_FILE_HEADER 0x02014b50
#define ZIP_SIGNATURE_CENTRAL_DIRECTORY_END 0x06054b50
#define ZIP_SIGNATURE_EXTRA_TIMESTAMP 0x5455
#define ZIP_SIGNATURE_EXTRA_NEW_UNIX 0x7875
#define ZIP_VERSION_EXTRACT 0x0014 /* ZIP version 2.0 is needed. */
#define ZIP_VERSION_BY 0x0314 /* Made by UNIX, using ZIP version 2.0. */
#define ZIP_FLAGS 0x08 /* Flagging bit 3 (count from 0) for using data descriptor. */
#define ZIP_FLAGS_UTF8_NAME	(1 << 11)

enum compression {
	COMPRESSION_STORE = 0
#ifdef HAVE_ZLIB_H
	,
	COMPRESSION_DEFLATE = 8
#endif
};

static ssize_t archive_write_zip_data(struct archive_write *,
		   const void *buff, size_t s);
static int archive_write_zip_close(struct archive_write *);
static int archive_write_zip_free(struct archive_write *);
static int archive_write_zip_finish_entry(struct archive_write *);
static int archive_write_zip_header(struct archive_write *,
	      struct archive_entry *);
static int archive_write_zip_options(struct archive_write *,
	      const char *, const char *);
static unsigned int dos_time(const time_t);
static size_t path_length(struct archive_entry *);
static int write_path(struct archive_entry *, struct archive_write *);

#define LOCAL_FILE_HEADER_SIGNATURE		0
#define LOCAL_FILE_HEADER_VERSION		4
#define LOCAL_FILE_HEADER_FLAGS			6
#define LOCAL_FILE_HEADER_COMPRESSION		8
#define LOCAL_FILE_HEADER_TIMEDATE		10
#define LOCAL_FILE_HEADER_CRC32			14
#define LOCAL_FILE_HEADER_COMPRESSED_SIZE	18
#define LOCAL_FILE_HEADER_UNCOMPRESSED_SIZE	22
#define LOCAL_FILE_HEADER_FILENAME_LENGTH	26
#define LOCAL_FILE_HEADER_EXTRA_LENGTH		28
#define SIZE_LOCAL_FILE_HEADER			30

#define FILE_HEADER_SIGNATURE			0
#define FILE_HEADER_VERSION_BY			4
#define FILE_HEADER_VERSION_EXTRACT		6
#define FILE_HEADER_FLAGS			8
#define FILE_HEADER_COMPRESSION			10
#define FILE_HEADER_TIMEDATE			12
#define FILE_HEADER_CRC32			16
#define FILE_HEADER_COMPRESSED_SIZE		20
#define FILE_HEADER_UNCOMPRESSED_SIZE		24
#define FILE_HEADER_FILENAME_LENGTH		28
#define FILE_HEADER_EXTRA_LENGTH		30
#define FILE_HEADER_COMMENT_LENGTH		32
#define FILE_HEADER_DISK_NUMBER			34
#define FILE_HEADER_ATTRIBUTES_INTERNAL		36
#define FILE_HEADER_ATTRIBUTES_EXTERNAL		38
#define FILE_HEADER_OFFSET			42
#define SIZE_FILE_HEADER			46

	/* Not mandatory, but recommended by specification. */
#define DATA_DESCRIPTOR_SIGNATURE		0
#define DATA_DESCRIPTOR_CRC32			4
#define DATA_DESCRIPTOR_COMPRESSED_SIZE		8
#define DATA_DESCRIPTOR_UNCOMPRESSED_SIZE	12
#define SIZE_DATA_DESCRIPTOR			16

#define EXTRA_DATA_LOCAL_TIME_ID		0
#define EXTRA_DATA_LOCAL_TIME_SIZE		2
#define EXTRA_DATA_LOCAL_TIME_FLAG		4
#define EXTRA_DATA_LOCAL_MTIME			5
#define EXTRA_DATA_LOCAL_ATIME			9
#define EXTRA_DATA_LOCAL_CTIME			13
#define EXTRA_DATA_LOCAL_UNIX_ID		17
#define EXTRA_DATA_LOCAL_UNIX_SIZE		19
#define EXTRA_DATA_LOCAL_UNIX_VERSION		21
#define EXTRA_DATA_LOCAL_UNIX_UID_SIZE		22
#define EXTRA_DATA_LOCAL_UNIX_UID		23
#define EXTRA_DATA_LOCAL_UNIX_GID_SIZE		27
#define EXTRA_DATA_LOCAL_UNIX_GID		28
#define SIZE_EXTRA_DATA_LOCAL			32

#define EXTRA_DATA_CENTRAL_TIME_ID		0
#define EXTRA_DATA_CENTRAL_TIME_SIZE		2
#define EXTRA_DATA_CENTRAL_TIME_FLAG		4
#define EXTRA_DATA_CENTRAL_MTIME		5
#define EXTRA_DATA_CENTRAL_UNIX_ID		9
#define EXTRA_DATA_CENTRAL_UNIX_SIZE		11
#define SIZE_EXTRA_DATA_CENTRAL			13

#define CENTRAL_DIRECTORY_END_SIGNATURE		0
#define CENTRAL_DIRECTORY_END_DISK		4
#define CENTRAL_DIRECTORY_END_START_DISK	6
#define CENTRAL_DIRECTORY_END_ENTRIES_DISK	8
#define CENTRAL_DIRECTORY_END_ENTRIES		10
#define CENTRAL_DIRECTORY_END_SIZE		12
#define CENTRAL_DIRECTORY_END_OFFSET		16
#define CENTRAL_DIRECTORY_END_COMMENT_LENGTH	20
#define SIZE_CENTRAL_DIRECTORY_END		22

struct zip_file_header_link {
	struct zip_file_header_link *next;
	struct archive_entry *entry;
	int64_t offset;
	unsigned long crc32;
	int64_t compressed_size;
	enum compression compression;
	int flags;
};

struct zip {
	uint8_t data_descriptor[SIZE_DATA_DESCRIPTOR];
	struct zip_file_header_link *central_directory;
	struct zip_file_header_link *central_directory_end;
	int64_t offset;
	int64_t written_bytes;
	int64_t remaining_data_bytes;
	enum compression compression;
	int flags;
	struct archive_string_conv *opt_sconv;
	struct archive_string_conv *sconv_default;
	int	init_default_conversion;

#ifdef HAVE_ZLIB_H
	z_stream stream;
	size_t len_buf;
	unsigned char *buf;
#endif
};

static int
archive_write_zip_options(struct archive_write *a, const char *key,
    const char *val)
{
	struct zip *zip = a->format_data;
	int ret = ARCHIVE_FAILED;

	if (strcmp(key, "compression") == 0) {
		if (val == NULL || val[0] == 0) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "%s: compression option needs a compression name",
			    a->format_name);
		} else if (strcmp(val, "deflate") == 0) {
#ifdef HAVE_ZLIB_H
			zip->compression = COMPRESSION_DEFLATE;
			ret = ARCHIVE_OK;
#else
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "deflate compression not supported");
#endif
		} else if (strcmp(val, "store") == 0) {
			zip->compression = COMPRESSION_STORE;
			ret = ARCHIVE_OK;
		}
		return (ret);
	} else if (strcmp(key, "hdrcharset")  == 0) {
		if (val == NULL || val[0] == 0) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "%s: hdrcharset option needs a character-set name",
			    a->format_name);
		} else {
			zip->opt_sconv = archive_string_conversion_to_charset(
			    &a->archive, val, 0);
			if (zip->opt_sconv != NULL)
				ret = ARCHIVE_OK;
			else
				ret = ARCHIVE_FATAL;
		}
		return (ret);
	}

	/* Note: The "warn" return is just to inform the options
	 * supervisor that we didn't handle it.  It will generate
	 * a suitable error if no one used this option. */
	return (ARCHIVE_WARN);
}

int
archive_write_zip_set_compression_deflate(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int ret = ARCHIVE_FAILED;
	
	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
		ARCHIVE_STATE_NEW | ARCHIVE_STATE_HEADER,
		"archive_write_zip_set_compression_deflate");
	if (a->archive.archive_format != ARCHIVE_FORMAT_ZIP) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		"Can only use archive_write_zip_set_compression_deflate"
		" with zip format");
		ret = ARCHIVE_FATAL;
	} else {
#ifdef HAVE_ZLIB_H
		struct zip *zip = a->format_data;
		zip->compression = COMPRESSION_DEFLATE;
		ret = ARCHIVE_OK;
#else
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			"deflate compression not supported");
#endif
	}
	return (ret);
}

int
archive_write_zip_set_compression_store(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct zip *zip = a->format_data;
	int ret = ARCHIVE_FAILED;
	
	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
		ARCHIVE_STATE_NEW | ARCHIVE_STATE_HEADER,
		"archive_write_zip_set_compression_deflate");
	if (a->archive.archive_format != ARCHIVE_FORMAT_ZIP) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			"Can only use archive_write_zip_set_compression_store"
			" with zip format");
		ret = ARCHIVE_FATAL;
	} else {
		zip->compression = COMPRESSION_STORE;
		ret = ARCHIVE_OK;
	}
	return (ret);
}

int
archive_write_set_format_zip(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct zip *zip;

	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_format_zip");

	/* If another format was already registered, unregister it. */
	if (a->format_free != NULL)
		(a->format_free)(a);

	zip = (struct zip *) calloc(1, sizeof(*zip));
	if (zip == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate zip data");
		return (ARCHIVE_FATAL);
	}
	zip->central_directory = NULL;
	zip->central_directory_end = NULL;
	zip->offset = 0;
	zip->written_bytes = 0;
	zip->remaining_data_bytes = 0;

#ifdef HAVE_ZLIB_H
	zip->compression = COMPRESSION_DEFLATE;
	zip->len_buf = 65536;
	zip->buf = malloc(zip->len_buf);
	if (zip->buf == NULL) {
		free(zip);
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate compression buffer");
		return (ARCHIVE_FATAL);
	}
#else
	zip->compression = COMPRESSION_STORE;
#endif

	a->format_data = zip;
	a->format_name = "zip";
	a->format_options = archive_write_zip_options;
	a->format_write_header = archive_write_zip_header;
	a->format_write_data = archive_write_zip_data;
	a->format_finish_entry = archive_write_zip_finish_entry;
	a->format_close = archive_write_zip_close;
	a->format_free = archive_write_zip_free;
	a->archive.archive_format = ARCHIVE_FORMAT_ZIP;
	a->archive.archive_format_name = "ZIP";

	archive_le32enc(&zip->data_descriptor[DATA_DESCRIPTOR_SIGNATURE],
	    ZIP_SIGNATURE_DATA_DESCRIPTOR);

	return (ARCHIVE_OK);
}

static int
is_all_ascii(const char *p)
{
	const unsigned char *pp = (const unsigned char *)p;

	while (*pp) {
		if (*pp++ > 127)
			return (0);
	}
	return (1);
}

static int
archive_write_zip_header(struct archive_write *a, struct archive_entry *entry)
{
	struct zip *zip;
	uint8_t h[SIZE_LOCAL_FILE_HEADER];
	uint8_t e[SIZE_EXTRA_DATA_LOCAL];
	uint8_t *d;
	struct zip_file_header_link *l;
	struct archive_string_conv *sconv;
	int ret, ret2 = ARCHIVE_OK;
	int64_t size;
	mode_t type;

	/* Entries other than a regular file or a folder are skipped. */
	type = archive_entry_filetype(entry);
	if (type != AE_IFREG && type != AE_IFDIR && type != AE_IFLNK) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Filetype not supported");
		return ARCHIVE_FAILED;
	};

	/* Directory entries should have a size of 0. */
	if (type == AE_IFDIR)
		archive_entry_set_size(entry, 0);

	zip = a->format_data;
	/* Setup default conversion. */
	if (zip->opt_sconv == NULL && !zip->init_default_conversion) {
		zip->sconv_default =
		    archive_string_default_conversion_for_write(&(a->archive));
		zip->init_default_conversion = 1;
	}

	if (zip->flags == 0) {
		/* Initialize the general purpose flags. */
		zip->flags = ZIP_FLAGS;
		if (zip->opt_sconv != NULL) {
			if (strcmp(archive_string_conversion_charset_name(
			    zip->opt_sconv), "UTF-8") == 0)
				zip->flags |= ZIP_FLAGS_UTF8_NAME;
#if HAVE_NL_LANGINFO
		} else if (strcmp(nl_langinfo(CODESET), "UTF-8") == 0) {
			zip->flags |= ZIP_FLAGS_UTF8_NAME;
#endif
		}
	}
	d = zip->data_descriptor;
	size = archive_entry_size(entry);
	zip->remaining_data_bytes = size;

	/* Append archive entry to the central directory data. */
	l = (struct zip_file_header_link *) malloc(sizeof(*l));
	if (l == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate zip header data");
		return (ARCHIVE_FATAL);
	}
#if defined(_WIN32) && !defined(__CYGWIN__)
	/* Make sure the path separators in pahtname, hardlink and symlink
	 * are all slash '/', not the Windows path separator '\'. */
	l->entry = __la_win_entry_in_posix_pathseparator(entry);
	if (l->entry == entry)
		l->entry = archive_entry_clone(entry);
#else
	l->entry = archive_entry_clone(entry);
#endif
	if (l->entry == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate zip header data");
		free(l);
		return (ARCHIVE_FATAL);
	}
	l->flags = zip->flags;
	if (zip->opt_sconv != NULL)
		sconv = zip->opt_sconv;
	else
		sconv = zip->sconv_default;
	if (sconv != NULL) {
		const char *p;
		size_t len;

		if (archive_entry_pathname_l(entry, &p, &len, sconv) != 0) {
			if (errno == ENOMEM) {
				archive_entry_free(l->entry);
				free(l);
				archive_set_error(&a->archive, ENOMEM,
				    "Can't allocate memory for Pathname");
				return (ARCHIVE_FATAL);
			}
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_FILE_FORMAT,
			    "Can't translate Pathname '%s' to %s",
			    archive_entry_pathname(entry),
			    archive_string_conversion_charset_name(sconv));
			ret2 = ARCHIVE_WARN;
		}
		if (len > 0)
			archive_entry_set_pathname(l->entry, p);

		/*
		 * Although there is no character-set regulation for Symlink,
		 * it is suitable to convert a character-set of Symlinke to
		 * what those of the Pathname has been converted to.
		 */
		if (type == AE_IFLNK) {
			if (archive_entry_symlink_l(entry, &p, &len, sconv)) {
				if (errno == ENOMEM) {
					archive_entry_free(l->entry);
					free(l);
					archive_set_error(&a->archive, ENOMEM,
					    "Can't allocate memory "
					    " for Symlink");
					return (ARCHIVE_FATAL);
				}
				/*
				 * Even if the strng conversion failed,
				 * we should not report the error since
				 * thre is no regulation for.
				 */
			} else if (len > 0)
				archive_entry_set_symlink(l->entry, p);
		}
	}
	/* If all characters in a filename are ASCII, Reset UTF-8 Name flag. */
	if ((l->flags & ZIP_FLAGS_UTF8_NAME) != 0 &&
	    is_all_ascii(archive_entry_pathname(l->entry)))
		l->flags &= ~ZIP_FLAGS_UTF8_NAME;

	/* Initialize the CRC variable and potentially the local crc32(). */
	l->crc32 = crc32(0, NULL, 0);
	if (type == AE_IFLNK) {
		const char *p = archive_entry_symlink(l->entry);
		if (p != NULL)
			size = strlen(p);
		else
			size = 0;
		zip->remaining_data_bytes = 0;
		archive_entry_set_size(l->entry, size);
		l->compression = COMPRESSION_STORE;
		l->compressed_size = size;
	} else {
		l->compression = zip->compression;
		l->compressed_size = 0;
	}
	l->next = NULL;
	if (zip->central_directory == NULL) {
		zip->central_directory = l;
	} else {
		zip->central_directory_end->next = l;
	}
	zip->central_directory_end = l;

	/* Store the offset of this header for later use in central
	 * directory. */
	l->offset = zip->written_bytes;

	memset(h, 0, sizeof(h));
	archive_le32enc(&h[LOCAL_FILE_HEADER_SIGNATURE],
		ZIP_SIGNATURE_LOCAL_FILE_HEADER);
	archive_le16enc(&h[LOCAL_FILE_HEADER_VERSION], ZIP_VERSION_EXTRACT);
	archive_le16enc(&h[LOCAL_FILE_HEADER_FLAGS], l->flags);
	archive_le16enc(&h[LOCAL_FILE_HEADER_COMPRESSION], l->compression);
	archive_le32enc(&h[LOCAL_FILE_HEADER_TIMEDATE],
		dos_time(archive_entry_mtime(entry)));
	archive_le16enc(&h[LOCAL_FILE_HEADER_FILENAME_LENGTH],
		(uint16_t)path_length(l->entry));

	switch (l->compression) {
	case COMPRESSION_STORE:
		/* Setting compressed and uncompressed sizes even when
		 * specification says to set to zero when using data
		 * descriptors. Otherwise the end of the data for an
		 * entry is rather difficult to find. */
		archive_le32enc(&h[LOCAL_FILE_HEADER_COMPRESSED_SIZE],
		    (uint32_t)size);
		archive_le32enc(&h[LOCAL_FILE_HEADER_UNCOMPRESSED_SIZE],
		    (uint32_t)size);
		break;
#ifdef HAVE_ZLIB_H
	case COMPRESSION_DEFLATE:
		archive_le32enc(&h[LOCAL_FILE_HEADER_UNCOMPRESSED_SIZE],
		    (uint32_t)size);

		zip->stream.zalloc = Z_NULL;
		zip->stream.zfree = Z_NULL;
		zip->stream.opaque = Z_NULL;
		zip->stream.next_out = zip->buf;
		zip->stream.avail_out = (uInt)zip->len_buf;
		if (deflateInit2(&zip->stream, Z_DEFAULT_COMPRESSION,
		    Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't init deflate compressor");
			return (ARCHIVE_FATAL);
		}
		break;
#endif
	}

	/* Formatting extra data. */
	archive_le16enc(&h[LOCAL_FILE_HEADER_EXTRA_LENGTH], sizeof(e));
	archive_le16enc(&e[EXTRA_DATA_LOCAL_TIME_ID],
		ZIP_SIGNATURE_EXTRA_TIMESTAMP);
	archive_le16enc(&e[EXTRA_DATA_LOCAL_TIME_SIZE], 1 + 4 * 3);
	e[EXTRA_DATA_LOCAL_TIME_FLAG] = 0x07;
	archive_le32enc(&e[EXTRA_DATA_LOCAL_MTIME],
	    (uint32_t)archive_entry_mtime(entry));
	archive_le32enc(&e[EXTRA_DATA_LOCAL_ATIME],
	    (uint32_t)archive_entry_atime(entry));
	archive_le32enc(&e[EXTRA_DATA_LOCAL_CTIME],
	    (uint32_t)archive_entry_ctime(entry));

	archive_le16enc(&e[EXTRA_DATA_LOCAL_UNIX_ID],
		ZIP_SIGNATURE_EXTRA_NEW_UNIX);
	archive_le16enc(&e[EXTRA_DATA_LOCAL_UNIX_SIZE], 1 + (1 + 4) * 2);
	e[EXTRA_DATA_LOCAL_UNIX_VERSION] = 1;
	e[EXTRA_DATA_LOCAL_UNIX_UID_SIZE] = 4;
	archive_le32enc(&e[EXTRA_DATA_LOCAL_UNIX_UID],
		(uint32_t)archive_entry_uid(entry));
	e[EXTRA_DATA_LOCAL_UNIX_GID_SIZE] = 4;
	archive_le32enc(&e[EXTRA_DATA_LOCAL_UNIX_GID],
		(uint32_t)archive_entry_gid(entry));

	archive_le32enc(&d[DATA_DESCRIPTOR_UNCOMPRESSED_SIZE],
	    (uint32_t)size);

	ret = __archive_write_output(a, h, sizeof(h));
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	zip->written_bytes += sizeof(h);

	ret = write_path(l->entry, a);
	if (ret <= ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	zip->written_bytes += ret;

	ret = __archive_write_output(a, e, sizeof(e));
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	zip->written_bytes += sizeof(e);

	if (type == AE_IFLNK) {
		const unsigned char *p;

		p = (const unsigned char *)archive_entry_symlink(l->entry);
		ret = __archive_write_output(a, p, (size_t)size);
		if (ret != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		zip->written_bytes += size;
		l->crc32 = crc32(l->crc32, p, (unsigned)size);
	}

	if (ret2 != ARCHIVE_OK)
		return (ret2);
	return (ARCHIVE_OK);
}

static ssize_t
archive_write_zip_data(struct archive_write *a, const void *buff, size_t s)
{
	int ret;
	struct zip *zip = a->format_data;
	struct zip_file_header_link *l = zip->central_directory_end;

	if ((int64_t)s > zip->remaining_data_bytes)
		s = (size_t)zip->remaining_data_bytes;

	if (s == 0) return 0;

	switch (l->compression) {
	case COMPRESSION_STORE:
		ret = __archive_write_output(a, buff, s);
		if (ret != ARCHIVE_OK) return (ret);
		zip->written_bytes += s;
		zip->remaining_data_bytes -= s;
		l->compressed_size += s;
		l->crc32 = crc32(l->crc32, buff, (unsigned)s);
		return (s);
#if HAVE_ZLIB_H
	case COMPRESSION_DEFLATE:
		zip->stream.next_in = (unsigned char*)(uintptr_t)buff;
		zip->stream.avail_in = (uInt)s;
		do {
			ret = deflate(&zip->stream, Z_NO_FLUSH);
			if (ret == Z_STREAM_ERROR)
				return (ARCHIVE_FATAL);
			if (zip->stream.avail_out == 0) {
				ret = __archive_write_output(a, zip->buf,
					zip->len_buf);
				if (ret != ARCHIVE_OK)
					return (ret);
				l->compressed_size += zip->len_buf;
				zip->written_bytes += zip->len_buf;
				zip->stream.next_out = zip->buf;
				zip->stream.avail_out = (uInt)zip->len_buf;
			}
		} while (zip->stream.avail_in != 0);
		zip->remaining_data_bytes -= s;
		/* If we have it, use zlib's fast crc32() */
		l->crc32 = crc32(l->crc32, buff, (uInt)s);
		return (s);
#endif

	default:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Invalid ZIP compression type");
		return ARCHIVE_FATAL;
	}
}

static int
archive_write_zip_finish_entry(struct archive_write *a)
{
	/* Write the data descripter after file data has been written. */
	int ret;
	struct zip *zip = a->format_data;
	uint8_t *d = zip->data_descriptor;
	struct zip_file_header_link *l = zip->central_directory_end;
#if HAVE_ZLIB_H
	size_t reminder;
#endif

	switch(l->compression) {
	case COMPRESSION_STORE:
		break;
#if HAVE_ZLIB_H
	case COMPRESSION_DEFLATE:
		for (;;) {
			ret = deflate(&zip->stream, Z_FINISH);
			if (ret == Z_STREAM_ERROR)
				return (ARCHIVE_FATAL);
			reminder = zip->len_buf - zip->stream.avail_out;
			ret = __archive_write_output(a, zip->buf, reminder);
			if (ret != ARCHIVE_OK)
				return (ret);
			l->compressed_size += reminder;
			zip->written_bytes += reminder;
			zip->stream.next_out = zip->buf;
			if (zip->stream.avail_out != 0)
				break;
			zip->stream.avail_out = (uInt)zip->len_buf;
		}
		deflateEnd(&zip->stream);
		break;
#endif
	}

	archive_le32enc(&d[DATA_DESCRIPTOR_CRC32], l->crc32);
	archive_le32enc(&d[DATA_DESCRIPTOR_COMPRESSED_SIZE],
		(uint32_t)l->compressed_size);
	ret = __archive_write_output(a, d, SIZE_DATA_DESCRIPTOR);
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	zip->written_bytes += SIZE_DATA_DESCRIPTOR;
	return (ARCHIVE_OK);
}

static int
archive_write_zip_close(struct archive_write *a)
{
	struct zip *zip;
	struct zip_file_header_link *l;
	uint8_t h[SIZE_FILE_HEADER];
	uint8_t end[SIZE_CENTRAL_DIRECTORY_END];
	uint8_t e[SIZE_EXTRA_DATA_CENTRAL];
	int64_t offset_start, offset_end;
	int entries;
	int ret;

	zip = a->format_data;
	l = zip->central_directory;

	/*
	 * Formatting central directory file header fields that are
	 * fixed for all entries.
	 * Fields not used (and therefor 0) are:
	 *
	 *   - comment_length
	 *   - disk_number
	 *   - attributes_internal
	 */
	memset(h, 0, sizeof(h));
	archive_le32enc(&h[FILE_HEADER_SIGNATURE], ZIP_SIGNATURE_FILE_HEADER);
	archive_le16enc(&h[FILE_HEADER_VERSION_BY], ZIP_VERSION_BY);
	archive_le16enc(&h[FILE_HEADER_VERSION_EXTRACT], ZIP_VERSION_EXTRACT);

	entries = 0;
	offset_start = zip->written_bytes;

	/* Formatting individual header fields per entry and
	 * writing each entry. */
	while (l != NULL) {
		archive_le16enc(&h[FILE_HEADER_FLAGS], l->flags);
		archive_le16enc(&h[FILE_HEADER_COMPRESSION], l->compression);
		archive_le32enc(&h[FILE_HEADER_TIMEDATE],
			dos_time(archive_entry_mtime(l->entry)));
		archive_le32enc(&h[FILE_HEADER_CRC32], l->crc32);
		archive_le32enc(&h[FILE_HEADER_COMPRESSED_SIZE],
			(uint32_t)l->compressed_size);
		archive_le32enc(&h[FILE_HEADER_UNCOMPRESSED_SIZE],
			(uint32_t)archive_entry_size(l->entry));
		archive_le16enc(&h[FILE_HEADER_FILENAME_LENGTH],
			(uint16_t)path_length(l->entry));
		archive_le16enc(&h[FILE_HEADER_EXTRA_LENGTH], sizeof(e));
		archive_le16enc(&h[FILE_HEADER_ATTRIBUTES_EXTERNAL+2],
			archive_entry_mode(l->entry));
		archive_le32enc(&h[FILE_HEADER_OFFSET], (uint32_t)l->offset);

		/* Formatting extra data. */
		archive_le16enc(&e[EXTRA_DATA_CENTRAL_TIME_ID],
			ZIP_SIGNATURE_EXTRA_TIMESTAMP);
		archive_le16enc(&e[EXTRA_DATA_CENTRAL_TIME_SIZE], 1 + 4);
		e[EXTRA_DATA_CENTRAL_TIME_FLAG] = 0x07;
		archive_le32enc(&e[EXTRA_DATA_CENTRAL_MTIME],
			(uint32_t)archive_entry_mtime(l->entry));
		archive_le16enc(&e[EXTRA_DATA_CENTRAL_UNIX_ID],
			ZIP_SIGNATURE_EXTRA_NEW_UNIX);
		archive_le16enc(&e[EXTRA_DATA_CENTRAL_UNIX_SIZE], 0x0000);

		ret = __archive_write_output(a, h, sizeof(h));
		if (ret != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		zip->written_bytes += sizeof(h);

		ret = write_path(l->entry, a);
		if (ret <= ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		zip->written_bytes += ret;

		ret = __archive_write_output(a, e, sizeof(e));
		if (ret != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		zip->written_bytes += sizeof(e);

		l = l->next;
		entries++;
	}
	offset_end = zip->written_bytes;

	/* Formatting end of central directory. */
	memset(end, 0, sizeof(end));
	archive_le32enc(&end[CENTRAL_DIRECTORY_END_SIGNATURE],
		ZIP_SIGNATURE_CENTRAL_DIRECTORY_END);
	archive_le16enc(&end[CENTRAL_DIRECTORY_END_ENTRIES_DISK], entries);
	archive_le16enc(&end[CENTRAL_DIRECTORY_END_ENTRIES], entries);
	archive_le32enc(&end[CENTRAL_DIRECTORY_END_SIZE],
		(uint32_t)(offset_end - offset_start));
	archive_le32enc(&end[CENTRAL_DIRECTORY_END_OFFSET],
		(uint32_t)offset_start);

	/* Writing end of central directory. */
	ret = __archive_write_output(a, end, sizeof(end));
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	zip->written_bytes += sizeof(end);
	return (ARCHIVE_OK);
}

static int
archive_write_zip_free(struct archive_write *a)
{
	struct zip *zip;
	struct zip_file_header_link *l;

	zip = a->format_data;
	while (zip->central_directory != NULL) {
	   l = zip->central_directory;
	   zip->central_directory = l->next;
	   archive_entry_free(l->entry);
	   free(l);
	}
#ifdef HAVE_ZLIB_H
	free(zip->buf);
#endif
	free(zip);
	a->format_data = NULL;
	return (ARCHIVE_OK);
}

/* Convert into MSDOS-style date/time. */
static unsigned int
dos_time(const time_t unix_time)
{
	struct tm *t;
	unsigned int dt;

	/* This will not preserve time when creating/extracting the archive
	 * on two systems with different time zones. */
	t = localtime(&unix_time);

	/* MSDOS-style date/time is only between 1980-01-01 and 2107-12-31 */
	if (t->tm_year < 1980 - 1900)
		/* Set minimum date/time '1980-01-01 00:00:00'. */
		dt = 0x00210000U;
	else if (t->tm_year > 2107 - 1900)
		/* Set maximum date/time '2107-12-31 23:59:58'. */
		dt = 0xff9fbf7dU;
	else {
		dt = 0;
		dt += ((t->tm_year - 80) & 0x7f) << 9;
		dt += ((t->tm_mon + 1) & 0x0f) << 5;
		dt += (t->tm_mday & 0x1f);
		dt <<= 16;
		dt += (t->tm_hour & 0x1f) << 11;
		dt += (t->tm_min & 0x3f) << 5;
		dt += (t->tm_sec & 0x3e) >> 1; /* Only counting every 2 seconds. */
	}
	return dt;
}

static size_t
path_length(struct archive_entry *entry)
{
	mode_t type;
	const char *path;

	type = archive_entry_filetype(entry);
	path = archive_entry_pathname(entry);

	if (path == NULL)
		return (0);
	if (type == AE_IFDIR &&
	    (path[0] == '\0' || path[strlen(path) - 1] != '/')) {
		return strlen(path) + 1;
	} else {
		return strlen(path);
	}
}

static int
write_path(struct archive_entry *entry, struct archive_write *archive)
{
	int ret;
	const char *path;
	mode_t type;
	size_t written_bytes;

	path = archive_entry_pathname(entry);
	type = archive_entry_filetype(entry);
	written_bytes = 0;

	ret = __archive_write_output(archive, path, strlen(path));
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	written_bytes += strlen(path);

	/* Folders are recognized by a traling slash. */
	if ((type == AE_IFDIR) & (path[strlen(path) - 1] != '/')) {
		ret = __archive_write_output(archive, "/", 1);
		if (ret != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		written_bytes += 1;
	}

	return ((int)written_bytes);
}
