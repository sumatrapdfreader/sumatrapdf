// Copyright (C) 2023 Artifex Software, Inc.
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

#include "lzxd-imp.h"

#include <string.h>
#include <limits.h>

typedef struct
{
	char *name;
	uint64_t offset;
	uint64_t section;
	uint64_t size;
} chm_entry;

typedef struct
{
	uint64_t offset;
	uint64_t length;
	int compressed;
} chm_section;

typedef struct
{
	fz_archive super;

	int max;
	int count;
	chm_entry *entries;

	chm_section section[2];
	uint32_t section0_offset;
	uint32_t chunk0_offset;
	uint64_t section1_block_table_offset;
	uint64_t section1_usize;
	uint64_t section1_csize;
	uint64_t section1_offset;
	uint64_t section1_data_len;
	uint32_t block_entries;
	uint32_t window_size;
	uint64_t reset_block_interval;
	fz_buffer *decomp;
} fz_chm_archive;

typedef struct
{
	fz_chm_archive *archive;
	chm_entry *entry;
	fz_lzxd_t *lzxd;
	uint8_t buffer[0x8000];
	uint8_t *comp;
	size_t comp_max;
	uint64_t next_decomp_block;
} chm_state;

static int
next_chm(fz_context *ctx, fz_stream *stm, size_t required)
{
	chm_state *state = stm->state;
	fz_chm_archive *chm = state->archive;
	uint64_t data_start;
	size_t data_len;

	if (stm->eof)
		return EOF;

	/* Most data acccesses will be in the compressed section. Do the
	 * uncompressed access first as it's simple. */
	if (state->entry->section == 0)
	{
		size_t n;
		size_t left = state->entry->size - stm->pos;
		if (left > sizeof(state->buffer))
			left = sizeof(state->buffer);
		if (left == 0)
		{
			stm->eof = 1;
			return EOF;
		}
		fz_seek(ctx, chm->super.file, chm->section0_offset + state->entry->offset + stm->pos, SEEK_SET);
		n = fz_read(ctx, chm->super.file, state->buffer, left);
		if (n < left)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Short read in CHM handling");
		stm->pos += n;
		stm->rp = state->buffer;
		stm->wp = stm->rp + left;
		return *stm->rp++;
	}

	if (state->entry->offset + state->entry->size <= state->next_decomp_block * 0x8000)
	{
		/* We've read it all already. */
		stm->rp = stm->wp = state->buffer;
		stm->eof = 1;
		return EOF;
	}

	do
	{
		/* Decompress the next block */
		uint64_t start, end;
		size_t n;
		/* Decompress the next block. How large is it? */
		fz_seek(ctx, chm->super.file, chm->section1_block_table_offset + 8 * state->next_decomp_block, SEEK_SET);
		start = fz_read_uint64_le(ctx, chm->super.file);
		state->next_decomp_block++;
		if (state->next_decomp_block < chm->block_entries)
			end = fz_read_uint64_le(ctx, chm->super.file);
		else
			end = chm->section1_data_len;
		if (end <= start || end > chm->section1_csize)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid CHM compressed data");

		/* Ensure our buffer is large enough for it. */
		if (state->comp_max < end-start)
		{
			fz_free(ctx, state->comp);
			state->comp = NULL;
			state->comp = fz_malloc(ctx, end-start);
			state->comp_max = end-start;
		}

		/* Read it in. */
		fz_seek(ctx, chm->super.file, chm->section1_offset + start, SEEK_SET);
		n = fz_read(ctx, chm->super.file, state->comp, end-start);
		if (n < end-start)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Short read in CHM handling");

		/* Do the actual decompression */
		fz_lzxd_decompress_chunk(ctx, state->lzxd, state->comp, end-start, state->buffer);

		/* If we haven't reached the start of our file yet, loop back to decompress the next block. */
	}
	while (state->next_decomp_block * 0x8000 <= state->entry->offset);

	/* So we have a block, and our data is guaranteed to be in there somewhere. */
	data_start = (state->next_decomp_block - 1) * 0x8000;
	data_len = 0x8000;

	stm->rp = state->buffer;
	if (data_start < state->entry->offset)
	{
		int64_t skip = state->entry->offset - data_start;
		stm->rp += skip;
		data_start += skip;
		data_len -= skip;
	}
	if (data_start + data_len > state->entry->offset + state->entry->size)
		data_len = state->entry->offset + state->entry->size - data_start;

	stm->wp = stm->rp + data_len;
	stm->pos += data_len;
	if (stm->rp == stm->wp)
	{
		stm->eof = 1;
		return EOF;
	}
	return *stm->rp++;
}

