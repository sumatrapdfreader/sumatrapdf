// Copyright (C) 2004-2022 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"

/*
	For the purposes of this code, and to save my tiny brain from
	overload, we will adopt the following notation:

	1) The PDF file contains bytes of data. These bytes are looked
	up in the MuPDF font handling to resolve to 'glyph ids' (gids).
	These account for all the different encodings etc in use,
	including the 'cmap' table within the font.

	2) We are given the list of gids that are used in the document.
	We arrange to keep any entries in the cmap or post tables that
	maps to these gids.

	We map the gids to the bottom of the range. This means that the
	cmap and post tables need to be updated.

	A similar optimisation would be to compress the range of cids
	used to a prefix of the range used. This would mean that the
	calling code needs to rewrite the data within the PDF file -
	both in terms of the strings used with the PDF streams, and in
	terms of the ToUnicode tables there (and the Widths etc).

	For now, we'll ignore this optimisation.

	Possibly, in the case of 'Identity' Tounicode mappings we
	wouldn't actually want to do this range compression? It'd only
	make the file larger.
*/

typedef struct
{
	uint16_t pid;
	uint16_t psid;

	uint32_t max;
	uint16_t gid[256];
} encoding_t;

typedef struct
{
	uint32_t tag;
	uint32_t checksum;
	fz_buffer *tab;
} tagged_table_t;

typedef struct
{
	int is_otf;
	int symbolic;
	encoding_t *encoding;
	uint32_t orig_num_glyphs;
	uint32_t new_num_glyphs;
	uint16_t index_to_loc_format;
	uint8_t *index_to_loc_formatp;
	uint16_t orig_num_long_hor_metrics;
	uint16_t new_num_long_hor_metrics;

	/* Pointer to the old tables (in the tagged table below) */
	uint8_t *loca;
	size_t *loca_len;
	uint8_t *maxp;

	/* Maps from old gid to new gid */
	uint16_t *gid_renum;

	int max;
	int len;
	tagged_table_t *table;
} ttf_t;

static uint32_t
checksum(fz_buffer *buf)
{
	size_t i;
	const uint8_t *d = (const uint8_t *)buf->data;
	uint32_t cs = 0;

	for (i = buf->len>>2; i > 0; i--)
	{
		cs += d[0]<<24;
		cs += d[1]<<16;
		cs += d[2]<<8;
		cs += d[3];
		d += 4;
	}
	i = buf->len - (buf->len & ~3);
	switch (i)
	{
	case 3:
		cs += d[2]<<8;
		/* fallthrough */
	case 2:
		cs += d[1]<<16;
		/* fallthrough */
	case 1:
		cs += d[0]<<24;
	default:
		break;
	}

	return cs;
}

static uint32_t
find_table(fz_context *ctx, fz_stream *stm, uint32_t tag, uint32_t *len)
{
	int num_tables;
	int i;

	fz_seek(ctx, stm, 4, SEEK_SET);
	num_tables = fz_read_int16(ctx, stm);
	fz_seek(ctx, stm, 12, SEEK_SET);

	for (i = 0; i < num_tables; i++)
	{
		uint32_t t = fz_read_uint32(ctx, stm);
		uint32_t cs = fz_read_uint32(ctx, stm);
		uint32_t off = fz_read_uint32(ctx, stm);
		cs = cs; /* UNUSED */
		*len = fz_read_uint32(ctx, stm);
		if (t == tag)
			return off;
	}

	return 0;
}

static fz_buffer *
read_table(fz_context *ctx, fz_stream *stm, uint32_t tag, int compulsory)
{
	uint32_t size;
	uint32_t off = find_table(ctx, stm, tag, &size);
	fz_buffer *buf;

	if (off == 0)
	{
		if (compulsory)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Required %c%c%c%c table missing", tag>>24, (tag>>16)&0xff, (tag>>8)&0xff, tag & 0xff);
		return NULL;
	}

	fz_seek(ctx, stm, off, SEEK_SET);
	buf = fz_new_buffer(ctx, size);

	fz_try(ctx)
	{
		fz_read(ctx, stm, buf->data, size);
		buf->len = size;
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_rethrow(ctx);
	}

	return buf;
}

#define TAG(s) \
	(	(((uint8_t)s[0])<<24) | \
		(((uint8_t)s[1])<<16) | \
		(((uint8_t)s[2])<<8) | \
		(((uint8_t)s[3])))

static void
add_table(fz_context *ctx, ttf_t *ttf, uint32_t tag, fz_buffer *tab)
{
	fz_try(ctx)
	{
		if (ttf->max == ttf->len)
		{
			int n = ttf->max * 2;
			if (n == 0)
				n = 16;
			ttf->table = fz_realloc(ctx, ttf->table, sizeof(*ttf->table) * n);
			ttf->max = n;
		}

		ttf->table[ttf->len].tag = tag;
		ttf->table[ttf->len].tab = tab;
		ttf->len++;
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, tab);
		fz_rethrow(ctx);
	}
}

static void
copy_table(fz_context *ctx, ttf_t *ttf, fz_stream *stm, uint32_t tag, int compulsory)
{
	fz_buffer *t;

	t = read_table(ctx, stm, tag, compulsory);
	if (t)
		add_table(ctx, ttf, tag, t);
}

static int
tabcmp(const void *a_, const void *b_)
{
	const tagged_table_t *a = (const tagged_table_t *)a_;
	const tagged_table_t *b = (const tagged_table_t *)b_;

	return (a->tag - b->tag);
}

static void
sort_tables(fz_context *ctx, ttf_t *ttf)
{
	qsort(ttf->table, ttf->len, sizeof(tagged_table_t), tabcmp);
}

