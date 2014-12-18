#include "mupdf/fitz.h"

#include <zlib.h>

#define ZIP_LOCAL_FILE_SIG 0x04034b50
#define ZIP_DATA_DESC_SIG 0x08074b50
#define ZIP_CENTRAL_DIRECTORY_SIG 0x02014b50
#define ZIP_END_OF_CENTRAL_DIRECTORY_SIG 0x06054b50

#define ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR_SIG 0x07064b50
#define ZIP64_END_OF_CENTRAL_DIRECTORY_SIG 0x06064b50
#define ZIP64_EXTRA_FIELD_SIG 0x0001

#define ZIP_ENCRYPTED_FLAG 0x1

struct zip_entry
{
	char *name;
	int offset, csize, usize;
};

struct fz_archive_s
{
	char *directory;
	fz_stream *file;
	int count;
	struct zip_entry *table;
};

static inline int getshort(fz_stream *file)
{
	int a = fz_read_byte(file);
	int b = fz_read_byte(file);
	return a | b << 8;
}

static inline int getlong(fz_stream *file)
{
	int a = fz_read_byte(file);
	int b = fz_read_byte(file);
	int c = fz_read_byte(file);
	int d = fz_read_byte(file);
	return a | b << 8 | c << 16 | d << 24;
}

static inline int getlong64(fz_stream *file)
{
	int a = getlong(file);
	int b = getlong(file);
	return b != 0 ? -1 : a;
}

static inline int zip_isdigit(int c)
{
	return c >= '0' && c <= '9';
}

static inline int zip_toupper(int c)
{
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 'A';
	return c;
}

static int zip_strcasecmp(const char *a, const char *b)
{
	while (zip_toupper(*a) == zip_toupper(*b))
	{
		if (*a++ == 0)
			return 0;
		b++;
	}
	return zip_toupper(*a) - zip_toupper(*b);
}

static int case_compare_entries(const void *a_, const void *b_)
{
	const struct zip_entry *a = a_;
	const struct zip_entry *b = b_;
	return zip_strcasecmp(a->name, b->name);
}

static struct zip_entry *lookup_zip_entry(fz_context *ctx, fz_archive *zip, const char *name)
{
	int l = 0;
	int r = zip->count - 1;
	while (l <= r)
	{
		int m = (l + r) >> 1;
		int c = zip_strcasecmp(name, zip->table[m].name);
		if (c < 0)
			r = m - 1;
		else if (c > 0)
			l = m + 1;
		else
			return &zip->table[m];
	}
	return NULL;
}

