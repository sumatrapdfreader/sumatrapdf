/*-
 * Copyright (c) 2003-2023 Tim Kientzle
 * Copyright (c) 2011-2012 Michihiro NAKAJIMA
 * Copyright (c) 2016 Martin Matuska
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
#include <stddef.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_acl_private.h" /* For ACL parsing routines. */
#include "archive_entry.h"
#include "archive_entry_locale.h"
#include "archive_private.h"
#include "archive_read_private.h"

#define tar_min(a,b) ((a) < (b) ? (a) : (b))

/*
 * Layout of POSIX 'ustar' tar header.
 */
struct archive_entry_header_ustar {
	char	name[100];
	char	mode[8];
	char	uid[8];
	char	gid[8];
	char	size[12];
	char	mtime[12];
	char	checksum[8];
	char	typeflag[1];
	char	linkname[100];	/* "old format" header ends here */
	char	magic[6];	/* For POSIX: "ustar\0" */
	char	version[2];	/* For POSIX: "00" */
	char	uname[32];
	char	gname[32];
	char	rdevmajor[8];
	char	rdevminor[8];
	char	prefix[155];
};

/*
 * Structure of GNU tar header
 */
struct gnu_sparse {
	char	offset[12];
	char	numbytes[12];
};

struct archive_entry_header_gnutar {
	char	name[100];
	char	mode[8];
	char	uid[8];
	char	gid[8];
	char	size[12];
	char	mtime[12];
	char	checksum[8];
	char	typeflag[1];
	char	linkname[100];
	char	magic[8];  /* "ustar  \0" (note blank/blank/null at end) */
	char	uname[32];
	char	gname[32];
	char	rdevmajor[8];
	char	rdevminor[8];
	char	atime[12];
	char	ctime[12];
	char	offset[12];
	char	longnames[4];
	char	unused[1];
	struct gnu_sparse sparse[4];
	char	isextended[1];
	char	realsize[12];
	/*
	 * Old GNU format doesn't use POSIX 'prefix' field; they use
	 * the 'L' (longname) entry instead.
	 */
};

/*
 * Data specific to this format.
 */
struct sparse_block {
	struct sparse_block	*next;
	int64_t	offset;
	int64_t	remaining;
	int hole;
};

struct tar {
	struct archive_string	 entry_pathname;
	/* For "GNU.sparse.name" and other similar path extensions. */
	struct archive_string	 entry_pathname_override;
	struct archive_string	 entry_uname;
	struct archive_string	 entry_gname;
	struct archive_string	 entry_linkpath;
	struct archive_string	 line;
	int			 pax_hdrcharset_utf8;
	int64_t			 entry_bytes_remaining;
	int64_t			 entry_offset;
	int64_t			 entry_padding;
	int64_t 		 entry_bytes_unconsumed;
	int64_t			 disk_size;
	int64_t			 GNU_sparse_realsize;
	int64_t			 GNU_sparse_size;
	int64_t			 SCHILY_sparse_realsize;
	int64_t			 pax_size;
	struct sparse_block	*sparse_list;
	struct sparse_block	*sparse_last;
	int64_t			 sparse_offset;
	int64_t			 sparse_numbytes;
	int			 sparse_gnu_major;
	int			 sparse_gnu_minor;
	char			 sparse_gnu_attributes_seen;
	char			 filetype;
	char			 size_fields; /* Bits defined below */

	struct archive_string	 localname;
	struct archive_string_conv *opt_sconv;
	struct archive_string_conv *sconv;
	struct archive_string_conv *sconv_acl;
	struct archive_string_conv *sconv_default;
	int			 init_default_conversion;
	int			 compat_2x;
	int			 process_mac_extensions;
	int			 read_concatenated_archives;
};

/* Track which size fields were present in the headers */
#define TAR_SIZE_PAX_SIZE 1
#define TAR_SIZE_GNU_SPARSE_REALSIZE 2
#define TAR_SIZE_GNU_SPARSE_SIZE 4
#define TAR_SIZE_SCHILY_SPARSE_REALSIZE 8


static int	archive_block_is_null(const char *p);
static char	*base64_decode(const char *, size_t, size_t *);
static int	gnu_add_sparse_entry(struct archive_read *, struct tar *,
		    int64_t offset, int64_t remaining);

static void	gnu_clear_sparse_list(struct tar *);
static int	gnu_sparse_old_read(struct archive_read *, struct tar *,
		    const struct archive_entry_header_gnutar *header, int64_t *);
static int	gnu_sparse_old_parse(struct archive_read *, struct tar *,
		    const struct gnu_sparse *sparse, int length);
static int	gnu_sparse_01_parse(struct archive_read *, struct tar *,
		    const char *, size_t);
static int64_t	gnu_sparse_10_read(struct archive_read *, struct tar *,
		    int64_t *);
static int	header_Solaris_ACL(struct archive_read *,  struct tar *,
		    struct archive_entry *, const void *, int64_t *);
static int	header_common(struct archive_read *,  struct tar *,
		    struct archive_entry *, const void *);
static int	header_old_tar(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *);
static int	header_pax_extension(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *, int64_t *);
static int	header_pax_global(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *h, int64_t *);
static int	header_gnu_longlink(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *h, int64_t *);
static int	header_gnu_longname(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *h, int64_t *);
static int	is_mac_metadata_entry(struct archive_entry *entry);
static int	read_mac_metadata_blob(struct archive_read *,
		    struct archive_entry *, int64_t *);
static int	header_volume(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *h, int64_t *);
static int	header_ustar(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *h);
static int	header_gnutar(struct archive_read *, struct tar *,
		    struct archive_entry *, const void *h, int64_t *);
static int	archive_read_format_tar_bid(struct archive_read *, int);
static int	archive_read_format_tar_options(struct archive_read *,
		    const char *, const char *);
static int	archive_read_format_tar_cleanup(struct archive_read *);
static int	archive_read_format_tar_read_data(struct archive_read *a,
		    const void **buff, size_t *size, int64_t *offset);
static int	archive_read_format_tar_skip(struct archive_read *a);
static int	archive_read_format_tar_read_header(struct archive_read *,
		    struct archive_entry *);
static int	checksum(struct archive_read *, const void *);
static int 	pax_attribute(struct archive_read *, struct tar *,
		    struct archive_entry *, const char *key, size_t key_length,
		    size_t value_length, int64_t *unconsumed);
static int	pax_attribute_LIBARCHIVE_xattr(struct archive_entry *,
		    const char *, size_t, const char *, size_t);
static int	pax_attribute_SCHILY_acl(struct archive_read *, struct tar *,
		    struct archive_entry *, size_t, int);
static int	pax_attribute_SUN_holesdata(struct archive_read *, struct tar *,
		    struct archive_entry *, const char *, size_t);
static void	pax_time(const char *, size_t, int64_t *sec, long *nanos);
static ssize_t	readline(struct archive_read *, struct tar *, const char **,
		    ssize_t limit, int64_t *);
static int	read_body_to_string(struct archive_read *, struct tar *,
		    struct archive_string *, const void *h, int64_t *);
static int	read_bytes_to_string(struct archive_read *,
		    struct archive_string *, size_t, int64_t *);
static int64_t	tar_atol(const char *, size_t);
static int64_t	tar_atol10(const char *, size_t);
static int64_t	tar_atol256(const char *, size_t);
static int64_t	tar_atol8(const char *, size_t);
static int	tar_read_header(struct archive_read *, struct tar *,
		    struct archive_entry *, int64_t *);
static int	tohex(int c);
static char	*url_decode(const char *, size_t);
static int	tar_flush_unconsumed(struct archive_read *, int64_t *);

/* Sanity limits:  These numbers should be low enough to
 * prevent a maliciously-crafted archive from forcing us to
 * allocate extreme amounts of memory.  But of course, they
 * need to be high enough for any correct value.  These
 * will likely need some adjustment as we get more experience. */
static const size_t guname_limit = 65536; /* Longest uname or gname: 64kiB */
static const size_t pathname_limit = 1048576; /* Longest path name: 1MiB */
static const size_t sparse_map_limit = 8 * 1048576; /* Longest sparse map: 8MiB */
static const size_t xattr_limit = 16 * 1048576; /* Longest xattr: 16MiB */
static const size_t fflags_limit = 512; /* Longest fflags */
static const size_t acl_limit = 131072; /* Longest textual ACL: 128kiB */
static const int64_t entry_limit = 0xfffffffffffffffLL; /* 2^60 bytes = 1 ExbiByte */

int
archive_read_support_format_gnutar(struct archive *a)
{
	archive_check_magic(a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_format_gnutar");
	return (archive_read_support_format_tar(a));
}


int
archive_read_support_format_tar(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct tar *tar;
	int r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_format_tar");

	tar = calloc(1, sizeof(*tar));
	if (tar == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate tar data");
		return (ARCHIVE_FATAL);
	}
#ifdef HAVE_COPYFILE_H
	/* Set this by default on Mac OS. */
	tar->process_mac_extensions = 1;
#endif

	r = __archive_read_register_format(a, tar, "tar",
	    archive_read_format_tar_bid,
	    archive_read_format_tar_options,
	    archive_read_format_tar_read_header,
	    archive_read_format_tar_read_data,
	    archive_read_format_tar_skip,
	    NULL,
	    archive_read_format_tar_cleanup,
	    NULL,
	    NULL);

	if (r != ARCHIVE_OK)
		free(tar);
	return (ARCHIVE_OK);
}

static int
archive_read_format_tar_cleanup(struct archive_read *a)
{
	struct tar *tar;

	tar = (struct tar *)(a->format->data);
	gnu_clear_sparse_list(tar);
	archive_string_free(&tar->entry_pathname);
	archive_string_free(&tar->entry_pathname_override);
	archive_string_free(&tar->entry_uname);
	archive_string_free(&tar->entry_gname);
	archive_string_free(&tar->entry_linkpath);
	archive_string_free(&tar->line);
	archive_string_free(&tar->localname);
	free(tar);
	(a->format->data) = NULL;
	return (ARCHIVE_OK);
}

/*
 * Validate number field
 *
 * This has to be pretty lenient in order to accommodate the enormous
 * variety of tar writers in the world:
 *  = POSIX (IEEE Std 1003.1-1988) ustar requires octal values with leading
 *    zeros and allows fields to be terminated with space or null characters
 *  = Many writers use different termination (in particular, libarchive
 *    omits terminator bytes to squeeze one or two more digits)
 *  = Many writers pad with space and omit leading zeros
 *  = GNU tar and star write base-256 values if numbers are too
 *    big to be represented in octal
 *
 *  Examples of specific tar headers that we should support:
 *  = Perl Archive::Tar terminates uid, gid, devminor and devmajor with two
 *    null bytes, pads size with spaces and other numeric fields with zeroes
 *  = plexus-archiver prior to 2.6.3 (before switching to commons-compress)
 *    may have uid and gid fields filled with spaces without any octal digits
 *    at all and pads all numeric fields with spaces
 *
 * This should tolerate all variants in use.  It will reject a field
 * where the writer just left garbage after a trailing NUL.
 */
static int
validate_number_field(const char* p_field, size_t i_size)
{
	unsigned char marker = (unsigned char)p_field[0];
	if (marker == 128 || marker == 255 || marker == 0) {
		/* Base-256 marker, there's nothing we can check. */
		return 1;
	} else {
		/* Must be octal */
		size_t i = 0;
		/* Skip any leading spaces */
		while (i < i_size && p_field[i] == ' ') {
			++i;
		}
		/* Skip octal digits. */
		while (i < i_size && p_field[i] >= '0' && p_field[i] <= '7') {
			++i;
		}
		/* Any remaining characters must be space or NUL padding. */
		while (i < i_size) {
			if (p_field[i] != ' ' && p_field[i] != 0) {
				return 0;
			}
			++i;
		}
		return 1;
	}
}

static int
archive_read_format_tar_bid(struct archive_read *a, int best_bid)
{
	int bid;
	const char *h;
	const struct archive_entry_header_ustar *header;

	(void)best_bid; /* UNUSED */

	bid = 0;

	/* Now let's look at the actual header and see if it matches. */
	h = __archive_read_ahead(a, 512, NULL);
	if (h == NULL)
		return (-1);

	/* If it's an end-of-archive mark, we can handle it. */
	if (h[0] == 0 && archive_block_is_null(h)) {
		/*
		 * Usually, I bid the number of bits verified, but
		 * in this case, 4096 seems excessive so I picked 10 as
		 * an arbitrary but reasonable-seeming value.
		 */
		return (10);
	}

	/* If it's not an end-of-archive mark, it must have a valid checksum.*/
	if (!checksum(a, h))
		return (0);
	bid += 48;  /* Checksum is usually 6 octal digits. */

	header = (const struct archive_entry_header_ustar *)h;

	/* Recognize POSIX formats. */
	if ((memcmp(header->magic, "ustar\0", 6) == 0)
	    && (memcmp(header->version, "00", 2) == 0))
		bid += 56;

	/* Recognize GNU tar format. */
	if ((memcmp(header->magic, "ustar ", 6) == 0)
	    && (memcmp(header->version, " \0", 2) == 0))
		bid += 56;

	/* Type flag must be null, digit or A-Z, a-z. */
	if (header->typeflag[0] != 0 &&
	    !( header->typeflag[0] >= '0' && header->typeflag[0] <= '9') &&
	    !( header->typeflag[0] >= 'A' && header->typeflag[0] <= 'Z') &&
	    !( header->typeflag[0] >= 'a' && header->typeflag[0] <= 'z') )
		return (0);
	bid += 2;  /* 6 bits of variation in an 8-bit field leaves 2 bits. */

	/*
	 * Check format of mode/uid/gid/mtime/size/rdevmajor/rdevminor fields.
	 */
	if (validate_number_field(header->mode, sizeof(header->mode)) == 0
	    || validate_number_field(header->uid, sizeof(header->uid)) == 0
	    || validate_number_field(header->gid, sizeof(header->gid)) == 0
	    || validate_number_field(header->mtime, sizeof(header->mtime)) == 0
	    || validate_number_field(header->size, sizeof(header->size)) == 0
	    || validate_number_field(header->rdevmajor, sizeof(header->rdevmajor)) == 0
	    || validate_number_field(header->rdevminor, sizeof(header->rdevminor)) == 0) {
		bid = 0;
	}

	return (bid);
}

static int
archive_read_format_tar_options(struct archive_read *a,
    const char *key, const char *val)
{
	struct tar *tar;
	int ret = ARCHIVE_FAILED;

	tar = (struct tar *)(a->format->data);
	if (strcmp(key, "compat-2x")  == 0) {
		/* Handle UTF-8 filenames as libarchive 2.x */
		tar->compat_2x = (val != NULL && val[0] != 0);
		tar->init_default_conversion = tar->compat_2x;
		return (ARCHIVE_OK);
	} else if (strcmp(key, "hdrcharset")  == 0) {
		if (val == NULL || val[0] == 0)
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "tar: hdrcharset option needs a character-set name");
		else {
			tar->opt_sconv =
			    archive_string_conversion_from_charset(
				&a->archive, val, 0);
			if (tar->opt_sconv != NULL)
				ret = ARCHIVE_OK;
			else
				ret = ARCHIVE_FATAL;
		}
		return (ret);
	} else if (strcmp(key, "mac-ext") == 0) {
		tar->process_mac_extensions = (val != NULL && val[0] != 0);
		return (ARCHIVE_OK);
	} else if (strcmp(key, "read_concatenated_archives") == 0) {
		tar->read_concatenated_archives = (val != NULL && val[0] != 0);
		return (ARCHIVE_OK);
	}

	/* Note: The "warn" return is just to inform the options
	 * supervisor that we didn't handle it.  It will generate
	 * a suitable error if no one used this option. */
	return (ARCHIVE_WARN);
}