static void
checksum_tables(fz_context *ctx, ttf_t *ttf)
{
	int i;

	for (i = 0; i < ttf->len; i++)
		ttf->table[i].checksum = checksum(ttf->table[i].tab);
}

static void
write_tables(fz_context *ctx, ttf_t *ttf, fz_output *out)
{
	int i = 0;
	uint32_t offset;

	/* scalar type - TTF for now - may need to cope with other types later. */
	if (ttf->is_otf)
		fz_write_int32_be(ctx, out, 0x4f54544f);
	else
		fz_write_int32_be(ctx, out, 0x00010000);

	/* number of tables */
	fz_write_uint16_be(ctx, out, ttf->len);

	while (1<<(i+1) < ttf->len)
		i++;

	/* searchRange */
	fz_write_uint16_be(ctx, out, (1<<i)<<4);

	/* entrySelector */
	fz_write_uint16_be(ctx, out, i);

	/* rangeShift*/
	fz_write_uint16_be(ctx, out, (ttf->len - (1<<i))<<4);

	/* Table directory */
	offset = 12 + ttf->len * 16;
	for (i = 0; i < ttf->len; i++)
	{
		fz_write_uint32_be(ctx, out, ttf->table[i].tag);
		fz_write_uint32_be(ctx, out, ttf->table[i].checksum);
		fz_write_uint32_be(ctx, out, offset);
		fz_write_uint32_be(ctx, out, (uint32_t)ttf->table[i].tab->len);
		offset += (uint32_t)ttf->table[i].tab->len;
	}

	/* Now the tables in turn */
	for (i = 0; i < ttf->len; i++)
	{
		fz_write_buffer(ctx, out, ttf->table[i].tab);
	}
}

static void
fix_checksum(fz_context *ctx, fz_buffer *buf)
{
	uint8_t *data;
	uint32_t sum = 0;
	size_t len = fz_buffer_storage(ctx, buf, &data);
	uint32_t namesize;
	fz_stream *stm = fz_open_buffer(ctx, buf);
	uint32_t csumpos = find_table(ctx, stm, TAG("head"), &namesize) + 8;

	len = len; // UNUSED

	fz_drop_stream(ctx, stm);

	/* First off, blat the old checksum */
	memset(data+csumpos, 0, 4);

	sum = checksum(buf);
	sum = 0xb1b0afba-sum;

	/* Insert it. */
	data[csumpos] = sum>>24;
	data[csumpos+1] = sum>>16;
	data[csumpos+2] = sum>>8;
	data[csumpos+3] = sum;
}

typedef struct
{
	uint16_t platform_id;
	uint16_t platform_specific_id;
	uint16_t language_id;
	uint16_t name_id;
	uint16_t len;
	uint16_t offset;
} name_record_t;

static uint32_t get32(const uint8_t *d)
{
	return (d[0]<<24)|(d[1]<<16)|(d[2]<<8)|d[3];
}

static uint32_t get16(const uint8_t *d)
{
	return (d[0]<<8)|d[1];
}

static void put32(uint8_t *d, uint32_t v)
{
	d[0] = v>>24;
	d[1] = v>>16;
	d[2] = v>>8;
	d[3] = v;
}

static void put16(uint8_t *d, uint32_t v)
{
	d[0] = v>>8;
	d[1] = v;
}

typedef struct
{
	/* First 2 fields aren't actually needed for the pointer list
	 * operation, but they serve as bounds for all the offsets used
	 * within the ptr list. */
	uint8_t *block;
	size_t block_len;

	uint32_t len;
	uint32_t max;
	uint8_t **ptr;
} ptr_list_t;

static void
ptr_list_add(fz_context *ctx, ptr_list_t *pl, uint8_t *ptr)
{
	if (pl->len == pl->max)
	{
		int n = pl->max * 2;
		if (n == 0)
			n = 32;
		pl->ptr = fz_realloc(ctx, pl->ptr, sizeof(*pl->ptr) * n);
		pl->max = n;
	}
	pl->ptr[pl->len++] = ptr;
}

typedef int (cmp_t)(const uint8_t **a, const uint8_t **b);
typedef int (void_cmp_t)(const void *, const void *);

static void
ptr_list_sort(fz_context *ctx, ptr_list_t *pl, cmp_t *cmp)
{
	qsort(pl->ptr, pl->len, sizeof(*pl->ptr), (void_cmp_t *)cmp);
}

static void
drop_ptr_list(fz_context *ctx, ptr_list_t *pl)
{
	fz_free(ctx, pl->ptr);
}

/* return 1 to keep, 0 to drop. */
typedef int (filter_t)(const uint8_t *ptr, const uint8_t *blk, size_t len);

/* This makes a pointer list from a filtered block, moving the underlying data as it filters. */
static void
ptr_list_compact(fz_context *ctx, ptr_list_t *pl, filter_t *fil, uint8_t *base, int n, size_t eltsize, uint8_t *block, size_t block_len)
{
	int i;
	uint8_t *s = base;
	uint8_t *d = base;

	pl->block = block;
	pl->block_len = block_len;

	if (base < block || (size_t)(base - block) > block_len || (size_t)(base - block) + n * eltsize >= block_len)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Ptr List creation failed");

	for (i = 0; i < n; i++)
	{
		if (fil(s, block, block_len))
		{
			ptr_list_add(ctx, pl, d);
			if (s != d)
				memmove(d, s, eltsize);
			d += eltsize;
		}
		s += eltsize;
	}
}

static int
names_by_size(const uint8_t **a, const uint8_t **b)
{
	return get16((*b)+8) - get16((*a)+8);
}

static int
filter_name_tables(const uint8_t *ptr, const uint8_t *block, size_t block_len)
{
	/* FIXME: For now, we keep everything. */
	return 1;
}

