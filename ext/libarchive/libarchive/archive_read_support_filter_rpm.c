/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "archive.h"
#include "archive_endian.h"
#include "archive_private.h"
#include "archive_read_private.h"

struct rpm {
	int data_reached;
};
#define RPM_LEAD_SIZE		96	/* Size of 'Lead' section. */
#define RPM_MIN_HEAD_SIZE	16	/* Minimum size of 'Head'. */

static int	rpm_bidder_bid(struct archive_read_filter_bidder *,
		    struct archive_read_filter *);
static int	rpm_bidder_init(struct archive_read_filter *);

static ssize_t	rpm_filter_read(struct archive_read_filter *,
		    const void **);
static int	rpm_filter_close(struct archive_read_filter *);

#if ARCHIVE_VERSION_NUMBER < 4000000
/* Deprecated; remove in libarchive 4.0 */
int
archive_read_support_compression_rpm(struct archive *a)
{
	return archive_read_support_filter_rpm(a);
}
#endif

static const struct archive_read_filter_bidder_vtable
rpm_bidder_vtable = {
	.bid = rpm_bidder_bid,
	.init = rpm_bidder_init,
};

int
archive_read_support_filter_rpm(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;

	return __archive_read_register_bidder(a, NULL, "rpm",
			&rpm_bidder_vtable);
}

static int
rpm_bidder_bid(struct archive_read_filter_bidder *self,
    struct archive_read_filter *filter)
{
	const unsigned char *b;
	int bits_checked;

	(void)self; /* UNUSED */

	b = __archive_read_filter_ahead(filter, 8, NULL);
	if (b == NULL)
		return (0);

	bits_checked = 0;
	/*
	 * Verify Header Magic Bytes : 0XED 0XAB 0XEE 0XDB
	 */
	if (memcmp(b, "\xED\xAB\xEE\xDB", 4) != 0)
		return (0);
	bits_checked += 32;
	/*
	 * Check major version.
	 */
	if (b[4] != 3 && b[4] != 4)
		return (0);
	bits_checked += 8;
	/*
	 * Check package type; binary or source.
	 */
	if (b[6] != 0)
		return (0);
	bits_checked += 8;
	if (b[7] != 0 && b[7] != 1)
		return (0);
	bits_checked += 8;

	return (bits_checked);
}

static const struct archive_read_filter_vtable
rpm_reader_vtable = {
	.read = rpm_filter_read,
	.close = rpm_filter_close,
};

static int
rpm_bidder_init(struct archive_read_filter *self)
{
	struct rpm   *rpm;

	self->code = ARCHIVE_FILTER_RPM;
	self->name = "rpm";

	rpm = calloc(1, sizeof(*rpm));
	if (rpm == NULL) {
		archive_set_error(&self->archive->archive, ENOMEM,
		    "Can't allocate data for rpm");
		return (ARCHIVE_FATAL);
	}

	self->data = rpm;
	rpm->data_reached = 0;
	self->vtable = &rpm_reader_vtable;

	return (ARCHIVE_OK);
}

static ssize_t
skip_padding(struct archive_read_filter *self)
{
	const unsigned char *h;
	ssize_t avail, count, r;

	do {
		h = __archive_read_filter_ahead(self->upstream, 1, &avail);
		if (h == NULL)
			return (ARCHIVE_FATAL);
		for (count = 0; count < avail && *h++ == '\0'; count++)
			;
		r = __archive_read_filter_consume(self->upstream, count);
		if (r < 0)
			return (r);
	} while (count == avail);

	return (ARCHIVE_OK);
}

static ssize_t
skip_prologue(struct archive_read_filter *self)
{
	const unsigned char *h;
	ssize_t r;
	int header, seen_header = 0;

	/* Skip lead size. */
	r = __archive_read_filter_consume(self->upstream, RPM_LEAD_SIZE);
	if (r < 0)
		return (r);

	do {
		/* Read header intro. */
		h = __archive_read_filter_ahead(self->upstream,
		    RPM_MIN_HEAD_SIZE, NULL);
		if (h == NULL)
			return (ARCHIVE_FATAL);

		header = (memcmp(h, "\x8E\xAD\xE8\x01", 4) == 0);
		if (header) {
			int64_t bytes, length, section;

			seen_header = 1;

			/* Calculate header length. */
			section = archive_be32dec(h + 8);
			bytes = archive_be32dec(h + 12);
			length = RPM_MIN_HEAD_SIZE + section * 16 + bytes;

			/* Skip header. */
			r = __archive_read_filter_consume(self->upstream,
			     length);
			if (r < 0)
				return (r);

			/* Skip padding. */
			r = skip_padding(self);
			if (r != ARCHIVE_OK)
				return (r);
		}
	} while (header);

	/* At least one header must have been encountered. */
	if (!seen_header) {
		archive_set_error(
		    &self->archive->archive,
		    ARCHIVE_ERRNO_FILE_FORMAT,
		    "Unrecognized rpm header");
		return (ARCHIVE_FATAL);
	}

	return (ARCHIVE_OK);
}

static ssize_t
rpm_filter_read(struct archive_read_filter *self, const void **buff)
{
	struct rpm *rpm;
	ssize_t r;

	rpm = (struct rpm *)self->data;

	if (!rpm->data_reached) {
		r = skip_prologue(self);
		if (r != ARCHIVE_OK)
			return (r);
		rpm->data_reached = 1;
	}

	*buff = __archive_read_filter_ahead(self->upstream, 1, &r);
	if (r > 0)
		__archive_read_filter_consume(self->upstream, r);

	return r;
}

static int
rpm_filter_close(struct archive_read_filter *self)
{
	struct rpm *rpm;

	rpm = (struct rpm *)self->data;
	free(rpm);

	return (ARCHIVE_OK);
}