/* utility function- this exists to centralize the logic of tracking
 * how much unconsumed data we have floating around, and to consume
 * anything outstanding since we're going to do read_aheads
 */
static int
tar_flush_unconsumed(struct archive_read *a, int64_t *unconsumed)
{
	if (*unconsumed) {
/*
		void *data = (void *)__archive_read_ahead(a, *unconsumed, NULL);
		 * this block of code is to poison claimed unconsumed space, ensuring
		 * things break if it is in use still.
		 * currently it WILL break things, so enable it only for debugging this issue
		if (data) {
			memset(data, 0xff, *unconsumed);
		}
*/
		int64_t consumed = __archive_read_consume(a, *unconsumed);
		if (consumed != *unconsumed) {
			return (ARCHIVE_FATAL);
		}
		*unconsumed = 0;
	}
	return (ARCHIVE_OK);
}

/*
 * The function invoked by archive_read_next_header().  This
 * just sets up a few things and then calls the internal
 * tar_read_header() function below.
 */
static int
archive_read_format_tar_read_header(struct archive_read *a,
    struct archive_entry *entry)
{
	/*
	 * When converting tar archives to cpio archives, it is
	 * essential that each distinct file have a distinct inode
	 * number.  To simplify this, we keep a static count here to
	 * assign fake dev/inode numbers to each tar entry.  Note that
	 * pax format archives may overwrite this with something more
	 * useful.
	 *
	 * Ideally, we would track every file read from the archive so
	 * that we could assign the same dev/ino pair to hardlinks,
	 * but the memory required to store a complete lookup table is
	 * probably not worthwhile just to support the relatively
	 * obscure tar->cpio conversion case.
	 */
	/* TODO: Move this into `struct tar` to avoid conflicts
	 * when reading multiple archives */
	static int default_inode;
	static int default_dev;
	struct tar *tar;
	const char *p;
	const wchar_t *wp;
	int r;
	size_t l;
	int64_t unconsumed = 0;

	/* Assign default device/inode values. */
	archive_entry_set_dev(entry, 1 + default_dev); /* Don't use zero. */
	archive_entry_set_ino(entry, ++default_inode); /* Don't use zero. */
	/* Limit generated st_ino number to 16 bits. */
	if (default_inode >= 0xffff) {
		++default_dev;
		default_inode = 0;
	}

	tar = (struct tar *)(a->format->data);
	tar->entry_offset = 0;
	gnu_clear_sparse_list(tar);
	tar->size_fields = 0; /* We don't have any size info yet */

	/* Setup default string conversion. */
	tar->sconv = tar->opt_sconv;
	if (tar->sconv == NULL) {
		if (!tar->init_default_conversion) {
			tar->sconv_default =
			    archive_string_default_conversion_for_read(&(a->archive));
			tar->init_default_conversion = 1;
		}
		tar->sconv = tar->sconv_default;
	}

	r = tar_read_header(a, tar, entry, &unconsumed);

	tar_flush_unconsumed(a, &unconsumed);

	/*
	 * "non-sparse" files are really just sparse files with
	 * a single block.
	 */
	if (tar->sparse_list == NULL) {
		if (gnu_add_sparse_entry(a, tar, 0, tar->entry_bytes_remaining)
		    != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
	} else {
		struct sparse_block *sb;

		for (sb = tar->sparse_list; sb != NULL; sb = sb->next) {
			if (!sb->hole)
				archive_entry_sparse_add_entry(entry,
				    sb->offset, sb->remaining);
		}
	}

	if (r == ARCHIVE_OK && archive_entry_filetype(entry) == AE_IFREG) {
		/*
		 * "Regular" entry with trailing '/' is really
		 * directory: This is needed for certain old tar
		 * variants and even for some broken newer ones.
		 */
		if ((wp = archive_entry_pathname_w(entry)) != NULL) {
			l = wcslen(wp);
			if (l > 0 && wp[l - 1] == L'/') {
				archive_entry_set_filetype(entry, AE_IFDIR);
				tar->entry_bytes_remaining = 0;
				tar->entry_padding = 0;
			}
		} else if ((p = archive_entry_pathname(entry)) != NULL) {
			l = strlen(p);
			if (l > 0 && p[l - 1] == '/') {
				archive_entry_set_filetype(entry, AE_IFDIR);
				tar->entry_bytes_remaining = 0;
				tar->entry_padding = 0;
			}
		}
	}
	return (r);
}

static int
archive_read_format_tar_read_data(struct archive_read *a,
    const void **buff, size_t *size, int64_t *offset)
{
	ssize_t bytes_read;
	struct tar *tar;
	struct sparse_block *p;

	tar = (struct tar *)(a->format->data);

	for (;;) {
		/* Remove exhausted entries from sparse list. */
		while (tar->sparse_list != NULL &&
		    tar->sparse_list->remaining == 0) {
			p = tar->sparse_list;
			tar->sparse_list = p->next;
			free(p);
		}

		if (tar->entry_bytes_unconsumed) {
			__archive_read_consume(a, tar->entry_bytes_unconsumed);
			tar->entry_bytes_unconsumed = 0;
		}

		/* If we're at end of file, return EOF. */
		if (tar->sparse_list == NULL ||
		    tar->entry_bytes_remaining == 0) {
			int64_t request = tar->entry_bytes_remaining +
			    tar->entry_padding;

			if (__archive_read_consume(a, request) != request)
				return (ARCHIVE_FATAL);
			tar->entry_padding = 0;
			*buff = NULL;
			*size = 0;
			*offset = tar->disk_size;
			return (ARCHIVE_EOF);
		}

		*buff = __archive_read_ahead(a, 1, &bytes_read);
		if (*buff == NULL) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Truncated tar archive"
			    " detected while reading data");
			return (ARCHIVE_FATAL);
		}
		if (bytes_read > tar->entry_bytes_remaining)
			bytes_read = (ssize_t)tar->entry_bytes_remaining;
		/* Don't read more than is available in the
		 * current sparse block. */
		if (tar->sparse_list->remaining < bytes_read)
			bytes_read = (ssize_t)tar->sparse_list->remaining;
		*size = bytes_read;
		*offset = tar->sparse_list->offset;
		tar->sparse_list->remaining -= bytes_read;
		tar->sparse_list->offset += bytes_read;
		tar->entry_bytes_remaining -= bytes_read;
		tar->entry_bytes_unconsumed = bytes_read;

		if (!tar->sparse_list->hole)
			return (ARCHIVE_OK);
		/* Current is hole data and skip this. */
	}
}

static int
archive_read_format_tar_skip(struct archive_read *a)
{
	int64_t request;
	struct tar* tar;

	tar = (struct tar *)(a->format->data);

	request = tar->entry_bytes_remaining + tar->entry_padding +
	    tar->entry_bytes_unconsumed;

	if (__archive_read_consume(a, request) != request)
		return (ARCHIVE_FATAL);

	tar->entry_bytes_remaining = 0;
	tar->entry_bytes_unconsumed = 0;
	tar->entry_padding = 0;

	/* Free the sparse list. */
	gnu_clear_sparse_list(tar);

	return (ARCHIVE_OK);
}

/*
 * This function resets the accumulated state while reading
 * a header.
 */
static void
tar_reset_header_state(struct tar *tar)
{
	tar->pax_hdrcharset_utf8 = 1;
	tar->sparse_gnu_attributes_seen = 0;
	archive_string_empty(&(tar->entry_gname));
	archive_string_empty(&(tar->entry_pathname));
	archive_string_empty(&(tar->entry_pathname_override));
	archive_string_empty(&(tar->entry_uname));
	archive_string_empty(&tar->entry_linkpath);
}

/*
 * This function reads and interprets all of the headers associated
 * with a single entry.
 */
static int
tar_read_header(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, int64_t *unconsumed)
{
	ssize_t bytes;
	int err = ARCHIVE_OK, err2;
	int eof_fatal = 0; /* EOF is okay at some points... */
	const char *h;
	const struct archive_entry_header_ustar *header;
	const struct archive_entry_header_gnutar *gnuheader;

	/* Bitmask of what header types we've seen. */
	int32_t seen_headers = 0;
	static const int32_t seen_A_header = 1;
	static const int32_t seen_g_header = 2;
	static const int32_t seen_K_header = 4;
	static const int32_t seen_L_header = 8;
	static const int32_t seen_V_header = 16;
	static const int32_t seen_x_header = 32; /* Also X */
	static const int32_t seen_mac_metadata = 512;

	tar_reset_header_state(tar);

	/* Ensure format is set. */
	if (a->archive.archive_format_name == NULL) {
		a->archive.archive_format = ARCHIVE_FORMAT_TAR;
		a->archive.archive_format_name = "tar";
	}

	/*
	 * TODO: Write global/default pax options into
	 * 'entry' struct here before overwriting with
	 * file-specific options.
	 */

	/* Loop over all the headers needed for the next entry */
	for (;;) {

		/* Find the next valid header record. */
		while (1) {
			if (tar_flush_unconsumed(a, unconsumed) != ARCHIVE_OK) {
				return (ARCHIVE_FATAL);
			}

			/* Read 512-byte header record */
			h = __archive_read_ahead(a, 512, &bytes);
			if (bytes == 0) { /* EOF at a block boundary. */
				if (eof_fatal) {
					/* We've read a special header already;
					 * if there's no regular header, then this is
					 * a premature EOF. */
					archive_set_error(&a->archive, EINVAL,
							  "Damaged tar archive (end-of-archive within a sequence of headers)");
					return (ARCHIVE_FATAL);
				} else {
					return (ARCHIVE_EOF);
				}
			}
			if (h == NULL) {  /* Short block at EOF; this is bad. */
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Truncated tar archive"
				    " detected while reading next header");
				return (ARCHIVE_FATAL);
			}
			*unconsumed += 512;

			if (h[0] == 0 && archive_block_is_null(h)) {
				/* We found a NULL block which indicates end-of-archive */

				if (tar->read_concatenated_archives) {
					/* We're ignoring NULL blocks, so keep going. */
					continue;
				}

				/* Try to consume a second all-null record, as well. */
				/* If we can't, that's okay. */
				tar_flush_unconsumed(a, unconsumed);
				h = __archive_read_ahead(a, 512, NULL);
				if (h != NULL && h[0] == 0 && archive_block_is_null(h))
						__archive_read_consume(a, 512);

				archive_clear_error(&a->archive);
				return (ARCHIVE_EOF);
			}

			/* This is NOT a null block, so it must be a valid header. */
			if (!checksum(a, h)) {
				if (tar_flush_unconsumed(a, unconsumed) != ARCHIVE_OK) {
					return (ARCHIVE_FATAL);
				}
				archive_set_error(&a->archive, EINVAL,
						  "Damaged tar archive (bad header checksum)");
				/* If we've read some critical information (pax headers, etc)
				 * and _then_ see a bad header, we can't really recover. */
				if (eof_fatal) {
					return (ARCHIVE_FATAL);
				} else {
					return (ARCHIVE_RETRY);
				}
			}
			break;
		}

		/* Determine the format variant. */
		header = (const struct archive_entry_header_ustar *)h;
		switch(header->typeflag[0]) {
		case 'A': /* Solaris tar ACL */
			if (seen_headers & seen_A_header) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
						  "Redundant 'A' header");
				return (ARCHIVE_FATAL);
			}
			seen_headers |= seen_A_header;
			a->archive.archive_format = ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE;
			a->archive.archive_format_name = "Solaris tar";
			err2 = header_Solaris_ACL(a, tar, entry, h, unconsumed);
			break;
		case 'g': /* POSIX-standard 'g' header. */
			if (seen_headers & seen_g_header) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
						  "Redundant 'g' header");
				return (ARCHIVE_FATAL);
			}
			seen_headers |= seen_g_header;
			a->archive.archive_format = ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE;
			a->archive.archive_format_name = "POSIX pax interchange format";
			err2 = header_pax_global(a, tar, entry, h, unconsumed);
			break;
		case 'K': /* Long link name (GNU tar, others) */
			if (seen_headers & seen_K_header) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
						  "Damaged archive: Redundant 'K' headers may cause linknames to be incorrect");
				err = err_combine(err, ARCHIVE_WARN);
			}
			seen_headers |= seen_K_header;
			a->archive.archive_format = ARCHIVE_FORMAT_TAR_GNUTAR;
			a->archive.archive_format_name = "GNU tar format";
			err2 = header_gnu_longlink(a, tar, entry, h, unconsumed);
			break;
		case 'L': /* Long filename (GNU tar, others) */
			if (seen_headers & seen_L_header) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
						  "Damaged archive: Redundant 'L' headers may cause filenames to be incorrect");
				err = err_combine(err, ARCHIVE_WARN);
			}
			seen_headers |= seen_L_header;
			a->archive.archive_format = ARCHIVE_FORMAT_TAR_GNUTAR;
			a->archive.archive_format_name = "GNU tar format";
			err2 = header_gnu_longname(a, tar, entry, h, unconsumed);
			break;
		case 'V': /* GNU volume header */
			if (seen_headers & seen_V_header) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
						  "Redundant 'V' header");
				err = err_combine(err, ARCHIVE_WARN);
			}
			seen_headers |= seen_V_header;
			a->archive.archive_format = ARCHIVE_FORMAT_TAR_GNUTAR;
			a->archive.archive_format_name = "GNU tar format";
			err2 = header_volume(a, tar, entry, h, unconsumed);
			break;
		case 'X': /* Used by SUN tar; same as 'x'. */
			if (seen_headers & seen_x_header) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
						  "Redundant 'X'/'x' header");
				return (ARCHIVE_FATAL);
			}
			seen_headers |= seen_x_header;
			a->archive.archive_format = ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE;
			a->archive.archive_format_name =
				"POSIX pax interchange format (Sun variant)";
			err2 = header_pax_extension(a, tar, entry, h, unconsumed);
			break;
		case 'x': /* POSIX-standard 'x' header. */
			if (seen_headers & seen_x_header) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
						  "Redundant 'x' header");
				return (ARCHIVE_FATAL);
			}
			seen_headers |= seen_x_header;
			a->archive.archive_format = ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE;
			a->archive.archive_format_name = "POSIX pax interchange format";
			err2 = header_pax_extension(a, tar, entry, h, unconsumed);
			break;
		default: /* Regular header: Legacy tar, GNU tar, or ustar */
			gnuheader = (const struct archive_entry_header_gnutar *)h;
			if (memcmp(gnuheader->magic, "ustar  \0", 8) == 0) {
				a->archive.archive_format = ARCHIVE_FORMAT_TAR_GNUTAR;
				a->archive.archive_format_name = "GNU tar format";
				err2 = header_gnutar(a, tar, entry, h, unconsumed);
			} else if (memcmp(header->magic, "ustar", 5) == 0) {
				if (a->archive.archive_format != ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE) {
					a->archive.archive_format = ARCHIVE_FORMAT_TAR_USTAR;
					a->archive.archive_format_name = "POSIX ustar format";
				}
				err2 = header_ustar(a, tar, entry, h);
			} else {
				a->archive.archive_format = ARCHIVE_FORMAT_TAR;
				a->archive.archive_format_name = "tar (non-POSIX)";
				err2 = header_old_tar(a, tar, entry, h);
			}
			err = err_combine(err, err2);
			/* We return warnings or success as-is.  Anything else is fatal. */
			if (err < ARCHIVE_WARN) {
				return (ARCHIVE_FATAL);
			}
			/* Filename of the form `._filename` is an AppleDouble
			 * extension entry.  The body is the macOS metadata blob;
			 * this is followed by another entry with the actual
			 * regular file data.
			 * This design has two drawbacks:
			 * = it's brittle; you might just have a file with such a name
			 * = it duplicates any long pathname extensions
			 *
			 * TODO: This probably shouldn't be here at all.  Consider
			 * just returning the contents as a regular entry here and
			 * then dealing with it when we write data to disk.
			 */
			if (tar->process_mac_extensions
			    && ((seen_headers & seen_mac_metadata) == 0)
			    && is_mac_metadata_entry(entry)) {
				err2 = read_mac_metadata_blob(a, entry, unconsumed);
				if (err2 < ARCHIVE_WARN) {
					return (ARCHIVE_FATAL);
				}
				err = err_combine(err, err2);
				/* Note: Other headers can appear again. */
				seen_headers = seen_mac_metadata;
				tar_reset_header_state(tar);
				break;
			}

			/* Reconcile GNU sparse attributes */
			if (tar->sparse_gnu_attributes_seen) {
				/* Only 'S' (GNU sparse) and ustar '0' regular files can be sparse */
				if (tar->filetype != 'S' && tar->filetype != '0') {
					archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
							  "Non-regular file cannot be sparse");
					return (ARCHIVE_WARN);
				} else if (tar->sparse_gnu_major == 0 &&
				    tar->sparse_gnu_minor == 0) {
					/* Sparse map already parsed from 'x' header */
				} else if (tar->sparse_gnu_major == 0 &&
				    tar->sparse_gnu_minor == 1) {
					/* Sparse map already parsed from 'x' header */
				} else if (tar->sparse_gnu_major == 1 &&
				    tar->sparse_gnu_minor == 0) {
					/* Sparse map is prepended to file contents */
					ssize_t bytes_read;
					bytes_read = gnu_sparse_10_read(a, tar, unconsumed);
					if (bytes_read < 0)
						return ((int)bytes_read);
					tar->entry_bytes_remaining -= bytes_read;
				} else {
					archive_set_error(&a->archive,
							  ARCHIVE_ERRNO_MISC,
							  "Unrecognized GNU sparse file format");
					return (ARCHIVE_WARN);
				}
			}
			return (err);
		}

		/* We're between headers ... */
		err = err_combine(err, err2);
		if (err == ARCHIVE_FATAL)
			return (err);

		/* The GNU volume header and the pax `g` global header
		 * are both allowed to be the only header in an
		 * archive.  If we've seen any other header, a
		 * following EOF is fatal. */
		if ((seen_headers & ~seen_V_header & ~seen_g_header) != 0) {
			eof_fatal = 1;
		}
	}
}