#define UNFOUND ((uint32_t)-1)

static uint32_t
find_string_in_block(const uint8_t *str, size_t str_len, const uint8_t *block, size_t block_len)
{
	const uint8_t *b = block;

	if (block_len == 0)
		return UNFOUND;

	assert(block_len >= str_len);

	block_len -= str_len-1;

	while (block_len--)
	{
		if (!memcmp(str, b, str_len))
			return (uint32_t)(b - block);
		b++;
	}

	return UNFOUND;
}

static void
subset_name_table(fz_context *ctx, ttf_t *ttf, fz_stream *stm)
{
	fz_buffer *t = read_table(ctx, stm, TAG("name"), 0);
	uint8_t *d;
	uint32_t i, n, off;
	ptr_list_t pl = { 0 };
	size_t name_data_size;
	uint8_t *new_name_data = NULL;
	size_t new_len;

	if (t == NULL)
		return; /* No name table */

	d = t->data;

	fz_var(new_name_data);

	fz_try(ctx)
	{
		if (get16(d) != 0 || t->len < 6)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Unsupported name table format");

		n = get16(d+2);
		off = get16(d+4);
		name_data_size = t->len - 6 - 12*n;

		if (t->len < 6 + 12*n)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated name table");

		ptr_list_compact(ctx, &pl, filter_name_tables, d+6, n, 12, d, t->len);

		/* Sort our list so that the ones with the largest name data blocks come first. */
		ptr_list_sort(ctx, &pl, names_by_size);

		new_name_data = fz_malloc(ctx, name_data_size);
		new_len = 0;
		for (i = 0; i < pl.len; i++)
		{
			uint32_t name_len = get16(pl.ptr[i] + 8);
			uint8_t *name = d+off+get16(pl.ptr[i] + 10);
			uint32_t offset = find_string_in_block(name, name_len, new_name_data, new_len);
			if (offset == UNFOUND)
			{
				memcpy(new_name_data + new_len, name, name_len);
				offset = (uint32_t)new_len;
				new_len += name_len;
			}
			put16(pl.ptr[i]+10, offset);
		}
		memcpy(d + 6 + 12*pl.len, new_name_data, new_len);
		t->len = 6 + 12*pl.len + new_len;
		put16(d+4, 6 + 12*pl.len);
	}
	fz_always(ctx)
	{
		drop_ptr_list(ctx, &pl);
		fz_free(ctx, new_name_data);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, t);
		fz_rethrow(ctx);
	}

	add_table(ctx, ttf, TAG("name"), t);
}

static encoding_t *
load_enc_tab0(fz_context *ctx, uint8_t *d, size_t data_size, uint32_t offset)
{
	encoding_t *enc;
	int i;

	if (data_size < 262)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated cmap 0 format table");

	enc = fz_malloc_struct(ctx, encoding_t);
	d += offset + 6;

	enc->max = 256;
	for (i = 0; i < 256; i++)
		enc->gid[i] = d[i];

	return enc;
}

static encoding_t *
load_enc_tab4(fz_context *ctx, uint8_t *d, size_t data_size, uint32_t offset)
{
	encoding_t *enc;
	uint16_t seg_count;
	uint32_t i;

	if (data_size < 26)
		fz_throw(ctx, FZ_ERROR_FORMAT, "cmap4 too small");

	seg_count = get16(d+offset+6); /* 2 * seg_count */

	if (seg_count & 1)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed cmap4 table");
	seg_count >>= 1;

	enc = fz_malloc_struct(ctx, encoding_t);
	enc->max = 256;

	/* Run through the segments, counting how many are used. */
	for (i = 0; i < seg_count; i++)
	{
		uint16_t seg_end = get16(d + offset + 14 + 2 * i);
		uint16_t seg_start = get16(d + offset + 14 + 2 * seg_count + 2 + 2 * i);
		uint16_t delta = get16(d + offset + 14 + 4 * seg_count + 2 + 2 * i);
		uint32_t offset_ptr = offset + 14 + 6 * seg_count + 2 + 2 * i;
		uint16_t offset = get16(d + offset_ptr);
		uint16_t target;
		uint32_t s;

		for (s = seg_start; s <= seg_end && s < enc->max; s++)
		{
			if (offset == 0)
			{
				target = delta + s;
			}
			else
			{
				/* Yes. This is very screwy. The offset is from the offset_ptr in use. */
				target = get16(d + offset_ptr + offset + 2 * (s - seg_start));
				if (target != 0)
					target += delta;
			}

			if (target != 0)
				enc->gid[s] = target;
		}
	}

	return enc;
}

static encoding_t *
load_enc_tab6(fz_context *ctx, uint8_t *d, size_t data_size, uint32_t offset)
{
	encoding_t *enc;
	uint16_t first_code;
	uint16_t entry_count;
	uint16_t length;
	uint32_t i;

	if (data_size < 10)
		fz_throw(ctx, FZ_ERROR_FORMAT, "cmap6 too small");

	length = get16(d+offset+2);
	first_code = get16(d+offset+6);
	entry_count = get16(d+offset+8);

	if (length < entry_count*2 + 10)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed cmap6 table");

	enc = fz_calloc(ctx, 1, sizeof(encoding_t) + sizeof(uint16_t) * (first_code + entry_count - 256));
	enc->max = first_code + entry_count;

	/* Run through the segments, counting how many are used. */
	for (i = 0; i < entry_count; i++)
	{
		enc->gid[first_code+i] = get16(d+offset+10+i*2);
	}

	return enc;
}