static void read_zip_dir_imp(fz_context *ctx, fz_archive *zip, int start_offset)
{
	fz_stream *file = zip->file;
	int sig;
	int offset, count;
	int namesize, metasize, commentsize;
	int i;

	fz_seek(file, start_offset, 0);

	sig = getlong(file);
	if (sig != ZIP_END_OF_CENTRAL_DIRECTORY_SIG)
		fz_throw(ctx, FZ_ERROR_GENERIC, "wrong zip end of central directory signature (0x%x)", sig);

	(void) getshort(file); /* this disk */
	(void) getshort(file); /* start disk */
	(void) getshort(file); /* entries in this disk */
	count = getshort(file); /* entries in central directory disk */
	(void) getlong(file); /* size of central directory */
	offset = getlong(file); /* offset to central directory */

	/* ZIP64 */
	if (count == 0xFFFF || offset == 0xFFFFFFFF)
	{
		int offset64, count64;

		fz_seek(file, start_offset - 20, 0);

		sig = getlong(file);
		if (sig != ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR_SIG)
			fz_throw(ctx, FZ_ERROR_GENERIC, "wrong zip64 end of central directory locator signature (0x%x)", sig);

		(void) getlong(file); /* start disk */
		offset64 = getlong64(file); /* offset to end of central directory record */
		if (offset64 < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "zip64 files larger than 2 GB aren't supported");

		fz_seek(file, offset64, 0);

		sig = getlong(file);
		if (sig != ZIP64_END_OF_CENTRAL_DIRECTORY_SIG)
			fz_throw(ctx, FZ_ERROR_GENERIC, "wrong zip64 end of central directory signature (0x%x)", sig);

		(void) getlong64(file); /* size of record */
		(void) getshort(file); /* version made by */
		(void) getshort(file); /* version to extract */
		(void) getlong(file); /* disk number */
		(void) getlong(file); /* disk number start */
		count64 = getlong64(file); /* entries in central directory disk */
		(void) getlong64(file); /* entries in central directory */
		(void) getlong64(file); /* size of central directory */
		offset64 = getlong64(file); /* offset to central directory */

		if (count == 0xFFFF)
			count = count64;
		if (offset == 0xFFFFFFFF)
			offset = offset64;
		if (count < 0 || offset < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "zip64 files larger than 2 GB aren't supported");
	}

	zip->count = count;
	zip->table = fz_malloc_array(ctx, count, sizeof *zip->table);
	memset(zip->table, 0, count * sizeof *zip->table);

	fz_seek(file, offset, 0);

	for (i = 0; i < count; i++)
	{
		sig = getlong(file);
		if (sig != ZIP_CENTRAL_DIRECTORY_SIG)
			fz_throw(ctx, FZ_ERROR_GENERIC, "wrong zip central directory signature (0x%x)", sig);

		(void) getshort(file); /* version made by */
		(void) getshort(file); /* version to extract */
		(void) getshort(file); /* general */
		(void) getshort(file); /* method */
		(void) getshort(file); /* last mod file time */
		(void) getshort(file); /* last mod file date */
		(void) getlong(file); /* crc-32 */
		zip->table[i].csize = getlong(file);
		zip->table[i].usize = getlong(file);
		namesize = getshort(file);
		metasize = getshort(file);
		commentsize = getshort(file);
		(void) getshort(file); /* disk number start */
		(void) getshort(file); /* int file atts */
		(void) getlong(file); /* ext file atts */
		zip->table[i].offset = getlong(file);

		zip->table[i].name = fz_malloc(ctx, namesize + 1);
		fz_read(file, (unsigned char*)zip->table[i].name, namesize);
		zip->table[i].name[namesize] = 0;

		while (metasize > 0)
		{
			int type = getshort(file);
			int size = getshort(file);
			if (type == ZIP64_EXTRA_FIELD_SIG)
			{
				int sizeleft = size;
				if (zip->table[i].usize == 0xFFFFFFFF && sizeleft >= 8)
				{
					zip->table[i].usize = getlong64(file);
					sizeleft -= 8;
				}
				if (zip->table[i].csize == 0xFFFFFFFF && sizeleft >= 8)
				{
					zip->table[i].csize = getlong64(file);
					sizeleft -= 8;
				}
				if (zip->table[i].offset == 0xFFFFFFFF && sizeleft >= 8)
				{
					zip->table[i].offset = getlong64(file);
					sizeleft -= 8;
				}
				fz_seek(file, sizeleft - size, 1);
			}
			fz_seek(file, size, 1);
			metasize -= 4 + size;
		}
		if (zip->table[i].usize < 0 || zip->table[i].csize < 0 || zip->table[i].offset < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "zip64 files larger than 2 GB are not supported");

		fz_seek(file, commentsize, 1);
	}

	qsort(zip->table, count, sizeof *zip->table, case_compare_entries);
}

static void read_zip_dir(fz_context *ctx, fz_archive *zip)
{
	fz_stream *file = zip->file;
	unsigned char buf[512];
	int size, back, maxback;
	int i, n;

	fz_seek(file, 0, SEEK_END);
	size = fz_tell(file);

	maxback = fz_mini(size, 0xFFFF + sizeof buf);
	back = fz_mini(maxback, sizeof buf);

	while (back < maxback)
	{
		fz_seek(file, size - back, 0);
		n = fz_read(file, buf, sizeof buf);
		for (i = n - 4; i > 0; i--)
		{
			if (!memcmp(buf + i, "PK\5\6", 4))
			{
				read_zip_dir_imp(ctx, zip, size - back + i);
				return;
			}
		}
		back += sizeof buf - 4;
	}

	fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find end of central directory");
}