/*
 * Return true if block checksum is correct.
 */
static int
checksum(struct archive_read *a, const void *h)
{
	const unsigned char *bytes;
	const struct archive_entry_header_ustar	*header;
	int check, sum;
	size_t i;

	(void)a; /* UNUSED */
	bytes = (const unsigned char *)h;
	header = (const struct archive_entry_header_ustar *)h;

	/* Checksum field must hold an octal number */
	for (i = 0; i < sizeof(header->checksum); ++i) {
		char c = header->checksum[i];
		if (c != ' ' && c != '\0' && (c < '0' || c > '7'))
			return 0;
	}

	/*
	 * Test the checksum.  Note that POSIX specifies _unsigned_
	 * bytes for this calculation.
	 */
	sum = (int)tar_atol(header->checksum, sizeof(header->checksum));
	check = 0;
	for (i = 0; i < 148; i++)
		check += (unsigned char)bytes[i];
	for (; i < 156; i++)
		check += 32;
	for (; i < 512; i++)
		check += (unsigned char)bytes[i];
	if (sum == check)
		return (1);

	/*
	 * Repeat test with _signed_ bytes, just in case this archive
	 * was created by an old BSD, Solaris, or HP-UX tar with a
	 * broken checksum calculation.
	 */
	check = 0;
	for (i = 0; i < 148; i++)
		check += (signed char)bytes[i];
	for (; i < 156; i++)
		check += 32;
	for (; i < 512; i++)
		check += (signed char)bytes[i];
	if (sum == check)
		return (1);

#if DONT_FAIL_ON_CRC_ERROR
	/* Speed up fuzzing by pretending the checksum is always right. */
	return (1);
#else
	return (0);
#endif
}

/*
 * Return true if this block contains only nulls.
 */
static int
archive_block_is_null(const char *p)
{
	unsigned i;

	for (i = 0; i < 512; i++)
		if (*p++)
			return (0);
	return (1);
}

/*
 * Interpret 'A' Solaris ACL header
 */
static int
header_Solaris_ACL(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h, int64_t *unconsumed)
{
	const struct archive_entry_header_ustar *header;
	struct archive_string	 acl_text;
	size_t size;
	int err, acl_type;
	uint64_t type;
	char *acl, *p;

	header = (const struct archive_entry_header_ustar *)h;
	size = (size_t)tar_atol(header->size, sizeof(header->size));
	archive_string_init(&acl_text);
	err = read_body_to_string(a, tar, &acl_text, h, unconsumed);
	if (err != ARCHIVE_OK) {
		archive_string_free(&acl_text);
		return (err);
	}

	/* TODO: Examine the first characters to see if this
	 * is an AIX ACL descriptor.  We'll likely never support
	 * them, but it would be polite to recognize and warn when
	 * we do see them. */

	/* Leading octal number indicates ACL type and number of entries. */
	p = acl = acl_text.s;
	type = 0;
	while (*p != '\0' && p < acl + size) {
		if (*p < '0' || *p > '7') {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Malformed Solaris ACL attribute (invalid digit)");
			archive_string_free(&acl_text);
			return(ARCHIVE_WARN);
		}
		type <<= 3;
		type += *p - '0';
		if (type > 077777777) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Malformed Solaris ACL attribute (count too large)");
			archive_string_free(&acl_text);
			return (ARCHIVE_WARN);
		}
		p++;
	}
	switch (type & ~0777777) {
	case 01000000:
		/* POSIX.1e ACL */
		acl_type = ARCHIVE_ENTRY_ACL_TYPE_ACCESS;
		break;
	case 03000000:
		/* NFSv4 ACL */
		acl_type = ARCHIVE_ENTRY_ACL_TYPE_NFS4;
		break;
	default:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Malformed Solaris ACL attribute (unsupported type %llu)",
		    (unsigned long long)type);
		archive_string_free(&acl_text);
		return (ARCHIVE_WARN);
	}
	p++;

	if (p >= acl + size) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Malformed Solaris ACL attribute (body overflow)");
		archive_string_free(&acl_text);
		return(ARCHIVE_WARN);
	}

	/* ACL text is null-terminated; find the end. */
	size -= (p - acl);
	acl = p;

	while (*p != '\0' && p < acl + size)
		p++;

	if (tar->sconv_acl == NULL) {
		tar->sconv_acl = archive_string_conversion_from_charset(
		    &(a->archive), "UTF-8", 1);
		if (tar->sconv_acl == NULL) {
			archive_string_free(&acl_text);
			return (ARCHIVE_FATAL);
		}
	}
	archive_strncpy(&(tar->localname), acl, p - acl);
	err = archive_acl_from_text_l(archive_entry_acl(entry),
	    tar->localname.s, acl_type, tar->sconv_acl);
	/* Workaround: Force perm_is_set() to be correct */
	/* If this bit were stored in the ACL, this wouldn't be needed */
	archive_entry_set_perm(entry, archive_entry_perm(entry));
	if (err != ARCHIVE_OK) {
		if (errno == ENOMEM) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate memory for ACL");
		} else
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Malformed Solaris ACL attribute (unparsable)");
	}
	archive_string_free(&acl_text);
	return (err);
}

/*
 * Interpret 'K' long linkname header.
 */
static int
header_gnu_longlink(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h, int64_t *unconsumed)
{
	int err;

	struct archive_string linkpath;
	archive_string_init(&linkpath);
	err = read_body_to_string(a, tar, &linkpath, h, unconsumed);
	if (err == ARCHIVE_OK) {
		archive_entry_set_link(entry, linkpath.s);
	}
	archive_string_free(&linkpath);
	return (err);
}

static int
set_conversion_failed_error(struct archive_read *a,
    struct archive_string_conv *sconv, const char *name)
{
	if (errno == ENOMEM) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate memory for %s", name);
		return (ARCHIVE_FATAL);
	}
	archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
	    "%s can't be converted from %s to current locale",
	    name, archive_string_conversion_charset_name(sconv));
	return (ARCHIVE_WARN);
}

/*
 * Interpret 'L' long filename header.
 */
static int
header_gnu_longname(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h, int64_t *unconsumed)
{
	int err;
	struct archive_string longname;

	archive_string_init(&longname);
	err = read_body_to_string(a, tar, &longname, h, unconsumed);
	if (err == ARCHIVE_OK) {
		if (archive_entry_copy_pathname_l(entry, longname.s,
		    archive_strlen(&longname), tar->sconv) != 0)
			err = set_conversion_failed_error(a, tar->sconv, "Pathname");
	}
	archive_string_free(&longname);
	return (err);
}

/*
 * Interpret 'V' GNU tar volume header.
 */
static int
header_volume(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h, int64_t *unconsumed)
{
	const struct archive_entry_header_ustar *header;
	int64_t size, to_consume;

	(void)a; /* UNUSED */
	(void)tar; /* UNUSED */
	(void)entry; /* UNUSED */

	header = (const struct archive_entry_header_ustar *)h;
	size = tar_atol(header->size, sizeof(header->size));
	if (size < 0 || size > (int64_t)pathname_limit) {
		return (ARCHIVE_FATAL);
	}
	to_consume = ((size + 511) & ~511);
	*unconsumed += to_consume;
	return (ARCHIVE_OK);
}

/*
 * Read the next `size` bytes into the provided string.
 * Null-terminate the string.
 */
static int
read_bytes_to_string(struct archive_read *a,
		     struct archive_string *as, size_t size,
		     int64_t *unconsumed) {
	const void *src;

	/* Fail if we can't make our buffer big enough. */
	if (archive_string_ensure(as, size + 1) == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "No memory");
		return (ARCHIVE_FATAL);
	}

	if (tar_flush_unconsumed(a, unconsumed) != ARCHIVE_OK) {
		return (ARCHIVE_FATAL);
	}

	/* Read the body into the string. */
	src = __archive_read_ahead(a, size, NULL);
	if (src == NULL) {
		archive_set_error(&a->archive, EINVAL,
		    "Truncated archive"
		    " detected while reading metadata");
		*unconsumed = 0;
		return (ARCHIVE_FATAL);
	}
	memcpy(as->s, src, size);
	as->s[size] = '\0';
	as->length = size;
	*unconsumed += size;
	return (ARCHIVE_OK);
}

/*
 * Read body of an archive entry into an archive_string object.
 */
static int
read_body_to_string(struct archive_read *a, struct tar *tar,
    struct archive_string *as, const void *h, int64_t *unconsumed)
{
	int64_t size;
	const struct archive_entry_header_ustar *header;
	int r;

	(void)tar; /* UNUSED */
	header = (const struct archive_entry_header_ustar *)h;
	size  = tar_atol(header->size, sizeof(header->size));
	if (size < 0 || size > entry_limit) {
		archive_set_error(&a->archive, EINVAL,
		    "Special header has invalid size: %lld",
		    (long long)size);
		return (ARCHIVE_FATAL);
	}
	if (size > (int64_t)pathname_limit) {
		archive_string_empty(as);
		int64_t to_consume = ((size + 511) & ~511);
		if (to_consume != __archive_read_consume(a, to_consume)) {
			return (ARCHIVE_FATAL);
		}
		archive_set_error(&a->archive, EINVAL,
		    "Special header too large: %lld > 1MiB",
		    (long long)size);
		return (ARCHIVE_WARN);
	}
	r = read_bytes_to_string(a, as, size, unconsumed);
	*unconsumed += 0x1ff & (-size);
	return(r);
}

/*
 * Parse out common header elements.
 *
 * This would be the same as header_old_tar, except that the
 * filename is handled slightly differently for old and POSIX
 * entries  (POSIX entries support a 'prefix').  This factoring
 * allows header_old_tar and header_ustar
 * to handle filenames differently, while still putting most of the
 * common parsing into one place.
 *
 * This is called _after_ ustar, GNU tar, Schily, etc, special
 * fields have already been parsed into the `tar` structure.
 * So we can make final decisions here about how to reconcile
 * size, mode, etc, information.
 */