static encoding_t *
load_enc(fz_context *ctx, fz_buffer *t, int pid, int psid)
{
	uint8_t *d = t->data;
	size_t data_size = t->len;
	uint32_t i, n;

	if (data_size < 6 || get16(d) != 0)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Unsupported cmap table format");

	n = get16(d+2);

	if (data_size < 4 + 8*n)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated cmap table");

	for (i = 0; i < n; i++)
	{
		uint16_t plat_id = get16(d + 4 + i * 8);
		uint16_t plat_spec_id = get16(d + 4 + i * 8 + 2);
		uint32_t offset = get32(d + 4 + i * 8 + 4);
		uint16_t fmt;
		encoding_t *enc;

		if (plat_id != pid || plat_spec_id != psid)
			continue;

		if (offset < 4 + 8 * n || offset + 2 >= data_size)
			fz_throw(ctx, FZ_ERROR_FORMAT, "cmap table data out of range");

		fmt = get16(d+offset);
		switch(fmt)
		{
		case 0:
			enc = load_enc_tab0(ctx, d, data_size, offset);
			break;
		case 4:
			enc = load_enc_tab4(ctx, d, data_size, offset);
			break;
		case 6:
			enc = load_enc_tab6(ctx, d, data_size, offset);
			break;
		default:
			fz_throw(ctx, FZ_ERROR_FORMAT, "Unsupported cmap table format %d", fmt);
		}

		enc->pid = pid;
		enc->psid = psid;

		return enc;
	}

	return NULL;
}

static void
load_encoding(fz_context *ctx, ttf_t *ttf, fz_stream *stm)
{
	fz_buffer *t = read_table(ctx, stm, TAG("cmap"), 1);
	encoding_t *enc = NULL;

	fz_var(enc);

	fz_try(ctx)
	{
		if (ttf->symbolic)
		{
			/* PDF Spec says that for symbolic fonts we look for (1,0). */
			/* (3, 0) may also be present, but we'll just use (1, 0) for
			 * now. If we find files with a (3,0), but not a (1,0), then
			 * we'll deal with that then. */
			enc = load_enc(ctx, t, 1, 0);
		}
		else
		{
			/* For non symbolic fonts, we look for (3,1) then (1,0). */
			enc = load_enc(ctx, t, 3, 1);
			if (!enc)
				enc = load_enc(ctx, t, 1, 0);
		}
		if (!enc)
			fz_throw(ctx, FZ_ERROR_FORMAT, "No suitable cmap table found");
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, t);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	ttf->encoding = enc;
}

static void
reduce_encoding(fz_context *ctx, ttf_t *ttf, int *gids, int num_gids)
{
	int i;
	encoding_t *enc = ttf->encoding;
	int n = enc->max;

	for (i = 0; i < n; i++)
	{
		int gid = enc->gid[i];
		int lo, hi;

		if (gid == 0)
			continue;

		lo = 0;
		hi = num_gids;
		while (lo < hi)
		{
			int mid = (lo + hi)>>1;
			int g = gids[mid];
			if (g < gid)
				lo = mid+1;
			else if (g > gid)
				hi = mid;
			else
				goto found; /* Leave this one as is. */
		}

		/* Not found */
		enc->gid[0] = 0;
	found:
		{}
	}
}

static void
make_cmap(fz_context *ctx, ttf_t *ttf)
{
	uint32_t i;
	uint32_t len;
	uint32_t segs = 0;
	uint32_t seg, seg_start, seg_end;
	encoding_t *enc = ttf->encoding;
	uint32_t n = enc->max;
	uint32_t entries = 0;
	fz_buffer *buf;
	uint8_t *d;
	uint32_t offset;

	/* Make a type 4 table. */

	/* Count the number of segments. */
	for (i = 0; i < n; i++)
	{
		if (enc->gid[i] == 0)
			continue;

		seg_start = i;
		seg_end = i;
		for (i++; i<n; i++)
		{
			if (enc->gid[i] != 0)
				seg_end = i;
			else if (i - seg_end > 4)
				break;
		}
		entries += seg_end - seg_start + 1;
		segs++;
	}
	segs++; /* For the terminator */




	len = 12 + 14 + 2 + segs * 2 * 4 + entries * 2;
	buf = fz_new_buffer(ctx, len);
	d = buf->data;

	/* cmap header */
	put16(d, 0); /* version */
	put16(d+2, 1); /* num sub tables */
	put16(d+4, enc->pid);
	put16(d+6, enc->psid);
	put32(d+8, 12); /* offset */
	d += 12;

	put16(d, 4); /* Format */
	put16(d + 2, len-12); /* Length */
	put16(d + 4, 0); /* FIXME: Language */
	put16(d + 6, segs * 2);
	i = 0;
	while (1U<<(i+1) <= segs)
		i++;
	/* So 1<<i <= segs < 1<<(i+1) */
	put16(d + 8, 1<<(i+1)); /* searchRange */
	put16(d + 10, i); /* entrySelector */
	put16(d + 12, 2 * segs - (1<<(i+1))); /* rangeShift */
	put16(d + 14 + segs * 2, 0); /* reserved */

	/* Now output the segment data */
	entries = 14 + segs * 2 * 4 + 2; /* offset of where to put entries.*/
	seg = 0;
	for (i = 0; i < n; i++)
	{
		if (enc->gid[i] == 0)
			continue;

		seg_start = i;
		seg_end = i;
		offset = 14 + segs * 2 * 3 + 2 + seg * 2;
		put16(d + offset - segs * 2, 0); /* Delta - always 0 for now. */
		put16(d + offset, entries - offset); /* offset */
		put16(d + entries, ttf->is_otf ? enc->gid[i] : ttf->gid_renum[enc->gid[i]]); /* Insert an entry */
		entries += 2;
		for (i++; i < n; i++)
		{
			if (enc->gid[i] != 0)
			{
				/* Include i in the range, which means we need to add entries for
				 * seg_end to i inclusive. */
				while (seg_end < i)
				{
					seg_end++;
					put16(d + entries, ttf->is_otf ? enc->gid[seg_end] : ttf->gid_renum[enc->gid[seg_end]]);
					entries += 2;
				}
			}
			else if (i - seg_end > 4)
				break;
		}
		put16(d + 14 + segs * 2 + seg * 2 + 2, seg_start);
		put16(d + 14 + seg * 2, seg_end);
		seg++;
	}
	offset = 14 + segs * 2 * 3 + 2 + seg * 2;
	put16(d + 14 + segs * 2 + seg * 2 + 2, 0xffff);
	put16(d + 14 + seg * 2, 0xffff);
	put16(d + offset - segs * 2, 1); /* Delta */
	put16(d + offset, 0); /* offset */
	buf->len = entries + 12;
	assert(buf->len == buf->cap);

	add_table(ctx, ttf, TAG("cmap"), buf);
}