static int read_zip_entry_header(fz_context *ctx, fz_archive *zip, struct zip_entry *ent)
{
	fz_stream *file = zip->file;
	int sig, general, method, namelength, extralength;

	fz_seek(file, ent->offset, 0);

	sig = getlong(file);
	if (sig != ZIP_LOCAL_FILE_SIG)
		fz_throw(ctx, FZ_ERROR_GENERIC, "wrong zip local file signature (0x%x)", sig);

	(void) getshort(file); /* version */
	general = getshort(file); /* general */
	if (general & ZIP_ENCRYPTED_FLAG)
		fz_throw(ctx, FZ_ERROR_GENERIC, "zip content is encrypted");

	method = getshort(file);
	(void) getshort(file); /* file time */
	(void) getshort(file); /* file date */
	(void) getlong(file); /* crc-32 */
	(void) getlong(file); /* csize */
	(void) getlong(file); /* usize */
	namelength = getshort(file);
	extralength = getshort(file);

	fz_seek(file, namelength + extralength, 1);

	return method;
}

static fz_stream *open_zip_entry(fz_context *ctx, fz_archive *zip, struct zip_entry *ent)
{
	fz_stream *file = zip->file;
	int method = read_zip_entry_header(ctx, zip, ent);
	if (method == 0)
		return fz_open_null(file, ent->usize, fz_tell(file));
	if (method == 8)
		return fz_open_flated(file, -15);
	fz_throw(ctx, FZ_ERROR_GENERIC, "unknown zip method: %d", method);
}

static fz_buffer *read_zip_entry(fz_context *ctx, fz_archive *zip, struct zip_entry *ent)
{
	fz_stream *file = zip->file;
	fz_buffer *ubuf;
	unsigned char *cbuf;
	int method;
	z_stream z;
	int code;

	method = read_zip_entry_header(ctx, zip, ent);

	ubuf = fz_new_buffer(ctx, ent->usize + 1); /* +1 because many callers will add a terminating zero */
	ubuf->len = ent->usize;

	if (method == 0)
	{
		fz_try(ctx)
		{
			fz_read(file, ubuf->data, ent->usize);
		}
		fz_catch(ctx)
		{
			fz_drop_buffer(ctx, ubuf);
			fz_rethrow(ctx);
		}
		return ubuf;
	}

	if (method == 8)
	{
		cbuf = fz_malloc(ctx, ent->csize);
		fz_try(ctx)
		{
			fz_read(file, cbuf, ent->csize);

			z.zalloc = (alloc_func) fz_malloc_array;
			z.zfree = (free_func) fz_free;
			z.opaque = ctx;
			z.next_in = cbuf;
			z.avail_in = ent->csize;
			z.next_out = ubuf->data;
			z.avail_out = ent->usize;

			code = inflateInit2(&z, -15);
			if (code != Z_OK)
			{
				fz_throw(ctx, FZ_ERROR_GENERIC, "zlib inflateInit2 error: %s", z.msg);
			}
			code = inflate(&z, Z_FINISH);
			if (code != Z_STREAM_END)
			{
				inflateEnd(&z);
				fz_throw(ctx, FZ_ERROR_GENERIC, "zlib inflate error: %s", z.msg);
			}
			code = inflateEnd(&z);
			if (code != Z_OK)
			{
				fz_throw(ctx, FZ_ERROR_GENERIC, "zlib inflateEnd error: %s", z.msg);
			}
		}
		fz_always(ctx)
		{
			fz_free(ctx, cbuf);
		}
		fz_catch(ctx)
		{
			fz_drop_buffer(ctx, ubuf);
			fz_rethrow(ctx);
		}
		return ubuf;
	}

	fz_drop_buffer(ctx, ubuf);
	fz_throw(ctx, FZ_ERROR_GENERIC, "unknown zip method: %d", method);
}