static int
header_common(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h)
{
	const struct archive_entry_header_ustar	*header;
	const char *existing_linkpath;
	const wchar_t *existing_wcs_linkpath;
	int     err = ARCHIVE_OK;

	header = (const struct archive_entry_header_ustar *)h;

	/* Parse out the numeric fields (all are octal) */

	/* Split mode handling: Set filetype always, perm only if not already set */
	archive_entry_set_filetype(entry,
	    (mode_t)tar_atol(header->mode, sizeof(header->mode)));
	if (!archive_entry_perm_is_set(entry)) {
		archive_entry_set_perm(entry,
			(mode_t)tar_atol(header->mode, sizeof(header->mode)));
	}

	/* Set uid, gid, mtime if not already set */
	if (!archive_entry_uid_is_set(entry)) {
		archive_entry_set_uid(entry, tar_atol(header->uid, sizeof(header->uid)));
	}
	if (!archive_entry_gid_is_set(entry)) {
		archive_entry_set_gid(entry, tar_atol(header->gid, sizeof(header->gid)));
	}
	if (!archive_entry_mtime_is_set(entry)) {
		archive_entry_set_mtime(entry, tar_atol(header->mtime, sizeof(header->mtime)), 0);
	}

	/* Reconcile the size info. */
	/* First, how big is the file on disk? */
	if ((tar->size_fields & TAR_SIZE_GNU_SPARSE_REALSIZE) != 0) {
		/* GNU sparse format 1.0 uses `GNU.sparse.realsize`
		 * to hold the size of the file on disk. */
		tar->disk_size = tar->GNU_sparse_realsize;
	} else if ((tar->size_fields & TAR_SIZE_GNU_SPARSE_SIZE) != 0
		   && (tar->sparse_gnu_major == 0)) {
		/* GNU sparse format 0.0 and 0.1 use `GNU.sparse.size`
		 * to hold the size of the file on disk. */
		tar->disk_size = tar->GNU_sparse_size;
	} else if ((tar->size_fields & TAR_SIZE_SCHILY_SPARSE_REALSIZE) != 0) {
		tar->disk_size = tar->SCHILY_sparse_realsize;
	} else if ((tar->size_fields & TAR_SIZE_PAX_SIZE) != 0) {
		tar->disk_size = tar->pax_size;
	} else {
		/* There wasn't a suitable pax header, so use the ustar info */
		tar->disk_size = tar_atol(header->size, sizeof(header->size));
	}

	if (tar->disk_size < 0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
				  "Tar entry has negative file size");
		return (ARCHIVE_FATAL);
	} else if (tar->disk_size > entry_limit) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
				  "Tar entry size overflow");
		return (ARCHIVE_FATAL);
	} else {
		archive_entry_set_size(entry, tar->disk_size);
	}

	/* Second, how big is the data in the archive? */
	if ((tar->size_fields & TAR_SIZE_GNU_SPARSE_SIZE) != 0
	    && (tar->sparse_gnu_major == 1)) {
		/* GNU sparse format 1.0 uses `GNU.sparse.size`
		 * to hold the size of the data in the archive. */
		tar->entry_bytes_remaining = tar->GNU_sparse_size;
	} else if ((tar->size_fields & TAR_SIZE_PAX_SIZE) != 0) {
		tar->entry_bytes_remaining = tar->pax_size;
	} else {
		tar->entry_bytes_remaining
			= tar_atol(header->size, sizeof(header->size));
	}
	if (tar->entry_bytes_remaining < 0) {
		tar->entry_bytes_remaining = 0;
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
				  "Tar entry has negative size");
		return (ARCHIVE_FATAL);
	} else if (tar->entry_bytes_remaining > entry_limit) {
		tar->entry_bytes_remaining = 0;
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
				  "Tar entry size overflow");
		return (ARCHIVE_FATAL);
	}

	/* Handle the tar type flag appropriately. */
	tar->filetype = header->typeflag[0];

	/*
	 * TODO: If the linkpath came from Pax extension header, then
	 * we should obey the hdrcharset_utf8 flag when converting these.
	 */
	switch (tar->filetype) {
	case '1': /* Hard link */
		archive_entry_set_link_to_hardlink(entry);
		existing_wcs_linkpath = archive_entry_hardlink_w(entry);
		existing_linkpath = archive_entry_hardlink(entry);
		if ((existing_linkpath == NULL || existing_linkpath[0] == '\0')
		    && (existing_wcs_linkpath == NULL || existing_wcs_linkpath[0] == '\0')) {
			struct archive_string linkpath;
			archive_string_init(&linkpath);
			archive_strncpy(&linkpath,
					header->linkname, sizeof(header->linkname));
			if (archive_entry_copy_hardlink_l(entry, linkpath.s,
							  archive_strlen(&linkpath), tar->sconv) != 0) {
				err = set_conversion_failed_error(a, tar->sconv,
								  "Linkname");
				if (err == ARCHIVE_FATAL) {
					archive_string_free(&linkpath);
					return (err);
				}
			}
			archive_string_free(&linkpath);
		}
		/*
		 * The following may seem odd, but: Technically, tar
		 * does not store the file type for a "hard link"
		 * entry, only the fact that it is a hard link.  So, I
		 * leave the type zero normally.  But, pax interchange
		 * format allows hard links to have data, which
		 * implies that the underlying entry is a regular
		 * file.
		 */
		if (archive_entry_size(entry) > 0)
			archive_entry_set_filetype(entry, AE_IFREG);

		/*
		 * A tricky point: Traditionally, tar readers have
		 * ignored the size field when reading hardlink
		 * entries, and some writers put non-zero sizes even
		 * though the body is empty.  POSIX blessed this
		 * convention in the 1988 standard, but broke with
		 * this tradition in 2001 by permitting hardlink
		 * entries to store valid bodies in pax interchange
		 * format, but not in ustar format.  Since there is no
		 * hard and fast way to distinguish pax interchange
		 * from earlier archives (the 'x' and 'g' entries are
		 * optional, after all), we need a heuristic.
		 */
		if (archive_entry_size(entry) == 0) {
			/* If the size is already zero, we're done. */
		}  else if (a->archive.archive_format
		    == ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE) {
			/* Definitely pax extended; must obey hardlink size. */
		} else if (a->archive.archive_format == ARCHIVE_FORMAT_TAR
		    || a->archive.archive_format == ARCHIVE_FORMAT_TAR_GNUTAR)
		{
			/* Old-style or GNU tar: we must ignore the size. */
			archive_entry_set_size(entry, 0);
			tar->entry_bytes_remaining = 0;
		} else if (archive_read_format_tar_bid(a, 50) > 50) {
			/*
			 * We don't know if it's pax: If the bid
			 * function sees a valid ustar header
			 * immediately following, then let's ignore
			 * the hardlink size.
			 */
			archive_entry_set_size(entry, 0);
			tar->entry_bytes_remaining = 0;
		}
		/*
		 * TODO: There are still two cases I'd like to handle:
		 *   = a ustar non-pax archive with a hardlink entry at
		 *     end-of-archive.  (Look for block of nulls following?)
		 *   = a pax archive that has not seen any pax headers
		 *     and has an entry which is a hardlink entry storing
		 *     a body containing an uncompressed tar archive.
		 * The first is worth addressing; I don't see any reliable
		 * way to deal with the second possibility.
		 */
		break;
	case '2': /* Symlink */
		archive_entry_set_link_to_symlink(entry);
		existing_wcs_linkpath = archive_entry_symlink_w(entry);
		existing_linkpath = archive_entry_symlink(entry);
		if ((existing_linkpath == NULL || existing_linkpath[0] == '\0')
		    && (existing_wcs_linkpath == NULL || existing_wcs_linkpath[0] == '\0')) {
			struct archive_string linkpath;
			archive_string_init(&linkpath);
			archive_strncpy(&linkpath,
					header->linkname, sizeof(header->linkname));
			if (archive_entry_copy_symlink_l(entry, linkpath.s,
			    archive_strlen(&linkpath), tar->sconv) != 0) {
				err = set_conversion_failed_error(a, tar->sconv,
				    "Linkname");
				if (err == ARCHIVE_FATAL) {
					archive_string_free(&linkpath);
					return (err);
				}
			}
			archive_string_free(&linkpath);
		}
		archive_entry_set_filetype(entry, AE_IFLNK);
		archive_entry_set_size(entry, 0);
		tar->entry_bytes_remaining = 0;
		break;
	case '3': /* Character device */
		archive_entry_set_filetype(entry, AE_IFCHR);
		archive_entry_set_size(entry, 0);
		tar->entry_bytes_remaining = 0;
		break;
	case '4': /* Block device */
		archive_entry_set_filetype(entry, AE_IFBLK);
		archive_entry_set_size(entry, 0);
		tar->entry_bytes_remaining = 0;
		break;
	case '5': /* Dir */
		archive_entry_set_filetype(entry, AE_IFDIR);
		archive_entry_set_size(entry, 0);
		tar->entry_bytes_remaining = 0;
		break;
	case '6': /* FIFO device */
		archive_entry_set_filetype(entry, AE_IFIFO);
		archive_entry_set_size(entry, 0);
		tar->entry_bytes_remaining = 0;
		break;
	case 'D': /* GNU incremental directory type */
		/*
		 * No special handling is actually required here.
		 * It might be nice someday to preprocess the file list and
		 * provide it to the client, though.
		 */
		archive_entry_set_filetype(entry, AE_IFDIR);
		break;
	case 'M': /* GNU "Multi-volume" (remainder of file from last archive)*/
		/*
		 * As far as I can tell, this is just like a regular file
		 * entry, except that the contents should be _appended_ to
		 * the indicated file at the indicated offset.  This may
		 * require some API work to fully support.
		 */
		break;
	case 'N': /* Old GNU "long filename" entry. */
		/* The body of this entry is a script for renaming
		 * previously-extracted entries.  Ugh.  It will never
		 * be supported by libarchive. */
		archive_entry_set_filetype(entry, AE_IFREG);
		break;
	case 'S': /* GNU sparse files */
		/*
		 * Sparse files are really just regular files with
		 * sparse information in the extended area.
		 */
		/* FALLTHROUGH */
	case '0': /* ustar "regular" file */
		/* FALLTHROUGH */
	default: /* Non-standard file types */
		/*
		 * Per POSIX: non-recognized types should always be
		 * treated as regular files.
		 */
		archive_entry_set_filetype(entry, AE_IFREG);
		break;
	}
	return (err);
}

/*
 * Parse out header elements for "old-style" tar archives.
 */
static int
header_old_tar(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h)
{
	const struct archive_entry_header_ustar	*header;
	int err = ARCHIVE_OK, err2;

	/*
	 * Copy filename over (to ensure null termination).
	 * Skip if pathname was already set e.g. by header_gnu_longname()
	 */
	header = (const struct archive_entry_header_ustar *)h;

	const char *existing_pathname = archive_entry_pathname(entry);
	const wchar_t *existing_wcs_pathname = archive_entry_pathname_w(entry);
	if ((existing_pathname == NULL || existing_pathname[0] == '\0')
	    && (existing_wcs_pathname == NULL || existing_wcs_pathname[0] == '\0') &&
	    archive_entry_copy_pathname_l(entry,
	    header->name, sizeof(header->name), tar->sconv) != 0) {
		err = set_conversion_failed_error(a, tar->sconv, "Pathname");
		if (err == ARCHIVE_FATAL)
			return (err);
	}

	/* Grab rest of common fields */
	err2 = header_common(a, tar, entry, h);
	if (err > err2)
		err = err2;

	tar->entry_padding = 0x1ff & (-tar->entry_bytes_remaining);
	return (err);
}

/*
 * Is this likely an AppleDouble extension?
 */
static int
is_mac_metadata_entry(struct archive_entry *entry) {
	const char *p, *name;
	const wchar_t *wp, *wname;

	wname = wp = archive_entry_pathname_w(entry);
	if (wp != NULL) {
		/* Find the last path element. */
		for (; *wp != L'\0'; ++wp) {
			if (wp[0] == '/' && wp[1] != L'\0')
				wname = wp + 1;
		}
		/*
		 * If last path element starts with "._", then
		 * this is a Mac extension.
		 */
		if (wname[0] == L'.' && wname[1] == L'_' && wname[2] != L'\0')
			return 1;
	} else {
		/* Find the last path element. */
		name = p = archive_entry_pathname(entry);
		if (p == NULL)
			return (ARCHIVE_FAILED);
		for (; *p != '\0'; ++p) {
			if (p[0] == '/' && p[1] != '\0')
				name = p + 1;
		}
		/*
		 * If last path element starts with "._", then
		 * this is a Mac extension.
		 */
		if (name[0] == '.' && name[1] == '_' && name[2] != '\0')
			return 1;
	}
	/* Not a mac extension */
	return 0;
}

/*
 * Read a Mac AppleDouble-encoded blob of file metadata,
 * if there is one.
 *
 * TODO: In Libarchive 4, we should consider ripping this
 * out -- instead, return a file starting with `._` as
 * a regular file and let the client (or archive_write logic)
 * handle it.
 */
static int
read_mac_metadata_blob(struct archive_read *a,
    struct archive_entry *entry, int64_t *unconsumed)
{
	int64_t size;
	size_t msize;
	const void *data;

 	/* Read the body as a Mac OS metadata blob. */
	size = archive_entry_size(entry);
	msize = (size_t)size;
	if (size < 0 || (uintmax_t)msize != (uintmax_t)size) {
		*unconsumed = 0;
		return (ARCHIVE_FATAL);
	}

	/* TODO: Should this merely skip the overlarge entry and
	 * WARN?  Or is xattr_limit sufficiently large that we can
	 * safely assume anything larger is malicious? */
	if (size > (int64_t)xattr_limit) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Oversized AppleDouble extension has size %llu > %llu",
		    (unsigned long long)size,
		    (unsigned long long)xattr_limit);
		return (ARCHIVE_FATAL);
	}

	/*
	 * TODO: Look beyond the body here to peek at the next header.
	 * If it's a regular header (not an extension header)
	 * that has the wrong name, just return the current
	 * entry as-is, without consuming the body here.
	 * That would reduce the risk of us mis-identifying
	 * an ordinary file that just happened to have
	 * a name starting with "._".
	 *
	 * Q: Is the above idea really possible?  Even
	 * when there are GNU or pax extension entries?
	 */
	if (tar_flush_unconsumed(a, unconsumed) != ARCHIVE_OK) {
		return (ARCHIVE_FATAL);
	}
	data = __archive_read_ahead(a, msize, NULL);
	if (data == NULL) {
		archive_set_error(&a->archive, EINVAL,
		    "Truncated archive"
		    " detected while reading macOS metadata");
		*unconsumed = 0;
		return (ARCHIVE_FATAL);
	}
	archive_entry_clear(entry);
	archive_entry_copy_mac_metadata(entry, data, msize);
	*unconsumed = (msize + 511) & ~ 511;
	return (ARCHIVE_OK);
}

/*
 * Parse a file header for a pax extended archive entry.
 */
static int
header_pax_global(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h, int64_t *unconsumed)
{
	const struct archive_entry_header_ustar *header;
	int64_t size, to_consume;

	(void)a; /* UNUSED */
	(void)tar; /* UNUSED */
	(void)entry; /* UNUSED */

	header = (const struct archive_entry_header_ustar *)h;
	size = tar_atol(header->size, sizeof(header->size));
	if (size < 0 || size > entry_limit) {
		archive_set_error(&a->archive, EINVAL,
		    "Special header has invalid size: %lld",
		    (long long)size);
		return (ARCHIVE_FATAL);
	}
	to_consume = ((size + 511) & ~511);
	*unconsumed += to_consume;
	return (ARCHIVE_OK);
}

/*
 * Parse a file header for a Posix "ustar" archive entry.  This also
 * handles "pax" or "extended ustar" entries.
 *
 * In order to correctly handle pax attributes (which precede this),
 * we have to skip parsing any field for which the entry already has
 * contents.
 */
static int
header_ustar(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h)
{
	const struct archive_entry_header_ustar	*header;
	struct archive_string as;
	int err = ARCHIVE_OK, r;

	header = (const struct archive_entry_header_ustar *)h;

	/* Copy name into an internal buffer to ensure null-termination. */
	const char *existing_pathname = archive_entry_pathname(entry);
	const wchar_t *existing_wcs_pathname = archive_entry_pathname_w(entry);
	if ((existing_pathname == NULL || existing_pathname[0] == '\0')
	    && (existing_wcs_pathname == NULL || existing_wcs_pathname[0] == '\0')) {
		archive_string_init(&as);
		if (header->prefix[0]) {
			archive_strncpy(&as, header->prefix, sizeof(header->prefix));
			if (as.s[archive_strlen(&as) - 1] != '/')
				archive_strappend_char(&as, '/');
			archive_strncat(&as, header->name, sizeof(header->name));
		} else {
			archive_strncpy(&as, header->name, sizeof(header->name));
		}
		if (archive_entry_copy_pathname_l(entry, as.s, archive_strlen(&as),
		    tar->sconv) != 0) {
			err = set_conversion_failed_error(a, tar->sconv, "Pathname");
			if (err == ARCHIVE_FATAL)
				return (err);
		}
		archive_string_free(&as);
	}

	/* Handle rest of common fields. */
	r = header_common(a, tar, entry, h);
	if (r == ARCHIVE_FATAL)
		return (r);
	if (r < err)
		err = r;

	/* Handle POSIX ustar fields. */
	const char *existing_uname = archive_entry_uname(entry);
	if (existing_uname == NULL || existing_uname[0] == '\0') {
		if (archive_entry_copy_uname_l(entry,
		    header->uname, sizeof(header->uname), tar->sconv) != 0) {
			err = set_conversion_failed_error(a, tar->sconv, "Uname");
			if (err == ARCHIVE_FATAL)
				return (err);
		}
	}

	const char *existing_gname = archive_entry_gname(entry);
	if (existing_gname == NULL || existing_gname[0] == '\0') {
		if (archive_entry_copy_gname_l(entry,
		    header->gname, sizeof(header->gname), tar->sconv) != 0) {
			err = set_conversion_failed_error(a, tar->sconv, "Gname");
			if (err == ARCHIVE_FATAL)
				return (err);
		}
	}

	/* Parse out device numbers only for char and block specials. */
	if (header->typeflag[0] == '3' || header->typeflag[0] == '4') {
		if (!archive_entry_rdev_is_set(entry)) {
			archive_entry_set_rdevmajor(entry, (dev_t)
			    tar_atol(header->rdevmajor, sizeof(header->rdevmajor)));
			archive_entry_set_rdevminor(entry, (dev_t)
			    tar_atol(header->rdevminor, sizeof(header->rdevminor)));
		}
	} else {
		archive_entry_set_rdev(entry, 0);
	}

	tar->entry_padding = 0x1ff & (-tar->entry_bytes_remaining);

	return (err);
}

