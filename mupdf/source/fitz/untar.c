#include "mupdf/fitz.h"
#include "fitz-imp.h"

#include <string.h>
#include <limits.h>

typedef struct tar_entry_s tar_entry;
typedef struct fz_tar_archive_s fz_tar_archive;

struct tar_entry_s
{
	char *name;
	int64_t offset;
	int size;
};

struct fz_tar_archive_s
{
	fz_archive super;

	int count;
	tar_entry *entries;
};

static inline int isoctdigit(char c)
{
	return c >= '0' && c <= '7';
}

static inline int64_t otoi(const char *s)
{
	int64_t value = 0;

	while (*s && isoctdigit(*s))
	{
		value *= 8;
		value += (*s) - '0';
		s++;
	}

	return value;
}

static void drop_tar_archive(fz_context *ctx, fz_archive *arch)
{
	fz_tar_archive *tar = (fz_tar_archive *) arch;
	int i;
	for (i = 0; i < tar->count; ++i)
		fz_free(ctx, tar->entries[i].name);
	fz_free(ctx, tar->entries);
}

static int is_zeroed(fz_context *ctx, unsigned char *buf, size_t size)
{
	size_t off;

	for (off = 0; off < size; off++)
		if (buf[off] != 0)
			return 0;

	return 1;
}

static void ensure_tar_entries(fz_context *ctx, fz_tar_archive *tar)
{
	fz_stream *file = tar->super.file;
	unsigned char record[512];
	char *longname = NULL;
	char name[101];
	char octsize[13];
	char typeflag;
	int64_t offset, blocks, size;
	size_t n;

	tar->count = 0;

	fz_seek(ctx, file, 0, SEEK_SET);

	while (1)
	{
		offset = fz_tell(ctx, file);
		n = fz_read(ctx, file, record, nelem(record));
		if (n == 0)
			break;
		if (n < nelem(record))
			fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of data in tar record");

		if (is_zeroed(ctx, record, nelem(record)))
			continue;

		memcpy(name, record + 0, nelem(name) - 1);
		name[nelem(name) - 1] = '\0';

		if (strlen(name) == 3)
			break;

		memcpy(octsize, record + 124, nelem(octsize) - 1);
		octsize[nelem(octsize) - 1] = '\0';

		size = otoi(octsize);
		if (size > INT_MAX)
			fz_throw(ctx, FZ_ERROR_GENERIC, "tar archive entry too large");

		typeflag = (char) record[156];

		if (typeflag == 'L')
		{
			longname = fz_malloc(ctx, size);
			n = fz_read(ctx, file, (unsigned char *) longname, size);
			if (n < (size_t) size)
				fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of data in tar long name entry name");

			fz_seek(ctx, file, 512 - (size % 512), 1);
		}

		if (typeflag != '0' && typeflag != '7' && typeflag != '\0')
			continue;

		blocks = (size + 511) / 512;
		fz_seek(ctx, file, blocks * 512, 1);

		tar->entries = fz_realloc_array(ctx, tar->entries, tar->count + 1, tar_entry);

		tar->entries[tar->count].offset = offset;
		tar->entries[tar->count].size = size;
		if (longname != NULL)
		{
			tar->entries[tar->count].name = longname;
			longname = NULL;
		}
		else
			tar->entries[tar->count].name = fz_strdup(ctx, name);

		tar->count++;
	}
}

static tar_entry *lookup_tar_entry(fz_context *ctx, fz_tar_archive *tar, const char *name)
{
	int i;
	for (i = 0; i < tar->count; i++)
		if (!fz_strcasecmp(name, tar->entries[i].name))
			return &tar->entries[i];
	return NULL;
}

static fz_stream *open_tar_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_tar_archive *tar = (fz_tar_archive *) arch;
	fz_stream *file = tar->super.file;
	tar_entry *ent;

	ent = lookup_tar_entry(ctx, tar, name);
	if (!ent)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find named tar archive entry");

	fz_seek(ctx, file, ent->offset + 512, 0);
	return fz_open_null_filter(ctx, file, ent->size, fz_tell(ctx, file));
}

