#include "fitz.h"
#include "muxps.h"

#include <zlib.h>

xps_part *
xps_new_part(xps_context *ctx, char *name, int size)
{
	xps_part *part;

	part = fz_malloc(sizeof(xps_part));
	part->name = fz_strdup(name);
	part->size = size;
	part->data = fz_malloc(size + 1);
	part->data[size] = 0; /* null-terminate for xml parser */

	return part;
}

void
xps_free_part(xps_context *ctx, xps_part *part)
{
	fz_free(part->name);
	fz_free(part->data);
	fz_free(part);
}

static inline int getshort(FILE *file)
{
	int a = getc(file);
	int b = getc(file);
	return a | (b << 8);
}

static inline int getlong(FILE *file)
{
	int a = getc(file);
	int b = getc(file);
	int c = getc(file);
	int d = getc(file);
	return a | (b << 8) | (c << 16) | (d << 24);
}

static void *
xps_zip_alloc_items(xps_context *ctx, int items, int size)
{
	return fz_calloc(items, size);
}

static void
xps_zip_free(xps_context *ctx, void *ptr)
{
	fz_free(ptr);
}

static int
xps_compare_entries(const void *a0, const void *b0)
{
	xps_entry *a = (xps_entry*) a0;
	xps_entry *b = (xps_entry*) b0;
	return xps_strcasecmp(a->name, b->name);
}

static xps_entry *
xps_find_zip_entry(xps_context *ctx, char *name)
{
	int l = 0;
	int r = ctx->zip_count - 1;
	while (l <= r)
	{
		int m = (l + r) >> 1;
		int c = xps_strcasecmp(name, ctx->zip_table[m].name);
		if (c < 0)
			r = m - 1;
		else if (c > 0)
			l = m + 1;
		else
			return &ctx->zip_table[m];
	}
	return NULL;
}

static int
xps_read_zip_entry(xps_context *ctx, xps_entry *ent, unsigned char *outbuf)
{
	z_stream stream;
	unsigned char *inbuf;
	int sig;
	int version, general, method;
	int namelength, extralength;
	int code;

	fseek(ctx->file, ent->offset, 0);

	sig = getlong(ctx->file);
	if (sig != ZIP_LOCAL_FILE_SIG)
		return fz_throw("wrong zip local file signature (0x%x)", sig);

	version = getshort(ctx->file);
	general = getshort(ctx->file);
	method = getshort(ctx->file);
	(void) getshort(ctx->file); /* file time */
	(void) getshort(ctx->file); /* file date */
	(void) getlong(ctx->file); /* crc-32 */
	(void) getlong(ctx->file); /* csize */
	(void) getlong(ctx->file); /* usize */
	namelength = getshort(ctx->file);
	extralength = getshort(ctx->file);

	fseek(ctx->file, namelength + extralength, 1);

	if (method == 0)
	{
		fread(outbuf, 1, ent->usize, ctx->file);
	}
	else if (method == 8)
	{
		inbuf = fz_malloc(ent->csize);

		fread(inbuf, 1, ent->csize, ctx->file);

		memset(&stream, 0, sizeof(z_stream));
		stream.zalloc = (alloc_func) xps_zip_alloc_items;
		stream.zfree = (free_func) xps_zip_free;
		stream.opaque = ctx;
		stream.next_in = inbuf;
		stream.avail_in = ent->csize;
		stream.next_out = outbuf;
		stream.avail_out = ent->usize;

		code = inflateInit2(&stream, -15);
		if (code != Z_OK)
			return fz_throw("zlib inflateInit2 error: %s", stream.msg);
		code = inflate(&stream, Z_FINISH);
		if (code != Z_STREAM_END)
		{
			inflateEnd(&stream);
			return fz_throw("zlib inflate error: %s", stream.msg);
		}
		code = inflateEnd(&stream);
		if (code != Z_OK)
			return fz_throw("zlib inflateEnd error: %s", stream.msg);

		fz_free(inbuf);
	}
	else
	{
		return fz_throw("unknown compression method (%d)", method);
	}

	return fz_okay;
}

/*
 * Read the central directory in a zip file.
 */