static int
header_pax_extension(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h, int64_t *unconsumed)
{
	/* Sanity checks: The largest `x` body I've ever heard of was
	 * a little over 4MB.  So I doubt there has ever been a
	 * well-formed archive with an `x` body over 1GiB.  Similarly,
	 * it seems plausible that no single attribute has ever been
	 * larger than 100MB.  So if we see a larger value here, it's
	 * almost certainly a sign of a corrupted/malicious archive. */

	/* Maximum sane size for extension body: 1 GiB */
	/* This cannot be raised to larger than 8GiB without
	 * exceeding the maximum size for a standard ustar
	 * entry. */
	const int64_t ext_size_limit = 1024 * 1024 * (int64_t)1024;
	/* Maximum size for a single line/attr: 100 million characters */
	/* This cannot be raised to more than 2GiB without exceeding
	 * a `size_t` on 32-bit platforms. */
	const size_t max_parsed_line_length = 99999999ULL;
	/* Largest attribute prolog:  size + name. */
	const size_t max_size_name = 512;

	/* Size and padding of the full extension body */
	int64_t ext_size, ext_padding;
	size_t line_length, value_length, name_length;
	ssize_t to_read, did_read;
	const struct archive_entry_header_ustar *header;
	const char *p, *attr_start, *name_start;
	struct archive_string_conv *sconv;
	struct archive_string *pas = NULL;
	struct archive_string attr_name;
	int err = ARCHIVE_OK, r;

	header = (const struct archive_entry_header_ustar *)h;
	ext_size  = tar_atol(header->size, sizeof(header->size));
	if (ext_size > entry_limit) {
		return (ARCHIVE_FATAL);
	}
	if (ext_size < 0) {
	  archive_set_error(&a->archive, EINVAL,
			    "pax extension header has invalid size: %lld",
			    (long long)ext_size);
	  return (ARCHIVE_FATAL);
	}

	ext_padding = 0x1ff & (-ext_size);
	if (ext_size > ext_size_limit) {
		/* Consume the pax extension body and return an error */
		if (ext_size + ext_padding != __archive_read_consume(a, ext_size + ext_padding)) {
			return (ARCHIVE_FATAL);
		}
		archive_set_error(&a->archive, EINVAL,
		    "Ignoring oversized pax extensions: %lld > %lld",
		    (long long)ext_size, (long long)ext_size_limit);
		return (ARCHIVE_WARN);
	}
	if (tar_flush_unconsumed(a, unconsumed) != ARCHIVE_OK) {
		return (ARCHIVE_FATAL);
	}

	/* Parse the size/name of each pax attribute in the body */
	archive_string_init(&attr_name);
	while (ext_size > 0) {
		/* Read enough bytes to parse the size/name of the next attribute */
		to_read = max_size_name;
		if (to_read > ext_size) {
			to_read = ext_size;
		}
		p = __archive_read_ahead(a, to_read, &did_read);
		if (p == NULL) { /* EOF */
			archive_set_error(&a->archive, EINVAL,
					  "Truncated tar archive"
					  " detected while reading pax attribute name");
			return (ARCHIVE_FATAL);
		}
		if (did_read > ext_size) {
			did_read = ext_size;
		}

		/* Parse size of attribute */
		line_length = 0;
		attr_start = p;
		while (1) {
			if (p >= attr_start + did_read) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
						  "Ignoring malformed pax attributes: overlarge attribute size field");
				*unconsumed += ext_size + ext_padding;
				return (ARCHIVE_WARN);
			}
			if (*p == ' ') {
				p++;
				break;
			}
			if (*p < '0' || *p > '9') {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
						  "Ignoring malformed pax attributes: malformed attribute size field");
				*unconsumed += ext_size + ext_padding;
				return (ARCHIVE_WARN);
			}
			line_length *= 10;
			line_length += *p - '0';
			if (line_length > max_parsed_line_length) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
						  "Ignoring malformed pax attribute: size > %lld",
						  (long long)max_parsed_line_length);
				*unconsumed += ext_size + ext_padding;
				return (ARCHIVE_WARN);
			}
			p++;
		}

		if ((int64_t)line_length > ext_size) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
						  "Ignoring malformed pax attribute:  %lld > %lld",
						  (long long)line_length, (long long)ext_size);
				*unconsumed += ext_size + ext_padding;
				return (ARCHIVE_WARN);
		}

		/* Parse name of attribute */
		if (p >= attr_start + did_read
		    || p >= attr_start + line_length
		    || *p == '=') {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
					  "Ignoring malformed pax attributes: empty name found");
			*unconsumed += ext_size + ext_padding;
			return (ARCHIVE_WARN);
		}
		name_start = p;
		while (1) {
			if (p >= attr_start + did_read || p >= attr_start + line_length) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
						  "Ignoring malformed pax attributes: overlarge attribute name");
				*unconsumed += ext_size + ext_padding;
				return (ARCHIVE_WARN);
			}
			if (*p == '=') {
				break;
			}
			p++;
		}
		name_length = p - name_start;
		p++; // Skip '='

		// Save the name before we consume it
		archive_strncpy(&attr_name, name_start, name_length);

		ext_size -= p - attr_start;
		value_length = line_length - (p - attr_start);

		/* Consume size, name, and `=` */
		*unconsumed += p - attr_start;
		if (tar_flush_unconsumed(a, unconsumed) != ARCHIVE_OK) {
			return (ARCHIVE_FATAL);
		}

		if (value_length == 0) {
			archive_set_error(&a->archive, EINVAL,
					  "Malformed pax attributes");
			*unconsumed += ext_size + ext_padding;
			return (ARCHIVE_WARN);
		}

		/* pax_attribute will consume value_length - 1 */
		r = pax_attribute(a, tar, entry, attr_name.s, archive_strlen(&attr_name), value_length - 1, unconsumed);
		ext_size -= value_length - 1;

		// Release the allocated attr_name (either here or before every return in this function)
		archive_string_free(&attr_name);

		if (r < ARCHIVE_WARN) {
			*unconsumed += ext_size + ext_padding;
			return (r);
		}
		err = err_combine(err, r);

		/* Consume the `\n` that follows the pax attribute value. */
		if (tar_flush_unconsumed(a, unconsumed) != ARCHIVE_OK) {
			return (ARCHIVE_FATAL);
		}
		p = __archive_read_ahead(a, 1, &did_read);
		if (p == NULL) {
			archive_set_error(&a->archive, EINVAL,
					  "Truncated tar archive"
					  " detected while completing pax attribute");
			return (ARCHIVE_FATAL);
		}
		if (p[0] != '\n') {
			archive_set_error(&a->archive, EINVAL,
					  "Malformed pax attributes");
			*unconsumed += ext_size + ext_padding;
			return (ARCHIVE_WARN);
		}
		ext_size -= 1;
		*unconsumed += 1;
		if (tar_flush_unconsumed(a, unconsumed) != ARCHIVE_OK) {
			return (ARCHIVE_FATAL);
		}
	}
	*unconsumed += ext_size + ext_padding;

	/*
	 * Some PAX values -- pathname, linkpath, uname, gname --
	 * can't be copied into the entry until we know the character
	 * set to use:
	 */
	if (!tar->pax_hdrcharset_utf8)
		/* PAX specified "BINARY", so use the default charset */
		sconv = tar->opt_sconv;
	else {
		/* PAX default UTF-8 */
		sconv = archive_string_conversion_from_charset(
		    &(a->archive), "UTF-8", 1);
		if (sconv == NULL)
			return (ARCHIVE_FATAL);
		if (tar->compat_2x)
			archive_string_conversion_set_opt(sconv,
			    SCONV_SET_OPT_UTF8_LIBARCHIVE2X);
	}

	/* Pathname */
	pas = NULL;
	if (archive_strlen(&(tar->entry_pathname_override)) > 0) {
		/* Prefer GNU.sparse.name attribute if present */
		/* GNU sparse files store a fake name under the standard
		 * "pathname" key. */
		pas = &(tar->entry_pathname_override);
	} else if (archive_strlen(&(tar->entry_pathname)) > 0) {
		/* Use standard "pathname" PAX extension */
		pas = &(tar->entry_pathname);
	}
	if (pas != NULL) {
		if (archive_entry_copy_pathname_l(entry, pas->s,
		    archive_strlen(pas), sconv) != 0) {
			err = set_conversion_failed_error(a, sconv, "Pathname");
			if (err == ARCHIVE_FATAL)
				return (err);
			/* Use raw name without conversion */
			archive_entry_copy_pathname(entry, pas->s);
		}
	}
	/* Uname */
	if (archive_strlen(&(tar->entry_uname)) > 0) {
		if (archive_entry_copy_uname_l(entry, tar->entry_uname.s,
		    archive_strlen(&(tar->entry_uname)), sconv) != 0) {
			err = set_conversion_failed_error(a, sconv, "Uname");
			if (err == ARCHIVE_FATAL)
				return (err);
			/* Use raw name without conversion */
			archive_entry_copy_uname(entry, tar->entry_uname.s);
		}
	}
	/* Gname */
	if (archive_strlen(&(tar->entry_gname)) > 0) {
		if (archive_entry_copy_gname_l(entry, tar->entry_gname.s,
		    archive_strlen(&(tar->entry_gname)), sconv) != 0) {
			err = set_conversion_failed_error(a, sconv, "Gname");
			if (err == ARCHIVE_FATAL)
				return (err);
			/* Use raw name without conversion */
			archive_entry_copy_gname(entry, tar->entry_gname.s);
		}
	}
	/* Linkpath */
	if (archive_strlen(&(tar->entry_linkpath)) > 0) {
		if (archive_entry_copy_link_l(entry, tar->entry_linkpath.s,
		    archive_strlen(&(tar->entry_linkpath)), sconv) != 0) {
			err = set_conversion_failed_error(a, sconv, "Linkpath");
			if (err == ARCHIVE_FATAL)
				return (err);
			/* Use raw name without conversion */
			archive_entry_copy_link(entry, tar->entry_linkpath.s);
		}
	}

	/* Extension may have given us a corrected `entry_bytes_remaining` for
	 * the main entry; update the padding appropriately. */
	tar->entry_padding = 0x1ff & (-tar->entry_bytes_remaining);
	return (err);
}

static int
pax_attribute_LIBARCHIVE_xattr(struct archive_entry *entry,
	const char *name, size_t name_length, const char *value, size_t value_length)
{
	char *name_decoded;
	void *value_decoded;
	size_t value_len;

	if (name_length < 1)
		return 3;

	/* URL-decode name */
	name_decoded = url_decode(name, name_length);
	if (name_decoded == NULL)
		return 2;

	/* Base-64 decode value */
	value_decoded = base64_decode(value, value_length, &value_len);
	if (value_decoded == NULL) {
		free(name_decoded);
		return 1;
	}

	archive_entry_xattr_add_entry(entry, name_decoded,
		value_decoded, value_len);

	free(name_decoded);
	free(value_decoded);
	return 0;
}

static int
pax_attribute_SCHILY_xattr(struct archive_entry *entry,
	const char *name, size_t name_length, const char *value, size_t value_length)
{
	if (name_length < 1 || name_length > 128) {
		return 1;
	}

	char * null_terminated_name = malloc(name_length + 1);
	if (null_terminated_name != NULL) {
		memcpy(null_terminated_name, name, name_length);
		null_terminated_name[name_length] = '\0';
		archive_entry_xattr_add_entry(entry, null_terminated_name, value, value_length);
		free(null_terminated_name);
	}

	return 0;
}

static int
pax_attribute_RHT_security_selinux(struct archive_entry *entry,
	const char *value, size_t value_length)
{
	archive_entry_xattr_add_entry(entry, "security.selinux",
            value, value_length);

	return 0;
}

static int
pax_attribute_SCHILY_acl(struct archive_read *a, struct tar *tar,
	struct archive_entry *entry, size_t value_length, int type)
{
	int r;
	const char *p;
	const char* errstr;

	switch (type) {
	case ARCHIVE_ENTRY_ACL_TYPE_ACCESS:
		errstr = "SCHILY.acl.access";
		break;
	case ARCHIVE_ENTRY_ACL_TYPE_DEFAULT:
		errstr = "SCHILY.acl.default";
		break;
	case ARCHIVE_ENTRY_ACL_TYPE_NFS4:
		errstr = "SCHILY.acl.ace";
		break;
	default:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Unknown ACL type: %d", type);
		return(ARCHIVE_FATAL);
	}

	if (tar->sconv_acl == NULL) {
		tar->sconv_acl =
		    archive_string_conversion_from_charset(
			&(a->archive), "UTF-8", 1);
		if (tar->sconv_acl == NULL)
			return (ARCHIVE_FATAL);
	}

	if (value_length > acl_limit) {
		__archive_read_consume(a, value_length);
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
				  "Unreasonably large ACL: %llu > %llu",
				  (unsigned long long)value_length,
				  (unsigned long long)acl_limit);
		return (ARCHIVE_WARN);
	}

	p = __archive_read_ahead(a, value_length, NULL);
	if (p == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
				  "Truncated tar archive "
				  "detected while reading ACL data");
		return (ARCHIVE_FATAL);
	}

	r = archive_acl_from_text_nl(archive_entry_acl(entry), p, value_length,
	    type, tar->sconv_acl);
	__archive_read_consume(a, value_length);
	/* Workaround: Force perm_is_set() to be correct */
	/* If this bit were stored in the ACL, this wouldn't be needed */
	archive_entry_set_perm(entry, archive_entry_perm(entry));
	if (r != ARCHIVE_OK) {
		if (r == ARCHIVE_FATAL) {
			archive_set_error(&a->archive, ENOMEM,
			    "%s %s", "Can't allocate memory for",
			    errstr);
			return (r);
		}
		archive_set_error(&a->archive,
		    ARCHIVE_ERRNO_MISC, "%s %s", "Parse error:", errstr);
	}
	return (r);
}

