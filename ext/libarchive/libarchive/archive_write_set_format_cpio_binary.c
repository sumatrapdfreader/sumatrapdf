/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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

#include "archive_platform.h"

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_entry_locale.h"
#include "archive_private.h"
#include "archive_write_private.h"
#include "archive_write_set_format_private.h"

static ssize_t	archive_write_binary_data(struct archive_write *,
		    const void *buff, size_t s);
static int	archive_write_binary_close(struct archive_write *);
static int	archive_write_binary_free(struct archive_write *);
static int	archive_write_binary_finish_entry(struct archive_write *);
static int	archive_write_binary_header(struct archive_write *,
		    struct archive_entry *);
static int	archive_write_binary_options(struct archive_write *,
		    const char *, const char *);
static int	write_header(struct archive_write *, struct archive_entry *);

struct cpio {
	uint64_t	  entry_bytes_remaining;

	int64_t		  ino_next;

	struct		 { int64_t old; int new;} *ino_list;
	size_t		  ino_list_size;
	size_t		  ino_list_next;

	struct archive_string_conv *opt_sconv;
	struct archive_string_conv *sconv_default;
	int		  init_default_conversion;
};

/* This struct needs to be packed to get the header right */

#if defined(__GNUC__)
#define PACKED(x) x __attribute__((packed))
#elif defined(_MSC_VER)
#define PACKED(x) __pragma(pack(push, 1)) x __pragma(pack(pop))
#else
#define PACKED(x) x
#endif

#define HSIZE 26

PACKED(struct cpio_binary_header {
	uint16_t	h_magic;
	uint16_t	h_dev;
	uint16_t	h_ino;
	uint16_t	h_mode;
	uint16_t	h_uid;
	uint16_t	h_gid;
	uint16_t	h_nlink;
	uint16_t	h_majmin;
	uint32_t	h_mtime;
	uint16_t	h_namesize;
	uint32_t	h_filesize;
});

/* Back in the day, the 7th Edition cpio.c had this, to
 * adapt to, as the comment said, "VAX, Interdata, ...":
 *
 * union { long l; short s[2]; char c[4]; } U;
 * #define MKSHORT(v,lv) {U.l=1L;if(U.c[0]) U.l=lv,v[0]=U.s[1],v[1]=U.s[0]; else U.l=lv,v[0]=U.s[0],v[1]=U.s[1];}
 * long mklong(v)
 * short v[];
 * {
 *         U.l = 1;
 *         if(U.c[0])
 *                 U.s[0] = v[1], U.s[1] = v[0];
 *         else
 *                 U.s[0] = v[0], U.s[1] = v[1];
 *         return U.l;
 * }
 *
 * Of course, that assumes that all machines have little-endian shorts,
 * and just adapts the others to the special endianness of the PDP-11.
 *
 * Now, we could do this:
 *
 * union { uint32_t l; uint16_t s[2]; uint8_t c[4]; } U;
 * #define PUTI16(v,sv) {U.s[0]=1;if(U.c[0]) v=sv; else U.s[0]=sv,U.c[2]=U.c[1],U.c[3]=U.c[0],v=U.s[1];}
 * #define PUTI32(v,lv) {char_t Ut;U.l=1;if(U.c[0]) U.l=lv,v[0]=U.s[1],v[1]=U.s[0]; else U.l=lv,Ut=U.c[0],U.c[0]=U.c[1],U.c[1]=Ut,Ut=U.c[2],U.c[2]=U.c[3],U.c[3]=Ut,v[0]=U.s[0],v[1]=U.s[1];}
 *
 * ...but it feels a little better to do it like this:
 */

static uint16_t la_swap16(uint16_t in) {
	union {
		uint16_t s[2];
		uint8_t c[4];
	} U;
	U.s[0] = 1;
	if (U.c[0])
		return in;
	else {
		U.s[0] = in;
		U.c[2] = U.c[1];
		U.c[3] = U.c[0];
		return U.s[1];
	}
	/* NOTREACHED */
}