static fz_buffer *read_tar_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_tar_archive *tar = (fz_tar_archive *) arch;
	fz_stream *file = tar->super.file;
	fz_buffer *ubuf;
	tar_entry *ent;

	ent = lookup_tar_entry(ctx, tar, name);
	if (!ent)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find named tar archive entry");

	ubuf = fz_new_buffer(ctx, ent->size);

	fz_try(ctx)
	{
		fz_seek(ctx, file, ent->offset + 512, 0);
		ubuf->len = fz_read(ctx, file, ubuf->data, ent->size);
		if (ubuf->len != (size_t)ent->size)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot read entire archive entry");
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, ubuf);
		fz_rethrow(ctx);
	}

	return ubuf;
}

static int has_tar_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_tar_archive *tar = (fz_tar_archive *) arch;
	tar_entry *ent = lookup_tar_entry(ctx, tar, name);
	return ent != NULL;
}

static const char *list_tar_entry(fz_context *ctx, fz_archive *arch, int idx)
{
	fz_tar_archive *tar = (fz_tar_archive *) arch;
	if (idx < 0 || idx >= tar->count)
		return NULL;
	return tar->entries[idx].name;
}

static int count_tar_entries(fz_context *ctx, fz_archive *arch)
{
	fz_tar_archive *tar = (fz_tar_archive *) arch;
	return tar->count;
}

/*
	Detect if stream object is a tar achieve.

	Assumes that the stream object is seekable.
*/
int
fz_is_tar_archive(fz_context *ctx, fz_stream *file)
{
	const unsigned char gnusignature[6] = { 'u', 's', 't', 'a', 'r', ' ' };
	const unsigned char paxsignature[6] = { 'u', 's', 't', 'a', 'r', '\0' };
	const unsigned char v7signature[6] = { '\0', '\0', '\0', '\0', '\0', '\0' };
	unsigned char data[6];
	size_t n;

	fz_seek(ctx, file, 257, 0);
	n = fz_read(ctx, file, data, nelem(data));
	if (n != nelem(data))
		return 0;
	if (!memcmp(data, gnusignature, nelem(gnusignature)))
		return 1;
	if (!memcmp(data, paxsignature, nelem(paxsignature)))
		return 1;
	if (!memcmp(data, v7signature, nelem(v7signature)))
		return 1;

	return 0;
}

/*
	Open a tar archive stream.

	Open an archive using a seekable stream object rather than
	opening a file or directory on disk.

	An exception is throw if the stream is not a tar archive as
	indicated by the presence of a tar signature.

*/
fz_archive *
fz_open_tar_archive_with_stream(fz_context *ctx, fz_stream *file)
{
	fz_tar_archive *tar;

	if (!fz_is_tar_archive(ctx, file))
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot recognize tar archive");

	tar = fz_new_derived_archive(ctx, file, fz_tar_archive);
	tar->super.format = "tar";
	tar->super.count_entries = count_tar_entries;
	tar->super.list_entry = list_tar_entry;
	tar->super.has_entry = has_tar_entry;
	tar->super.read_entry = read_tar_entry;
	tar->super.open_entry = open_tar_entry;
	tar->super.drop_archive = drop_tar_archive;

	fz_try(ctx)
	{
		ensure_tar_entries(ctx, tar);
	}
	fz_catch(ctx)
	{
		fz_drop_archive(ctx, &tar->super);
		fz_rethrow(ctx);
	}

	return &tar->super;
}

/*
	Open a tar archive file.

	An exception is throw if the file is not a tar archive as
	indicated by the presence of a tar signature.

	filename: a path to a tar archive file as it would be given to
	open(2).
*/
fz_archive *
fz_open_tar_archive(fz_context *ctx, const char *filename)
{
	fz_archive *tar = NULL;
	fz_stream *file;

	file = fz_open_file(ctx, filename);

	fz_try(ctx)
		tar = fz_open_tar_archive_with_stream(ctx, file);
	fz_always(ctx)
		fz_drop_stream(ctx, file);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return tar;
}