static int
pax_attribute_read_time(struct archive_read *a, size_t value_length, int64_t *ps, long *pn, int64_t *unconsumed) {
	struct archive_string as;
	int r;

	if (value_length > 128) {
		__archive_read_consume(a, value_length);
		*ps = 0;
		*pn = 0;
		return (ARCHIVE_FATAL);
	}

	archive_string_init(&as);
	r = read_bytes_to_string(a, &as, value_length, unconsumed);
	if (r < ARCHIVE_OK) {
		archive_string_free(&as);
		*ps = 0;
		*pn = 0;
		return (r);
	}

	pax_time(as.s, archive_strlen(&as), ps, pn);
	archive_string_free(&as);
	if (*ps == INT64_MIN) {
		*ps = 0;
		*pn = 0;
		return (ARCHIVE_WARN);
	}
	return (ARCHIVE_OK);
}

static int
pax_attribute_read_number(struct archive_read *a, size_t value_length, int64_t *result) {
	struct archive_string as;
	int64_t unconsumed = 0;
	int r;

	if (value_length > 64) {
		__archive_read_consume(a, value_length);
		*result = 0;
		return (ARCHIVE_FATAL);
	}

	archive_string_init(&as);
	r = read_bytes_to_string(a, &as, value_length, &unconsumed);
	if (tar_flush_unconsumed(a, &unconsumed) != ARCHIVE_OK) {
		*result = 0;
		return (ARCHIVE_FATAL);
	}
	if (r < ARCHIVE_OK) {
		archive_string_free(&as);
		*result = 0;
		return (r);
	}

	*result = tar_atol10(as.s, archive_strlen(&as));
	archive_string_free(&as);
	if (*result < 0 || *result == INT64_MAX) {
		*result = INT64_MAX;
		return (ARCHIVE_WARN);
	}
	return (ARCHIVE_OK);
}

/*
 * Parse a single key=value attribute.
 *
 * POSIX reserves all-lowercase keywords.  Vendor-specific extensions
 * should always have keywords of the form "VENDOR.attribute" In
 * particular, it's quite feasible to support many different vendor
 * extensions here.  I'm using "LIBARCHIVE" for extensions unique to
 * this library.
 *
 * TODO: Investigate other vendor-specific extensions and see if
 * any of them look useful.
 */
static int
pax_attribute(struct archive_read *a, struct tar *tar, struct archive_entry *entry,
	      const char *key, size_t key_length, size_t value_length, int64_t *unconsumed)
{
	int64_t t;
	long n;
	const char *p;
	ssize_t bytes_read;
	int err = ARCHIVE_OK;

	switch (key[0]) {
	case 'G':
		/* GNU.* extensions */
		if (key_length > 4 && memcmp(key, "GNU.", 4) == 0) {
			key += 4;
			key_length -= 4;

			/* GNU.sparse marks the existence of GNU sparse information */
			if (key_length == 6 && memcmp(key, "sparse", 6) == 0) {
				tar->sparse_gnu_attributes_seen = 1;
			}

			/* GNU.sparse.* extensions */
			else if (key_length > 7 && memcmp(key, "sparse.", 7) == 0) {
				tar->sparse_gnu_attributes_seen = 1;
				key += 7;
				key_length -= 7;

				/* GNU "0.0" sparse pax format. */
				if (key_length == 9 && memcmp(key, "numblocks", 9) == 0) {
					/* GNU.sparse.numblocks */
					tar->sparse_offset = -1;
					tar->sparse_numbytes = -1;
					tar->sparse_gnu_major = 0;
					tar->sparse_gnu_minor = 0;
				}
				else if (key_length == 6 && memcmp(key, "offset", 6) == 0) {
					/* GNU.sparse.offset */
					if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK) {
						tar->sparse_offset = t;
						if (tar->sparse_numbytes != -1) {
							if (gnu_add_sparse_entry(a, tar,
									 tar->sparse_offset, tar->sparse_numbytes)
							    != ARCHIVE_OK)
								return (ARCHIVE_FATAL);
							tar->sparse_offset = -1;
							tar->sparse_numbytes = -1;
						}
					}
					return (err);
				}
				else if (key_length == 8 && memcmp(key, "numbytes", 8) == 0) {
					/* GNU.sparse.numbytes */
					if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK) {
						tar->sparse_numbytes = t;
						if (tar->sparse_offset != -1) {
							if (gnu_add_sparse_entry(a, tar,
									 tar->sparse_offset, tar->sparse_numbytes)
							    != ARCHIVE_OK)
								return (ARCHIVE_FATAL);
							tar->sparse_offset = -1;
							tar->sparse_numbytes = -1;
						}
					}
					return (err);
				}
				else if (key_length == 4 && memcmp(key, "size", 4) == 0) {
					/* GNU.sparse.size */
					/* This is either the size of stored entry OR the size of data on disk,
					 * depending on which GNU sparse format version is in use.
					 * Since pax attributes can be in any order, we may not actually
					 * know at this point how to interpret this. */
					if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK) {
						tar->GNU_sparse_size = t;
						tar->size_fields |= TAR_SIZE_GNU_SPARSE_SIZE;
					}
					return (err);
				}

				/* GNU "0.1" sparse pax format. */
				else if (key_length == 3 && memcmp(key, "map", 3) == 0) {
					/* GNU.sparse.map */
					tar->sparse_gnu_major = 0;
					tar->sparse_gnu_minor = 1;
					if (value_length > sparse_map_limit) {
						archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
								  "Unreasonably large sparse map: %llu > %llu",
								  (unsigned long long)value_length,
								  (unsigned long long)sparse_map_limit);
						err = ARCHIVE_FAILED;
					} else {
						p = __archive_read_ahead(a, value_length, &bytes_read);
						if (p == NULL) {
							archive_set_error(&a->archive, EINVAL,
									  "Truncated archive"
									  " detected while reading GNU sparse data");
							return (ARCHIVE_FATAL);
						}
						if (gnu_sparse_01_parse(a, tar, p, value_length) != ARCHIVE_OK) {
							err = ARCHIVE_WARN;
						}
					}
					__archive_read_consume(a, value_length);
					return (err);
				}

				/* GNU "1.0" sparse pax format */
				else if (key_length == 5 && memcmp(key, "major", 5) == 0) {
					/* GNU.sparse.major */
					if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK
					    && t >= 0
					    && t <= 10) {
						tar->sparse_gnu_major = (int)t;
					}
					return (err);
				}
				else if (key_length == 5 && memcmp(key, "minor", 5) == 0) {
					/* GNU.sparse.minor */
					if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK
					    && t >= 0
					    && t <= 10) {
						tar->sparse_gnu_minor = (int)t;
					}
					return (err);
				}
				else if (key_length == 4 && memcmp(key, "name", 4) == 0) {
					/* GNU.sparse.name */
					/*
					 * The real filename; when storing sparse
					 * files, GNU tar puts a synthesized name into
					 * the regular 'path' attribute in an attempt
					 * to limit confusion. ;-)
					 */
					if (value_length > pathname_limit) {
						*unconsumed += value_length;
						err = ARCHIVE_WARN;
					} else {
						err = read_bytes_to_string(a, &(tar->entry_pathname_override),
									   value_length, unconsumed);
					}
					return (err);
				}
				else if (key_length == 8 && memcmp(key, "realsize", 8) == 0) {
					/* GNU.sparse.realsize = size of file on disk */
					if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK) {
						tar->GNU_sparse_realsize = t;
						tar->size_fields |= TAR_SIZE_GNU_SPARSE_REALSIZE;
					}
					return (err);
				}
			}
		}
		break;
	case 'L':
		/* LIBARCHIVE extensions */
		if (key_length > 11 && memcmp(key, "LIBARCHIVE.", 11) == 0) {
			key_length -= 11;
			key += 11;

			/* TODO: Handle arbitrary extended attributes... */
			/*
			  if (strcmp(key, "LIBARCHIVE.xxxxxxx") == 0)
				  archive_entry_set_xxxxxx(entry, value);
			*/
			if (key_length == 12 && memcmp(key, "creationtime", 12) == 0) {
				/* LIBARCHIVE.creationtime */
				if ((err = pax_attribute_read_time(a, value_length, &t, &n, unconsumed)) == ARCHIVE_OK) {
					archive_entry_set_birthtime(entry, t, n);
				}
				return (err);
			}
			else if (key_length == 11 && memcmp(key, "symlinktype", 11) == 0) {
				/* LIBARCHIVE.symlinktype */
				if (value_length < 16) {
					p = __archive_read_ahead(a, value_length, &bytes_read);
					if (p == NULL) {
						archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
								  "Truncated tar archive "
								  "detected while reading `symlinktype` attribute");
						return (ARCHIVE_FATAL);
					}
					if (value_length == 4 && memcmp(p, "file", 4) == 0) {
						archive_entry_set_symlink_type(entry,
									       AE_SYMLINK_TYPE_FILE);
					} else if (value_length == 3 && memcmp(p, "dir", 3) == 0) {
							archive_entry_set_symlink_type(entry,
										       AE_SYMLINK_TYPE_DIRECTORY);
					} else {
						archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
								  "Unrecognized symlink type");
						err = ARCHIVE_WARN;
					}
				} else {
					archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
							  "symlink type is very long"
							  "(longest recognized value is 4 bytes, this is %llu)",
							  (unsigned long long)value_length);
					err = ARCHIVE_WARN;
				}
				__archive_read_consume(a, value_length);
				return (err);
			}
			else if (key_length > 6 && memcmp(key, "xattr.", 6) == 0) {
				key_length -= 6;
				key += 6;
				if (value_length > xattr_limit) {
					err = ARCHIVE_WARN;
				} else {
					p = __archive_read_ahead(a, value_length, &bytes_read);
					if (p == NULL) {
						archive_set_error(&a->archive, EINVAL,
								  "Truncated archive"
								  " detected while reading xattr information");
						return (ARCHIVE_FATAL);
					}
					if (pax_attribute_LIBARCHIVE_xattr(entry, key, key_length, p, value_length)) {
						/* TODO: Unable to parse xattr */
						err = ARCHIVE_WARN;
					}
				}
				__archive_read_consume(a, value_length);
				return (err);
			}
		}
		break;
	case 'R':
		/* GNU tar uses RHT.security header to store SELinux xattrs
		 * SCHILY.xattr.security.selinux == RHT.security.selinux */
		if (key_length == 20 && memcmp(key, "RHT.security.selinux", 20) == 0) {
			if (value_length > xattr_limit) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
						  "Ignoring unreasonably large security.selinux attribute:"
						  " %llu > %llu",
						  (unsigned long long)value_length,
						  (unsigned long long)xattr_limit);
				/* TODO: Should this be FAILED instead? */
				err = ARCHIVE_WARN;
			} else {
				p = __archive_read_ahead(a, value_length, &bytes_read);
				if (p == NULL) {
					archive_set_error(&a->archive, EINVAL,
							  "Truncated archive"
							  " detected while reading selinux data");
					return (ARCHIVE_FATAL);
				}
				if (pax_attribute_RHT_security_selinux(entry, p, value_length)) {
					/* TODO: Unable to parse xattr */
					err = ARCHIVE_WARN;
				}
			}
			__archive_read_consume(a, value_length);
			return (err);
		}
		break;
	case 'S':
		/* SCHILY.* extensions used by "star" archiver */
		if (key_length > 7 && memcmp(key, "SCHILY.", 7) == 0) {
			key_length -= 7;
			key += 7;

			if (key_length == 10 && memcmp(key, "acl.access", 10) == 0) {
				err = pax_attribute_SCHILY_acl(a, tar, entry, value_length,
						      ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
				// TODO: Mark mode as set
				return (err);
			}
			else if (key_length == 11 && memcmp(key, "acl.default", 11) == 0) {
				err = pax_attribute_SCHILY_acl(a, tar, entry, value_length,
						      ARCHIVE_ENTRY_ACL_TYPE_DEFAULT);
				return (err);
			}
			else if (key_length == 7 && memcmp(key, "acl.ace", 7) == 0) {
				err = pax_attribute_SCHILY_acl(a, tar, entry, value_length,
						      ARCHIVE_ENTRY_ACL_TYPE_NFS4);
				// TODO: Mark mode as set
				return (err);
			}
			else if (key_length == 8 && memcmp(key, "devmajor", 8) == 0) {
				if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK) {
					archive_entry_set_rdevmajor(entry, (dev_t)t);
				}
				return (err);
			}
			else if (key_length == 8 && memcmp(key, "devminor", 8) == 0) {
				if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK) {
					archive_entry_set_rdevminor(entry, (dev_t)t);
				}
				return (err);
			}
			else if (key_length == 6 && memcmp(key, "fflags", 6) == 0) {
				if (value_length < fflags_limit) {
					p = __archive_read_ahead(a, value_length, &bytes_read);
					if (p == NULL) {
						/* Truncated archive */
						archive_set_error(&a->archive, EINVAL,
								  "Truncated archive"
								  " detected while reading SCHILY.fflags");
						return (ARCHIVE_FATAL);
					}
					archive_entry_copy_fflags_text_len(entry, p, value_length);
					err = ARCHIVE_OK;
				} else {
					/* Overlong fflags field */
					err = ARCHIVE_WARN;
				}
				__archive_read_consume(a, value_length);
				return (err);
			}
			else if (key_length == 3 && memcmp(key, "dev", 3) == 0) {
				if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK) {
					archive_entry_set_dev(entry, (dev_t)t);
				}
				return (err);
			}
			else if (key_length == 3 && memcmp(key, "ino", 3) == 0) {
				if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK) {
					archive_entry_set_ino(entry, t);
				}
				return (err);
			}
			else if (key_length == 5 && memcmp(key, "nlink", 5) == 0) {
				if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK) {
					archive_entry_set_nlink(entry, (unsigned int)t);
				}
				return (err);
			}
			else if (key_length == 8 && memcmp(key, "realsize", 8) == 0) {
				if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK) {
					tar->SCHILY_sparse_realsize = t;
					tar->size_fields |= TAR_SIZE_SCHILY_SPARSE_REALSIZE;
				}
				return (err);
			}
			/* TODO: Is there a SCHILY.sparse.size similar to GNU.sparse.size ? */
			else if (key_length > 6 && memcmp(key, "xattr.", 6) == 0) {
				key_length -= 6;
				key += 6;
				if (value_length < xattr_limit) {
					p = __archive_read_ahead(a, value_length, &bytes_read);
					if (p == NULL) {
						archive_set_error(&a->archive, EINVAL,
								  "Truncated archive"
								  " detected while reading SCHILY.xattr");
						return (ARCHIVE_FATAL);
					}
					if (pax_attribute_SCHILY_xattr(entry, key, key_length, p, value_length)) {
						/* TODO: Unable to parse xattr */
						err = ARCHIVE_WARN;
					}
				} else {
					archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
							  "Unreasonably large xattr: %llu > %llu",
							  (unsigned long long)value_length,
							  (unsigned long long)xattr_limit);
					err = ARCHIVE_WARN;
				}
				__archive_read_consume(a, value_length);
				return (err);
			}
		}
		/* SUN.* extensions from Solaris tar */
		if (key_length > 4 && memcmp(key, "SUN.", 4) == 0) {
			key_length -= 4;
			key += 4;

			if (key_length == 9 && memcmp(key, "holesdata", 9) == 0) {
				/* SUN.holesdata */
				if (value_length < sparse_map_limit) {
					p = __archive_read_ahead(a, value_length, &bytes_read);
					if (p == NULL) {
						archive_set_error(&a->archive, EINVAL,
								  "Truncated archive"
								  " detected while reading SUN.holesdata");
						return (ARCHIVE_FATAL);
					}
					err = pax_attribute_SUN_holesdata(a, tar, entry, p, value_length);
					if (err < ARCHIVE_OK) {
						archive_set_error(&a->archive,
								  ARCHIVE_ERRNO_MISC,
								  "Parse error: SUN.holesdata");
					}
				} else {
					archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
							  "Unreasonably large sparse map: %llu > %llu",
							  (unsigned long long)value_length,
							  (unsigned long long)sparse_map_limit);
					err = ARCHIVE_FAILED;
				}
				__archive_read_consume(a, value_length);
				return (err);
			}
		}
		break;
	case 'a':
		if (key_length == 5 && memcmp(key, "atime", 5) == 0) {
			if ((err = pax_attribute_read_time(a, value_length, &t, &n, unconsumed)) == ARCHIVE_OK) {
				archive_entry_set_atime(entry, t, n);
			}
			return (err);
		}
		break;
	case 'c':
		if (key_length == 5 && memcmp(key, "ctime", 5) == 0) {
			if ((err = pax_attribute_read_time(a, value_length, &t, &n, unconsumed)) == ARCHIVE_OK) {
				archive_entry_set_ctime(entry, t, n);
			}
			return (err);
		} else if (key_length == 7 && memcmp(key, "charset", 7) == 0) {
			/* TODO: Publish charset information in entry. */
		} else if (key_length == 7 && memcmp(key, "comment", 7) == 0) {
			/* TODO: Publish comment in entry. */
		}
		break;
	case 'g':
		if (key_length == 3 && memcmp(key, "gid", 3) == 0) {
			if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK) {
				archive_entry_set_gid(entry, t);
			}
			return (err);
		} else if (key_length == 5 && memcmp(key, "gname", 5) == 0) {
			if (value_length > guname_limit) {
				*unconsumed += value_length;
				err = ARCHIVE_WARN;
			} else {
				err = read_bytes_to_string(a, &(tar->entry_gname), value_length, unconsumed);
			}
			return (err);
		}
		break;
	case 'h':
		if (key_length == 10 && memcmp(key, "hdrcharset", 10) == 0) {
			if (value_length < 64) {
				p = __archive_read_ahead(a, value_length, &bytes_read);
				if (p == NULL) {
					archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
							  "Truncated tar archive "
							  "detected while reading hdrcharset attribute");
					return (ARCHIVE_FATAL);
				}
				if (value_length == 6
				    && memcmp(p, "BINARY", 6) == 0) {
					/* Binary  mode. */
					tar->pax_hdrcharset_utf8 = 0;
					err = ARCHIVE_OK;
				} else if (value_length == 23
					   && memcmp(p, "ISO-IR 10646 2000 UTF-8", 23) == 0) {
					tar->pax_hdrcharset_utf8 = 1;
					err = ARCHIVE_OK;
				} else {
					/* TODO: Unrecognized character set */
					err  = ARCHIVE_WARN;
				}
			} else {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
						  "hdrcharset attribute is unreasonably large (%llu bytes)",
						  (unsigned long long)value_length);
				err = ARCHIVE_WARN;
			}
			__archive_read_consume(a, value_length);
			return (err);
		}
		break;
	case 'l':
		/* pax interchange doesn't distinguish hardlink vs. symlink. */
		if (key_length == 8 && memcmp(key, "linkpath", 8) == 0) {
			if (value_length > pathname_limit) {
				*unconsumed += value_length;
				err = ARCHIVE_WARN;
			} else {
				err = read_bytes_to_string(a, &tar->entry_linkpath, value_length, unconsumed);
			}
			return (err);
		}
		break;
	case 'm':
		if (key_length == 5 && memcmp(key, "mtime", 5) == 0) {
			if ((err = pax_attribute_read_time(a, value_length, &t, &n, unconsumed)) == ARCHIVE_OK) {
				archive_entry_set_mtime(entry, t, n);
			}
			return (err);
		}
		break;
	case 'p':
		if (key_length == 4 && memcmp(key, "path", 4) == 0) {
			if (value_length > pathname_limit) {
				*unconsumed += value_length;
				err = ARCHIVE_WARN;
			} else {
				err = read_bytes_to_string(a, &(tar->entry_pathname), value_length, unconsumed);
			}
			return (err);
		}
		break;
	case 'r':
		/* POSIX has reserved 'realtime.*' */
		break;
	case 's':
		/* POSIX has reserved 'security.*' */
		/* Someday: if (strcmp(key, "security.acl") == 0) { ... } */
		if (key_length == 4 && memcmp(key, "size", 4) == 0) {
			/* "size" is the size of the data in the entry. */
			if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK) {
				tar->pax_size = t;
				tar->size_fields |= TAR_SIZE_PAX_SIZE;
			}
			else if (t == INT64_MAX) {
				/* Note: pax_attr_read_number returns INT64_MAX on overflow or < 0 */
				tar->entry_bytes_remaining = 0;
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_MISC,
				    "Tar size attribute overflow");
				return (ARCHIVE_FATAL);
			}
			return (err);
		}
		break;
	case 'u':
		if (key_length == 3 && memcmp(key, "uid", 3) == 0) {
			if ((err = pax_attribute_read_number(a, value_length, &t)) == ARCHIVE_OK) {
				archive_entry_set_uid(entry, t);
			}
			return (err);
		} else if (key_length == 5 && memcmp(key, "uname", 5) == 0) {
			if (value_length > guname_limit) {
				*unconsumed += value_length;
				err = ARCHIVE_WARN;
			} else {
				err = read_bytes_to_string(a, &(tar->entry_uname), value_length, unconsumed);
			}
			return (err);
		}
		break;
	}

	/* Unrecognized key, just skip the entire value. */
	__archive_read_consume(a, value_length);
	return (err);
}



