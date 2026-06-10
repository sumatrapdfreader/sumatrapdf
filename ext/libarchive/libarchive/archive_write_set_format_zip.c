/*-
 * Copyright (c) 2008 Anselm Strauss
 * Copyright (c) 2009 Joerg Sonnenberger
 * Copyright (c) 2011-2012,2014 Michihiro NAKAJIMA
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

#include "archive_platform.h"

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
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif
#ifdef HAVE_LZMA_H
#include <lzma.h>
#endif
#ifdef HAVE_BZLIB_H
#include <bzlib.h>
#endif
#ifdef HAVE_ZSTD_H
#include <zstd.h>
#endif

#include "archive.h"
#include "archive_cryptor_private.h"
#include "archive_endian.h"
#include "archive_entry.h"
#include "archive_entry_locale.h"
#include "archive_hmac_private.h"
#include "archive_private.h"
#include "archive_random_private.h"
#include "archive_time_private.h"
#include "archive_write_private.h"
#include "archive_write_set_format_private.h"

#ifndef HAVE_ZLIB_H
#include "archive_crc32.h"
#endif

#define ZIP_ENTRY_FLAG_ENCRYPTED	(1 << 0)
#define ZIP_ENTRY_FLAG_LZMA_EOPM	(1 << 1)
#define ZIP_ENTRY_FLAG_DEFLATE_MAX	(1 << 1) /* i.e. compression levels 8 & 9 */
#define ZIP_ENTRY_FLAG_DEFLATE_FAST	(1 << 2) /* i.e. compression levels 3 & 4 */
#define ZIP_ENTRY_FLAG_DEFLATE_SUPER_FAST	(1 << 1) | (1 << 2) /* i.e. compression levels 1 & 2 */
#define ZIP_ENTRY_FLAG_LENGTH_AT_END	(1 << 3)
#define ZIP_ENTRY_FLAG_UTF8_NAME	(1 << 11)

#define ZIP_4GB_MAX ARCHIVE_LITERAL_LL(0xffffffff)
#define ZIP_4GB_MAX_UNCOMPRESSED ARCHIVE_LITERAL_LL(0xff000000)

enum compression {
	COMPRESSION_UNSPECIFIED = -1,
	COMPRESSION_STORE = 0,
	COMPRESSION_DEFLATE = 8,
	COMPRESSION_BZIP2 = 12,
	COMPRESSION_LZMA = 14,
	COMPRESSION_ZSTD = 93,
	COMPRESSION_XZ = 95
};

#ifdef HAVE_ZLIB_H
#define COMPRESSION_DEFAULT	COMPRESSION_DEFLATE
#else
#define COMPRESSION_DEFAULT	COMPRESSION_STORE
#endif

enum encryption {
	ENCRYPTION_NONE	= 0,
	ENCRYPTION_TRADITIONAL, /* Traditional PKWARE encryption. */
	ENCRYPTION_WINZIP_AES128, /* WinZIP AES-128 encryption. */
	ENCRYPTION_WINZIP_AES256, /* WinZIP AES-256 encryption. */
};

#define TRAD_HEADER_SIZE	12
/*
 * See "WinZip - AES Encryption Information"
 *     http://www.winzip.com/aes_info.htm
 */
/* Value used in compression method. */
#define WINZIP_AES_ENCRYPTION	99
/* A WinZip AES header size which is stored at the beginning of
 * file contents. */
#define WINZIP_AES128_HEADER_SIZE	(8 + 2)
#define WINZIP_AES256_HEADER_SIZE	(16 + 2)
/* AES vendor version. */
#define AES_VENDOR_AE_1 0x0001
#define AES_VENDOR_AE_2 0x0002
/* Authentication code size. */
#define AUTH_CODE_SIZE		10
/**/
#define MAX_DERIVED_KEY_BUF_SIZE (AES_MAX_KEY_SIZE * 2 + 2)

struct cd_segment {
	struct cd_segment *next;
	size_t buff_size;
	unsigned char *buff;
	unsigned char *p;
};

struct trad_enc_ctx {
	uint32_t keys[3];
};

struct zip {
	int64_t entry_offset;
	int64_t entry_compressed_size;
	int64_t entry_uncompressed_size;
	int64_t entry_compressed_written;
	int64_t entry_uncompressed_written;
	int64_t entry_uncompressed_limit;
	struct archive_entry *entry;
	uint32_t entry_crc32;
	enum compression entry_compression;
	enum encryption  entry_encryption;
	int entry_flags;
	int experiments;
	struct trad_enc_ctx tctx;
	char tctx_valid;
	unsigned char trad_chkdat;
	unsigned aes_vendor;
	archive_crypto_ctx cctx;
	char cctx_valid;
	archive_hmac_sha1_ctx hctx;
	char hctx_valid;

	unsigned char *file_header;
	size_t file_header_extra_offset;
	unsigned long (*crc32func)(unsigned long crc, const void *buff, size_t len);

	struct cd_segment *central_directory;
	struct cd_segment *central_directory_last;
	size_t central_directory_bytes;
	size_t central_directory_entries;

	int64_t written_bytes; /* Overall position in file. */

	struct archive_string_conv *opt_sconv;
	struct archive_string_conv *sconv_default;
	enum compression requested_compression;
	short compression_level;
	int init_default_conversion;
	enum encryption encryption_type;
	short threads;

#define ZIP_FLAG_AVOID_ZIP64 1
#define ZIP_FLAG_FORCE_ZIP64 2
#define ZIP_FLAG_EXPERIMENT_xl 4
	int flags;
#if defined(HAVE_LZMA_H) || defined(HAVE_ZLIB_H) || defined(HAVE_BZLIB_H) || defined(HAVE_ZSTD_H)
	union {
#ifdef HAVE_LZMA_H
		/* ZIP's XZ format (id 95) is easy enough: copy Deflate, mutatis
		 * mutandis the library changes. ZIP's LZMA format (id 14),
		 * however, is rather more involved, starting here: it being a
		 * modified LZMA Alone format requires a bit more
		 * book-keeping. */
		struct {
			char headers_to_write;
			lzma_options_lzma options;
			lzma_stream context;
		} lzma;
#endif
#ifdef HAVE_ZLIB_H
		z_stream deflate;
#endif
#ifdef HAVE_BZLIB_H
		bz_stream bzip2;
#endif
#if defined(HAVE_ZSTD_H) && HAVE_ZSTD_compressStream
		struct {
			/* Libzstd's init function gives a pointer to a memory area
			 * it manages rather than asking for memory to initialise. */
			ZSTD_CStream* context;
			ZSTD_inBuffer in;
			ZSTD_outBuffer out;
		} zstd;
#endif
	} stream;
#endif
	size_t len_buf;
	unsigned char *buf;
};

/* Don't call this min or MIN, since those are already defined
   on lots of platforms (but not all). */
#define zipmin(a, b) ((a) > (b) ? (b) : (a))

static ssize_t archive_write_zip_data(struct archive_write *,
		   const void *buff, size_t s);
static int archive_write_zip_close(struct archive_write *);
static int archive_write_zip_free(struct archive_write *);
static int archive_write_zip_finish_entry(struct archive_write *);
static int archive_write_zip_header(struct archive_write *,
	      struct archive_entry *);
static int archive_write_zip_options(struct archive_write *,
	      const char *, const char *);
static size_t path_length(struct archive_entry *);
static int write_path(struct archive_entry *, struct archive_write *);
static void copy_path(struct archive_entry *, unsigned char *);
static struct archive_string_conv *get_sconv(struct archive_write *, struct zip *);
static int trad_enc_init(struct trad_enc_ctx *, const char *, size_t);
static unsigned trad_enc_encrypt_update(struct trad_enc_ctx *, const uint8_t *,
    size_t, uint8_t *, size_t);
static int init_traditional_pkware_encryption(struct archive_write *);
static int is_traditional_pkware_encryption_supported(void);
static int init_winzip_aes_encryption(struct archive_write *);
static int is_winzip_aes_encryption_supported(int encryption);

#ifdef HAVE_LZMA_H
/* ZIP's LZMA format requires the use of a alas not exposed in LibLZMA
 * function to write the ZIP header. Given our internal version never
 * fails, no need for a non-void return type. */
static void
lzma_lzma_props_encode(const lzma_options_lzma* options, uint8_t* out)
{
	out[0] = (options->pb * 5 + options->lp) * 9 + options->lc;
	archive_le32enc(out + 1, options->dict_size);
}
#endif

#if defined(HAVE_LZMA_H) && !defined(HAVE_LZMA_STREAM_ENCODER_MT)
/* Dummy mt declarations, to avoid spaghetti includes below. Defined with
 * macros being renamed afterwards to shadow liblzma's types in order to
 * avoid some compiler errors. */
#define lzma_stream_encoder_mt(str, opt) dummy_mt(str, opt)
#define lzma_mt dummy_options

typedef struct {
	void* filters;
	uint32_t preset;
	lzma_check check;
	short threads;
	char flags;
	char block_size;
	char timeout;
} dummy_options;

static inline lzma_ret
dummy_mt(lzma_stream* stream, const lzma_mt* options)
{
	(void)stream; /* UNUSED */
	(void)options; /* UNUSED */
	return LZMA_PROG_ERROR;
}
#endif

static unsigned char *
cd_alloc(struct zip *zip, size_t length)
{
	unsigned char *p;

	if (zip->central_directory == NULL
	    || (zip->central_directory_last->p + length
		> zip->central_directory_last->buff + zip->central_directory_last->buff_size)) {
		struct cd_segment *segment = calloc(1, sizeof(*segment));
		if (segment == NULL)
			return NULL;
		segment->buff_size = 64 * 1024;
		segment->buff = malloc(segment->buff_size);
		if (segment->buff == NULL) {
			free(segment);
			return NULL;
		}
		segment->p = segment->buff;

		if (zip->central_directory == NULL) {
			zip->central_directory
			    = zip->central_directory_last
			    = segment;
		} else {
			zip->central_directory_last->next = segment;
			zip->central_directory_last = segment;
		}
	}

	p = zip->central_directory_last->p;
	zip->central_directory_last->p += length;
	zip->central_directory_bytes += length;
	return (p);
}

static unsigned long
real_crc32(unsigned long crc, const void *buff, size_t len)
{
	return crc32(crc, buff, (unsigned int)len);
}

static unsigned long
fake_crc32(unsigned long crc, const void *buff, size_t len)
{
	(void)crc; /* UNUSED */
	(void)buff; /* UNUSED */
	(void)len; /* UNUSED */
	return 0;
}

