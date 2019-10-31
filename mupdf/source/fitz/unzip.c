#include "mupdf/fitz.h"
#include "fitz-imp.h"

#include <string.h>
#include <limits.h>

#include <zlib.h>

#if !defined (INT32_MAX)
#define INT32_MAX 2147483647L
#endif

#define ZIP_LOCAL_FILE_SIG 0x04034b50
#define ZIP_DATA_DESC_SIG 0x08074b50
#define ZIP_CENTRAL_DIRECTORY_SIG 0x02014b50
#define ZIP_END_OF_CENTRAL_DIRECTORY_SIG 0x06054b50

#define ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR_SIG 0x07064b50
#define ZIP64_END_OF_CENTRAL_DIRECTORY_SIG 0x06064b50
#define ZIP64_EXTRA_FIELD_SIG 0x0001

#define ZIP_ENCRYPTED_FLAG 0x1

typedef struct zip_entry_s zip_entry;
typedef struct fz_zip_archive_s fz_zip_archive;

struct zip_entry_s
{
	char *name;
	uint64_t offset, csize, usize;
};

struct fz_zip_archive_s
{
	fz_archive super;

	int count;
	zip_entry *entries;
};

static void drop_zip_archive(fz_context *ctx, fz_archive *arch)
{
	fz_zip_archive *zip = (fz_zip_archive *) arch;
	int i;
	for (i = 0; i < zip->count; ++i)
		fz_free(ctx, zip->entries[i].name);
	fz_free(ctx, zip->entries);
}

