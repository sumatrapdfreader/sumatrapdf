/*-
 * Copyright (c) 2017 Sean Purcell
 * Copyright (c) 2023-2024 Klara, Inc.
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ZSTD_H
#include <zstd.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_string.h"
#include "archive_write_private.h"

/* Don't compile this if we don't have zstd.h */

struct private_data {
	int		 compression_level;
	int		 threads;
	int		 long_distance;
#if HAVE_ZSTD_H && HAVE_ZSTD_compressStream
	enum {
		running,
		finishing,
		resetting,
	} state;
	int		 frame_per_file;
	size_t		 min_frame_in;
	size_t		 max_frame_in;
	size_t		 min_frame_out;
	size_t		 max_frame_out;
	size_t		 cur_frame;
	size_t		 cur_frame_in;
	size_t		 cur_frame_out;
	size_t		 total_in;
	ZSTD_CStream	*cstream;
	ZSTD_outBuffer	 out;
#else
	struct archive_write_program_data *pdata;
#endif
};

/* If we don't have the library use default range values (zstdcli.c v1.4.0) */
#define CLEVEL_MIN -99
#define CLEVEL_STD_MIN 0 /* prior to 1.3.4 and more recent without using --fast */
#define CLEVEL_DEFAULT 3
#define CLEVEL_STD_MAX 19 /* without using --ultra */
#define CLEVEL_MAX 22

#define LONG_STD 27

#define MINVER_NEGCLEVEL 10304
#define MINVER_MINCLEVEL 10306
#define MINVER_LONG 10302

static int archive_compressor_zstd_options(struct archive_write_filter *,
		    const char *, const char *);
static int archive_compressor_zstd_open(struct archive_write_filter *);
static int archive_compressor_zstd_write(struct archive_write_filter *,
		    const void *, size_t);
static int archive_compressor_zstd_flush(struct archive_write_filter *);
static int archive_compressor_zstd_close(struct archive_write_filter *);
static int archive_compressor_zstd_free(struct archive_write_filter *);
#if HAVE_ZSTD_H && HAVE_ZSTD_compressStream
static int drive_compressor(struct archive_write_filter *,
		    struct private_data *, int, const void *, size_t);
#endif


/*
 * Add a zstd compression filter to this write handle.
 */
int
archive_write_add_filter_zstd(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct archive_write_filter *f = __archive_write_allocate_filter(_a);
	struct private_data *data;
	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_add_filter_zstd");

	data = calloc(1, sizeof(*data));
	if (data == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Out of memory");
		return (ARCHIVE_FATAL);
	}
	f->data = data;
	f->open = &archive_compressor_zstd_open;
	f->options = &archive_compressor_zstd_options;
	f->flush = &archive_compressor_zstd_flush;
	f->close = &archive_compressor_zstd_close;
	f->free = &archive_compressor_zstd_free;
	f->code = ARCHIVE_FILTER_ZSTD;
	f->name = "zstd";
	data->compression_level = CLEVEL_DEFAULT;
	data->threads = 0;
	data->long_distance = 0;
#if HAVE_ZSTD_H && HAVE_ZSTD_compressStream
	data->frame_per_file = 0;
	data->min_frame_in = 0;
	data->max_frame_in = SIZE_MAX;
	data->min_frame_out = 0;
	data->max_frame_out = SIZE_MAX;
	data->cur_frame_in = 0;
	data->cur_frame_out = 0;
	data->cstream = ZSTD_createCStream();
	if (data->cstream == NULL) {
		free(data);
		archive_set_error(&a->archive, ENOMEM,
		    "Failed to allocate zstd compressor object");
		return (ARCHIVE_FATAL);
	}

	return (ARCHIVE_OK);
#else
	data->pdata = __archive_write_program_allocate("zstd");
	if (data->pdata == NULL) {
		free(data);
		archive_set_error(&a->archive, ENOMEM, "Out of memory");
		return (ARCHIVE_FATAL);
	}
	archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
	    "Using external zstd program");
	return (ARCHIVE_WARN);
#endif
}

static int
archive_compressor_zstd_free(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;
#if HAVE_ZSTD_H && HAVE_ZSTD_compressStream
	ZSTD_freeCStream(data->cstream);
	free(data->out.dst);
#else
	__archive_write_program_free(data->pdata);
#endif
	free(data);
	f->data = NULL;
	return (ARCHIVE_OK);
}