static int
archive_write_zip_options(struct archive_write *a, const char *key,
    const char *val)
{
	struct zip *zip = a->format_data;
	int ret = ARCHIVE_FAILED;

	if (strcmp(key, "compression") == 0) {
		/*
		 * Set compression to use on all future entries.
		 * This only affects regular files.
		 */
		if (val == NULL || val[0] == 0) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "%s: compression option needs a compression name",
			    a->format_name);
		} else if (strcmp(val, "deflate") == 0) {
#ifdef HAVE_ZLIB_H
			zip->requested_compression = COMPRESSION_DEFLATE;
			ret = ARCHIVE_OK;
#else
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "deflate compression not supported");
#endif
		} else if (strcmp(val, "store") == 0) {
			zip->requested_compression = COMPRESSION_STORE;
			ret = ARCHIVE_OK;
		} else if (strcmp(val, "bzip2") == 0) {
#ifdef HAVE_BZLIB_H
			zip->requested_compression = COMPRESSION_BZIP2;
			ret = ARCHIVE_OK;
#else
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "bzip2 compression not supported");
#endif
		} else if (strcmp(val, "lzma") == 0) {
#ifdef HAVE_LZMA_H
			zip->requested_compression = COMPRESSION_LZMA;
			ret = ARCHIVE_OK;
#else
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "lzma compression not supported");
#endif
		} else if (strcmp(val, "xz") == 0) {
#ifdef HAVE_LZMA_H
			zip->requested_compression = COMPRESSION_XZ;
			ret = ARCHIVE_OK;
#else
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "xz compression not supported");
#endif
		} else if (strcmp(val, "zstd") == 0) {
#if defined(HAVE_ZSTD_H) && HAVE_ZSTD_compressStream
			zip->requested_compression = COMPRESSION_ZSTD;
			ret = ARCHIVE_OK;
#else
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "zstd compression not supported");
#endif
		}
		return (ret);
	} else if (strcmp(key, "compression-level") == 0) {
		char *endptr;
		unsigned long v;

		if (val == NULL)
			return (ARCHIVE_WARN);
		errno = 0;
		v = strtoul(val, &endptr, 10);
		if (errno != 0 || *endptr != '\0' || v > 9) {
			zip->compression_level = 6; // set to default
			return (ARCHIVE_WARN);
		}
		zip->compression_level = (short)v;

		if (zip->compression_level == 0) {
			zip->requested_compression = COMPRESSION_STORE;
			return ARCHIVE_OK;
		} else {
#if defined(HAVE_ZLIB_H) || defined(HAVE_LZMA_H) || defined(HAVE_BZLIB_H) || (defined(HAVE_ZSTD_H) && HAVE_ZSTD_compressStream)
			// Not forcing an already specified compression algorithm
			if (zip->requested_compression == COMPRESSION_UNSPECIFIED) {
#ifdef HAVE_ZLIB_H
				zip->requested_compression = COMPRESSION_DEFLATE;
#elif defined(HAVE_BZLIB_H)
				zip->requested_compression = COMPRESSION_BZIP2;
#elif defined(HAVE_LZMA_H)
				// Arbitrarily choosing LZMA of the two LZMA methods
				zip->requested_compression = COMPRESSION_LZMA;
#else
				zip->requested_compression = COMPRESSION_ZSTD;
#endif
			}
			return ARCHIVE_OK;
#else
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "compression not supported");
#endif
		}
	} else if (strcmp(key, "threads") == 0) {
		char *endptr;
		unsigned long v;

		if (val == NULL)
			return (ARCHIVE_FAILED);
		errno = 0;
		v = strtoul(val, &endptr, 10);
		if (errno != 0 || *endptr != '\0' || v > SHRT_MAX) {
			zip->threads = 1;
			archive_set_error(&(a->archive), ARCHIVE_ERRNO_MISC,
			    "Illegal value `%s'", val);
			return (ARCHIVE_FAILED);
		}
		zip->threads = (short)v;
		if (zip->threads == 0) {
#ifdef HAVE_LZMA_STREAM_ENCODER_MT
			zip->threads = lzma_cputhreads();
#elif defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
			zip->threads = sysconf(_SC_NPROCESSORS_ONLN);
#elif !defined(__CYGWIN__) && defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0601
			/* Windows 7 and up */
			DWORD activeProcs = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
			zip->threads = activeProcs <= SHRT_MAX ? (short)activeProcs : SHRT_MAX;
#else
			zip->threads = 1;
#endif
		}
		return (ARCHIVE_OK);
	} else if (strcmp(key, "encryption") == 0) {
		if (val == NULL) {
			zip->encryption_type = ENCRYPTION_NONE;
			ret = ARCHIVE_OK;
		} else if (val[0] == '1' || strcmp(val, "traditional") == 0
		    || strcmp(val, "zipcrypt") == 0
		    || strcmp(val, "ZipCrypt") == 0) {
			if (is_traditional_pkware_encryption_supported()) {
				zip->encryption_type = ENCRYPTION_TRADITIONAL;
				ret = ARCHIVE_OK;
			} else {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
				    "encryption not supported");
			}
		} else if (strcmp(val, "aes128") == 0) {
			if (is_winzip_aes_encryption_supported(
			    ENCRYPTION_WINZIP_AES128)) {
				zip->encryption_type = ENCRYPTION_WINZIP_AES128;
				ret = ARCHIVE_OK;
			} else {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
				    "encryption not supported");
			}
		} else if (strcmp(val, "aes256") == 0) {
			if (is_winzip_aes_encryption_supported(
			    ENCRYPTION_WINZIP_AES256)) {
				zip->encryption_type = ENCRYPTION_WINZIP_AES256;
				ret = ARCHIVE_OK;
			} else {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
				    "encryption not supported");
			}
		} else {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "%s: unknown encryption '%s'", a->format_name, val);
		}
		return (ret);
	} else if (strcmp(key, "experimental") == 0) {
		if (val == NULL || val[0] == 0) {
			zip->flags &= ~ ZIP_FLAG_EXPERIMENT_xl;
		} else {
			zip->flags |= ZIP_FLAG_EXPERIMENT_xl;
		}
		return (ARCHIVE_OK);
	} else if (strcmp(key, "fakecrc32") == 0) {
		/*
		 * FOR TESTING ONLY:  disable CRC calculation to speed up
		 * certain complex tests.
		 */
		if (val == NULL || val[0] == 0) {
			zip->crc32func = real_crc32;
		} else {
			zip->crc32func = fake_crc32;
		}
		return (ARCHIVE_OK);
	} else if (strcmp(key, "hdrcharset")  == 0) {
		/*
		 * Set the character set used in translating filenames.
		 */
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
	} else if (strcmp(key, "zip64") == 0) {
		/*
		 * Bias decisions about Zip64: force them to be
		 * generated in certain cases where they are not
		 * forbidden or avoid them in certain cases where they
		 * are not strictly required.
		 */
		if (val != NULL && *val != '\0') {
			zip->flags |= ZIP_FLAG_FORCE_ZIP64;
			zip->flags &= ~ZIP_FLAG_AVOID_ZIP64;
		} else {
			zip->flags &= ~ZIP_FLAG_FORCE_ZIP64;
			zip->flags |= ZIP_FLAG_AVOID_ZIP64;
		}
		return (ARCHIVE_OK);
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
		ARCHIVE_STATE_NEW | ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
		"archive_write_zip_set_compression_deflate");
	if (a->archive.archive_format != ARCHIVE_FORMAT_ZIP) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		"Can only use archive_write_zip_set_compression_deflate"
		" with zip format");
		ret = ARCHIVE_FATAL;
	} else {
#ifdef HAVE_ZLIB_H
		struct zip *zip = a->format_data;
		zip->requested_compression = COMPRESSION_DEFLATE;
		ret = ARCHIVE_OK;
#else
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			"deflate compression not supported");
		ret = ARCHIVE_FAILED;
#endif
	}
	return (ret);
}

int
archive_write_zip_set_compression_bzip2(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int ret = ARCHIVE_FAILED;

	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
		ARCHIVE_STATE_NEW | ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
		"archive_write_zip_set_compression_bzip2");
	if (a->archive.archive_format != ARCHIVE_FORMAT_ZIP) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		"Can only use archive_write_zip_set_compression_bzip2"
		" with zip format");
		ret = ARCHIVE_FATAL;
	} else {
#ifdef HAVE_BZLIB_H
		struct zip *zip = a->format_data;
		zip->requested_compression = COMPRESSION_BZIP2;
		ret = ARCHIVE_OK;
#else
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			"bzip2 compression not supported");
		ret = ARCHIVE_FAILED;
#endif
	}
	return (ret);
}

int
archive_write_zip_set_compression_zstd(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int ret = ARCHIVE_FAILED;

	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
		ARCHIVE_STATE_NEW | ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
		"archive_write_zip_set_compression_zstd");
	if (a->archive.archive_format != ARCHIVE_FORMAT_ZIP) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		"Can only use archive_write_zip_set_compression_zstd"
		" with zip format");
		ret = ARCHIVE_FATAL;
	} else {
#if defined(HAVE_ZSTD_H) && HAVE_ZSTD_compressStream
		struct zip *zip = a->format_data;
		zip->requested_compression = COMPRESSION_ZSTD;
		ret = ARCHIVE_OK;
#else
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			"zstd compression not supported");
		ret = ARCHIVE_FAILED;
#endif
	}
	return (ret);
}

int
archive_write_zip_set_compression_lzma(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int ret = ARCHIVE_FAILED;

	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
		ARCHIVE_STATE_NEW | ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
		"archive_write_zip_set_compression_lzma");
	if (a->archive.archive_format != ARCHIVE_FORMAT_ZIP) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		"Can only use archive_write_zip_set_compression_lzma"
		" with zip format");
		ret = ARCHIVE_FATAL;
	} else {
#ifdef HAVE_LZMA_H
		struct zip *zip = a->format_data;
		zip->requested_compression = COMPRESSION_LZMA;
		ret = ARCHIVE_OK;
#else
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			"lzma compression not supported");
		ret = ARCHIVE_FAILED;
#endif
	}
	return (ret);
}