static void
read_maxp(fz_context *ctx, ttf_t *ttf, fz_stream *stm)
{
	fz_buffer *t = read_table(ctx, stm, TAG("maxp"), 1);

	if (t->len < 6)
	{
		fz_drop_buffer(ctx, t);
		fz_throw(ctx, FZ_ERROR_FORMAT, "truncated maxp table");
	}

	ttf->orig_num_glyphs = get16(t->data+4);

	add_table(ctx, ttf, TAG("maxp"), t);
	ttf->maxp = t->data;
}

static void
read_head(fz_context *ctx, ttf_t *ttf, fz_stream *stm)
{
	uint32_t version;
	fz_buffer *t = read_table(ctx, stm, TAG("head"), 1);

	if (t->len < 54)
	{
		fz_drop_buffer(ctx, t);
		fz_throw(ctx, FZ_ERROR_FORMAT, "truncated head table");
	}

	version = get32(t->data);
	if (version != 0x00010000)
	{
		fz_drop_buffer(ctx, t);
		fz_throw(ctx, FZ_ERROR_FORMAT, "Unsupported head table version 0x%08x", version);
	}

	ttf->index_to_loc_formatp = t->data+50;
	ttf->index_to_loc_format = get16(ttf->index_to_loc_formatp);
	if (ttf->index_to_loc_format & ~1)
	{
		fz_drop_buffer(ctx, t);
		fz_throw(ctx, FZ_ERROR_FORMAT, "Unsupported index_to_loc_format 0x%04x", ttf->index_to_loc_format);
	}

	add_table(ctx, ttf, TAG("head"), t);
}

static void
read_loca(fz_context *ctx, ttf_t *ttf, fz_stream *stm)
{
	fz_buffer *t;
	uint32_t len = (2<<ttf->index_to_loc_format) * (ttf->orig_num_glyphs+1);

	t = read_table(ctx, stm, TAG("loca"), 1);

	if (t->len < len)
	{
		fz_drop_buffer(ctx, t);
		fz_throw(ctx, FZ_ERROR_FORMAT, "truncated loca table");
	}

	ttf->loca = t->data;
	ttf->loca_len = &t->len;

	add_table(ctx, ttf, TAG("loca"), t);
}

static void
read_hhea(fz_context *ctx, ttf_t *ttf, fz_stream *stm)
{
	uint32_t version;
	fz_buffer *t = read_table(ctx, stm, TAG("hhea"), 1);
	uint16_t i;

	if (t->len < 36)
	{
		fz_drop_buffer(ctx, t);
		fz_throw(ctx, FZ_ERROR_FORMAT, "truncated hhea table");
	}

	version = get32(t->data);
	if (version != 0x00010000)
	{
		fz_drop_buffer(ctx, t);
		fz_throw(ctx, FZ_ERROR_FORMAT, "Unsupported hhea table version 0x%08x", version);
	}

	ttf->orig_num_long_hor_metrics = get16(t->data+34);

	add_table(ctx, ttf, TAG("hhea"), t);

	/* Previously gids 0 to orig_num_long_hor_metrics-1 were described with
	 * hor metrics, and the ones afterwards were fixed widths. Find where
	 * that dividing line is in our new reduced set. */
	if (ttf->encoding && !ttf->is_otf)
	{
		ttf->new_num_long_hor_metrics = 0;
		for (i = ttf->orig_num_long_hor_metrics-1; i > 0; i--)
			if (ttf->gid_renum[i])
			{
				ttf->new_num_long_hor_metrics = ttf->gid_renum[i]+1;
				break;
			}

		put16(t->data+34, ttf->new_num_long_hor_metrics);
	}
	else
	{
		ttf->new_num_long_hor_metrics = ttf->orig_num_long_hor_metrics;
	}
}

static uint32_t
get_loca(fz_context *ctx, ttf_t *ttf, uint32_t n)
{
	if (ttf->index_to_loc_format == 0)
	{
		/* Short index - convert from words to bytes */
		return get16(ttf->loca + n*2) * 2;
	}
	else
	{
		/* Long index - in bytes already */
		return get32(ttf->loca + n*4);
	}
}

static void
put_loca(fz_context *ctx, ttf_t *ttf, uint32_t n, uint32_t off)
{
	if (ttf->index_to_loc_format == 0)
	{
		/* Short index - convert from bytes to words */
		assert((off & 1) == 0);
		put16(ttf->loca + n*2, off/2);
	}
	else
	{
		/* Long index - in bytes already */
		put32(ttf->loca + n*4, off);
	}
}