static void read_zip_dir_imp(fz_context *ctx, fz_zip_archive *zip, int64_t start_offset)
{
	fz_stream *file = zip->super.file;
	uint32_t sig;
	int i;
	int namesize, metasize, commentsize;
	uint64_t count, offset;
	uint64_t csize, usize;
	char *name = NULL;
	size_t n;

	fz_var(name);

	zip->count = 0;

	fz_seek(ctx, file, start_offset, 0);

	sig = fz_read_uint32_le(ctx, file);
	if (sig != ZIP_END_OF_CENTRAL_DIRECTORY_SIG)
		fz_throw(ctx, FZ_ERROR_GENERIC, "wrong zip end of central directory signature (0x%x)", sig);

	(void) fz_read_uint16_le(ctx, file); /* this disk */
	(void) fz_read_uint16_le(ctx, file); /* start disk */
	(void) fz_read_uint16_le(ctx, file); /* entries in this disk */
	count = fz_read_uint16_le(ctx, file); /* entries in central directory disk */
	(void) fz_read_uint32_le(ctx, file); /* size of central directory */
	offset = fz_read_uint32_le(ctx, file); /* offset to central directory */

	/* ZIP64 */
	if (count == 0xFFFF || offset == 0xFFFFFFFF)
	{
		int64_t offset64, count64;

		fz_seek(ctx, file, start_offset - 20, 0);

		sig = fz_read_uint32_le(ctx, file);
		if (sig != ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR_SIG)
			fz_throw(ctx, FZ_ERROR_GENERIC, "wrong zip64 end of central directory locator signature (0x%x)", sig);

		(void) fz_read_uint32_le(ctx, file); /* start disk */
		offset64 = fz_read_uint64_le(ctx, file); /* offset to end of central directory record */

		fz_seek(ctx, file, offset64, 0);

		sig = fz_read_uint32_le(ctx, file);
		if (sig != ZIP64_END_OF_CENTRAL_DIRECTORY_SIG)
			fz_throw(ctx, FZ_ERROR_GENERIC, "wrong zip64 end of central directory signature (0x%x)", sig);

		(void) fz_read_uint64_le(ctx, file); /* size of record */
		(void) fz_read_uint16_le(ctx, file); /* version made by */
		(void) fz_read_uint16_le(ctx, file); /* version to extract */
		(void) fz_read_uint32_le(ctx, file); /* disk number */
		(void) fz_read_uint32_le(ctx, file); /* disk number start */
		count64 = fz_read_uint64_le(ctx, file); /* entries in central directory disk */
		(void) fz_read_uint64_le(ctx, file); /* entries in central directory */
		(void) fz_read_uint64_le(ctx, file); /* size of central directory */
		offset64 = fz_read_uint64_le(ctx, file); /* offset to central directory */

		if (count == 0xFFFF)
		{
			count = count64;
		}
		if (offset == 0xFFFFFFFF)
		{
			offset = offset64;
		}
	}

	fz_seek(ctx, file, offset, 0);

	fz_try(ctx)
	{
		if (count > INT_MAX)
			count = INT_MAX;
		for (i = 0; i < (int)count; i++)
		{
			sig = fz_read_uint32_le(ctx, file);
			if (sig != ZIP_CENTRAL_DIRECTORY_SIG)
				fz_throw(ctx, FZ_ERROR_GENERIC, "wrong zip central directory signature (0x%x)", sig);

			(void) fz_read_uint16_le(ctx, file); /* version made by */
			(void) fz_read_uint16_le(ctx, file); /* version to extract */
			(void) fz_read_uint16_le(ctx, file); /* general */
			(void) fz_read_uint16_le(ctx, file); /* method */
			(void) fz_read_uint16_le(ctx, file); /* last mod file time */
			(void) fz_read_uint16_le(ctx, file); /* last mod file date */
			(void) fz_read_uint32_le(ctx, file); /* crc-32 */
			csize = fz_read_uint32_le(ctx, file);
			usize = fz_read_uint32_le(ctx, file);
			namesize = fz_read_uint16_le(ctx, file);
			metasize = fz_read_uint16_le(ctx, file);
			commentsize = fz_read_uint16_le(ctx, file);
			(void) fz_read_uint16_le(ctx, file); /* disk number start */
			(void) fz_read_uint16_le(ctx, file); /* int file atts */
			(void) fz_read_uint32_le(ctx, file); /* ext file atts */
			offset = fz_read_uint32_le(ctx, file);

			if (namesize < 0 || metasize < 0 || commentsize < 0)
				fz_throw(ctx, FZ_ERROR_GENERIC, "invalid size in zip entry");

			name = Memento_label(fz_malloc(ctx, namesize + 1), "zip_name");

			n = fz_read(ctx, file, (unsigned char*)name, namesize);
			if (n < (size_t)namesize)
				fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of data in zip entry name");
			name[namesize] = '\0';

			while (metasize > 0)
			{
				int type = fz_read_uint16_le(ctx, file);
				int size = fz_read_uint16_le(ctx, file);

				if (type == ZIP64_EXTRA_FIELD_SIG)
				{
					int sizeleft = size;
					if (usize == 0xFFFFFFFF && sizeleft >= 8)
					{
						usize = fz_read_uint64_le(ctx, file);
						sizeleft -= 8;
					}
					if (csize == 0xFFFFFFFF && sizeleft >= 8)
					{
						csize = fz_read_uint64_le(ctx, file);
						sizeleft -= 8;
					}
					if (offset == 0xFFFFFFFF && sizeleft >= 8)
					{
						offset = fz_read_uint64_le(ctx, file);
						sizeleft -= 8;
					}
					fz_seek(ctx, file, sizeleft - size, 1);
				}
				fz_seek(ctx, file, size, 1);
				metasize -= 4 + size;
			}

			if (usize > INT32_MAX || csize > INT32_MAX)
				fz_throw(ctx, FZ_ERROR_GENERIC, "zip archive entry larger than 2 GB");

			fz_seek(ctx, file, commentsize, 1);

			zip->entries = Memento_label(fz_realloc_array(ctx, zip->entries, zip->count + 1, zip_entry), "zip_entries");

			zip->entries[zip->count].offset = offset;
			zip->entries[zip->count].csize = csize;
			zip->entries[zip->count].usize = usize;
			zip->entries[zip->count].name = name;
			name = NULL;

			zip->count++;
		}
	}
	fz_always(ctx)
		fz_free(ctx, name);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static int read_zip_entry_header(fz_context *ctx, fz_zip_archive *zip, zip_entry *ent)
{
	fz_stream *file = zip->super.file;
	uint32_t sig;
	int general, method, namelength, extralength;

	fz_seek(ctx, file, ent->offset, 0);

	sig = fz_read_uint32_le(ctx, file);
	if (sig != ZIP_LOCAL_FILE_SIG)
		fz_throw(ctx, FZ_ERROR_GENERIC, "wrong zip local file signature (0x%x)", sig);

	(void) fz_read_uint16_le(ctx, file); /* version */
	general = fz_read_uint16_le(ctx, file); /* general */
	if (general & ZIP_ENCRYPTED_FLAG)
		fz_throw(ctx, FZ_ERROR_GENERIC, "zip content is encrypted");

	method = fz_read_uint16_le(ctx, file);
	(void) fz_read_uint16_le(ctx, file); /* file time */
	(void) fz_read_uint16_le(ctx, file); /* file date */
	(void) fz_read_uint32_le(ctx, file); /* crc-32 */
	(void) fz_read_uint32_le(ctx, file); /* csize */
	(void) fz_read_uint32_le(ctx, file); /* usize */
	namelength = fz_read_uint16_le(ctx, file);
	extralength = fz_read_uint16_le(ctx, file);

	fz_seek(ctx, file, namelength + extralength, 1);

	return method;
}

static void ensure_zip_entries(fz_context *ctx, fz_zip_archive *zip)
{
	fz_stream *file = zip->super.file;
	unsigned char buf[512];
	size_t size, back, maxback;
	size_t i, n;

	fz_seek(ctx, file, 0, SEEK_END);
	size = fz_tell(ctx, file);

	maxback = fz_minz(size, 0xFFFF + sizeof buf);
	back = fz_minz(maxback, sizeof buf);

	while (back <= maxback)
	{
		fz_seek(ctx, file, (int64_t)(size - back), 0);
		n = fz_read(ctx, file, buf, sizeof buf);
		if (n < 4)
			break;
		for (i = n - 4; i > 0; i--)
			if (!memcmp(buf + i, "PK\5\6", 4))
			{
				read_zip_dir_imp(ctx, zip, size - back + i);
				return;
			}
		back += sizeof buf - 4;
	}

	fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find end of central directory");
}

static zip_entry *lookup_zip_entry(fz_context *ctx, fz_zip_archive *zip, const char *name)
{
	int i;
	if (name[0] == '/')
		++name;
	for (i = 0; i < zip->count; i++)
		if (!fz_strcasecmp(name, zip->entries[i].name))
			return &zip->entries[i];
	return NULL;
}

static fz_stream *open_zip_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_zip_archive *zip = (fz_zip_archive *) arch;
	fz_stream *file = zip->super.file;
	int method;
	zip_entry *ent;

	ent = lookup_zip_entry(ctx, zip, name);
	if (!ent)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find named zip archive entry");

	method = read_zip_entry_header(ctx, zip, ent);
	if (method == 0)
		return fz_open_null_filter(ctx, file, ent->usize, fz_tell(ctx, file));
	if (method == 8)
		return fz_open_flated(ctx, file, -15);
	fz_throw(ctx, FZ_ERROR_GENERIC, "unknown zip method: %d", method);
}