int
archive_write_zip_set_compression_xz(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	int ret = ARCHIVE_FAILED;

	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
		ARCHIVE_STATE_NEW | ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
		"archive_write_zip_set_compression_xz");
	if (a->archive.archive_format != ARCHIVE_FORMAT_ZIP) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		"Can only use archive_write_zip_set_compression_xz"
		" with zip format");
		ret = ARCHIVE_FATAL;
	} else {
#ifdef HAVE_LZMA_H
		struct zip *zip = a->format_data;
		zip->requested_compression = COMPRESSION_XZ;
		ret = ARCHIVE_OK;
#else
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			"xz compression not supported");
		ret = ARCHIVE_FAILED;
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
		ARCHIVE_STATE_NEW | ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
		"archive_write_zip_set_compression_store");
	if (a->archive.archive_format != ARCHIVE_FORMAT_ZIP) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			"Can only use archive_write_zip_set_compression_store"
			" with zip format");
		ret = ARCHIVE_FATAL;
	} else {
		zip->requested_compression = COMPRESSION_STORE;
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

	zip = calloc(1, sizeof(*zip));
	if (zip == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate zip data");
		return (ARCHIVE_FATAL);
	}

	/* "Unspecified" lets us choose the appropriate compression. */
	zip->requested_compression = COMPRESSION_UNSPECIFIED;
	/* Following the 7-zip write support's lead, setting the default
	 * compression level explicitly to 6 no matter what. */
	zip->compression_level = 6;
	/* Following the xar write support's lead, the default number of
	 * threads is 1 (i.e. the xz compression, the only one caring about
	 * that, not being multi-threaded even if the multi-threaded encoder
	 * were available) */
	zip->threads = 1;
	zip->crc32func = real_crc32;

	/* A buffer used for both compression and encryption. */
	zip->len_buf = 65536;
	zip->buf = malloc(zip->len_buf);
	if (zip->buf == NULL) {
		free(zip);
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate compression buffer");
		return (ARCHIVE_FATAL);
	}

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
	unsigned char local_header[32];
	unsigned char local_extra[144];
	struct zip *zip = a->format_data;
	unsigned char *e;
	unsigned char *cd_extra;
	size_t filename_length;
	const char *slink = NULL;
	size_t slink_size = 0;
	struct archive_string_conv *sconv = get_sconv(a, zip);
	int ret, ret2 = ARCHIVE_OK;
	mode_t type;
	int version_needed = 10;
#define MIN_VERSION_NEEDED(x) do { if (version_needed < x) { version_needed = x; } } while (0)

	/* Sanity check. */
	if (archive_entry_pathname(entry) == NULL
#if defined(_WIN32) && !defined(__CYGWIN__)
	    && archive_entry_pathname_w(entry) == NULL
#endif
	    ) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Can't record entry in zip file without pathname");
		return ARCHIVE_FAILED;
	}

	/* Ignore types of entries that we don't support. */
	type = archive_entry_filetype(entry);
	if (type != AE_IFREG && type != AE_IFDIR && type != AE_IFLNK) {
		__archive_write_entry_filetype_unsupported(
		    &a->archive, entry, "zip");
		return ARCHIVE_FAILED;
	}

	/* If we're not using Zip64, reject large files. */
	if (zip->flags & ZIP_FLAG_AVOID_ZIP64) {
		/* Reject entries over 4GB. */
		if (archive_entry_size_is_set(entry)
		    && (archive_entry_size(entry) > ZIP_4GB_MAX)) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Files > 4GB require Zip64 extensions");
			return ARCHIVE_FAILED;
		}
		/* Reject entries if archive is > 4GB. */
		if (zip->written_bytes > ZIP_4GB_MAX) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Archives > 4GB require Zip64 extensions");
			return ARCHIVE_FAILED;
		}
	}

	/* Only regular files can have size > 0. */
	if (type != AE_IFREG)
		archive_entry_set_size(entry, 0);

	/* Reset information from last entry. */
	zip->entry_offset = zip->written_bytes;
	zip->entry_uncompressed_limit = INT64_MAX;
	/* Zero size values implies that we're using a trailing data descriptor */
	zip->entry_compressed_size = 0;
	zip->entry_uncompressed_size = 0;
	zip->entry_compressed_written = 0;
	zip->entry_uncompressed_written = 0;
	zip->entry_flags = 0;
	zip->entry_crc32 = zip->crc32func(0, NULL, 0);
	zip->entry_encryption = 0;
	archive_entry_free(zip->entry);
	zip->entry = NULL;

	if (zip->cctx_valid)
		archive_encrypto_aes_ctr_release(&zip->cctx);
	if (zip->hctx_valid)
		archive_hmac_sha1_cleanup(&zip->hctx);
	zip->tctx_valid = zip->cctx_valid = zip->hctx_valid = 0;

	if (type == AE_IFREG
		    &&(!archive_entry_size_is_set(entry)
			|| archive_entry_size(entry) > 0)) {
		switch (zip->encryption_type) {
		case ENCRYPTION_TRADITIONAL:
		case ENCRYPTION_WINZIP_AES128:
		case ENCRYPTION_WINZIP_AES256:
			zip->entry_flags |= ZIP_ENTRY_FLAG_ENCRYPTED;
			zip->entry_encryption = zip->encryption_type;
			break;
		case ENCRYPTION_NONE:
		default:
			break;
		}
	}

#if defined(_WIN32) && !defined(__CYGWIN__)
	/* Make sure the path separators in pathname, hardlink and symlink
	 * are all slash '/', not the Windows path separator '\'. */
	zip->entry = __la_win_entry_in_posix_pathseparator(entry);
	if (zip->entry == entry)
		zip->entry = archive_entry_clone(entry);
#else
	zip->entry = archive_entry_clone(entry);