static int
string_to_number(const char *string, intmax_t *numberp)
{
	char *end;

	if (string == NULL || *string == '\0')
		return (ARCHIVE_WARN);
	*numberp = strtoimax(string, &end, 10);
	if (end == string || *end != '\0' || errno == EOVERFLOW) {
		*numberp = 0;
		return (ARCHIVE_WARN);
	}
	return (ARCHIVE_OK);
}

#if HAVE_ZSTD_H && HAVE_ZSTD_compressStream
static int
string_to_size(const char *string, size_t *numberp)
{
	uintmax_t number;
	char *end;
	unsigned int shift = 0;

	if (string == NULL || *string == '\0' || *string == '-')
		return (ARCHIVE_WARN);
	number = strtoumax(string, &end, 10);
	if (end > string) {
		if (*end == 'K' || *end == 'k') {
			shift = 10;
			end++;
		} else if (*end == 'M' || *end == 'm') {
			shift = 20;
			end++;
		} else if (*end == 'G' || *end == 'g') {
			shift = 30;
			end++;
		}
		if (*end == 'B' || *end == 'b') {
			end++;
		}
	}
	if (end == string || *end != '\0' || errno == EOVERFLOW) {
		return (ARCHIVE_WARN);
	}
	if (number > (uintmax_t)SIZE_MAX >> shift) {
		return (ARCHIVE_WARN);
	}
	*numberp = (size_t)(number << shift);
	return (ARCHIVE_OK);
}
#endif

/*
 * Set write options.
 */
static int
archive_compressor_zstd_options(struct archive_write_filter *f, const char *key,
    const char *value)
{
	struct private_data *data = (struct private_data *)f->data;

	if (strcmp(key, "compression-level") == 0) {
		intmax_t level;
		if (string_to_number(value, &level) != ARCHIVE_OK) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "compression-level invalid");
			return (ARCHIVE_FAILED);
		}
		/* If we don't have the library, hard-code the max level */
		int minimum = CLEVEL_MIN;
		int maximum = CLEVEL_MAX;
#if HAVE_ZSTD_H && HAVE_ZSTD_compressStream
		maximum = ZSTD_maxCLevel();
#if ZSTD_VERSION_NUMBER >= MINVER_MINCLEVEL
		if (ZSTD_versionNumber() >= MINVER_MINCLEVEL) {
			minimum = ZSTD_minCLevel();
		}
		else
#endif
		if (ZSTD_versionNumber() < MINVER_NEGCLEVEL) {
			minimum = CLEVEL_STD_MIN;
		}
#endif
		if (level < minimum || level > maximum) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "compression-level out of range");
			return (ARCHIVE_FAILED);
		}
		data->compression_level = (int)level;
		return (ARCHIVE_OK);
	} else if (strcmp(key, "threads") == 0) {
		intmax_t threads;
		if (string_to_number(value, &threads) != ARCHIVE_OK) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "threads invalid");
			return (ARCHIVE_FAILED);
		}

#if defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
		if (threads == 0) {
			threads = sysconf(_SC_NPROCESSORS_ONLN);
		}
#elif !defined(__CYGWIN__) && defined(_WIN32_WINNT) && \
    _WIN32_WINNT >= 0x0601 /* _WIN32_WINNT_WIN7 */
		if (threads == 0) {
			DWORD winCores = GetActiveProcessorCount(
			    ALL_PROCESSOR_GROUPS);
			threads = (intmax_t)winCores;
		}
#endif
		if (threads < 0 || threads > INT_MAX) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "threads out of rnage");
			return (ARCHIVE_FAILED);
		}
		data->threads = (int)threads;
		return (ARCHIVE_OK);