static void
close_chm_stream(fz_context *ctx, void *state_)
{
	chm_state *state = (chm_state *)state_;

	fz_drop_lzxd(ctx, state->lzxd);

	fz_drop_archive(ctx, &state->archive->super);
	fz_free(ctx, state->comp);
	fz_free(ctx, state);
}

static fz_stream *
chm_stream(fz_context *ctx, fz_chm_archive *archive, chm_entry *entry)
{
	chm_state *state;

	state = fz_malloc_struct(ctx, chm_state);
	state->entry = entry;
	state->archive = archive;

	if (entry->section == 0)
	{
		/* No prep work to do. */
	}
	else if (entry->section == 1)
	{
		/* Which block does our data start in? */
		state->next_decomp_block = state->entry->offset / 0x8000;
		/* We have to start decompressing from an appropriate reset interval.
		 * reset_interval == window_size. */
		state->next_decomp_block -= (state->next_decomp_block % archive->reset_block_interval);
		fz_try(ctx)
			state->lzxd = fz_new_lzxd(ctx, archive->window_size, archive->window_size);
		fz_catch(ctx)
		{
			fz_free(ctx, state);
			fz_rethrow(ctx);
		}
	}
	else
	{
		fz_free(ctx, state);
		fz_throw(ctx, FZ_ERROR_FORMAT, "CHM entry in invalid section");
	}

	(void)fz_keep_archive(ctx, &archive->super);

	return fz_new_stream(ctx, state, next_chm, close_chm_stream);
}

static void drop_chm_archive(fz_context *ctx, fz_archive *arch)
{
	fz_chm_archive *chm = (fz_chm_archive *) arch;
	int i;
	for (i = 0; i < chm->count; ++i)
		fz_free(ctx, chm->entries[i].name);
	fz_free(ctx, chm->entries);
}

static chm_entry *lookup_chm_entry(fz_context *ctx, fz_chm_archive *chm, const char *name)
{
	int i;
	if (name[0] == '/')
		++name;
	for (i = 0; i < chm->count; i++)
	{
		const char *ename = chm->entries[i].name;
		if (ename[0] == '/')
			ename++;
		if (!fz_strcasecmp(name, ename))
			return &chm->entries[i];
	}
	return NULL;
}

static fz_stream *open_chm_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_chm_archive *chm = (fz_chm_archive *) arch;
	chm_entry *ent = lookup_chm_entry(ctx, chm, name);

	if (!ent)
		return NULL;

	return chm_stream(ctx, chm, ent);
}

static fz_buffer *read_chm_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_chm_archive *chm = (fz_chm_archive *) arch;
	fz_buffer *buf = NULL;
	fz_stream *stm;
	chm_entry *ent = lookup_chm_entry(ctx, chm, name);

	if (!ent)
		return NULL;

	stm = chm_stream(ctx, chm, ent);

	fz_var(buf);

	fz_try(ctx)
	{
		/* +1 because many callers will add a terminating zero */
		buf = fz_read_all(ctx, stm, ent->size+1);
	}
	fz_always(ctx)
		fz_drop_stream(ctx, stm);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return buf;
}

static int has_chm_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_chm_archive *chm = (fz_chm_archive *) arch;
	chm_entry *ent = lookup_chm_entry(ctx, chm, name);
	return ent != NULL;
}