#endif
	if (zip->entry == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate zip header data");
		return (ARCHIVE_FATAL);
	}

	{
		const char *p;
		size_t len;

		if (archive_entry_pathname_l(zip->entry, &p, &len, sconv) != 0) {
			const char* p_mbs;
			if (errno == ENOMEM) {
				archive_set_error(&a->archive, ENOMEM,
				    "Can't allocate memory for Pathname");
				return (ARCHIVE_FATAL);
			}
			p_mbs = archive_entry_pathname(zip->entry);
			if (p_mbs) {
				/* We have a wrongly-encoded MBS pathname.  Warn and use it.  */
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Can't translate pathname '%s' to %s", p_mbs,
				    archive_string_conversion_charset_name(sconv));
				ret2 = ARCHIVE_WARN;
			} else {
				/* We have no MBS pathname.  Fail.  */
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Can't translate pathname to %s",
				    archive_string_conversion_charset_name(sconv));
				return ARCHIVE_FAILED;
			}
		}
		if (len > 0)
			archive_entry_set_pathname(zip->entry, p);

		/*
		 * There is no standard for symlink handling; we convert
		 * it using the same character-set translation that we use
		 * for filename.
		 */
		if (type == AE_IFLNK) {
			if (archive_entry_symlink_l(zip->entry, &p, &len, sconv)) {
				if (errno == ENOMEM) {
					archive_set_error(&a->archive, ENOMEM,
					    "Can't allocate memory "
					    " for Symlink");
					return (ARCHIVE_FATAL);
				}
				/* No error if we can't convert. */
			} else if (len > 0)
				archive_entry_set_symlink(zip->entry, p);
		}
	}

	/* If filename isn't ASCII and we can use UTF-8, set the UTF-8 flag. */
	if (!is_all_ascii(archive_entry_pathname(zip->entry))) {
		if (zip->opt_sconv != NULL) {
			if (strcmp(archive_string_conversion_charset_name(
					zip->opt_sconv), "UTF-8") == 0)
				zip->entry_flags |= ZIP_ENTRY_FLAG_UTF8_NAME;
#if HAVE_NL_LANGINFO
		} else if (strcmp(nl_langinfo(CODESET), "UTF-8") == 0) {
			zip->entry_flags |= ZIP_ENTRY_FLAG_UTF8_NAME;
#endif
		}
	}
	filename_length = path_length(zip->entry);

	/* Determine appropriate compression and size for this entry. */
	if (type == AE_IFLNK) {
		slink = archive_entry_symlink(zip->entry);
		if (slink != NULL)
			slink_size = strlen(slink);
		else
			slink_size = 0;
		zip->entry_uncompressed_limit = slink_size;
		zip->entry_compressed_size = slink_size;
		zip->entry_uncompressed_size = slink_size;
		zip->entry_crc32 = zip->crc32func(zip->entry_crc32,
		    (const unsigned char *)slink, slink_size);
		zip->entry_compression = COMPRESSION_STORE;
		MIN_VERSION_NEEDED(20);
	} else if (type != AE_IFREG) {
		zip->entry_compression = COMPRESSION_STORE;
		zip->entry_uncompressed_limit = 0;
		MIN_VERSION_NEEDED(20);
	} else if (archive_entry_size_is_set(zip->entry)) {
		int64_t size = archive_entry_size(zip->entry);
		int64_t additional_size = 0;

		zip->entry_uncompressed_limit = size;
		zip->entry_compression = zip->requested_compression;
		if (zip->entry_compression == COMPRESSION_UNSPECIFIED) {
			zip->entry_compression = COMPRESSION_DEFAULT;
		}
		switch (zip->entry_compression) {
		case COMPRESSION_STORE:
			zip->entry_compressed_size = size;
			zip->entry_uncompressed_size = size;
			MIN_VERSION_NEEDED(10);
			break;
		case COMPRESSION_ZSTD:
			zip->entry_uncompressed_size = size;
			MIN_VERSION_NEEDED(63);
			break;
		case COMPRESSION_LZMA:
			zip->entry_uncompressed_size = size;
			zip->entry_flags |= ZIP_ENTRY_FLAG_LZMA_EOPM;
			MIN_VERSION_NEEDED(63);
			break;
		case COMPRESSION_XZ:
			zip->entry_uncompressed_size = size;
			MIN_VERSION_NEEDED(63);
			break;
		case COMPRESSION_BZIP2:
			zip->entry_uncompressed_size = size;
			MIN_VERSION_NEEDED(46);
			break;
		default: // i.e. deflate compression
			zip->entry_uncompressed_size = size;
			switch (zip->compression_level) {
			case 1:
			case 2:
				zip->entry_flags |= ZIP_ENTRY_FLAG_DEFLATE_SUPER_FAST;
				break;
			case 3:
			case 4:
				zip->entry_flags |= ZIP_ENTRY_FLAG_DEFLATE_FAST;
				break;
			case 8:
			case 9:
				zip->entry_flags |= ZIP_ENTRY_FLAG_DEFLATE_MAX;
				break;
			default:
				break;
			}
			MIN_VERSION_NEEDED(20);
			break;
		}

		if (zip->entry_flags & ZIP_ENTRY_FLAG_ENCRYPTED) {
			switch (zip->entry_encryption) {
			case ENCRYPTION_TRADITIONAL:
				additional_size = TRAD_HEADER_SIZE;
				MIN_VERSION_NEEDED(20);
				break;
			case ENCRYPTION_WINZIP_AES128:
				additional_size = WINZIP_AES128_HEADER_SIZE
				    + AUTH_CODE_SIZE;
				MIN_VERSION_NEEDED(20);
				break;
			case ENCRYPTION_WINZIP_AES256:
				additional_size = WINZIP_AES256_HEADER_SIZE
				    + AUTH_CODE_SIZE;
				MIN_VERSION_NEEDED(20);
				break;
			case ENCRYPTION_NONE:
			default:
				break;
			}
			if (zip->entry_compression == COMPRESSION_STORE)
				zip->entry_compressed_size += additional_size;
		}

		/*
		 * Set Zip64 extension in any of the following cases
		 * (this was suggested by discussion on info-zip-dev
		 * mailing list):
		 *  = Zip64 is being forced by user
		 *  = File is over 4GiB uncompressed
		 *    (including encryption header, if any)
		 *  = File is close to 4GiB and is being compressed
		 *    (compression might make file larger)
		 */
		if ((zip->flags & ZIP_FLAG_FORCE_ZIP64)
		    || (zip->entry_uncompressed_size + additional_size > ZIP_4GB_MAX)
		    || (zip->entry_uncompressed_size > ZIP_4GB_MAX_UNCOMPRESSED
			&& zip->entry_compression != COMPRESSION_STORE)) {
			MIN_VERSION_NEEDED(45);
		}

		/* We may know the size, but never the CRC. */
		zip->entry_flags |= ZIP_ENTRY_FLAG_LENGTH_AT_END;
	} else {
		/* We don't know the size. Use the default
		 * compression unless specified otherwise.
		 */
		zip->entry_compression = zip->requested_compression;
		if (zip->entry_compression == COMPRESSION_UNSPECIFIED) {
			zip->entry_compression = COMPRESSION_DEFAULT;
		}

		zip->entry_flags |= ZIP_ENTRY_FLAG_LENGTH_AT_END;
		if ((zip->flags & ZIP_FLAG_AVOID_ZIP64) == 0) {
			/* We might use zip64 extensions, so require 4.5 */
			MIN_VERSION_NEEDED(45);
		}
		switch (zip->entry_compression) {
		case COMPRESSION_STORE:
			MIN_VERSION_NEEDED(10);
			break;
		case COMPRESSION_ZSTD:
			MIN_VERSION_NEEDED(63);
			break;
		case COMPRESSION_LZMA:
			zip->entry_flags |= ZIP_ENTRY_FLAG_LZMA_EOPM;
			MIN_VERSION_NEEDED(63);
			break;
		case COMPRESSION_XZ:
			MIN_VERSION_NEEDED(63);
			break;
		case COMPRESSION_BZIP2:
			MIN_VERSION_NEEDED(46);
			break;
		default: // i.e. deflate compression
			switch (zip->compression_level) {
			case 1:
			case 2:
				zip->entry_flags |= ZIP_ENTRY_FLAG_DEFLATE_SUPER_FAST;
				break;
			case 3:
			case 4:
				zip->entry_flags |= ZIP_ENTRY_FLAG_DEFLATE_FAST;
				break;
			case 8:
			case 9:
				zip->entry_flags |= ZIP_ENTRY_FLAG_DEFLATE_MAX;
				break;
			default:
				break;
			}
			MIN_VERSION_NEEDED(20);
			break;
		}

		if (zip->entry_flags & ZIP_ENTRY_FLAG_ENCRYPTED) {
			switch (zip->entry_encryption) {
			case ENCRYPTION_TRADITIONAL:
			case ENCRYPTION_WINZIP_AES128:
			case ENCRYPTION_WINZIP_AES256:
				MIN_VERSION_NEEDED(20);
				break;
			case ENCRYPTION_NONE:
			default:
				break;
			}
		}
	}

	/* Format the local header. */
	memset(local_header, 0, sizeof(local_header));
	memcpy(local_header, "PK\003\004", 4);
	archive_le16enc(local_header + 4, version_needed);
	archive_le16enc(local_header + 6, zip->entry_flags);
	if (zip->entry_encryption == ENCRYPTION_WINZIP_AES128
	    || zip->entry_encryption == ENCRYPTION_WINZIP_AES256)
		archive_le16enc(local_header + 8, WINZIP_AES_ENCRYPTION);
	else
		archive_le16enc(local_header + 8, zip->entry_compression);
	archive_le32enc(local_header + 10,
		unix_to_dos(archive_entry_mtime(zip->entry)));
	if ((zip->entry_flags & ZIP_ENTRY_FLAG_LENGTH_AT_END) == 0) {
		archive_le32enc(local_header + 14, zip->entry_crc32);
		archive_le32enc(local_header + 18, (uint32_t)zip->entry_compressed_size);
		archive_le32enc(local_header + 22, (uint32_t)zip->entry_uncompressed_size);
	}
	archive_le16enc(local_header + 26, (uint16_t)filename_length);

	if (zip->entry_encryption == ENCRYPTION_TRADITIONAL) {
		if (zip->entry_flags & ZIP_ENTRY_FLAG_LENGTH_AT_END)
			zip->trad_chkdat = local_header[11];
		else
			zip->trad_chkdat = local_header[17];
	}

	/* Format as much of central directory file header as we can: */
	zip->file_header = cd_alloc(zip, 46);
	/* If (zip->file_header == NULL) XXXX */
	++zip->central_directory_entries;
	memset(zip->file_header, 0, 46);
	memcpy(zip->file_header, "PK\001\002", 4);
	/* "Made by PKZip 2.0 on Unix." */
	archive_le16enc(zip->file_header + 4, 3 * 256 + version_needed);
	archive_le16enc(zip->file_header + 6, version_needed);
	archive_le16enc(zip->file_header + 8, zip->entry_flags);
	if (zip->entry_encryption == ENCRYPTION_WINZIP_AES128
	    || zip->entry_encryption == ENCRYPTION_WINZIP_AES256)
		archive_le16enc(zip->file_header + 10, WINZIP_AES_ENCRYPTION);
	else
		archive_le16enc(zip->file_header + 10, zip->entry_compression);
	archive_le32enc(zip->file_header + 12,
		unix_to_dos(archive_entry_mtime(zip->entry)));
	archive_le16enc(zip->file_header + 28, (uint16_t)filename_length);
	/* Following Info-Zip, store mode in the "external attributes" field. */
	archive_le32enc(zip->file_header + 38,
	    ((uint32_t)archive_entry_mode(zip->entry)) << 16);
	e = cd_alloc(zip, filename_length);
	/* If (e == NULL) XXXX */
	copy_path(zip->entry, e);

	/* Format extra data. */
	memset(local_extra, 0, sizeof(local_extra));
	e = local_extra;

	/* First, extra blocks that are the same between
	 * the local file header and the central directory.
	 * We format them once and then duplicate them. */

	/* ux Unix extra data, length 11, version 1 */
	if (archive_entry_uid_is_set(entry) || archive_entry_gid_is_set(entry)) {
		/* TODO: If uid < 64k, use 2 bytes, ditto for gid. */
		memcpy(e, "ux\013\000\001", 5);
		e += 5;
		*e++ = 4; /* Length of following UID */
		archive_le32enc(e, (uint32_t)archive_entry_uid(entry));
		e += 4;
		*e++ = 4; /* Length of following GID */
		archive_le32enc(e, (uint32_t)archive_entry_gid(entry));
		e += 4;
	}

	/* AES extra data field: WinZIP AES information, ID=0x9901 */
	if ((zip->entry_flags & ZIP_ENTRY_FLAG_ENCRYPTED)
	    && (zip->entry_encryption == ENCRYPTION_WINZIP_AES128
	        || zip->entry_encryption == ENCRYPTION_WINZIP_AES256)) {

		memcpy(e, "\001\231\007\000\001\000AE", 8);
		/* AES vendor version AE-2 does not store a CRC.
		 * WinZip 11 uses AE-1, which does store the CRC,
		 * but it does not store the CRC when the file size
		 * is less than 20 bytes. So we simulate what
		 * WinZip 11 does.
		 * NOTE: WinZip 9.0 and 10.0 uses AE-2 by default. */
		if (archive_entry_size_is_set(zip->entry)
		    && archive_entry_size(zip->entry) < 20) {
			archive_le16enc(e+4, AES_VENDOR_AE_2);
			zip->aes_vendor = AES_VENDOR_AE_2;/* no CRC. */
		} else
			zip->aes_vendor = AES_VENDOR_AE_1;
		e += 8;
		/* AES encryption strength. */
		*e++ = (zip->entry_encryption == ENCRYPTION_WINZIP_AES128)?1:3;
		/* Actual compression method. */
		archive_le16enc(e, zip->entry_compression);
		e += 2;
	}

	/* Copy ux, AES-extra into central directory as well. */
	zip->file_header_extra_offset = zip->central_directory_bytes;
	cd_extra = cd_alloc(zip, e - local_extra);
	memcpy(cd_extra, local_extra, e - local_extra);

	/*
	 * Following extra blocks vary between local header and
	 * central directory. These are the local header versions.
	 * Central directory versions get formatted in
	 * archive_write_zip_finish_entry() below.
	 */

	/* UT timestamp: length depends on what timestamps are set.
	 * This header appears in the Central Directory also, but
	 * according to Info-Zip specification, the CD form
	 * only holds mtime, so we format it separately. */
	if (archive_entry_mtime_is_set(entry)
	    || archive_entry_atime_is_set(entry)
	    || archive_entry_ctime_is_set(entry)) {
		unsigned char *ut = e;
		memcpy(e, "UT\000\000", 4);
		e += 4;
		*e++ = (archive_entry_mtime_is_set(entry) ? 1 : 0)
			| (archive_entry_atime_is_set(entry) ? 2 : 0)
			| (archive_entry_ctime_is_set(entry) ? 4 : 0);
		if (archive_entry_mtime_is_set(entry)) {
			archive_le32enc(e, (uint32_t)archive_entry_mtime(entry));
			e += 4;
		}
		if (archive_entry_atime_is_set(entry)) {
			archive_le32enc(e, (uint32_t)archive_entry_atime(entry));
			e += 4;
		}
		if (archive_entry_ctime_is_set(entry)) {
			archive_le32enc(e, (uint32_t)archive_entry_ctime(entry));
			e += 4;
		}
		archive_le16enc(ut + 2, (uint16_t)(e - ut - 4));
	}

	/*
	 * Note about Zip64 Extended Information Extra Field:
	 * Because libarchive always writes in a streaming
	 * fashion, we never know the CRC when we're writing
	 * the local header.  So we have to use length-at-end, which
	 * prevents us from putting size information into a Zip64
	 * extra field.  However, apparently some readers find it
	 * a helpful clue to have an empty such field so they
	 * can expect a 64-bit length-at-end marker.
	 */
	if (archive_entry_size_is_set(zip->entry)
	    && (zip->entry_uncompressed_size > ZIP_4GB_MAX
		|| zip->entry_compressed_size > ZIP_4GB_MAX)) {
		/* Header ID 0x0001, size 0 */
		memcpy(e, "\001\000\000\000", 4);
		e += 4;
	}

	if (zip->flags & ZIP_FLAG_EXPERIMENT_xl) {
		/* Experimental 'xl' extension to improve streaming. */
		unsigned char *external_info = e;
		int included = 7;
		memcpy(e, "xl\000\000", 4); // 0x6c65 + 2-byte length
		e += 4;
		e[0] = included; /* bitmap of included fields */
		e += 1;
		if (included & 1) {
			archive_le16enc(e, /* "Version created by" */
			    3 * 256 + version_needed);
			e += 2;
		}
		if (included & 2) {
			archive_le16enc(e, 0); /* internal file attributes */
			e += 2;
		}
		if (included & 4) {
			archive_le32enc(e,  /* external file attributes */
			    ((uint32_t)archive_entry_mode(zip->entry)) << 16);
			e += 4;
		}
		if (included & 8) {
			// Libarchive does not currently support file comments.
		}
		archive_le16enc(external_info + 2, (uint16_t)(e - (external_info + 4)));
	}

	/* Update local header with size of extra data and write it all out: */
	archive_le16enc(local_header + 28, (uint16_t)(e - local_extra));

	ret = __archive_write_output(a, local_header, 30);
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	zip->written_bytes += 30;

	ret = write_path(zip->entry, a);
	if (ret <= ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	zip->written_bytes += ret;

	ret = __archive_write_output(a, local_extra, e - local_extra);
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	zip->written_bytes += e - local_extra;

	/* For symlinks, write the body now. */
	if (slink != NULL) {
		ret = __archive_write_output(a, slink, slink_size);
		if (ret != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		zip->entry_compressed_written += slink_size;
		zip->entry_uncompressed_written += slink_size;
		zip->written_bytes += slink_size;
	}

	switch (zip->entry_compression) {
#ifdef HAVE_ZLIB_H
	case COMPRESSION_DEFLATE:
		zip->stream.deflate.zalloc = Z_NULL;
		zip->stream.deflate.zfree = Z_NULL;
		zip->stream.deflate.opaque = Z_NULL;
		zip->stream.deflate.next_out = zip->buf;
		zip->stream.deflate.avail_out = (uInt)zip->len_buf;
		if (deflateInit2(&zip->stream.deflate, zip->compression_level,
		    Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't init deflate compressor");
			return (ARCHIVE_FATAL);
		}
		break;
#endif
#ifdef HAVE_BZLIB_H
	case COMPRESSION_BZIP2:
		memset(&zip->stream.bzip2, 0, sizeof(bz_stream));
		zip->stream.bzip2.next_out = (char*)zip->buf;
		zip->stream.bzip2.avail_out = (unsigned int)zip->len_buf;
		if (BZ2_bzCompressInit(&zip->stream.bzip2, zip->compression_level, 0, 0) != BZ_OK) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't init bzip2 compressor");
			return (ARCHIVE_FATAL);
		}
		break;
#endif
#if defined(HAVE_ZSTD_H) && HAVE_ZSTD_compressStream
	case COMPRESSION_ZSTD:
		{/* Libzstd, contrary to many compression libraries, doesn't use
		 * zlib's 0 to 9 scale and its negative scale is way bigger than
		 * its positive one. So setting 1 as the lowest allowed compression
		 * level and rescaling to 2 to 9 to libzstd's positive scale. */
		int zstd_compression_level = zip->compression_level == 1
			? ZSTD_minCLevel() // ZSTD_minCLevel is negative !
			: (zip->compression_level - 1) * ZSTD_maxCLevel() / 8;
		zip->stream.zstd.context = ZSTD_createCStream();
		size_t zret = ZSTD_initCStream(zip->stream.zstd.context, zstd_compression_level);
		if (ZSTD_isError(zret)) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't init zstd compressor");
			return (ARCHIVE_FATAL);
		}
		/* Asking for the multi-threaded compressor is a no-op in zstd if
		 * it's not supported, so no need to explicitly check for it */
		ZSTD_CCtx_setParameter(zip->stream.zstd.context, ZSTD_c_nbWorkers, zip->threads);
		zip->stream.zstd.out.dst = zip->buf;
		zip->stream.zstd.out.size = zip->len_buf;
		zip->stream.zstd.out.pos = 0;
		break;}
#endif
#ifdef HAVE_LZMA_H
	case COMPRESSION_LZMA:
		{/* Set compression level 9 as the no-holds barred one */
		uint32_t lzma_compression_level = zip->compression_level == 9
			? LZMA_PRESET_EXTREME | zip->compression_level
			: (uint32_t)zip->compression_level;
		/* Forcibly setting up the encoder to use the LZMA1 variant, as
		 * it is the one LZMA Alone uses. */
		lzma_filter filters[2] = {
			{
				.id = LZMA_FILTER_LZMA1,
				.options = &zip->stream.lzma.options
			},
			{
				.id = LZMA_VLI_UNKNOWN
			}
		};
		memset(&zip->stream.lzma.context, 0, sizeof(lzma_stream));
		lzma_lzma_preset(&zip->stream.lzma.options, lzma_compression_level);
		zip->stream.lzma.headers_to_write = 1;
		/* We'll be writing the headers ourselves, so using the raw
		 * encoder */
		if (lzma_raw_encoder(&zip->stream.lzma.context, filters) != LZMA_OK) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't init lzma compressor");
			return (ARCHIVE_FATAL);
		}
		zip->stream.lzma.context.next_out = zip->buf;
		zip->stream.lzma.context.avail_out = (unsigned int)zip->len_buf;
		break;}
	case COMPRESSION_XZ:
		{/* Set compression level 9 as the no-holds barred one */
		uint32_t lzma_compression_level = zip->compression_level == 9
			? LZMA_PRESET_EXTREME | zip->compression_level
			: (uint32_t)zip->compression_level;
		lzma_ret retval;
#ifndef HAVE_LZMA_STREAM_ENCODER_MT
		/* Force the number of threads to one, and thus to a mono-threaded
		 * encoder in case we don't have the multi-threaded one */
		zip->threads = 1;
#endif
		memset(&zip->stream.lzma.context, 0, sizeof(lzma_stream));
		/* The XZ check will be arbitrarily set to none: ZIP already has
		 * a CRC-32 check of its own */
		if (zip->threads == 1) {
			/* XZ uses LZMA2. */
			lzma_filter filters[2] = {
				{
					.id = LZMA_FILTER_LZMA2,
					.options = &zip->stream.lzma.options
				},
				{
					.id = LZMA_VLI_UNKNOWN
				}
			};
			/* Might as well use the lzma_options we already allocated,
			 * even if we'll never use it after the initialisation */
			lzma_lzma_preset(&zip->stream.lzma.options, lzma_compression_level);
			/* 1 thread requested, so non multi-threaded encoder */
			retval = lzma_stream_encoder(&zip->stream.lzma.context,
				filters, LZMA_CHECK_NONE);
		}
		else {
			lzma_mt options = {
				.flags = 0,
				.block_size = 0,
				.timeout = 0,
				.filters = NULL,
				.check = LZMA_CHECK_NONE,
				.preset = lzma_compression_level,
				.threads = zip->threads
			};
			/* More than 1 thread requested, so multi-threaded encoder
			 * which always outputs XZ */
			retval = lzma_stream_encoder_mt(&zip->stream.lzma.context,
				&options);
		}
		if (retval != LZMA_OK) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't init xz compressor");
			return (ARCHIVE_FATAL);
		}
		zip->stream.lzma.context.next_out = zip->buf;
		zip->stream.lzma.context.avail_out = (unsigned int)zip->len_buf;
		break;}