static void
glyph_used(fz_context *ctx, ttf_t *ttf, fz_buffer *glyf, uint16_t i)
{
	uint32_t offset, len;
	const uint8_t *data;
	uint16_t flags;

	if (ttf->gid_renum[i] != 0)
		return;

	if (i > ttf->orig_num_glyphs)
	{
		fz_warn(ctx, "TTF subsetting; gid > num_gids!");
		return;
	}

	ttf->gid_renum[i] = 1;

	/* If this glyf is composite, then we need to add any dependencies of it. */
	offset = get_loca(ctx, ttf, i);
	len = get_loca(ctx, ttf, i+1) - offset;
	if (len == 0)
		return;
	if (offset+2 > glyf->len)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Corrupt glyf data");
	data = glyf->data + offset;
	if ((int16_t)get16(data) >= 0)
		return; /* Single glyph - no dependencies */
	data += 4 * 2 + 2;
	if (len < 4*2 + 2)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Corrupt glyf data");
	len -= 4 * 2 + 2;
	do
	{
		uint16_t idx, skip;

		if (len < 4)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Corrupt glyf data");

		flags = get16(data);
		idx = get16(data+2);

		glyph_used(ctx, ttf, glyf, idx);

#define ARGS_1_AND_2_ARE_WORDS 1
#define ARGS_ARE_XY_VALUES 2
#define WE_HAVE_A_SCALE 8
#define MORE_COMPONENTS 32
#define WE_HAVE_AN_X_AND_Y_SCALE 64
#define WE_HAVE_A_TWO_BY_TWO 128

		/* Skip the X and Y offsets */
		if (flags & ARGS_1_AND_2_ARE_WORDS)
			skip = 4 + 4;
		else
			skip = 4 + 2;

		/* Skip the transformation */
		switch (flags & (WE_HAVE_A_SCALE + WE_HAVE_AN_X_AND_Y_SCALE + WE_HAVE_A_TWO_BY_TWO))
		{
		case 0:
			/* No extra to skip */
			break;
		case WE_HAVE_A_SCALE:
			skip += 2;
			break;
		case WE_HAVE_AN_X_AND_Y_SCALE:
			skip += 4;
			break;
		case WE_HAVE_A_TWO_BY_TWO:
			skip += 8;
			break;
		}
		if (len < skip)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Corrupt glyf data");
		data += skip;
		len -= skip;
	}
	while(flags & MORE_COMPONENTS);
}

static void
renumber_composite(fz_context *ctx, ttf_t *ttf, uint8_t *data, uint32_t len)
{
	uint16_t flags;

	data += 4 * 2 + 2;
	if (len < 4*2 + 2)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Corrupt glyf data");
	len -= 4 * 2 + 2;
	do
	{
		uint16_t skip;

		if (len < 4)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Corrupt glyf data");

		flags = get16(data);
		put16(data+2, ttf->gid_renum[get16(data+2)]);

		/* Skip the X and Y offsets */
		if (flags & ARGS_1_AND_2_ARE_WORDS)
			skip = 4 + 4;
		else
			skip = 4 + 2;

		/* Skip the transformation */
		switch (flags & (WE_HAVE_A_SCALE + WE_HAVE_AN_X_AND_Y_SCALE + WE_HAVE_A_TWO_BY_TWO))
		{
		case 0:
			/* No extra to skip */
			break;
		case WE_HAVE_A_SCALE:
			skip += 2;
			break;
		case WE_HAVE_AN_X_AND_Y_SCALE:
			skip += 4;
			break;
		case WE_HAVE_A_TWO_BY_TWO:
			skip += 8;
			break;
		}
		if (len < skip)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Corrupt glyf data");
		data += skip;
		len -= skip;
	}
	while(flags & MORE_COMPONENTS);
}

static void
read_glyf(fz_context *ctx, ttf_t *ttf, fz_stream *stm, int *gids, int num_gids)
{
	uint32_t len = get_loca(ctx, ttf, ttf->orig_num_glyphs);
	fz_buffer *t = read_table(ctx, stm, TAG("glyf"), 1);
	encoding_t *enc = ttf->encoding;
	uint32_t i, j;
	uint32_t new_start, old_start, old_end;

	if (t->len < len)
	{
		fz_drop_buffer(ctx, t);
		fz_throw(ctx, FZ_ERROR_FORMAT, "truncated glyf table");
	}

	add_table(ctx, ttf, TAG("glyf"), t);

	/* Now, make the renumber list for the glyphs. */
	ttf->gid_renum = fz_calloc(ctx, ttf->orig_num_glyphs, sizeof(uint16_t));

	/* Initially, we'll use it just as a usage list. 0 = unused, 1 used */

	/* glyph 0 is always used. */
	glyph_used(ctx, ttf, t, 0);

	if (enc)
	{
		uint32_t n = enc->max;
		/* If we have an encoding table, run through it, and keep anything needed from there. */
		for (i = 0; i < n; i++)
			if (enc->gid[i])
				glyph_used(ctx, ttf, t, enc->gid[i]);

		/* Now convert from a usage table to a renumbering table. */
		ttf->gid_renum[0] = 0;
		j = 1;
		for (i = 1; i < ttf->orig_num_glyphs; i++)
			if (ttf->gid_renum[i])
				ttf->gid_renum[i] = j++;
		ttf->new_num_glyphs = j;
	}
	else
	{
		/* We're a cid font. The cids are gids. */
		for (i = 0; i < (uint32_t)num_gids; i++)
			glyph_used(ctx, ttf, t, gids[i]);
		ttf->new_num_glyphs = ttf->orig_num_glyphs;
	}

	/* Now subset the glyf table. */
	new_start = 0;
	old_start = get_loca(ctx, ttf, 0);
	for (i = 0; i < ttf->orig_num_glyphs; i++)
	{
		old_end = get_loca(ctx, ttf, i+1);
		if (i == 0 || ttf->gid_renum[i] != 0)
		{
			len = old_end - old_start;
			memmove(t->data + new_start, t->data + old_start, len);
			if (enc)
			{
				if ((int16_t)get16(t->data + new_start) < 0)
					renumber_composite(ctx, ttf, t->data, (uint32_t)t->len);
				put_loca(ctx, ttf, ttf->gid_renum[i], new_start);
			}
			else
				put_loca(ctx, ttf, i, new_start);
			new_start += len;
		}
		else if (!enc)
			put_loca(ctx, ttf, i, new_start);
		old_start = old_end;
	}
	put_loca(ctx, ttf, ttf->new_num_glyphs, new_start);
	*ttf->loca_len = (ttf->new_num_glyphs+1) * (2<<ttf->index_to_loc_format);
	t->len = new_start;
}