int
fz_has_archive_entry(fz_context *ctx, fz_archive *zip, const char *name)
{
	if (zip->directory)
	{
		char path[2048];
		FILE *file;
		fz_strlcpy(path, zip->directory, sizeof path);
		fz_strlcat(path, "/", sizeof path);
		fz_strlcat(path, name, sizeof path);
		file = fopen(path, "rb");
		if (file)
			fclose(file);
		return file != NULL;
	}
	else
	{
		return lookup_zip_entry(ctx, zip, name) != NULL;
	}
}

fz_stream *
fz_open_archive_entry(fz_context *ctx, fz_archive *zip, const char *name)
{
	if (zip->directory)
	{
		char path[2048];
		fz_strlcpy(path, zip->directory, sizeof path);
		fz_strlcat(path, "/", sizeof path);
		fz_strlcat(path, name, sizeof path);
		return fz_open_file(ctx, path);
	}
	else
	{
		struct zip_entry *ent = lookup_zip_entry(ctx, zip, name);
		if (!ent)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find zip entry: '%s'", name);
		return open_zip_entry(ctx, zip, ent);
	}
}

fz_buffer *
fz_read_archive_entry(fz_context *ctx, fz_archive *zip, const char *name)
{
	if (zip->directory)
	{
		char path[2048];
		fz_strlcpy(path, zip->directory, sizeof path);
		fz_strlcat(path, "/", sizeof path);
		fz_strlcat(path, name, sizeof path);
		return fz_read_file(ctx, path);
	}
	else
	{
		struct zip_entry *ent = lookup_zip_entry(ctx, zip, name);
		if (!ent)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find zip entry: '%s'", name);
		return read_zip_entry(ctx, zip, ent);
	}
}

int
fz_count_archive_entries(fz_context *ctx, fz_archive *zip)
{
	return zip->count;
}

const char *
fz_list_archive_entry(fz_context *ctx, fz_archive *zip, int idx)
{
	if (idx < 0 || idx >= zip->count)
		return NULL;
	return zip->table[idx].name;
}

void
fz_close_archive(fz_context *ctx, fz_archive *zip)
{
	int i;
	if (zip)
	{
		fz_free(ctx, zip->directory);
		fz_close(zip->file);
		for (i = 0; i < zip->count; ++i)
			fz_free(ctx, zip->table[i].name);
		fz_free(ctx, zip->table);
		fz_free(ctx, zip);
	}
}

void
fz_rebind_archive(fz_archive *zip, fz_context *ctx)
{
	if (zip->file)
		fz_rebind_stream(zip->file, ctx);
}

fz_archive *
fz_open_directory(fz_context *ctx, const char *dirname)
{
	fz_archive *zip = fz_malloc_struct(ctx, fz_archive);
	zip->directory = fz_strdup(ctx, dirname);
	return zip;
}

fz_archive *
fz_open_archive_with_stream(fz_context *ctx, fz_stream *file)
{
	fz_archive *zip;

	zip = fz_malloc_struct(ctx, fz_archive);
	zip->file = fz_keep_stream(file);
	zip->count = 0;
	zip->table = NULL;

	fz_try(ctx)
	{
		read_zip_dir(ctx, zip);
	}
	fz_catch(ctx)
	{
		fz_close_archive(ctx, zip);
		fz_rethrow(ctx);
	}

	return zip;
}

fz_archive *
fz_open_archive(fz_context *ctx, const char *filename)
{
	fz_stream *file;
	fz_archive *zip;

	file = fz_open_file(ctx, filename);
	fz_try(ctx)
	{
		zip = fz_open_archive_with_stream(ctx, file);
	}
	fz_always(ctx)
	{
		fz_close(file);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return zip;
}