static fz_buffer *read_zip_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_zip_archive *zip = (fz_zip_archive *) arch;
	fz_stream *file = zip->super.file;
	fz_buffer *ubuf;
	unsigned char *cbuf;
	int method;
	z_stream z;
	int code;
	uint64_t len;
	zip_entry *ent;

	fz_var(cbuf);

	ent = lookup_zip_entry(ctx, zip, name);
	if (!ent)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find named zip archive entry");

	method = read_zip_entry_header(ctx, zip, ent);
	ubuf = fz_new_buffer(ctx, ent->usize + 1); /* +1 because many callers will add a terminating zero */

	if (method == 0)
	{
		fz_try(ctx)
		{
			ubuf->len = fz_read(ctx, file, ubuf->data, ent->usize);
			if (ubuf->len < (size_t)ent->usize)
				fz_warn(ctx, "premature end of data in stored zip archive entry");
		}
		fz_catch(ctx)
		{
			fz_drop_buffer(ctx, ubuf);
			fz_rethrow(ctx);
		}
		return ubuf;
	}
	else if (method == 8)
	{
		fz_try(ctx)
		{
			cbuf = fz_malloc(ctx, ent->csize);

			fz_read(ctx, file, cbuf, ent->csize);

			z.zalloc = fz_zlib_alloc;
			z.zfree = fz_zlib_free;
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

			len = ent->usize - z.avail_out;
			if (len < ent->usize)
				fz_warn(ctx, "premature end of data in compressed archive entry");
			ubuf->len = len;
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

static int has_zip_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_zip_archive *zip = (fz_zip_archive *) arch;
	zip_entry *ent = lookup_zip_entry(ctx, zip, name);
	return ent != NULL;
}

static const char *list_zip_entry(fz_context *ctx, fz_archive *arch, int idx)
{
	fz_zip_archive *zip = (fz_zip_archive *) arch;
	if (idx < 0 || idx >= zip->count)
		return NULL;
	return zip->entries[idx].name;
}

static int count_zip_entries(fz_context *ctx, fz_archive *arch)
{
	fz_zip_archive *zip = (fz_zip_archive *) arch;
	return zip->count;
}

/*
	Detect if stream object is a zip archive.

	Assumes that the stream object is seekable.
*/
int
fz_is_zip_archive(fz_context *ctx, fz_stream *file)
{
	const unsigned char signature[4] = { 'P', 'K', 0x03, 0x04 };
	unsigned char data[4];
	size_t n;

	fz_seek(ctx, file, 0, 0);
	n = fz_read(ctx, file, data, nelem(data));
	if (n != nelem(signature))
		return 0;
	if (memcmp(data, signature, nelem(signature)))
		return 0;

	return 1;
}

/*
	Open a zip archive stream.

	Open an archive using a seekable stream object rather than
	opening a file or directory on disk.

	An exception is throw if the stream is not a zip archive as
	indicated by the presence of a zip signature.

*/
fz_archive *
fz_open_zip_archive_with_stream(fz_context *ctx, fz_stream *file)
{
	fz_zip_archive *zip;

	if (!fz_is_zip_archive(ctx, file))
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot recognize zip archive");

	zip = fz_new_derived_archive(ctx, file, fz_zip_archive);
	zip->super.format = "zip";
	zip->super.count_entries = count_zip_entries;
	zip->super.list_entry = list_zip_entry;
	zip->super.has_entry = has_zip_entry;
	zip->super.read_entry = read_zip_entry;
	zip->super.open_entry = open_zip_entry;
	zip->super.drop_archive = drop_zip_archive;

	fz_try(ctx)
	{
		ensure_zip_entries(ctx, zip);
	}
	fz_catch(ctx)
	{
		fz_drop_archive(ctx, &zip->super);
		fz_rethrow(ctx);
	}

	return &zip->super;
}

/*
	Open a zip archive file.

	An exception is throw if the file is not a zip archive as
	indicated by the presence of a zip signature.

	filename: a path to a zip archive file as it would be given to
	open(2).
*/
fz_archive *
fz_open_zip_archive(fz_context *ctx, const char *filename)
{
	fz_archive *zip = NULL;
	fz_stream *file;

	file = fz_open_file(ctx, filename);

	fz_var(zip);

	fz_try(ctx)
		zip = fz_open_zip_archive_with_stream(ctx, file);
	fz_always(ctx)
		fz_drop_stream(ctx, file);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return zip;
}