/*
 * Parse a decimal time value, which may include a fractional portion
 *
 * Sets ps to INT64_MIN on error.
 */
static void
pax_time(const char *p, size_t length, int64_t *ps, long *pn)
{
	char digit;
	int64_t	s;
	unsigned long l;
	int sign;
	int64_t limit, last_digit_limit;

	limit = INT64_MAX / 10;
	last_digit_limit = INT64_MAX % 10;

	if (length <= 0) {
		*ps = 0;
		*pn = 0;
		return;
	}
	s = 0;
	sign = 1;
	if (*p == '-') {
		sign = -1;
		p++;
		length--;
	}
	while (length > 0 && *p >= '0' && *p <= '9') {
		digit = *p - '0';
		if (s > limit ||
		    (s == limit && digit > last_digit_limit)) {
			*ps = INT64_MIN;
			*pn = 0;
			return;
		}
		s = (s * 10) + digit;
		++p;
		--length;
	}

	*ps = s * sign;

	/* Calculate nanoseconds. */
	*pn = 0;

	if (length <= 0 || *p != '.')
		return;

	l = 100000000UL;
	do {
		++p;
		--length;
		if (length > 0 && *p >= '0' && *p <= '9')
			*pn += (*p - '0') * l;
		else
			break;
	} while (l /= 10);
}

/*
 * Parse GNU tar header
 */
static int
header_gnutar(struct archive_read *a, struct tar *tar,
    struct archive_entry *entry, const void *h, int64_t *unconsumed)
{
	const struct archive_entry_header_gnutar *header;
	int64_t t;
	int err = ARCHIVE_OK;

	/*
	 * GNU header is like POSIX ustar, except 'prefix' is
	 * replaced with some other fields. This also means the
	 * filename is stored as in old-style archives.
	 */

	/* Copy filename over (to ensure null termination). */
	header = (const struct archive_entry_header_gnutar *)h;
	const char *existing_pathname = archive_entry_pathname(entry);
	const wchar_t *existing_wcs_pathname = archive_entry_pathname_w(entry);
	if ((existing_pathname == NULL || existing_pathname[0] == '\0')
	    && (existing_wcs_pathname == NULL || existing_wcs_pathname[0] == L'\0')) {
		if (archive_entry_copy_pathname_l(entry,
		    header->name, sizeof(header->name), tar->sconv) != 0) {
			err = set_conversion_failed_error(a, tar->sconv, "Pathname");
			if (err == ARCHIVE_FATAL)
				return (err);
		}
	}

	/* Fields common to ustar and GNU */
	/* XXX Can the following be factored out since it's common
	 * to ustar and gnu tar?  Is it okay to move it down into
	 * header_common, perhaps?  */
	const char *existing_uname = archive_entry_uname(entry);
	if (existing_uname == NULL || existing_uname[0] == '\0') {
		if (archive_entry_copy_uname_l(entry,
		    header->uname, sizeof(header->uname), tar->sconv) != 0) {
			err = set_conversion_failed_error(a, tar->sconv, "Uname");
			if (err == ARCHIVE_FATAL)
				return (err);
		}
	}

	const char *existing_gname = archive_entry_gname(entry);
	if (existing_gname == NULL || existing_gname[0] == '\0') {
		if (archive_entry_copy_gname_l(entry,
		    header->gname, sizeof(header->gname), tar->sconv) != 0) {
			err = set_conversion_failed_error(a, tar->sconv, "Gname");
			if (err == ARCHIVE_FATAL)
				return (err);
		}
	}

	/* Parse out device numbers only for char and block specials */
	if (header->typeflag[0] == '3' || header->typeflag[0] == '4') {
		if (!archive_entry_rdev_is_set(entry)) {
			archive_entry_set_rdevmajor(entry, (dev_t)
			    tar_atol(header->rdevmajor, sizeof(header->rdevmajor)));
			archive_entry_set_rdevminor(entry, (dev_t)
			    tar_atol(header->rdevminor, sizeof(header->rdevminor)));
		}
	} else {
		archive_entry_set_rdev(entry, 0);
	}

	/* Grab GNU-specific fields. */
	if (!archive_entry_atime_is_set(entry)) {
		t = tar_atol(header->atime, sizeof(header->atime));
		if (t > 0)
			archive_entry_set_atime(entry, t, 0);
	}
	if (!archive_entry_ctime_is_set(entry)) {
		t = tar_atol(header->ctime, sizeof(header->ctime));
		if (t > 0)
			archive_entry_set_ctime(entry, t, 0);
	}

	if (header->realsize[0] != 0) {
		/* Treat as a synonym for the pax GNU.sparse.realsize attr */
		tar->GNU_sparse_realsize
		    = tar_atol(header->realsize, sizeof(header->realsize));
		tar->size_fields |= TAR_SIZE_GNU_SPARSE_REALSIZE;
	}

	if (header->sparse[0].offset[0] != 0) {
		if (gnu_sparse_old_read(a, tar, header, unconsumed)
		    != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
	} else {
		if (header->isextended[0] != 0) {
			/* XXX WTF? XXX */
		}
	}

	/* Grab fields common to all tar variants. */
	err = header_common(a, tar, entry, h);
	if (err == ARCHIVE_FATAL)
		return (err);

	tar->entry_padding = 0x1ff & (-tar->entry_bytes_remaining);

	return (err);
}

static int
gnu_add_sparse_entry(struct archive_read *a, struct tar *tar,
    int64_t offset, int64_t remaining)
{
	struct sparse_block *p;

	p = calloc(1, sizeof(*p));
	if (p == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Out of memory");
		return (ARCHIVE_FATAL);
	}
	if (tar->sparse_last != NULL)
		tar->sparse_last->next = p;
	else
		tar->sparse_list = p;
	tar->sparse_last = p;
	if (remaining < 0 || offset < 0 || offset > INT64_MAX - remaining) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC, "Malformed sparse map data");
		return (ARCHIVE_FATAL);
	}
	p->offset = offset;
	p->remaining = remaining;
	return (ARCHIVE_OK);
}

static void
gnu_clear_sparse_list(struct tar *tar)
{
	struct sparse_block *p;

	while (tar->sparse_list != NULL) {
		p = tar->sparse_list;
		tar->sparse_list = p->next;
		free(p);
	}
	tar->sparse_last = NULL;
}

/*
 * GNU tar old-format sparse data.
 *
 * GNU old-format sparse data is stored in a fixed-field
 * format.  Offset/size values are 11-byte octal fields (same
 * format as 'size' field in ustart header).  These are
 * stored in the header, allocating subsequent header blocks
 * as needed.  Extending the header in this way is a pretty
 * severe POSIX violation; this design has earned GNU tar a
 * lot of criticism.
 */

static int
gnu_sparse_old_read(struct archive_read *a, struct tar *tar,
    const struct archive_entry_header_gnutar *header, int64_t *unconsumed)
{
	ssize_t bytes_read;
	const void *data;
	struct extended {
		struct gnu_sparse sparse[21];
		char	isextended[1];
		char	padding[7];
	};
	const struct extended *ext;

	if (gnu_sparse_old_parse(a, tar, header->sparse, 4) != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	if (header->isextended[0] == 0)
		return (ARCHIVE_OK);

	do {
		if (tar_flush_unconsumed(a, unconsumed) != ARCHIVE_OK) {
			return (ARCHIVE_FATAL);
		}
		data = __archive_read_ahead(a, 512, &bytes_read);
		if (data == NULL) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
			    "Truncated tar archive "
			    "detected while reading sparse file data");
			return (ARCHIVE_FATAL);
		}
		*unconsumed = 512;
		ext = (const struct extended *)data;
		if (gnu_sparse_old_parse(a, tar, ext->sparse, 21) != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
	} while (ext->isextended[0] != 0);
	if (tar->sparse_list != NULL)
		tar->entry_offset = tar->sparse_list->offset;
	return (ARCHIVE_OK);
}