static uint32_t la_swap32(uint32_t in) {
	union {
		uint32_t l;
		uint16_t s[2];
		uint8_t c[4];
	} U;
	U.l = 1;
	if (U.c[0]) {		/* Little-endian */
		uint16_t t;
		U.l = in;
		t = U.s[0];
		U.s[0] = U.s[1];
		U.s[1] = t;
	} else if (U.c[3]) {	/* Big-endian */
		U.l = in;
		U.s[0] = la_swap16(U.s[0]);
		U.s[1] = la_swap16(U.s[1]);
	} else {		/* PDP-endian */
		U.l = in;
	}
	return U.l;
}

/*
 * Set output format to the selected binary variant
 */
static int
archive_write_set_format_cpio_binary(struct archive *_a, int format)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct cpio *cpio;

	if (sizeof(struct cpio_binary_header) != HSIZE) {
		archive_set_error(&a->archive, EINVAL,
				  "Binary cpio format not supported on this platform");
		return (ARCHIVE_FATAL);
	}

	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_format_cpio_binary");

	/* If someone else was already registered, unregister them. */
	if (a->format_free != NULL)
		(a->format_free)(a);

	cpio = calloc(1, sizeof(*cpio));
	if (cpio == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate cpio data");
		return (ARCHIVE_FATAL);
	}
	a->format_data = cpio;
	a->format_name = "cpio";
	a->format_options = archive_write_binary_options;
	a->format_write_header = archive_write_binary_header;
	a->format_write_data = archive_write_binary_data;
	a->format_finish_entry = archive_write_binary_finish_entry;
	a->format_close = archive_write_binary_close;
	a->format_free = archive_write_binary_free;
	a->archive.archive_format = format;
	switch (format) {
	case ARCHIVE_FORMAT_CPIO_PWB:
		a->archive.archive_format_name = "PWB cpio";
		break;
	case ARCHIVE_FORMAT_CPIO_BIN_LE:
		a->archive.archive_format_name = "7th Edition cpio";
		break;
	default:
		archive_set_error(&a->archive, EINVAL, "binary format must be 'pwb' or 'bin'");
		return (ARCHIVE_FATAL);
	}
	return (ARCHIVE_OK);
}

/*
 * Set output format to PWB (6th Edition) binary format
 */
int
archive_write_set_format_cpio_pwb(struct archive *_a)
{
	return archive_write_set_format_cpio_binary(_a, ARCHIVE_FORMAT_CPIO_PWB);
}

/*
 * Set output format to 7th Edition binary format
 */
int
archive_write_set_format_cpio_bin(struct archive *_a)
{
	return archive_write_set_format_cpio_binary(_a, ARCHIVE_FORMAT_CPIO_BIN_LE);
}