static int
xps_read_zip_dir(xps_context *ctx, int start_offset)
{
	int sig;
	int offset, count;
	int namesize, metasize, commentsize;
	int i;

	fseek(ctx->file, start_offset, 0);

	sig = getlong(ctx->file);
	if (sig != ZIP_END_OF_CENTRAL_DIRECTORY_SIG)
		return fz_throw("wrong zip end of central directory signature (0x%x)", sig);

	(void) getshort(ctx->file); /* this disk */
	(void) getshort(ctx->file); /* start disk */
	(void) getshort(ctx->file); /* entries in this disk */
	count = getshort(ctx->file); /* entries in central directory disk */
	(void) getlong(ctx->file); /* size of central directory */
	offset = getlong(ctx->file); /* offset to central directory */

	ctx->zip_count = count;
	ctx->zip_table = fz_calloc(count, sizeof(xps_entry));
	memset(ctx->zip_table, 0, sizeof(xps_entry) * count);

	fseek(ctx->file, offset, 0);

	for (i = 0; i < count; i++)
	{
		sig = getlong(ctx->file);
		if (sig != ZIP_CENTRAL_DIRECTORY_SIG)
			return fz_throw("wrong zip central directory signature (0x%x)", sig);

		(void) getshort(ctx->file); /* version made by */
		(void) getshort(ctx->file); /* version to extract */
		(void) getshort(ctx->file); /* general */
		(void) getshort(ctx->file); /* method */
		(void) getshort(ctx->file); /* last mod file time */
		(void) getshort(ctx->file); /* last mod file date */
		(void) getlong(ctx->file); /* crc-32 */
		ctx->zip_table[i].csize = getlong(ctx->file);
		ctx->zip_table[i].usize = getlong(ctx->file);
		namesize = getshort(ctx->file);
		metasize = getshort(ctx->file);
		commentsize = getshort(ctx->file);
		(void) getshort(ctx->file); /* disk number start */
		(void) getshort(ctx->file); /* int file atts */
		(void) getlong(ctx->file); /* ext file atts */
		ctx->zip_table[i].offset = getlong(ctx->file);

		ctx->zip_table[i].name = fz_malloc(namesize + 1);
		fread(ctx->zip_table[i].name, 1, namesize, ctx->file);
		ctx->zip_table[i].name[namesize] = 0;

		fseek(ctx->file, metasize, 1);
		fseek(ctx->file, commentsize, 1);
	}

	qsort(ctx->zip_table, count, sizeof(xps_entry), xps_compare_entries);

	return fz_okay;
}

static int
xps_find_and_read_zip_dir(xps_context *ctx)
{
	int filesize, back, maxback;
	int i, n;
	char buf[512];

	fseek(ctx->file, 0, SEEK_END);
	filesize = ftell(ctx->file);

	maxback = MIN(filesize, 0xFFFF + sizeof buf);
	back = MIN(maxback, sizeof buf);

	while (back < maxback)
	{
		fseek(ctx->file, filesize - back, 0);

		n = fread(buf, 1, sizeof buf, ctx->file);
		if (n < 0)
			return fz_throw("cannot read end of central directory");

		for (i = n - 4; i > 0; i--)
			if (!memcmp(buf + i, "PK\5\6", 4))
				return xps_read_zip_dir(ctx, filesize - back + i);

		back += sizeof buf - 4;
	}

	return fz_throw("cannot find end of central directory");
}

/*
 * Read and interleave split parts from a ZIP file.
 */
static xps_part *
xps_read_zip_part(xps_context *ctx, char *partname)
{
	char buf[2048];
	xps_entry *ent;
	xps_part *part;
	int count, size, offset, i;
	char *name;

	name = partname;
	if (name[0] == '/')
		name ++;

	/* All in one piece */
	ent = xps_find_zip_entry(ctx, name);
	if (ent)
	{
		part = xps_new_part(ctx, partname, ent->usize);
		xps_read_zip_entry(ctx, ent, part->data);
		return part;
	}

	/* Count the number of pieces and their total size */
	count = 0;
	size = 0;
	while (1)
	{
		sprintf(buf, "%s/[%d].piece", name, count);
		ent = xps_find_zip_entry(ctx, buf);
		if (!ent)
		{
			sprintf(buf, "%s/[%d].last.piece", name, count);
			ent = xps_find_zip_entry(ctx, buf);
		}
		if (!ent)
			break;
		count ++;
		size += ent->usize;
	}

	/* Inflate the pieces */
	if (count)
	{
		part = xps_new_part(ctx, partname, size);
		offset = 0;
		for (i = 0; i < count; i++)
		{
			if (i < count - 1)
				sprintf(buf, "%s/[%d].piece", name, i);
			else
				sprintf(buf, "%s/[%d].last.piece", name, i);
			ent = xps_find_zip_entry(ctx, buf);
			xps_read_zip_entry(ctx, ent, part->data + offset);
			offset += ent->usize;
		}
		return part;
	}

	return NULL;
}