#if HAVE_ZSTD_H && HAVE_ZSTD_compressStream
	} else if (strcmp(key, "frame-per-file") == 0) {
		data->frame_per_file = 1;
		return (ARCHIVE_OK);
	} else if (strcmp(key, "min-frame-in") == 0) {
		if (string_to_size(value, &data->min_frame_in) != ARCHIVE_OK) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "min-frame-in invalid");
			return (ARCHIVE_FAILED);
		}
		return (ARCHIVE_OK);
	} else if (strcmp(key, "min-frame-out") == 0 ||
	    strcmp(key, "min-frame-size") == 0) {
		if (string_to_size(value, &data->min_frame_out) != ARCHIVE_OK) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "min-frame-out invalid");
			return (ARCHIVE_FAILED);
		}
		return (ARCHIVE_OK);
	} else if (strcmp(key, "max-frame-in") == 0 ||
	    strcmp(key, "max-frame-size") == 0) {
		if (string_to_size(value, &data->max_frame_in) != ARCHIVE_OK ||
		    data->max_frame_in < 1024) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "max-frame-size invalid");
			return (ARCHIVE_FAILED);
		}
		return (ARCHIVE_OK);
	} else if (strcmp(key, "max-frame-out") == 0) {
		if (string_to_size(value, &data->max_frame_out) != ARCHIVE_OK ||
		    data->max_frame_out < 1024) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "max-frame-out invalid");
			return (ARCHIVE_FAILED);
		}
		return (ARCHIVE_OK);
#endif
	}
	else if (strcmp(key, "long") == 0) {
		intmax_t long_distance;
		if (string_to_number(value, &long_distance) != ARCHIVE_OK) {
			archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "long invalid");
			return (ARCHIVE_FAILED);
		}
#if HAVE_ZSTD_H && HAVE_ZSTD_compressStream && ZSTD_VERSION_NUMBER >= MINVER_LONG
		ZSTD_bounds bounds = ZSTD_cParam_getBounds(ZSTD_c_windowLog);
		if (ZSTD_isError(bounds.error)) {
			int max_distance = ((int)(sizeof(size_t) == 4 ? 30 : 31));
			if (((int)long_distance) < 10 || (int)long_distance > max_distance) {
				archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "long out of range");
				return (ARCHIVE_FAILED);
			}
		} else {
			if ((int)long_distance < bounds.lowerBound || (int)long_distance > bounds.upperBound) {
				archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
			    "long out of range");
				return (ARCHIVE_FAILED);
			}
		}
#else
		int max_distance = ((int)(sizeof(size_t) == 4 ? 30 : 31));
		if (((int)long_distance) < 10 || (int)long_distance > max_distance)
		    return (ARCHIVE_FAILED);
#endif
		data->long_distance = (int)long_distance;
		return (ARCHIVE_OK);
	}

	/* Note: The "warn" return is just to inform the options
	 * supervisor that we didn't handle it.  It will generate
	 * a suitable error if no one used this option. */
	return (ARCHIVE_WARN);
}

#if HAVE_ZSTD_H && HAVE_ZSTD_compressStream
/*
 * Setup callback.
 */
static int
archive_compressor_zstd_open(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;

	if (data->out.dst == NULL) {
		size_t bs = ZSTD_CStreamOutSize(), bpb;
		if (f->archive->magic == ARCHIVE_WRITE_MAGIC) {
			/* Buffer size should be a multiple number of
			 * the of bytes per block for performance. */
			bpb = archive_write_get_bytes_per_block(f->archive);
			if (bpb > bs)
				bs = bpb;
			else if (bpb != 0)
				bs -= bs % bpb;
		}
		data->out.size = bs;
		data->out.pos = 0;
		data->out.dst = malloc(data->out.size);
		if (data->out.dst == NULL) {
			archive_set_error(f->archive, ENOMEM,
			    "Can't allocate data for compression buffer");
			return (ARCHIVE_FATAL);
		}
	}

	f->write = archive_compressor_zstd_write;

	if (ZSTD_isError(ZSTD_initCStream(data->cstream,
	    data->compression_level))) {
		archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
		    "Internal error initializing zstd compressor object");
		return (ARCHIVE_FATAL);
	}

	ZSTD_CCtx_setParameter(data->cstream, ZSTD_c_nbWorkers, data->threads);

	ZSTD_CCtx_setParameter(data->cstream, ZSTD_c_checksumFlag, 1);

#if ZSTD_VERSION_NUMBER >= MINVER_LONG
	ZSTD_CCtx_setParameter(data->cstream, ZSTD_c_windowLog, data->long_distance);
#endif

	return (ARCHIVE_OK);
}

/*
 * Write data to the compressed stream.
 */
static int
archive_compressor_zstd_write(struct archive_write_filter *f, const void *buff,
    size_t length)
{
	struct private_data *data = (struct private_data *)f->data;

	return (drive_compressor(f, data, 0, buff, length));
}

/*
 * Flush the compressed stream.
 */