static int
archive_write_binary_options(struct archive_write *a, const char *key,
    const char *val)
{
	struct cpio *cpio = (struct cpio *)a->format_data;
	int ret = ARCHIVE_FAILED;

	if (strcmp(key, "hdrcharset")  == 0) {
		if (val == NULL || val[0] == 0)
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "%s: hdrcharset option needs a character-set name",
			    a->format_name);
		else {
			cpio->opt_sconv = archive_string_conversion_to_charset(
			    &a->archive, val, 0);
			if (cpio->opt_sconv != NULL)
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

/*
 * Ino values are as long as 64 bits on some systems; cpio format
 * only allows 16 bits and relies on the ino values to identify hardlinked
 * files.  So, we can't merely "hash" the ino numbers since collisions
 * would corrupt the archive.  Instead, we generate synthetic ino values
 * to store in the archive and maintain a map of original ino values to
 * synthetic ones so we can preserve hardlink information.
 *
 * TODO: Make this more efficient.  It's not as bad as it looks (most
 * files don't have any hardlinks and we don't do any work here for those),
 * but it wouldn't be hard to do better.
 *
 * TODO: Work with dev/ino pairs here instead of just ino values.
 */
static int
synthesize_ino_value(struct cpio *cpio, struct archive_entry *entry)
{
	int64_t ino = archive_entry_ino64(entry);
	int ino_new;
	size_t i;

	/*
	 * If no index number was given, don't assign one.  In
	 * particular, this handles the end-of-archive marker
	 * correctly by giving it a zero index value.  (This is also
	 * why we start our synthetic index numbers with one below.)
	 */
	if (ino == 0)
		return (0);

	/* Don't store a mapping if we don't need to. */
	if (archive_entry_nlink(entry) < 2) {
		return (int)(++cpio->ino_next);
	}

	/* Look up old ino; if we have it, this is a hardlink
	 * and we reuse the same value. */
	for (i = 0; i < cpio->ino_list_next; ++i) {
		if (cpio->ino_list[i].old == ino)
			return (cpio->ino_list[i].new);
	}

	/* Assign a new index number. */
	ino_new = (int)(++cpio->ino_next);

	/* Ensure space for the new mapping. */
	if (cpio->ino_list_size <= cpio->ino_list_next) {
		size_t newsize = cpio->ino_list_size < 512
		    ? 512 : cpio->ino_list_size * 2;
		void *newlist = realloc(cpio->ino_list,
		    sizeof(cpio->ino_list[0]) * newsize);
		if (newlist == NULL)
			return (-1);

		cpio->ino_list_size = newsize;
		cpio->ino_list = newlist;
	}

	/* Record and return the new value. */
	cpio->ino_list[cpio->ino_list_next].old = ino;
	cpio->ino_list[cpio->ino_list_next].new = ino_new;
	++cpio->ino_list_next;
	return (ino_new);
}


static struct archive_string_conv *
get_sconv(struct archive_write *a)
{
	struct cpio *cpio;
	struct archive_string_conv *sconv;

	cpio = (struct cpio *)a->format_data;
	sconv = cpio->opt_sconv;
	if (sconv == NULL) {
		if (!cpio->init_default_conversion) {
			cpio->sconv_default =
			    archive_string_default_conversion_for_write(
			      &(a->archive));
			cpio->init_default_conversion = 1;
		}
		sconv = cpio->sconv_default;
	}
	return (sconv);
}

static int
archive_write_binary_header(struct archive_write *a, struct archive_entry *entry)
{
	const char *path;
	size_t len;

	if (archive_entry_filetype(entry) == 0 && archive_entry_hardlink(entry) == NULL) {
		archive_set_error(&a->archive, -1, "Filetype required");
		return (ARCHIVE_FAILED);
	}

	if (archive_entry_pathname_l(entry, &path, &len, get_sconv(a)) != 0
	    && errno == ENOMEM) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate memory for Pathname");
		return (ARCHIVE_FATAL);
	}
	if (len == 0 || path == NULL || path[0] == '\0') {
		archive_set_error(&a->archive, -1, "Pathname required");
		return (ARCHIVE_FAILED);
	}

	if (!archive_entry_size_is_set(entry) || archive_entry_size(entry) < 0) {
		archive_set_error(&a->archive, -1, "Size required");
		return (ARCHIVE_FAILED);
	}
	return write_header(a, entry);
}

static int
write_header(struct archive_write *a, struct archive_entry *entry)
{
	struct cpio *cpio;
	const char *p, *path;
	int pathlength, ret, ret_final;
	int64_t	ino;
	struct cpio_binary_header h;
	struct archive_string_conv *sconv;
	struct archive_entry *entry_main;
	size_t len;

	cpio = (struct cpio *)a->format_data;
	ret_final = ARCHIVE_OK;
	sconv = get_sconv(a);

#if defined(_WIN32) && !defined(__CYGWIN__)
	/* Make sure the path separators in pathname, hardlink and symlink
	 * are all slash '/', not the Windows path separator '\'. */
	entry_main = __la_win_entry_in_posix_pathseparator(entry);
	if (entry_main == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate ustar data");
		return(ARCHIVE_FATAL);
	}
	if (entry != entry_main)
		entry = entry_main;
	else
		entry_main = NULL;
#else
	entry_main = NULL;
#endif

	ret = archive_entry_pathname_l(entry, &path, &len, sconv);
	if (ret != 0) {
		if (errno == ENOMEM) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate memory for Pathname");
			ret_final = ARCHIVE_FATAL;
			goto exit_write_header;
		}
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Can't translate pathname '%s' to %s",
		    archive_entry_pathname(entry),
		    archive_string_conversion_charset_name(sconv));
		ret_final = ARCHIVE_WARN;
	}
	/* Include trailing null */
	pathlength = (int)len + 1;

	h.h_magic = la_swap16(070707);
	h.h_dev = la_swap16(archive_entry_dev(entry));

	ino = synthesize_ino_value(cpio, entry);
	if (ino < 0) {
		archive_set_error(&a->archive, ENOMEM,
		    "No memory for ino translation table");
		ret_final = ARCHIVE_FATAL;
		goto exit_write_header;
	} else if (ino > 077777) {
		archive_set_error(&a->archive, ERANGE,
		    "Too many files for this cpio format");
		ret_final = ARCHIVE_FATAL;
		goto exit_write_header;
	}
	h.h_ino = la_swap16((uint16_t)ino);

	h.h_mode = archive_entry_mode(entry);
	if (((h.h_mode & AE_IFMT) == AE_IFSOCK) || ((h.h_mode & AE_IFMT) == AE_IFIFO)) {
		archive_set_error(&a->archive, EINVAL,
				  "sockets and fifos cannot be represented in the binary cpio formats");
		ret_final = ARCHIVE_FATAL;
		goto exit_write_header;
	}
	if (a->archive.archive_format == ARCHIVE_FORMAT_CPIO_PWB) {
		if ((h.h_mode & AE_IFMT) == AE_IFLNK) {
			archive_set_error(&a->archive, EINVAL,
					  "symbolic links cannot be represented in the PWB cpio format");
			ret_final = ARCHIVE_FATAL;
			goto exit_write_header;
		}
		/* we could turn off AE_IFREG here, but it does no harm, */
		/* and allows v7 cpio to read the entry without confusion */
	}
	h.h_mode = la_swap16(h.h_mode);

	h.h_uid = la_swap16((uint16_t)archive_entry_uid(entry));
	h.h_gid = la_swap16((uint16_t)archive_entry_gid(entry));
	h.h_nlink = la_swap16((uint16_t)archive_entry_nlink(entry));

	if (archive_entry_filetype(entry) == AE_IFBLK
	    || archive_entry_filetype(entry) == AE_IFCHR)
		h.h_majmin = la_swap16(archive_entry_rdev(entry));
	else
		h.h_majmin = 0;

	h.h_mtime = la_swap32((uint32_t)archive_entry_mtime(entry));
	h.h_namesize = la_swap16(pathlength);

	/* Non-regular files don't store bodies. */
	if (archive_entry_filetype(entry) != AE_IFREG)
		archive_entry_set_size(entry, 0);

	/* Symlinks get the link written as the body of the entry. */
	ret = archive_entry_symlink_l(entry, &p, &len, sconv);
	if (ret != 0) {
		if (errno == ENOMEM) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate memory for Linkname");
			ret_final = ARCHIVE_FATAL;
			goto exit_write_header;
		}
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Can't translate linkname '%s' to %s",
		    archive_entry_symlink(entry),
		    archive_string_conversion_charset_name(sconv));
		ret_final = ARCHIVE_WARN;
	}

	if (len > 0 && p != NULL  &&  *p != '\0') {
		if (a->archive.archive_format == ARCHIVE_FORMAT_CPIO_PWB) {
			archive_set_error(&a->archive, EINVAL,
					  "symlinks are not supported by UNIX V6 or by PWB cpio");
			ret_final = ARCHIVE_FATAL;
			goto exit_write_header;
		}
		h.h_filesize = la_swap32((uint32_t)strlen(p)); /* symlink */
	} else {
		if ((a->archive.archive_format == ARCHIVE_FORMAT_CPIO_PWB) &&
		    (archive_entry_size(entry) > 256*256*256-1)) {
			archive_set_error(&a->archive, ERANGE,
					  "File is too large for PWB binary cpio format");
			ret_final = ARCHIVE_FAILED;
			goto exit_write_header;
		} else if (archive_entry_size(entry) > INT32_MAX) {
			archive_set_error(&a->archive, ERANGE,
					  "File is too large for binary cpio format");
			ret_final = ARCHIVE_FAILED;
			goto exit_write_header;
		}
		h.h_filesize = la_swap32((uint32_t)archive_entry_size(entry)); /* file */
	}

	ret = __archive_write_output(a, &h, HSIZE);
	if (ret != ARCHIVE_OK) {
		ret_final = ARCHIVE_FATAL;
		goto exit_write_header;
	}

	ret = __archive_write_output(a, path, pathlength);
	if ((ret == ARCHIVE_OK) && ((pathlength % 2) != 0))
		ret = __archive_write_nulls(a, 1);
	if (ret != ARCHIVE_OK) {
		ret_final = ARCHIVE_FATAL;
		goto exit_write_header;
	}

	cpio->entry_bytes_remaining = archive_entry_size(entry);
	if ((cpio->entry_bytes_remaining % 2) != 0)
		cpio->entry_bytes_remaining++;

	/* Write the symlink now. */
	if (p != NULL  &&  *p != '\0') {
		ret = __archive_write_output(a, p, strlen(p));
		if ((ret == ARCHIVE_OK) && ((strlen(p) % 2) != 0))
			ret = __archive_write_nulls(a, 1);
		if (ret != ARCHIVE_OK) {
			ret_final = ARCHIVE_FATAL;
			goto exit_write_header;
		}
	}