static int
gnu_sparse_old_parse(struct archive_read *a, struct tar *tar,
    const struct gnu_sparse *sparse, int length)
{
	while (length > 0 && sparse->offset[0] != 0) {
		if (gnu_add_sparse_entry(a, tar,
		    tar_atol(sparse->offset, sizeof(sparse->offset)),
		    tar_atol(sparse->numbytes, sizeof(sparse->numbytes)))
		    != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		sparse++;
		length--;
	}
	return (ARCHIVE_OK);
}

/*
 * GNU tar sparse format 0.0
 *
 * Beginning with GNU tar 1.15, sparse files are stored using
 * information in the pax extended header.  The GNU tar maintainers
 * have gone through a number of variations in the process of working
 * out this scheme; fortunately, they're all numbered.
 *
 * Sparse format 0.0 uses attribute GNU.sparse.numblocks to store the
 * number of blocks, and GNU.sparse.offset/GNU.sparse.numbytes to
 * store offset/size for each block.  The repeated instances of these
 * latter fields violate the pax specification (which frowns on
 * duplicate keys), so this format was quickly replaced.
 */

/*
 * GNU tar sparse format 0.1
 *
 * This version replaced the offset/numbytes attributes with
 * a single "map" attribute that stored a list of integers.  This
 * format had two problems: First, the "map" attribute could be very
 * long, which caused problems for some implementations.  More
 * importantly, the sparse data was lost when extracted by archivers
 * that didn't recognize this extension.
 */
static int
gnu_sparse_01_parse(struct archive_read *a, struct tar *tar, const char *p, size_t length)
{
	const char *e;
	int64_t offset = -1, size = -1;

	for (;;) {
		e = p;
		while (length > 0 && *e != ',') {
			if (*e < '0' || *e > '9')
				return (ARCHIVE_WARN);
			e++;
			length--;
		}
		if (offset < 0) {
			offset = tar_atol10(p, e - p);
			if (offset < 0)
				return (ARCHIVE_WARN);
		} else {
			size = tar_atol10(p, e - p);
			if (size < 0)
				return (ARCHIVE_WARN);
			if (gnu_add_sparse_entry(a, tar, offset, size)
			    != ARCHIVE_OK)
				return (ARCHIVE_FATAL);
			offset = -1;
		}
		if (length == 0)
			return (ARCHIVE_OK);
		p = e + 1;
		length--;
	}
}

/*
 * GNU tar sparse format 1.0
 *
 * The idea: The offset/size data is stored as a series of base-10
 * ASCII numbers prepended to the file data, so that dearchivers that
 * don't support this format will extract the block map along with the
 * data and a separate post-process can restore the sparseness.
 *
 * Unfortunately, GNU tar 1.16 had a bug that added unnecessary
 * padding to the body of the file when using this format.  GNU tar
 * 1.17 corrected this bug without bumping the version number, so
 * it's not possible to support both variants.  This code supports
 * the later variant at the expense of not supporting the former.
 *
 * This variant also introduced the GNU.sparse.major/GNU.sparse.minor attributes.
 */

/*
 * Read the next line from the input, and parse it as a decimal
 * integer followed by '\n'.  Returns positive integer value or
 * negative on error.
 */
static int64_t
gnu_sparse_10_atol(struct archive_read *a, struct tar *tar,
    int64_t *remaining, int64_t *unconsumed)
{
	int64_t l, limit, last_digit_limit;
	const char *p;
	ssize_t bytes_read;
	int base, digit;

	base = 10;
	limit = INT64_MAX / base;
	last_digit_limit = INT64_MAX % base;

	/*
	 * Skip any lines starting with '#'; GNU tar specs
	 * don't require this, but they should.
	 */
	do {
		bytes_read = readline(a, tar, &p,
			(ssize_t)tar_min(*remaining, 100), unconsumed);
		if (bytes_read <= 0)
			return (ARCHIVE_FATAL);
		*remaining -= bytes_read;
	} while (p[0] == '#');

	l = 0;
	while (bytes_read > 0) {
		if (*p == '\n')
			return (l);
		if (*p < '0' || *p >= '0' + base)
			return (ARCHIVE_WARN);
		digit = *p - '0';
		if (l > limit || (l == limit && digit > last_digit_limit))
			l = INT64_MAX; /* Truncate on overflow. */
		else
			l = (l * base) + digit;
		p++;
		bytes_read--;
	}
	/* TODO: Error message. */
	return (ARCHIVE_WARN);
}

/*
 * Returns length (in bytes) of the sparse data description
 * that was read.
 */
static int64_t
gnu_sparse_10_read(struct archive_read *a, struct tar *tar, int64_t *unconsumed)
{
	int64_t bytes_read, entries, offset, size, to_skip, remaining;

	/* Clear out the existing sparse list. */
	gnu_clear_sparse_list(tar);

	remaining = tar->entry_bytes_remaining;

	/* Parse entries. */
	entries = gnu_sparse_10_atol(a, tar, &remaining, unconsumed);
	if (entries < 0)
		return (ARCHIVE_FATAL);
	/* Parse the individual entries. */
	while (entries-- > 0) {
		/* Parse offset/size */
		offset = gnu_sparse_10_atol(a, tar, &remaining, unconsumed);
		if (offset < 0)
			return (ARCHIVE_FATAL);
		size = gnu_sparse_10_atol(a, tar, &remaining, unconsumed);
		if (size < 0)
			return (ARCHIVE_FATAL);
		/* Add a new sparse entry. */
		if (gnu_add_sparse_entry(a, tar, offset, size) != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
	}
	/* Skip rest of block... */
	if (tar_flush_unconsumed(a, unconsumed) != ARCHIVE_OK) {
		return (ARCHIVE_FATAL);
	}
	bytes_read = tar->entry_bytes_remaining - remaining;
	to_skip = 0x1ff & -bytes_read;
	/* Fail if tar->entry_bytes_remaing would get negative */
	if (to_skip > remaining)
		return (ARCHIVE_FATAL);
	if (to_skip != __archive_read_consume(a, to_skip))
		return (ARCHIVE_FATAL);
	return (bytes_read + to_skip);
}

/*
 * Solaris pax extension for a sparse file. This is recorded with the
 * data and hole pairs. The way recording sparse information by Solaris'
 * pax simply indicates where data and sparse are, so the stored contents
 * consist of both data and hole.
 */
static int
pax_attribute_SUN_holesdata(struct archive_read *a, struct tar *tar,
	struct archive_entry *entry, const char *p, size_t length)
{
	const char *e;
	int64_t start, end;
	int hole = 1;

	(void)entry; /* UNUSED */

	end = 0;
	if (length <= 0)
		return (ARCHIVE_WARN);
	if (*p == ' ') {
		p++;
		length--;
	} else {
		return (ARCHIVE_WARN);
	}
	for (;;) {
		e = p;
		while (length > 0 && *e != ' ') {
			if (*e < '0' || *e > '9')
				return (ARCHIVE_WARN);
			e++;
			length--;
		}
		start = end;
		end = tar_atol10(p, e - p);
		if (end < 0)
			return (ARCHIVE_WARN);
		if (start < end) {
			if (gnu_add_sparse_entry(a, tar, start,
			    end - start) != ARCHIVE_OK)
				return (ARCHIVE_FATAL);
			tar->sparse_last->hole = hole;
		}
		if (length == 0 || *e == '\n') {
			if (length == 0 && *e == '\n') {
				return (ARCHIVE_OK);
			} else {
				return (ARCHIVE_WARN);
			}
		}
		p = e + 1;
		length--;
		hole = hole == 0;
	}
}

/*-
 * Convert text->integer.
 *
 * Traditional tar formats (including POSIX) specify base-8 for
 * all of the standard numeric fields.  This is a significant limitation
 * in practice:
 *   = file size is limited to 8GB
 *   = rdevmajor and rdevminor are limited to 21 bits
 *   = uid/gid are limited to 21 bits
 *
 * There are two workarounds for this:
 *   = pax extended headers, which use variable-length string fields
 *   = GNU tar and STAR both allow either base-8 or base-256 in
 *      most fields.  The high bit is set to indicate base-256.
 *
 * On read, this implementation supports both extensions.
 */
static int64_t
tar_atol(const char *p, size_t char_cnt)
{
	/*
	 * Technically, GNU tar considers a field to be in base-256
	 * only if the first byte is 0xff or 0x80.
	 */
	if (*p & 0x80)
		return (tar_atol256(p, char_cnt));
	return (tar_atol8(p, char_cnt));
}

/*
 * Note that this implementation does not (and should not!) obey
 * locale settings; you cannot simply substitute strtol here, since
 * it does obey locale.
 */
static int64_t
tar_atol_base_n(const char *p, size_t char_cnt, int base)
{
	int64_t	l, maxval, limit, last_digit_limit;
	int digit, sign;

	maxval = INT64_MAX;
	limit = INT64_MAX / base;
	last_digit_limit = INT64_MAX % base;

	/* the pointer will not be dereferenced if char_cnt is zero
	 * due to the way the && operator is evaluated.
	 */
	while (char_cnt != 0 && (*p == ' ' || *p == '\t')) {
		p++;
		char_cnt--;
	}

	sign = 1;
	if (char_cnt != 0 && *p == '-') {
		sign = -1;
		p++;
		char_cnt--;

		maxval = INT64_MIN;
		limit = -(INT64_MIN / base);
		last_digit_limit = -(INT64_MIN % base);
	}

	l = 0;
	if (char_cnt != 0) {
		digit = *p - '0';
		while (digit >= 0 && digit < base  && char_cnt != 0) {
			if (l>limit || (l == limit && digit >= last_digit_limit)) {
				return maxval; /* Truncate on overflow. */
			}
			l = (l * base) + digit;
			digit = *++p - '0';
			char_cnt--;
		}
	}
	return (sign < 0) ? -l : l;
}

static int64_t
tar_atol8(const char *p, size_t char_cnt)
{
	return tar_atol_base_n(p, char_cnt, 8);
}

static int64_t
tar_atol10(const char *p, size_t char_cnt)
{
	return tar_atol_base_n(p, char_cnt, 10);
}

/*
 * Parse a base-256 integer.  This is just a variable-length
 * twos-complement signed binary value in big-endian order, except
 * that the high-order bit is ignored.  The values here can be up to
 * 12 bytes, so we need to be careful about overflowing 64-bit
 * (8-byte) integers.
 *
 * This code unashamedly assumes that the local machine uses 8-bit
 * bytes and twos-complement arithmetic.
 */
static int64_t
tar_atol256(const char *_p, size_t char_cnt)
{
	uint64_t l;
	const unsigned char *p = (const unsigned char *)_p;
	unsigned char c, neg;

	/* Extend 7-bit 2s-comp to 8-bit 2s-comp, decide sign. */
	c = *p;
	if (c & 0x40) {
		neg = 0xff;
		c |= 0x80;
		l = ~ARCHIVE_LITERAL_ULL(0);
	} else {
		neg = 0;
		c &= 0x7f;
		l = 0;
	}

	/* If more than 8 bytes, check that we can ignore
	 * high-order bits without overflow. */
	while (char_cnt > sizeof(int64_t)) {
		--char_cnt;
		if (c != neg)
			return neg ? INT64_MIN : INT64_MAX;
		c = *++p;
	}

	/* c is first byte that fits; if sign mismatch, return overflow */
	if ((c ^ neg) & 0x80) {
		return neg ? INT64_MIN : INT64_MAX;
	}

	/* Accumulate remaining bytes. */
	while (--char_cnt > 0) {
		l = (l << 8) | c;
		c = *++p;
	}
	l = (l << 8) | c;
	/* Return signed twos-complement value. */
	return (int64_t)(l);
}

/*
 * Returns length of line (including trailing newline)
 * or negative on error.  'start' argument is updated to
 * point to first character of line.  This avoids copying
 * when possible.
 */
static ssize_t
readline(struct archive_read *a, struct tar *tar, const char **start,
    ssize_t limit, int64_t *unconsumed)
{
	ssize_t bytes_read;
	ssize_t total_size = 0;
	const void *t;
	const char *s;
	void *p;

	if (tar_flush_unconsumed(a, unconsumed) != ARCHIVE_OK) {
		return (ARCHIVE_FATAL);
	}

	t = __archive_read_ahead(a, 1, &bytes_read);
	if (bytes_read <= 0 || t == NULL)
		return (ARCHIVE_FATAL);
	s = t;  /* Start of line? */
	p = memchr(t, '\n', bytes_read);
	/* If we found '\n' in the read buffer, return pointer to that. */
	if (p != NULL) {
		bytes_read = 1 + ((const char *)p) - s;
		if (bytes_read > limit) {
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_FILE_FORMAT,
			    "Line too long");
			return (ARCHIVE_FATAL);
		}
		*unconsumed = bytes_read;
		*start = s;
		return (bytes_read);
	}
	*unconsumed = bytes_read;
	/* Otherwise, we need to accumulate in a line buffer. */
	for (;;) {
		if (total_size + bytes_read > limit) {
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_FILE_FORMAT,
			    "Line too long");
			return (ARCHIVE_FATAL);
		}
		if (archive_string_ensure(&tar->line, total_size + bytes_read) == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate working buffer");
			return (ARCHIVE_FATAL);
		}
		memcpy(tar->line.s + total_size, t, bytes_read);
		tar_flush_unconsumed(a, unconsumed);
		total_size += bytes_read;
		/* If we found '\n', clean up and return. */
		if (p != NULL) {
			*start = tar->line.s;
			return (total_size);
		}
		/* Read some more. */
		t = __archive_read_ahead(a, 1, &bytes_read);
		if (bytes_read <= 0 || t == NULL)
			return (ARCHIVE_FATAL);
		s = t;  /* Start of line? */
		p = memchr(t, '\n', bytes_read);
		/* If we found '\n', trim the read. */
		if (p != NULL) {
			bytes_read = 1 + ((const char *)p) - s;
		}
		*unconsumed = bytes_read;
	}
}

/*
 * base64_decode - Base64 decode
 *
 * This accepts most variations of base-64 encoding, including:
 *    * with or without line breaks
 *    * with or without the final group padded with '=' or '_' characters
 * (The most economical Base-64 variant does not pad the last group and
 * omits line breaks; RFC1341 used for MIME requires both.)
 */
static char *
base64_decode(const char *s, size_t len, size_t *out_len)
{
	static const unsigned char digits[64] = {
		'A','B','C','D','E','F','G','H','I','J','K','L','M','N',
		'O','P','Q','R','S','T','U','V','W','X','Y','Z','a','b',
		'c','d','e','f','g','h','i','j','k','l','m','n','o','p',
		'q','r','s','t','u','v','w','x','y','z','0','1','2','3',
		'4','5','6','7','8','9','+','/' };
	static unsigned char decode_table[128];
	char *out, *d;
	const unsigned char *src = (const unsigned char *)s;

	/* If the decode table is not yet initialized, prepare it. */
	if (decode_table[digits[1]] != 1) {
		unsigned i;
		memset(decode_table, 0xff, sizeof(decode_table));
		for (i = 0; i < sizeof(digits); i++)
			decode_table[digits[i]] = i;
	}

	/* Allocate enough space to hold the entire output. */
	/* Note that we may not use all of this... */
	out = malloc(len - len / 4 + 1);
	if (out == NULL) {
		*out_len = 0;
		return (NULL);
	}
	d = out;

	while (len > 0) {
		/* Collect the next group of (up to) four characters. */
		int v = 0;
		int group_size = 0;
		while (group_size < 4 && len > 0) {
			/* '=' or '_' padding indicates final group. */
			if (*src == '=' || *src == '_') {
				len = 0;
				break;
			}
			/* Skip illegal characters (including line breaks) */
			if (*src > 127 || *src < 32
			    || decode_table[*src] == 0xff) {
				len--;
				src++;
				continue;
			}
			v <<= 6;
			v |= decode_table[*src++];
			len --;
			group_size++;
		}
		/* Align a short group properly. */
		v <<= 6 * (4 - group_size);
		/* Unpack the group we just collected. */
		switch (group_size) {
		case 4: d[2] = v & 0xff;
			/* FALLTHROUGH */
		case 3: d[1] = (v >> 8) & 0xff;
			/* FALLTHROUGH */
		case 2: d[0] = (v >> 16) & 0xff;
			break;
		case 1: /* this is invalid! */
			break;
		}
		d += group_size * 3 / 4;
	}

	*out_len = d - out;
	return (out);
}

static char *
url_decode(const char *in, size_t length)
{
	char *out, *d;
	const char *s;

	out = malloc(length + 1);
	if (out == NULL)
		return (NULL);
	for (s = in, d = out; length > 0 && *s != '\0'; ) {
		if (s[0] == '%' && length > 2) {
			/* Try to convert % escape */
			int digit1 = tohex(s[1]);
			int digit2 = tohex(s[2]);
			if (digit1 >= 0 && digit2 >= 0) {
				/* Looks good, consume three chars */
				s += 3;
				length -= 3;
				/* Convert output */
				*d++ = ((digit1 << 4) | digit2);
				continue;
			}
			/* Else fall through and treat '%' as normal char */
		}
		*d++ = *s++;
		--length;
	}
	*d = '\0';
	return (out);
}

static int
tohex(int c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	else if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	else if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	else
		return (-1);
}