static int
archive_compressor_zstd_flush(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;

	if (data->frame_per_file && data->state == running) {
		if (data->cur_frame_in > data->min_frame_in &&
		    data->cur_frame_out > data->min_frame_out) {
			data->state = finishing;
		}
	}
	return (drive_compressor(f, data, 1, NULL, 0));
}

/*
 * Finish the compression...
 */
static int
archive_compressor_zstd_close(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;

	if (data->state == running)
		data->state = finishing;
	return (drive_compressor(f, data, 1, NULL, 0));
}

/*
 * Utility function to push input data through compressor,
 * writing full output blocks as necessary.
 */
static int
drive_compressor(struct archive_write_filter *f,
    struct private_data *data, int flush, const void *src, size_t length)
{
	ZSTD_inBuffer in = { .src = src, .size = length, .pos = 0 };
	size_t ipos, opos, zstdret = 0;
	int ret;

	for (;;) {
		ipos = in.pos;
		opos = data->out.pos;
		switch (data->state) {
		case running:
			if (in.pos == in.size)
				return (ARCHIVE_OK);
			zstdret = ZSTD_compressStream(data->cstream,
			    &data->out, &in);
			if (ZSTD_isError(zstdret))
				goto zstd_fatal;
			break;
		case finishing:
			zstdret = ZSTD_endStream(data->cstream, &data->out);
			if (ZSTD_isError(zstdret))
				goto zstd_fatal;
			if (zstdret == 0)
				data->state = resetting;
			break;
		case resetting:
			ZSTD_CCtx_reset(data->cstream, ZSTD_reset_session_only);
			data->cur_frame++;
			data->cur_frame_in = 0;
			data->cur_frame_out = 0;
			data->state = running;
			break;
		}
		data->total_in += in.pos - ipos;
		data->cur_frame_in += in.pos - ipos;
		data->cur_frame_out += data->out.pos - opos;
		if (data->state == running) {
			if (data->cur_frame_in >= data->max_frame_in ||
			    data->cur_frame_out >= data->max_frame_out) {
				data->state = finishing;
			}
		}
		if (data->out.pos == data->out.size ||
		    (flush && data->out.pos > 0)) {
			ret = __archive_write_filter(f->next_filter,
			    data->out.dst, data->out.pos);
			if (ret != ARCHIVE_OK)
				goto fatal;
			data->out.pos = 0;
		}
	}
zstd_fatal:
	archive_set_error(f->archive, ARCHIVE_ERRNO_MISC,
	    "Zstd compression failed: %s",
	    ZSTD_getErrorName(zstdret));
fatal:
	return (ARCHIVE_FATAL);
}

#else /* HAVE_ZSTD_H && HAVE_ZSTD_compressStream */

static int
archive_compressor_zstd_open(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;
	struct archive_string as;
	int r;

	archive_string_init(&as);
	/* --no-check matches library default */
	archive_strcpy(&as, "zstd --no-check");

	if (data->compression_level < CLEVEL_STD_MIN) {
		archive_string_sprintf(&as, " --fast=%d", -data->compression_level);
	} else {
		archive_string_sprintf(&as, " -%d", data->compression_level);
	}

	if (data->compression_level > CLEVEL_STD_MAX) {
		archive_strcat(&as, " --ultra");
	}

	if (data->threads != 0) {
		archive_string_sprintf(&as, " --threads=%d", data->threads);
	}

	if (data->long_distance != 0) {
		archive_string_sprintf(&as, " --long=%d", data->long_distance);
	}

	f->write = archive_compressor_zstd_write;
	r = __archive_write_program_open(f, data->pdata, as.s);
	archive_string_free(&as);
	return (r);
}

static int
archive_compressor_zstd_write(struct archive_write_filter *f, const void *buff,
    size_t length)
{
	struct private_data *data = (struct private_data *)f->data;

	return __archive_write_program_write(f, data->pdata, buff, length);
}

static int
archive_compressor_zstd_flush(struct archive_write_filter *f)
{
	(void)f; /* UNUSED */

	return (ARCHIVE_OK);
}

static int
archive_compressor_zstd_close(struct archive_write_filter *f)
{
	struct private_data *data = (struct private_data *)f->data;

	return __archive_write_program_close(f, data->pdata);
}

#endif /* HAVE_ZSTD_H && HAVE_ZSTD_compressStream */