exit_write_header:
	archive_entry_free(entry_main);
	return (ret_final);
}

static ssize_t
archive_write_binary_data(struct archive_write *a, const void *buff, size_t s)
{
	struct cpio *cpio;
	int ret;

	cpio = (struct cpio *)a->format_data;
	if (s > cpio->entry_bytes_remaining)
		s = (size_t)cpio->entry_bytes_remaining;

	ret = __archive_write_output(a, buff, s);
	cpio->entry_bytes_remaining -= s;
	if (ret >= 0)
		return (s);
	else
		return (ret);
}

static int
archive_write_binary_close(struct archive_write *a)
{
	int er;
	struct archive_entry *trailer;

	trailer = archive_entry_new2(NULL);
	if (trailer == NULL) {
		return ARCHIVE_FATAL;
	}
	/* nlink = 1 here for GNU cpio compat. */
	archive_entry_set_nlink(trailer, 1);
	archive_entry_set_size(trailer, 0);
	archive_entry_set_pathname(trailer, "TRAILER!!!");
	er = write_header(a, trailer);
	archive_entry_free(trailer);
	return (er);
}

static int
archive_write_binary_free(struct archive_write *a)
{
	struct cpio *cpio;

	cpio = (struct cpio *)a->format_data;
	free(cpio->ino_list);
	free(cpio);
	a->format_data = NULL;
	return (ARCHIVE_OK);
}

static int
archive_write_binary_finish_entry(struct archive_write *a)
{
	struct cpio *cpio;

	cpio = (struct cpio *)a->format_data;
	return (__archive_write_nulls(a,
		(size_t)cpio->entry_bytes_remaining));
}