#endif
	default:
		break;
	}

	return (ret2);
}

static ssize_t
archive_write_zip_data(struct archive_write *a, const void *buff, size_t s)
{
	int ret;
	struct zip *zip = a->format_data;

	if ((int64_t)s > zip->entry_uncompressed_limit)
		s = (size_t)zip->entry_uncompressed_limit;
	zip->entry_uncompressed_written += s;

	if (s == 0) return 0;

	if (zip->entry_flags & ZIP_ENTRY_FLAG_ENCRYPTED) {
		switch (zip->entry_encryption) {
		case ENCRYPTION_TRADITIONAL:
			/* Initialize traditional PKWARE encryption context. */
			if (!zip->tctx_valid) {
				ret = init_traditional_pkware_encryption(a);
				if (ret != ARCHIVE_OK)
					return (ret);
				zip->tctx_valid = 1;
			}
			break;
		case ENCRYPTION_WINZIP_AES128:
		case ENCRYPTION_WINZIP_AES256:
			if (!zip->cctx_valid) {
				ret = init_winzip_aes_encryption(a);
				if (ret != ARCHIVE_OK)
					return (ret);
				zip->cctx_valid = zip->hctx_valid = 1;
			}
			break;
		case ENCRYPTION_NONE:
		default:
			break;
		}
	}

	switch (zip->entry_compression) {
	case COMPRESSION_STORE:
		if (zip->tctx_valid || zip->cctx_valid) {
			const uint8_t *rb = (const uint8_t *)buff;
			const uint8_t * const re = rb + s;

			while (rb < re) {
				size_t l;

				if (zip->tctx_valid) {
					l = trad_enc_encrypt_update(&zip->tctx,
					    rb, re - rb,
					    zip->buf, zip->len_buf);
				} else {
					l = zip->len_buf;
					ret = archive_encrypto_aes_ctr_update(
					    &zip->cctx,
					    rb, re - rb, zip->buf, &l);
					if (ret < 0) {
						archive_set_error(&a->archive,
						    ARCHIVE_ERRNO_MISC,
						    "Failed to encrypt file");
						return (ARCHIVE_FAILED);
					}
					archive_hmac_sha1_update(&zip->hctx,
					    zip->buf, l);
				}
				ret = __archive_write_output(a, zip->buf, l);
				if (ret != ARCHIVE_OK)
					return (ret);
				zip->entry_compressed_written += l;
				zip->written_bytes += l;
				rb += l;
			}
		} else {
			ret = __archive_write_output(a, buff, s);
			if (ret != ARCHIVE_OK)
				return (ret);
			zip->written_bytes += s;
			zip->entry_compressed_written += s;
		}
		break;
#ifdef HAVE_ZLIB_H
	case COMPRESSION_DEFLATE:
		zip->stream.deflate.next_in = (unsigned char*)(uintptr_t)buff;
		zip->stream.deflate.avail_in = (uInt)s;
		do {
			ret = deflate(&zip->stream.deflate, Z_NO_FLUSH);
			if (ret == Z_STREAM_ERROR)
				return (ARCHIVE_FATAL);
			if (zip->stream.deflate.avail_out == 0) {
				if (zip->tctx_valid) {
					trad_enc_encrypt_update(&zip->tctx,
					    zip->buf, zip->len_buf,
					    zip->buf, zip->len_buf);
				} else if (zip->cctx_valid) {
					size_t outl = zip->len_buf;
					ret = archive_encrypto_aes_ctr_update(
					    &zip->cctx,
					    zip->buf, zip->len_buf,
					    zip->buf, &outl);
					if (ret < 0) {
						archive_set_error(&a->archive,
						    ARCHIVE_ERRNO_MISC,
						    "Failed to encrypt file");
						return (ARCHIVE_FAILED);
					}
					archive_hmac_sha1_update(&zip->hctx,
					    zip->buf, zip->len_buf);
				}
				ret = __archive_write_output(a, zip->buf,
					zip->len_buf);
				if (ret != ARCHIVE_OK)
					return (ret);
				zip->entry_compressed_written += zip->len_buf;
				zip->written_bytes += zip->len_buf;
				zip->stream.deflate.next_out = zip->buf;
				zip->stream.deflate.avail_out = (uInt)zip->len_buf;
			}
		} while (zip->stream.deflate.avail_in != 0);
		break;
#endif
#if defined(HAVE_ZSTD_H) && HAVE_ZSTD_compressStream
	case COMPRESSION_ZSTD:
		zip->stream.zstd.in.src = buff;
		zip->stream.zstd.in.size = s;
		zip->stream.zstd.in.pos = 0;
		do {
			size_t zret = ZSTD_compressStream(zip->stream.zstd.context,
				&zip->stream.zstd.out, &zip->stream.zstd.in);
			if (ZSTD_isError(zret))
				return (ARCHIVE_FATAL);
			if (zip->stream.zstd.out.pos == zip->stream.zstd.out.size) {
				if (zip->tctx_valid) {
					trad_enc_encrypt_update(&zip->tctx,
						zip->buf, zip->len_buf,
						zip->buf, zip->len_buf);
				} else if (zip->cctx_valid) {
					size_t outl = zip->len_buf;
					ret = archive_encrypto_aes_ctr_update(
						&zip->cctx,
						zip->buf, zip->len_buf,
						zip->buf, &outl);
					if (ret < 0) {
						archive_set_error(&a->archive,
							ARCHIVE_ERRNO_MISC,
							"Failed to encrypt file");
						return (ARCHIVE_FAILED);
					}
					archive_hmac_sha1_update(&zip->hctx,
						zip->buf, zip->len_buf);
				}
				ret = __archive_write_output(a, zip->buf,
					zip->len_buf);
				if (ret != ARCHIVE_OK)
					return (ret);
				zip->entry_compressed_written += zip->len_buf;
				zip->written_bytes += zip->len_buf;
				zip->stream.zstd.out.dst = zip->buf;
				zip->stream.zstd.out.size = zip->len_buf;
				zip->stream.zstd.out.pos = 0;
			}
		} while (zip->stream.zstd.in.pos != zip->stream.zstd.in.size);
		break;
#endif
#ifdef HAVE_BZLIB_H
	case COMPRESSION_BZIP2:
		zip->stream.bzip2.next_in = (char*)(uintptr_t)buff;
		zip->stream.bzip2.avail_in = (unsigned int)s;
		do {
			ret = BZ2_bzCompress(&zip->stream.bzip2, BZ_RUN);
			if (ret != BZ_RUN_OK)
				return (ARCHIVE_FATAL);
			if (zip->stream.bzip2.avail_out == 0) {
				if (zip->tctx_valid) {
					trad_enc_encrypt_update(&zip->tctx,
						zip->buf, zip->len_buf,
						zip->buf, zip->len_buf);
				} else if (zip->cctx_valid) {
					size_t outl = zip->len_buf;
					ret = archive_encrypto_aes_ctr_update(
						&zip->cctx,
						zip->buf, zip->len_buf,
						zip->buf, &outl);
					if (ret < 0) {
						archive_set_error(&a->archive,
							ARCHIVE_ERRNO_MISC,
							"Failed to encrypt file");
						return (ARCHIVE_FAILED);
					}
					archive_hmac_sha1_update(&zip->hctx,
						zip->buf, zip->len_buf);
				}
				ret = __archive_write_output(a, zip->buf,
					zip->len_buf);
				if (ret != ARCHIVE_OK)
					return (ret);
				zip->entry_compressed_written += zip->len_buf;
				zip->written_bytes += zip->len_buf;
				zip->stream.bzip2.next_out = (char*)zip->buf;
				zip->stream.bzip2.avail_out = (unsigned int)zip->len_buf;
			}
		} while (zip->stream.bzip2.avail_in != 0);
		break;
#endif
#ifdef HAVE_LZMA_H
	case COMPRESSION_LZMA:
		if (zip->stream.lzma.headers_to_write) {
			/* LZMA Alone and ZIP's LZMA format (i.e. id 14) are almost
			 * the same. Here's an example of a structure of LZMA Alone:
			 *
			 * $ cat /bin/ls | lzma | xxd | head -n 1
			 * 00000000: 5d00 0080 00ff ffff ffff ffff ff00 2814
			 *
			 *    5 bytes        8 bytes        n bytes
			 * <lzma_params><uncompressed_size><data...>
			 *
			 * lzma_params is a 5-byte blob that has to be decoded to
			 * extract parameters of this LZMA stream. The
			 * uncompressed_size field is an uint64_t value that contains
			 * information about the size of the uncompressed file, or
			 * UINT64_MAX if this value is unknown. The <data...> part is
			 * the actual LZMA-compressed data stream.
			 *
			 * Now here's the structure of ZIP's LZMA format:
			 *
			 * $ cat stream_inside_zipx | xxd | head -n 1
			 * 00000000: 0914 0500 5d00 8000 0000 2814 .... ....
			 *
			 *  2byte   2byte    5 bytes     n bytes
			 * <magic1><magic2><lzma_params><data...>
			 *
			 * This means that ZIP's LZMA format contains an additional
			 * magic1 and magic2 headers, the lzma_params field contains
			 * the same parameter set as in LZMA Alone, and the <data...>
			 * field is the same as in LZMA Alone as well. However, note
			 * that ZIP's format is missing the uncompressed_size field.
			 *
			 * So we need to write a raw LZMA stream, set up for LZMA1
			 * (i.e. the algorithm variant LZMA Alone uses), which was
			 * done above in the initialisation but first we need to
			 * write ZIP's LZMA header, as if it were Stored data. Then
			 * we can use the raw stream as if it were any other. magic1
			 * being version numbers and magic2 being lzma_params's size,
			 * they get written in without further ado but lzma_params
			 * requires to use other functions than the usual lzma_stream
			 * manipulating ones, hence the additional book-keeping
			 * required alongside the lzma_stream.
			 */
			uint8_t buf[9] = { LZMA_VERSION_MAJOR, LZMA_VERSION_MINOR, 5, 0 };
			lzma_lzma_props_encode(&zip->stream.lzma.options, buf + 4);
			const size_t sh = 9;
			if (zip->tctx_valid || zip->cctx_valid) {
				uint8_t* header = buf;
				const uint8_t * const rh = header + sh;

				while (header < rh) {
					size_t l;

					if (zip->tctx_valid) {
						l = trad_enc_encrypt_update(&zip->tctx,
							header, rh - header,
							zip->buf, zip->len_buf);
					} else {
						l = zip->len_buf;
						ret = archive_encrypto_aes_ctr_update(
							&zip->cctx,
							header, rh - header, zip->buf, &l);
						if (ret < 0) {
							archive_set_error(&a->archive,
								ARCHIVE_ERRNO_MISC,
								"Failed to encrypt file");
							return (ARCHIVE_FAILED);
						}
						archive_hmac_sha1_update(&zip->hctx,
							zip->buf, l);
					}
					ret = __archive_write_output(a, zip->buf, l);
					if (ret != ARCHIVE_OK)
						return (ret);
					zip->entry_compressed_written += l;
					zip->written_bytes += l;
					header += l;
				}
			} else {
				ret = __archive_write_output(a, buf, sh);
				if (ret != ARCHIVE_OK)
					return (ret);
				zip->written_bytes += sh;
				zip->entry_compressed_written += sh;
			}
			zip->stream.lzma.headers_to_write = 0;
		}
		/* FALLTHROUGH */
	case COMPRESSION_XZ:
		zip->stream.lzma.context.next_in = (unsigned char*)(uintptr_t)buff;
		zip->stream.lzma.context.avail_in = (unsigned int)s;
		do {
			ret = lzma_code(&zip->stream.lzma.context, LZMA_RUN);
			if (ret == LZMA_MEM_ERROR)
				return (ARCHIVE_FATAL);
			if (zip->stream.lzma.context.avail_out == 0) {
				if (zip->tctx_valid) {
					trad_enc_encrypt_update(&zip->tctx,
						zip->buf, zip->len_buf,
						zip->buf, zip->len_buf);
				} else if (zip->cctx_valid) {
					size_t outl = zip->len_buf;
					ret = archive_encrypto_aes_ctr_update(
						&zip->cctx,
						zip->buf, zip->len_buf,
						zip->buf, &outl);
					if (ret < 0) {
						archive_set_error(&a->archive,
							ARCHIVE_ERRNO_MISC,
							"Failed to encrypt file");
						return (ARCHIVE_FAILED);
					}
					archive_hmac_sha1_update(&zip->hctx,
						zip->buf, zip->len_buf);
				}
				ret = __archive_write_output(a, zip->buf,
					zip->len_buf);
				if (ret != ARCHIVE_OK)
					return (ret);
				zip->entry_compressed_written += zip->len_buf;
				zip->written_bytes += zip->len_buf;
				zip->stream.lzma.context.next_out = zip->buf;
				zip->stream.lzma.context.avail_out = (unsigned int)zip->len_buf;
			}
		} while (zip->stream.lzma.context.avail_in != 0);
		break;
#endif
	case COMPRESSION_UNSPECIFIED:
	default:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Invalid ZIP compression type");
		return ARCHIVE_FATAL;
	}

	zip->entry_uncompressed_limit -= s;
	if (!zip->cctx_valid || zip->aes_vendor != AES_VENDOR_AE_2)
		zip->entry_crc32 =
		    zip->crc32func(zip->entry_crc32, buff, (unsigned)s);
	return (s);
}