static const char *list_chm_entry(fz_context *ctx, fz_archive *arch, int idx)
{
	fz_chm_archive *chm = (fz_chm_archive *) arch;
	if (idx < 0 || idx >= chm->count)
		return NULL;
	return chm->entries[idx].name;
}

static int count_chm_entries(fz_context *ctx, fz_archive *arch)
{
	fz_chm_archive *chm = (fz_chm_archive *) arch;
	return chm->count;
}

static const
unsigned char signature[4] = { 'I', 'T', 'S', 'F' };

int
fz_is_chm_archive(fz_context *ctx, fz_stream *file)
{
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

static uint32_t
get_encint(fz_context *ctx, fz_stream *stm, uint32_t *left)
{
	uint32_t v = 0;
	uint32_t w;
	int res, n = 4;

	do
	{
		if (n-- == 0 || *left == 0)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Overly long encoded int in CHM");

		(*left) -= 1;
		res = fz_read_byte(ctx, stm);
		if (res == EOF)
			fz_throw(ctx, FZ_ERROR_FORMAT, "EOF in encoded int in CHM");
		w = res;
		v = (v<<7) | (w & 127);
	}
	while (w & 128);

	return v;
}

static void
read_listing_chunk(fz_context *ctx, fz_chm_archive *chm, uint32_t dir_chunk_size)
{
	fz_stream *stm = chm->super.file;
	unsigned char data[4];
	size_t n;
	static const uint8_t pmgl[] = { 'P', 'M', 'G', 'L' };
	uint32_t left;
	char *name = NULL;

	n = fz_read(ctx, stm, data, nelem(data));
	if (n != nelem(pmgl) || memcmp(data, pmgl, nelem(pmgl)))
		fz_throw(ctx, FZ_ERROR_FORMAT, "Expected a PMGL chunk in CHM");

	left = fz_read_uint32_le(ctx, stm);
	if (left > dir_chunk_size)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Overlong PMGL chunk in CHM");

	fz_skip(ctx, stm, 12); /* Always 0, prev chunk, next chunk */
	left = dir_chunk_size - left;
	if (left <= 20)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Short PMGL chunk");
	left -= 20;

	fz_var(name);

	fz_try(ctx)
	{
		while (left)
		{
			uint32_t namelen = get_encint(ctx, stm, &left);
			uint64_t sec, off, len;
			if (left <= namelen)
				fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated name in CHM");
			name = fz_malloc(ctx, namelen+1);
			n = fz_read(ctx, stm, (uint8_t *)name, namelen);
			if (n < namelen)
				fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated name in CHM");
			if (namelen > 1 && name[namelen - 1] == '/')
				name[namelen - 1] = 0;
			else
				name[namelen] = 0;
			left -= namelen;
			sec = get_encint(ctx, stm, &left);
			off = get_encint(ctx, stm, &left);
			len = get_encint(ctx, stm, &left);
#ifdef DEBUG_CHM_CONTENTS
			fz_write_printf(ctx, fz_stddbg(ctx), "%s @%d:%d+%d\n", name, sec, off, len);
#endif
			if (chm->max == chm->count)
			{
				int newmax = chm->max * 2;
				if (newmax == 0)
					newmax = 32;
				chm->entries = fz_realloc(ctx, chm->entries, newmax * sizeof(*chm->entries));
				chm->max = newmax;
			}

			chm->entries[chm->count].name = name;
			chm->entries[chm->count].section = sec;
			chm->entries[chm->count].offset = off;
			chm->entries[chm->count].size = len;
			chm->count++;
			name = NULL;
		}
	}
	fz_always(ctx)
		fz_free(ctx, name);
	fz_catch(ctx)
		fz_rethrow(ctx);

}

static void
prep_decompress(fz_context *ctx, fz_chm_archive *chm)
{
	chm_entry *entry = lookup_chm_entry(ctx, chm, "::DataSpace/Storage/MSCompressed/ControlData");
	chm_entry *reset_entry = lookup_chm_entry(ctx, chm, "::DataSpace/Storage/MSCompressed/Transform/{7FC28940-9D31-11D0-9B27-00A0C91E9C7C}/InstanceData/ResetTable");
	chm_entry *data_entry = lookup_chm_entry(ctx, chm, "::DataSpace/Storage/MSCompressed/Content");
	uint32_t v, reset_interval;
	uint64_t q;

	if (entry == NULL || reset_entry == NULL || data_entry == NULL || entry->section != 0 || reset_entry->section != 0 || data_entry->section != 0)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Can't decompress data with missing control data");

	// First, read the control data.
	fz_seek(ctx, chm->super.file, chm->section0_offset + entry->offset, SEEK_SET);
	v = fz_read_uint32_le(ctx, chm->super.file);
	if (v < 5)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed CHM control data");
	v = fz_read_uint32_le(ctx, chm->super.file);
	if (v != 0x43585a4c) // LZXC
		fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed CHM control data");
	v = fz_read_uint32_le(ctx, chm->super.file);
	if (v < 1 || v > 2)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Unsupported CHM control data version");
	reset_interval = fz_read_uint32_le(ctx, chm->super.file);
	chm->window_size = fz_read_uint32_le(ctx, chm->super.file);
	if (v == 2)
	{
		reset_interval *= 0x8000;
		chm->window_size *= 0x8000;
	}
	if (chm->window_size == 0)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Unsupported CHM window size");
	if (reset_interval != chm->window_size || (chm->window_size & 0x7fff))
		fz_throw(ctx, FZ_ERROR_FORMAT, "Unsupported CHM reset interval");
	chm->reset_block_interval = chm->window_size / 0x8000;

	// Now read the reset table.
	fz_seek(ctx, chm->super.file, chm->section0_offset + reset_entry->offset, SEEK_SET);
	fz_skip(ctx, chm->super.file, 4); // 2
	chm->block_entries = fz_read_uint32_le(ctx, chm->super.file);
	v = fz_read_uint32_le(ctx, chm->super.file);
	if (v != 8)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed CHM reset table data");
	v = fz_read_uint32_le(ctx, chm->super.file);
	if (v != 0x28)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed CHM reset table data");
	q = fz_read_uint64_le(ctx, chm->super.file);
	chm->section1_usize = (size_t)q;
	if (q != (uint64_t)chm->section1_usize)
		fz_throw(ctx, FZ_ERROR_FORMAT, "CHM data too large");
	q = fz_read_uint64_le(ctx, chm->super.file);
	chm->section1_csize = (size_t)q;
	if (q != (uint64_t)chm->section1_csize)
		fz_throw(ctx, FZ_ERROR_FORMAT, "CHM data too large");
	q = fz_read_uint64_le(ctx, chm->super.file);
	if (q != 0x8000)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed CHM reset table data");
	chm->section1_block_table_offset = fz_tell(ctx, chm->super.file);

	/* Looks like they don't inlude an n+1th one. */
	if ((chm->section1_usize+0x7fff) / 0x8000 != chm->block_entries)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed CHM reset table data");

	chm->section1_offset = chm->section0_offset + data_entry->offset;
	chm->section1_data_len = data_entry->size;
}

fz_archive *
fz_open_chm_archive_with_stream(fz_context *ctx, fz_stream *file)
{
	fz_chm_archive *chm;

	if (!fz_is_chm_archive(ctx, file))
		fz_throw(ctx, FZ_ERROR_FORMAT, "cannot recognize chm archive");

	chm = fz_new_derived_archive(ctx, file, fz_chm_archive);
	chm->super.format = "chm";
	chm->super.count_entries = count_chm_entries;
	chm->super.list_entry = list_chm_entry;
	chm->super.has_entry = has_chm_entry;
	chm->super.read_entry = read_chm_entry;
	chm->super.open_entry = open_chm_entry;
	chm->super.drop_archive = drop_chm_archive;

	fz_try(ctx)
	{
		/* Read the sig */
		unsigned char data[4];
		size_t n;
		uint32_t v;
		uint32_t header_len, dir_header_len, dir_chunk_size;
		uint32_t first_listing_chunk, last_listing_chunk;
		static const uint8_t itsp[] = { 'I', 'T', 'S', 'P' };
		chm_entry *entry;

		fz_seek(ctx, file, 0, SEEK_SET);
		n = fz_read(ctx, file, data, nelem(data));
		if (n != nelem(signature) || memcmp(data, signature, nelem(signature)))
			fz_throw(ctx, FZ_ERROR_FORMAT, "Not a CHM file");

		/* Version */
		v = fz_read_uint32_le(ctx, file);
		if (v != 3)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Unsupported CHM version");

		header_len = fz_read_uint32_le(ctx, file);
		fz_skip(ctx, file, 4 + 4 + 4 + 32); /* 1 - Unknown, timestamp, language id, 2 * GUIDs */

		chm->section[0].offset = fz_read_uint64_le(ctx, file);
		chm->section[0].length = fz_read_uint64_le(ctx, file);
		chm->section[1].offset = fz_read_uint64_le(ctx, file);
		chm->section[1].length = fz_read_uint64_le(ctx, file);

		/* 8 bytes of data here; Offset within file of content section 0 */
		chm->section0_offset = fz_read_uint64_le(ctx, file);

		/* Header section 0: 0x18 bytes we just skip. */
		/* Header section 1: The directory listing. */
		fz_seek(ctx, file, header_len + 0x18, SEEK_SET);
		n = fz_read(ctx, file, data, nelem(data));
		if (n != nelem(itsp) || memcmp(data, itsp, nelem(itsp)))
			fz_throw(ctx, FZ_ERROR_FORMAT, "Not a CHM file");
		v = fz_read_uint32_le(ctx, file);
		if (v != 1)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Unsupported CHM ITSP version");

		dir_header_len = fz_read_uint32_le(ctx, file);
		fz_skip(ctx, file, 4); /* 0x0a - unknown */
		dir_chunk_size = fz_read_uint32_le(ctx, file);
		fz_skip(ctx, file, 12); /* density, depth, root_index_chunk */
		first_listing_chunk = fz_read_uint32_le(ctx, file);
		last_listing_chunk = fz_read_uint32_le(ctx, file);
		fz_skip(ctx, file, 8); /* -1 - unknown, num_dir_chunks */

		chm->chunk0_offset = header_len + dir_header_len + 0x18;
		for (v = first_listing_chunk; v <= last_listing_chunk; v++)
		{
			fz_seek(ctx, file, chm->chunk0_offset + v * dir_chunk_size, SEEK_SET);
			read_listing_chunk(ctx, chm, dir_chunk_size);
		}

		/* Can't be bothered to go through the rigmarole of parsing a load of files
		 * just to know that section 0 is uncompressed, and section 1 is compressed.
		 * This is always the case. If we ever find a file that differs from this,
		 * we can revisit. */

		/* Now we want to actually decompress the data. */
		entry = lookup_chm_entry(ctx, chm, "::DataSpace/Storage/MSCompressed/Content");
		if (entry == NULL)
			fz_throw(ctx, FZ_ERROR_FORMAT, "CHM with no compressed data");
		prep_decompress(ctx, chm);
	}
	fz_catch(ctx)
	{
		fz_drop_archive(ctx, &chm->super);
		fz_rethrow(ctx);
	}

	return &chm->super;
}

fz_archive *
fz_open_chm_archive(fz_context *ctx, const char *filename)
{
	fz_archive *chm = NULL;
	fz_stream *file;

	file = fz_open_file(ctx, filename);

	fz_var(chm);

	fz_try(ctx)
		chm = fz_open_chm_archive_with_stream(ctx, file);
	fz_always(ctx)
		fz_drop_stream(ctx, file);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return chm;
}