/*
 * Read and interleave split parts from files in the directory.
 */
static xps_part *
xps_read_dir_part(xps_context *ctx, char *name)
{
	char buf[2048];
	xps_part *part;
	FILE *file;
	int count, size, offset, i, n;

	fz_strlcpy(buf, ctx->directory, sizeof buf);
	fz_strlcat(buf, name, sizeof buf);

	/* All in one piece */
	file = fopen(buf, "rb");
	if (file)
	{
		fseek(file, 0, SEEK_END);
		size = ftell(file);
		fseek(file, 0, SEEK_SET);
		part = xps_new_part(ctx, name, size);
		fread(part->data, 1, size, file);
		fclose(file);
		return part;
	}

	/* Count the number of pieces and their total size */
	count = 0;
	size = 0;
	while (1)
	{
		sprintf(buf, "%s%s/[%d].piece", ctx->directory, name, count);
		file = fopen(buf, "rb");
		if (!file)
		{
			sprintf(buf, "%s%s/[%d].last.piece", ctx->directory, name, count);
			file = fopen(buf, "rb");
		}
		if (!file)
			break;
		count ++;
		fseek(file, 0, SEEK_END);
		size += ftell(file);
		fclose(file);
	}

	/* Inflate the pieces */
	if (count)
	{
		part = xps_new_part(ctx, name, size);
		offset = 0;
		for (i = 0; i < count; i++)
		{
			if (i < count - 1)
				sprintf(buf, "%s%s/[%d].piece", ctx->directory, name, i);
			else
				sprintf(buf, "%s%s/[%d].last.piece", ctx->directory, name, i);
			file = fopen(buf, "rb");
			n = fread(part->data + offset, 1, size - offset, file);
			offset += n;
			fclose(file);
		}
		return part;
	}

	return NULL;
}

xps_part *
xps_read_part(xps_context *ctx, char *partname)
{
	if (ctx->directory)
		return xps_read_dir_part(ctx, partname);
	return xps_read_zip_part(ctx, partname);
}

int
xps_open_file(xps_context *ctx, char *filename)
{
	char buf[2048];
	int code;
	char *p;

	ctx->file = fopen(filename, "rb");
	if (!ctx->file)
		return fz_throw("cannot open file: '%s'", filename);

	if (strstr(filename, "/_rels/.rels") || strstr(filename, "\\_rels\\.rels"))
	{
		fz_strlcpy(buf, filename, sizeof buf);
		p = strstr(buf, "/_rels/.rels");
		if (!p)
			p = strstr(buf, "\\_rels\\.rels");
		*p = 0;
		ctx->directory = fz_strdup(buf);
	}
	else
	{
		code = xps_find_and_read_zip_dir(ctx);
		if (code < 0)
			return fz_rethrow(code, "cannot read zip central directory");
	}

	code = xps_read_page_list(ctx);
	if (code)
		return fz_rethrow(code, "cannot read page list");

	return fz_okay;
}

xps_context *
xps_new_context(void)
{
	xps_context *ctx;

	ctx = fz_malloc(sizeof(xps_context));

	memset(ctx, 0, sizeof(xps_context));

	ctx->font_table = xps_hash_new();
	ctx->colorspace_table = xps_hash_new();

	ctx->start_part = NULL;

	return ctx;
}

static void xps_free_key_func(void *ptr)
{
	fz_free(ptr);
}

static void xps_free_font_func(void *ptr)
{
	fz_dropfont(ptr);
}

static void xps_free_colorspace_func(void *ptr)
{
	fz_dropcolorspace(ptr);
}

int
xps_free_context(xps_context *ctx)
{
	int i;

	if (ctx->file)
		fclose(ctx->file);

	if (ctx->start_part)
		fz_free(ctx->start_part);

	for (i = 0; i < ctx->zip_count; i++)
		fz_free(ctx->zip_table[i].name);
	fz_free(ctx->zip_table);

	xps_hash_free(ctx->font_table, xps_free_key_func, xps_free_font_func);
	xps_hash_free(ctx->colorspace_table, xps_free_key_func, xps_free_colorspace_func);

	xps_free_page_list(ctx);

	fz_free(ctx);

	return 0;
}