static void
update_num_glyphs(fz_context *ctx, ttf_t *ttf)
{
	put16(ttf->maxp + 4, ttf->new_num_glyphs);
}

static void
subset_hmtx(fz_context *ctx, ttf_t *ttf, fz_stream *stm)
{
	fz_buffer *t = read_table(ctx, stm, TAG("hmtx"), 1);
	uint16_t i, max16;
	uint8_t *s = t->data;
	uint8_t *d = t->data;
	int cidfont = (ttf->encoding == NULL);
	size_t max = t->len;

	if (ttf->orig_num_long_hor_metrics * 4 > max)
	{
		fz_drop_buffer(ctx, t);
		fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed hmtx table");
	}
	max -= ttf->orig_num_long_hor_metrics * 4;
	max /= 2;
	if (max > ttf->orig_num_glyphs)
		max = ttf->orig_num_glyphs;
	/* We know orig_num_glyphs is 16bit, so this cast safe. */
	max16 = (uint16_t)max;

	for (i = 0; i < ttf->orig_num_long_hor_metrics; i++)
	{
		if (i == 0 || ttf->is_otf || ttf->gid_renum[i])
		{
			put32(d, get32(s));
			d += 4;
		}
		else if (cidfont)
		{
			put32(d, 0);
			d += 4;
		}
		s += 4;
	}
	for ( ; i < max16; i++)
	{
		if (i == 0 || ttf->is_otf || ttf->gid_renum[i])
		{
			put16(d, get16(s));
			d += 2;
		}
		else if (cidfont)
		{
			put16(d, 0);
			d += 2;
		}
		s += 2;
	}
	t->len = (d - t->data);

	add_table(ctx, ttf, TAG("hmtx"), t);
}

static void
shrink_loca_if_possible(fz_context *ctx, ttf_t *ttf)
{
	uint32_t len;
	uint16_t i, n;
	uint8_t *loca;

	if (ttf->index_to_loc_format == 0)
		return; /* Can't shrink cos it's already shrunk! */

	n = ttf->new_num_glyphs;
	len = get_loca(ctx, ttf, n);
	if (len >= 65536)
		return; /* We can't shrink it, cos it's too big. */

	loca = ttf->loca;
	for (i = 0; i <= n; i++)
	{
		if (get32(loca + 4*i) & 1)
			return; /* Can't shrink it, because an offset is not even */
	}

	for (i = 0; i <= n; i++)
	{
		put16(loca + 2*i, get32(loca + 4*i)/2);
	}
	*ttf->loca_len = 2*(n+1);
	put16(ttf->index_to_loc_formatp, 0);
}

static size_t
subset_post2(fz_context *ctx, ttf_t *ttf, uint8_t *d, size_t len, int *gids, int num_gids)
{
	int i, n;
	int j;
	fz_int2_heap heap = { 0 };
	uint8_t *d0, *e, *idx;

	if (len < 2 + 2 * ttf->orig_num_glyphs)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated post table");

	n = get16(d);
	if ((uint32_t)n != ttf->orig_num_glyphs)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed post table");

	d0 = d;
	d += 2; len -= 2;
	idx = d;

	/* Store all the indexes. */
	j = 0;
	if (len < (size_t)n*2)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed post table");
	len -= (size_t)n*2;
	for (i = 0; i < n; i++)
	{
		uint16_t o = get16(d);
		fz_int2 i2;
		d += 2;

		/* We're only keeping gids we want. */
		if (j >= num_gids || gids[j] != i)
		{
			put16(d-2, 0);
			continue;
		}

		/* We want this gid. */
		j++;

		/* 257 or smaller: same as in the basic order. */
		if (o <= 257)
			continue;

		/* Store the index. */
		i2.a = o - 258;
		i2.b = i;
		fz_int2_heap_insert(ctx, &heap, i2);
	}

	fz_int2_heap_sort(ctx, &heap);

	/* So, the heap is sorted on i2.a (the string indexes we want to keep),
	 * and i2.b is the gid that refers to that index. */

	/* Run through the list moving the strings down that we care about. */
	j = 0;
	e = d;
	n = heap.len;
	for (i = 0; i < n; i++)
	{
		uint8_t slen;

		if (len < 1)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed post table");
		slen = *d+1;
		if (len < slen)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed post table");
		len -= slen;

		if (j >= heap.len || heap.heap[j].a != i)
		{
			/* Drop this one. */
			d += slen;
		}

		memmove(e, d, slen);
		d += slen;
		e += slen;

		put16(idx + 2*i, 258 + j);
		j++;
	}

	fz_free(ctx, heap.heap);

	return e - d0;
}