static int
archive_write_zip_finish_entry(struct archive_write *a)
{
	struct zip *zip = a->format_data;
	int ret;
#if defined(HAVE_BZLIB_H) || (defined(HAVE_ZSTD_H) && HAVE_ZSTD_compressStream) || HAVE_LZMA_H
	char finishing;
#endif

	switch (zip->entry_compression) {
#ifdef HAVE_ZLIB_H
	case COMPRESSION_DEFLATE:
		for (;;) {
			size_t remainder;

			ret = deflate(&zip->stream.deflate, Z_FINISH);
			if (ret == Z_STREAM_ERROR)
				return (ARCHIVE_FATAL);
			remainder = zip->len_buf - zip->stream.deflate.avail_out;
			if (zip->tctx_valid) {
				trad_enc_encrypt_update(&zip->tctx,
				    zip->buf, remainder, zip->buf, remainder);
			} else if (zip->cctx_valid) {
				size_t outl = remainder;
				ret = archive_encrypto_aes_ctr_update(
				    &zip->cctx, zip->buf, remainder,
				    zip->buf, &outl);
				if (ret < 0) {
					archive_set_error(&a->archive,
					    ARCHIVE_ERRNO_MISC,
					    "Failed to encrypt file");
					return (ARCHIVE_FAILED);
				}
				archive_hmac_sha1_update(&zip->hctx,
				    zip->buf, remainder);
			}
			ret = __archive_write_output(a, zip->buf, remainder);
			if (ret != ARCHIVE_OK)
			{
				deflateEnd(&zip->stream.deflate);
				return (ret);
			}
			zip->entry_compressed_written += remainder;
			zip->written_bytes += remainder;
			zip->stream.deflate.next_out = zip->buf;
			if (zip->stream.deflate.avail_out != 0)
				break;
			zip->stream.deflate.avail_out = (uInt)zip->len_buf;
		}
		deflateEnd(&zip->stream.deflate);
		break;
#endif
#ifdef HAVE_BZLIB_H
	case COMPRESSION_BZIP2:
		finishing = 1;
		do {
			size_t remainder;

			ret = BZ2_bzCompress(&zip->stream.bzip2, BZ_FINISH);
			if (ret == BZ_STREAM_END)
				finishing = 0;
			else if (ret != BZ_RUN_OK && ret != BZ_FINISH_OK)
				return (ARCHIVE_FATAL);
			remainder = zip->len_buf - zip->stream.bzip2.avail_out;
			if (zip->tctx_valid) {
				trad_enc_encrypt_update(&zip->tctx,
				    zip->buf, remainder, zip->buf, remainder);
			} else if (zip->cctx_valid) {
				size_t outl = remainder;
				ret = archive_encrypto_aes_ctr_update(
				    &zip->cctx, zip->buf, remainder,
				    zip->buf, &outl);
				if (ret < 0) {
					archive_set_error(&a->archive,
					    ARCHIVE_ERRNO_MISC,
					    "Failed to encrypt file");
					return (ARCHIVE_FAILED);
				}
				archive_hmac_sha1_update(&zip->hctx,
				    zip->buf, remainder);
			}
			ret = __archive_write_output(a, zip->buf, remainder);
			if (ret != ARCHIVE_OK)
			{
				BZ2_bzCompressEnd(&zip->stream.bzip2);
				return (ret);
			}
			zip->entry_compressed_written += remainder;
			zip->written_bytes += remainder;
			zip->stream.bzip2.next_out = (char*)zip->buf;
			if (zip->stream.bzip2.avail_out != 0)
				finishing = 0;
			zip->stream.bzip2.avail_out = (unsigned int)zip->len_buf;
		} while (finishing);
		BZ2_bzCompressEnd(&zip->stream.bzip2);
		break;
#endif
#if defined(HAVE_ZSTD_H) && HAVE_ZSTD_compressStream
	case COMPRESSION_ZSTD:
		finishing = 1;
		do {
			size_t remainder;

			size_t zret = ZSTD_endStream(zip->stream.zstd.context, &zip->stream.zstd.out);
			if (zret == 0)
				finishing = 0;
			else if (ZSTD_isError(zret))
				return (ARCHIVE_FATAL);
			remainder = zip->len_buf - (zip->stream.zstd.out.size - zip->stream.zstd.out.pos);
			if (zip->tctx_valid) {
				trad_enc_encrypt_update(&zip->tctx,
				    zip->buf, remainder, zip->buf, remainder);
			} else if (zip->cctx_valid) {
				size_t outl = remainder;
				ret = archive_encrypto_aes_ctr_update(
				    &zip->cctx, zip->buf, remainder,
				    zip->buf, &outl);
				if (ret < 0) {
					archive_set_error(&a->archive,
					    ARCHIVE_ERRNO_MISC,
					    "Failed to encrypt file");
					return (ARCHIVE_FAILED);
				}
				archive_hmac_sha1_update(&zip->hctx,
				    zip->buf, remainder);
			}
			ret = __archive_write_output(a, zip->buf, remainder);
			if (ret != ARCHIVE_OK)
			{
				ZSTD_freeCStream(zip->stream.zstd.context);
				return (ret);
			}
			zip->entry_compressed_written += remainder;
			zip->written_bytes += remainder;
			if (zip->stream.zstd.out.pos != zip->stream.zstd.out.size)
				finishing = 0;
			zip->stream.zstd.out.dst = zip->buf;
			zip->stream.zstd.out.size = zip->len_buf;
			zip->stream.zstd.out.pos = 0;
		} while (finishing);
		ZSTD_freeCStream(zip->stream.zstd.context);
		break;
#endif
#ifdef HAVE_LZMA_H
	/* XZ and LZMA share clean-up code */
	case COMPRESSION_LZMA:
	case COMPRESSION_XZ:
		finishing = 1;
		do {
			size_t remainder;

			ret = lzma_code(&zip->stream.lzma.context, LZMA_FINISH);
			if (ret == LZMA_STREAM_END)
				finishing = 0;
			else if (ret == LZMA_MEM_ERROR)
				return (ARCHIVE_FATAL);
			remainder = zip->len_buf - zip->stream.lzma.context.avail_out;
			if (zip->tctx_valid) {
				trad_enc_encrypt_update(&zip->tctx,
				    zip->buf, remainder, zip->buf, remainder);
			} else if (zip->cctx_valid) {
				size_t outl = remainder;
				ret = archive_encrypto_aes_ctr_update(
				    &zip->cctx, zip->buf, remainder,
				    zip->buf, &outl);
				if (ret < 0) {
					archive_set_error(&a->archive,
					    ARCHIVE_ERRNO_MISC,
					    "Failed to encrypt file");
					return (ARCHIVE_FAILED);
				}
				archive_hmac_sha1_update(&zip->hctx,
				    zip->buf, remainder);
			}
			ret = __archive_write_output(a, zip->buf, remainder);
			if (ret != ARCHIVE_OK)
			{
				lzma_end(&zip->stream.lzma.context);
				return (ret);
			}
			zip->entry_compressed_written += remainder;
			zip->written_bytes += remainder;
			zip->stream.lzma.context.next_out = zip->buf;
			if (zip->stream.lzma.context.avail_out != 0)
				finishing = 0;
			zip->stream.lzma.context.avail_out = (unsigned int)zip->len_buf;
		} while (finishing);
		lzma_end(&zip->stream.lzma.context);
		break;
#endif
	default:
		break;
	}
	if (zip->hctx_valid) {
		uint8_t hmac[20];
		size_t hmac_len = 20;

		archive_hmac_sha1_final(&zip->hctx, hmac, &hmac_len);
		ret = __archive_write_output(a, hmac, AUTH_CODE_SIZE);
		if (ret != ARCHIVE_OK)
			return (ret);
		zip->entry_compressed_written += AUTH_CODE_SIZE;
		zip->written_bytes += AUTH_CODE_SIZE;
	}

	/* Write trailing data descriptor. */
	if ((zip->entry_flags & ZIP_ENTRY_FLAG_LENGTH_AT_END) != 0) {
		char d[24];
		memcpy(d, "PK\007\010", 4);
		if (zip->cctx_valid && zip->aes_vendor == AES_VENDOR_AE_2)
			archive_le32enc(d + 4, 0);/* no CRC.*/
		else
			archive_le32enc(d + 4, zip->entry_crc32);
		if (zip->entry_compressed_written > ZIP_4GB_MAX
		    || zip->entry_uncompressed_written > ZIP_4GB_MAX
		    || zip->flags & ZIP_FLAG_FORCE_ZIP64) {
			archive_le64enc(d + 8,
				(uint64_t)zip->entry_compressed_written);
			archive_le64enc(d + 16,
				(uint64_t)zip->entry_uncompressed_written);
			ret = __archive_write_output(a, d, 24);
			zip->written_bytes += 24;
		} else {
			archive_le32enc(d + 8,
				(uint32_t)zip->entry_compressed_written);
			archive_le32enc(d + 12,
				(uint32_t)zip->entry_uncompressed_written);
			ret = __archive_write_output(a, d, 16);
			zip->written_bytes += 16;
		}
		if (ret != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
	}

	/* UT timestamp: Info-Zip specifies that _only_ the mtime should
	 * be recorded here; ctime and atime are also included in the
	 * local file descriptor. */
	if (archive_entry_mtime_is_set(zip->entry)) {
		unsigned char ut[9];
		unsigned char *u = ut, *ud;
		memcpy(u, "UT\005\000\001", 5);
		u += 5;
		archive_le32enc(u, (uint32_t)archive_entry_mtime(zip->entry));
		u += 4;
		ud = cd_alloc(zip, u - ut);
		if (ud == NULL) {
			archive_set_error(&a->archive, ENOMEM,
					  "Can't allocate zip data");
			return (ARCHIVE_FATAL);
		}
		memcpy(ud, ut, u - ut);
	}

	/* Fill in size information in the central directory entry. */
	/* Fix up central directory file header. */
	if (zip->cctx_valid && zip->aes_vendor == AES_VENDOR_AE_2)
		archive_le32enc(zip->file_header + 16, 0);/* no CRC.*/
	else
		archive_le32enc(zip->file_header + 16, zip->entry_crc32);
	/* Truncate to 32 bits; we'll fix up below. */
	archive_le32enc(zip->file_header + 20, (uint32_t)zip->entry_compressed_written);
	archive_le32enc(zip->file_header + 24, (uint32_t)zip->entry_uncompressed_written);
	archive_le16enc(zip->file_header + 30,
	    (uint16_t)(zip->central_directory_bytes - zip->file_header_extra_offset));
	archive_le32enc(zip->file_header + 42, (uint32_t)zip->entry_offset);

	/* If any of the values immediately above are too large, we'll
	 * need to put the corresponding value in a Zip64 extra field
	 * and set the central directory value to 0xffffffff as a flag. */
	if (zip->entry_compressed_written >= ZIP_4GB_MAX
	    || zip->entry_uncompressed_written >= ZIP_4GB_MAX
	    || zip->entry_offset > ZIP_4GB_MAX) {
		unsigned char zip64[32];
		unsigned char *z = zip64, *zd;
		memcpy(z, "\001\000\000\000", 4);
		z += 4;
		if (zip->entry_uncompressed_written >= ZIP_4GB_MAX) {
			archive_le32enc(zip->file_header + 24, ZIP_4GB_MAX);
			archive_le64enc(z, zip->entry_uncompressed_written);
			z += 8;
		}
		if (zip->entry_compressed_written >= ZIP_4GB_MAX) {
			archive_le32enc(zip->file_header + 20, ZIP_4GB_MAX);
			archive_le64enc(z, zip->entry_compressed_written);
			z += 8;
		}
		if (zip->entry_offset >= ZIP_4GB_MAX) {
			archive_le32enc(zip->file_header + 42, ZIP_4GB_MAX);
			archive_le64enc(z, zip->entry_offset);
			z += 8;
		}
		archive_le16enc(zip64 + 2, (uint16_t)(z - (zip64 + 4)));
		zd = cd_alloc(zip, z - zip64);
		if (zd == NULL) {
			archive_set_error(&a->archive, ENOMEM,
				"Can't allocate zip data");
			return (ARCHIVE_FATAL);
		}
		memcpy(zd, zip64, z - zip64);
		/* Zip64 means version needs to be set to at least 4.5 */
		if (archive_le16dec(zip->file_header + 6) < 45)
			archive_le16enc(zip->file_header + 6, 45);
	}

	/* Fix up central directory file header. */
	if (zip->cctx_valid && zip->aes_vendor == AES_VENDOR_AE_2)
		archive_le32enc(zip->file_header + 16, 0);/* no CRC.*/
	else
		archive_le32enc(zip->file_header + 16, zip->entry_crc32);
	archive_le32enc(zip->file_header + 20,
		(uint32_t)zipmin(zip->entry_compressed_written,
				 ZIP_4GB_MAX));
	archive_le32enc(zip->file_header + 24,
		(uint32_t)zipmin(zip->entry_uncompressed_written,
				 ZIP_4GB_MAX));
	archive_le16enc(zip->file_header + 30,
	    (uint16_t)(zip->central_directory_bytes - zip->file_header_extra_offset));
	archive_le32enc(zip->file_header + 42,
		(uint32_t)zipmin(zip->entry_offset,
				 ZIP_4GB_MAX));

	return (ARCHIVE_OK);
}

static int
archive_write_zip_close(struct archive_write *a)
{
	uint8_t buff[64];
	int64_t offset_start, offset_end;
	struct zip *zip = a->format_data;
	struct cd_segment *segment;
	int ret;

	offset_start = zip->written_bytes;
	segment = zip->central_directory;
	while (segment != NULL) {
		ret = __archive_write_output(a,
		    segment->buff, segment->p - segment->buff);
		if (ret != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		zip->written_bytes += segment->p - segment->buff;
		segment = segment->next;
	}
	offset_end = zip->written_bytes;

	/* If central dir info is too large, write Zip64 end-of-cd */
	if (offset_end - offset_start > ZIP_4GB_MAX
	    || offset_start > ZIP_4GB_MAX
	    || zip->central_directory_entries > 0xffffUL
	    || (zip->flags & ZIP_FLAG_FORCE_ZIP64)) {
	  /* Zip64 end-of-cd record */
	  memset(buff, 0, 56);
	  memcpy(buff, "PK\006\006", 4);
	  archive_le64enc(buff + 4, 44);
	  archive_le16enc(buff + 12, 45);
	  archive_le16enc(buff + 14, 45);
	  /* This is disk 0 of 0. */
	  archive_le64enc(buff + 24, zip->central_directory_entries);
	  archive_le64enc(buff + 32, zip->central_directory_entries);
	  archive_le64enc(buff + 40, offset_end - offset_start);
	  archive_le64enc(buff + 48, offset_start);
	  ret = __archive_write_output(a, buff, 56);
	  if (ret != ARCHIVE_OK)
		  return (ARCHIVE_FATAL);
	  zip->written_bytes += 56;

	  /* Zip64 end-of-cd locator record. */
	  memset(buff, 0, 20);
	  memcpy(buff, "PK\006\007", 4);
	  archive_le32enc(buff + 4, 0);
	  archive_le64enc(buff + 8, offset_end);
	  archive_le32enc(buff + 16, 1);
	  ret = __archive_write_output(a, buff, 20);
	  if (ret != ARCHIVE_OK)
		  return (ARCHIVE_FATAL);
	  zip->written_bytes += 20;
	}

	/* Format and write end of central directory. */
	memset(buff, 0, sizeof(buff));
	memcpy(buff, "PK\005\006", 4);
	archive_le16enc(buff + 8, (uint16_t)zipmin(0xffffU,
		zip->central_directory_entries));
	archive_le16enc(buff + 10, (uint16_t)zipmin(0xffffU,
		zip->central_directory_entries));
	archive_le32enc(buff + 12,
		(uint32_t)zipmin(ZIP_4GB_MAX, (offset_end - offset_start)));
	archive_le32enc(buff + 16,
		(uint32_t)zipmin(ZIP_4GB_MAX, offset_start));
	ret = __archive_write_output(a, buff, 22);
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	zip->written_bytes += 22;
	return (ARCHIVE_OK);
}

static int
archive_write_zip_free(struct archive_write *a)
{
	struct zip *zip;
	struct cd_segment *segment;

	zip = a->format_data;
	while (zip->central_directory != NULL) {
		segment = zip->central_directory;
		zip->central_directory = segment->next;
		free(segment->buff);
		free(segment);
	}
	free(zip->buf);
	archive_entry_free(zip->entry);
	if (zip->cctx_valid)
		archive_encrypto_aes_ctr_release(&zip->cctx);
	if (zip->hctx_valid)
		archive_hmac_sha1_cleanup(&zip->hctx);
	/* TODO: Free opt_sconv, sconv_default */

	free(zip);
	a->format_data = NULL;
	return (ARCHIVE_OK);
}

static size_t
path_length(struct archive_entry *entry)
{
	mode_t type;
	const char *path;
	size_t len;

	type = archive_entry_filetype(entry);
	path = archive_entry_pathname(entry);

	if (path == NULL)
		return (0);
	len = strlen(path);
	if (type == AE_IFDIR && (path[0] == '\0' || path[len - 1] != '/'))
		++len; /* Space for the trailing / */
	return len;
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

	if (path == NULL)
		return (ARCHIVE_FATAL);

	ret = __archive_write_output(archive, path, strlen(path));
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	written_bytes += strlen(path);

	/* Folders are recognized by a trailing slash. */
	if ((type == AE_IFDIR) & (path[strlen(path) - 1] != '/')) {
		ret = __archive_write_output(archive, "/", 1);
		if (ret != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		written_bytes += 1;
	}

	return ((int)written_bytes);
}

static void
copy_path(struct archive_entry *entry, unsigned char *p)
{
	const char *path;
	size_t pathlen;
	mode_t type;

	path = archive_entry_pathname(entry);
	pathlen = strlen(path);
	type = archive_entry_filetype(entry);

	memcpy(p, path, pathlen);

	/* Folders are recognized by a trailing slash. */
	if ((type == AE_IFDIR) && (path[pathlen - 1] != '/'))
		p[pathlen] = '/';
}

static struct archive_string_conv *
get_sconv(struct archive_write *a, struct zip *zip)
{
	if (zip->opt_sconv != NULL)
		return (zip->opt_sconv);

	if (!zip->init_default_conversion) {
		zip->sconv_default =
		    archive_string_default_conversion_for_write(&(a->archive));
		zip->init_default_conversion = 1;
	}
	return (zip->sconv_default);
}

/*
  Traditional PKWARE Decryption functions.
 */

static void
trad_enc_update_keys(struct trad_enc_ctx *ctx, uint8_t c)
{
	uint8_t t;
#define CRC32(c, b) (crc32(c ^ 0xffffffffUL, &b, 1) ^ 0xffffffffUL)

	ctx->keys[0] = CRC32(ctx->keys[0], c);
	ctx->keys[1] = (ctx->keys[1] + (ctx->keys[0] & 0xff)) * 134775813L + 1;
	t = (ctx->keys[1] >> 24) & 0xff;
	ctx->keys[2] = CRC32(ctx->keys[2], t);
#undef CRC32
}

static uint8_t
trad_enc_decrypt_byte(struct trad_enc_ctx *ctx)
{
	unsigned temp = ctx->keys[2] | 2;
	return (uint8_t)((temp * (temp ^ 1)) >> 8) & 0xff;
}

static unsigned
trad_enc_encrypt_update(struct trad_enc_ctx *ctx, const uint8_t *in,
    size_t in_len, uint8_t *out, size_t out_len)
{
	unsigned i, max;

	max = (unsigned)((in_len < out_len)? in_len: out_len);

	for (i = 0; i < max; i++) {
		uint8_t t = in[i];
		out[i] = t ^ trad_enc_decrypt_byte(ctx);
		trad_enc_update_keys(ctx, t);
	}
	return i;
}

static int
trad_enc_init(struct trad_enc_ctx *ctx, const char *pw, size_t pw_len)
{
	ctx->keys[0] = 305419896L;
	ctx->keys[1] = 591751049L;
	ctx->keys[2] = 878082192L;

	for (;pw_len; --pw_len)
		trad_enc_update_keys(ctx, *pw++);
	return 0;
}

static int
is_traditional_pkware_encryption_supported(void)
{
	uint8_t key[TRAD_HEADER_SIZE];

	if (archive_random(key, sizeof(key)-1) != ARCHIVE_OK)
		return (0);
	return (1);
}

static int
init_traditional_pkware_encryption(struct archive_write *a)
{
	struct zip *zip = a->format_data;
	const char *passphrase;
	uint8_t key[TRAD_HEADER_SIZE];
	uint8_t key_encrypted[TRAD_HEADER_SIZE];
	int ret;

	passphrase = __archive_write_get_passphrase(a);
	if (passphrase == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Encryption needs passphrase");
		return ARCHIVE_FAILED;
	}
	if (archive_random(key, sizeof(key)-1) != ARCHIVE_OK) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Can't generate random number for encryption");
		return ARCHIVE_FATAL;
	}
	trad_enc_init(&zip->tctx, passphrase, strlen(passphrase));
	/* Set the last key code which will be used as a check code
	 * for verifying passphrase in decryption. */
	key[TRAD_HEADER_SIZE-1] = zip->trad_chkdat;
	trad_enc_encrypt_update(&zip->tctx, key, TRAD_HEADER_SIZE,
	    key_encrypted, TRAD_HEADER_SIZE);
	/* Write encrypted keys in the top of the file content. */
	ret = __archive_write_output(a, key_encrypted, TRAD_HEADER_SIZE);
	if (ret != ARCHIVE_OK)
		return (ret);
	zip->written_bytes += TRAD_HEADER_SIZE;
	zip->entry_compressed_written += TRAD_HEADER_SIZE;
	return (ret);
}

static int
init_winzip_aes_encryption(struct archive_write *a)
{
	struct zip *zip = a->format_data;
	const char *passphrase;
	size_t key_len, salt_len;
	uint8_t salt[16 + 2];
	uint8_t derived_key[MAX_DERIVED_KEY_BUF_SIZE];
	int ret;

	passphrase = __archive_write_get_passphrase(a);
	if (passphrase == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Encryption needs passphrase");
		return (ARCHIVE_FAILED);
	}
	if (zip->entry_encryption == ENCRYPTION_WINZIP_AES128) {
		salt_len = 8;
		key_len = 16;
	} else {
		/* AES 256 */
		salt_len = 16;
		key_len = 32;
	}
	if (archive_random(salt, salt_len) != ARCHIVE_OK) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Can't generate random number for encryption");
		return (ARCHIVE_FATAL);
	}
	ret = archive_pbkdf2_sha1(passphrase, strlen(passphrase),
	    salt, salt_len, 1000, derived_key, key_len * 2 + 2);
	if (ret != 0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    ret == CRYPTOR_STUB_FUNCTION ? "Encryption is unsupported due to "
			"lack of crypto library" : "Failed to process passphrase");
		return (ARCHIVE_FAILED);
	}

	ret = archive_encrypto_aes_ctr_init(&zip->cctx, derived_key, key_len);
	if (ret != 0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Failed to initialize AES CTR mode");
		return (ARCHIVE_FAILED);
	}
	ret = archive_hmac_sha1_init(&zip->hctx, derived_key + key_len,
	    key_len);
	if (ret != 0) {
		archive_encrypto_aes_ctr_release(&zip->cctx);
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Failed to initialize HMAC-SHA1");
		return (ARCHIVE_FAILED);
        }

	/* Set a password verification value after the 'salt'. */
	salt[salt_len] = derived_key[key_len * 2];
	salt[salt_len + 1] = derived_key[key_len * 2 + 1];

	/* Write encrypted keys in the top of the file content. */
	ret = __archive_write_output(a, salt, salt_len + 2);
	if (ret != ARCHIVE_OK)
		return (ret);
	zip->written_bytes += salt_len + 2;
	zip->entry_compressed_written += salt_len + 2;

	return (ARCHIVE_OK);
}

static int
is_winzip_aes_encryption_supported(int encryption)
{
	size_t key_len, salt_len;
	uint8_t salt[16 + 2];
	uint8_t derived_key[MAX_DERIVED_KEY_BUF_SIZE];
	archive_crypto_ctx cctx;
	archive_hmac_sha1_ctx hctx;
	int ret;

	if (encryption == ENCRYPTION_WINZIP_AES128) {
		salt_len = 8;
		key_len = 16;
	} else {
		/* AES 256 */
		salt_len = 16;
		key_len = 32;
	}
	if (archive_random(salt, salt_len) != ARCHIVE_OK)
		return (0);
	ret = archive_pbkdf2_sha1("p", 1, salt, salt_len, 1000,
	    derived_key, key_len * 2 + 2);
	if (ret != 0)
		return (0);

	ret = archive_encrypto_aes_ctr_init(&cctx, derived_key, key_len);
	if (ret != 0)
		return (0);
	ret = archive_hmac_sha1_init(&hctx, derived_key + key_len,
	    key_len);
	archive_encrypto_aes_ctr_release(&cctx);
	if (ret != 0)
		return (0);
	archive_hmac_sha1_cleanup(&hctx);
	return (1);
}