static void
subset_post(fz_context *ctx, ttf_t *ttf, fz_stream *stm, int *gids, int num_gids)
{
	fz_buffer *t = read_table(ctx, stm, TAG("post"), 0);
	uint8_t *d;
	size_t len;
	uint32_t fmt;

	if (t == NULL)
		return;

	d = t->data;
	len = t->len;

	if (len < 32)
	{
		fz_drop_buffer(ctx, t);
		fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated post table");
	}

	fmt = get32(d);

	if (fmt != 0x00020000)
	{
		/* Fmt 1: Nothing to be gained by having this table. The cmap should
		 * have all the mappings anyway, and we'll have broken it by renumbering
		 * the gids down anyway. */
		/* Fmt 2.5 deprecated. */
		/* Fmt 3 and 4: should not be used for PDF. */
		/* No other formats defined. */
		fz_drop_buffer(ctx, t);
		return;
	}
	d += 32; len -= 32;
	len = subset_post2(ctx, ttf, d, len, gids, num_gids);

	t->len = len;

	add_table(ctx, ttf, TAG("post"), t);
}

static void
subset_CFF(fz_context *ctx, ttf_t *ttf, fz_stream *stm, int *gids, int num_gids, int symbolic, int cidfont)
{
	fz_buffer *t = read_table(ctx, stm, TAG("CFF "), 1);
	fz_buffer *sub = NULL;

	fz_var(sub);

	fz_try(ctx)
		sub = fz_subset_cff_for_gids(ctx, t, gids, num_gids, symbolic, cidfont);
	fz_always(ctx)
		fz_drop_buffer(ctx, t);
	fz_catch(ctx)
		fz_rethrow(ctx);

	add_table(ctx, ttf, TAG("CFF "), sub);
}

fz_buffer *
fz_subset_ttf_for_gids(fz_context *ctx, fz_buffer *orig, int *gids, int num_gids, int symbolic, int cidfont)
{
	fz_stream *stm = fz_open_buffer(ctx, orig);
	ttf_t ttf = { 0 };
	fz_buffer *newbuf = NULL;
	fz_output *out = NULL;

	fz_var(newbuf);
	fz_var(out);

	fz_try(ctx)
	{
		ttf.is_otf = (fz_read_uint32_le(ctx, stm) == 0x4f54544f);
		ttf.symbolic = symbolic;

		/* Subset the name table. No other dependencies. */
		subset_name_table(ctx, &ttf, stm);

		if (!cidfont)
		{
			/* Load the encoding. Populates the encoding table from the cmap table
			 * in the original. cmap table is then discarded. */
			load_encoding(ctx, &ttf, stm);

			/* Blank out the bits of the encoding we don't need. */
			reduce_encoding(ctx, &ttf, gids, num_gids);
		}

		/* Read maxp and store the table. Rememeber orig_num_glyphs. */
		read_maxp(ctx, &ttf, stm);

		/* Read head and store the table. Remember the loca index size. */
		read_head(ctx, &ttf, stm);

		if (ttf.is_otf)
		{
			subset_CFF(ctx, &ttf, stm, gids, num_gids, symbolic, cidfont);
		}

		/* Read loca and store it. Stash a pointer to the table for quick access. */
		if (!ttf.is_otf)
		{
			read_loca(ctx, &ttf, stm);

			/* Read the glyf data, and scan it for composites. This makes the gid_renum table,
			 * subsets the glyf data, and rewrites the loca table. */
			read_glyf(ctx, &ttf, stm, gids, num_gids);
		}

		/* Read hhea and store it. Remember numOfLongHorMetrics. */
		read_hhea(ctx, &ttf, stm);

		/* Read and subset hmtx. */
		subset_hmtx(ctx, &ttf, stm);

#ifdef DEBUG_SUBSETTING
		if (!cidfont)
		{
			encoding_t *enc = ttf.encoding;
			uint32_t i, n = enc->max;

			for (i = 0; i < n; i++)
				if (enc->gid[i])
					printf("cid %x '%c'-> orig gid %d -> gid %d\n", i, (char)i, enc->gid[i], ttf.gid_renum[enc->gid[i]]);
		}
		{
			uint32_t i;

			for (i = 0; i < ttf.orig_num_glyphs; i++)
				if (ttf.gid_renum[i])
					printf("gid %d -> %d\n", i, ttf.gid_renum[i]);

			for (i = 0; i <= ttf.new_num_glyphs; i++)
				printf("LOCA %d = %x\n", i, get_loca(ctx, &ttf, i));
		}
#endif
		if (!ttf.is_otf)
		{
			shrink_loca_if_possible(ctx, &ttf);

			update_num_glyphs(ctx, &ttf);
		}

		if (!cidfont)
		{
			/* Now we can make the new cmap. */
			make_cmap(ctx, &ttf);
		}

		if (!cidfont)
		{
			/* subset the post table */
			subset_post(ctx, &ttf, stm, gids, num_gids);
		}

		copy_table(ctx, &ttf, stm, TAG("OS/2"), 0);
		copy_table(ctx, &ttf, stm, TAG("cvt "), 0);
		copy_table(ctx, &ttf, stm, TAG("fpgm"), 0);
		copy_table(ctx, &ttf, stm, TAG("prep"), 0);

		sort_tables(ctx, &ttf);
		checksum_tables(ctx, &ttf);

		newbuf = fz_new_buffer(ctx, 1024);
		out = fz_new_output_with_buffer(ctx, newbuf);

		write_tables(ctx, &ttf, out);

		fz_close_output(ctx, out);

		fix_checksum(ctx, newbuf);
	}
	fz_always(ctx)
	{
		int i;

		fz_drop_output(ctx, out);
		fz_drop_stream(ctx, stm);
		for (i = 0; i < ttf.len; i++)
			fz_drop_buffer(ctx, ttf.table[i].tab);
		fz_free(ctx, ttf.table);
		fz_free(ctx, ttf.gid_renum);
		fz_free(ctx, ttf.encoding);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, newbuf);
		fz_rethrow(ctx);
	}

	return newbuf;
}
