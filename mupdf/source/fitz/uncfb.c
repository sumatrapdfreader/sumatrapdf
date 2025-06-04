// Copyright (C) 2023-2025 Artifex Software, Inc.
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

#include <string.h>
#include <limits.h>

#define MAXREGSID 0xfffffffa
#define NOSTREAM 0xffffffff
#define MAXREGSECT 0xfffffffa
#define DIRSECT 0xfffffffc
#define FATSECT 0xfffffffd
#define ENDOFCHAIN 0xfffffffe
#define FREESECT 0xffffffff

#undef DEBUG_DIRENTRIES

typedef struct
{
	char *name;
	uint32_t sector;
	uint64_t size;
	uint32_t l, r, d;
	/* Flag word used for various different things.
	 * initially the type, then marked as to whether the DFS reached it
	 * then finally the original node number for debug. */
	uint32_t t;
} cfb_entry;

typedef struct
{
	fz_archive super;

	int max;
	int count;
	cfb_entry *entries;

	/* Header information from the file */
	uint16_t major;
	uint16_t sector_shift;
	uint32_t num_dir_sectors;
	uint32_t num_fat_sectors;
	uint32_t dir_sector0;
	uint32_t mini_fat_sector0;
	uint32_t num_mini_fat_sectors;
	uint32_t difat_sector0;
	uint32_t num_difat_sectors;
	uint32_t mini_stream_sector0;
	uint64_t mini_stream_len;
	uint32_t difat[109];

	uint32_t fatcache_sector;
	uint8_t fatcache[4096];

	uint32_t minifatcache_real_sector;
	uint32_t minifatcache_sector;
	uint8_t minifatcache[4096];

} fz_cfb_archive;

static void
read(fz_context *ctx, fz_stream *stm, uint8_t *buf, size_t size)
{
	size_t n = fz_read(ctx, stm, buf, size);

	if (n != size)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Short read in CFB handling");
}

static uint16_t
get16(const uint8_t *b)
{
	return b[0] + (b[1]<<8);
}

static uint32_t
get32(const uint8_t *b)
{
	return b[0] + (b[1]<<8) + (b[2]<<16) + (b[3]<<24);
}

static uint64_t
get64(const uint8_t *b)
{
	return b[0] +
		(((uint64_t)b[1])<<8) +
		(((uint64_t)b[2])<<16) +
		(((uint64_t)b[3])<<24) +
		(((uint64_t)b[4])<<32) +
		(((uint64_t)b[5])<<40) +
		(((uint64_t)b[6])<<48) +
		(((uint64_t)b[7])<<56);
}

static uint64_t
get_len(fz_context *ctx, fz_cfb_archive *cfb, const uint8_t *b)
{
	uint64_t len = get64(b);

	/* In v3 files the top 32bits *should* be zero, but may not be. The
	 * top bit of the lower 32bits should not be set though. */
	if (cfb->major == 3)
	{
		if (len & 0x80000000)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Illegal length in CFB");
		len &= 0xFFFFFFFFU;
	}
	return len;
}

static void
sector_seek(fz_context *ctx, fz_cfb_archive *cfb, uint32_t sector, uint32_t offset)
{
	fz_seek(ctx, cfb->super.file, ((sector + (uint64_t)1)<<cfb->sector_shift)+offset, SEEK_SET);
}

static uint32_t
read_difat(fz_context *ctx, fz_cfb_archive *cfb, uint32_t sector)
{
	uint32_t entries_per_sector;
	uint32_t sect;

	if (sector < 109)
	{
		return cfb->difat[sector];
	}
	sector -= 109;

	/* Run down the difat chain until we find the right sector. */
	entries_per_sector = (1<<(cfb->sector_shift-2)) - 1;
	sect = cfb->difat_sector0;
	while (sector > entries_per_sector)
	{
		sector_seek(ctx, cfb, sect, entries_per_sector * 4);
		sect = fz_read_uint32_le(ctx, cfb->super.file);
		sector -= entries_per_sector;
	}

	/* Now get the actual entry. */
	sector_seek(ctx, cfb, sect, sector * 4);

	return fz_read_uint32_le(ctx, cfb->super.file);
}

static uint32_t
read_fat(fz_context *ctx, fz_cfb_archive *cfb, uint32_t sector)
{
	uint32_t sector_size = 1<<cfb->sector_shift;
	/* We want to read the entry for sector 'sector' from the FAT. This
	 * will be in FAT sector 'fatsect'. */
	uint32_t fatsect = sector>>(cfb->sector_shift-2);
	/* FAT sector fatsect will be physical sector real_sect. */
	uint32_t real_sect = read_difat(ctx, cfb, fatsect);

	if (real_sect > MAXREGSECT)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Corrupt FAT");

	if (real_sect != cfb->fatcache_sector)
	{
		sector_seek(ctx, cfb, real_sect, 0);
		read(ctx, cfb->super.file, &cfb->fatcache[0], sector_size);
		cfb->fatcache_sector = real_sect;
	}

	sector &= (sector_size>>2)-1;

	return get32(&cfb->fatcache[sector*4]);
}

static uint32_t
read_mini_fat(fz_context *ctx, fz_cfb_archive *cfb, uint32_t sector)
{
	uint32_t sector_size = 1<<cfb->sector_shift;
	/* A mini fat sector has lots of mini sector numbers in (each 4 bytes) */
	uint32_t mini_sectors_in_mini_fat_sector = (1<<(cfb->sector_shift-2));
	/* We want to read the entry for sector 'sector' from the mini FAT. This
	 * will be in mini FAT sector 'minifatsect'. */
	uint32_t minifatsect = sector / mini_sectors_in_mini_fat_sector;
	uint32_t index_within_minifatsect = sector - minifatsect * mini_sectors_in_mini_fat_sector;
	int cache_valid = 1;

	/* minifatsect is a count of how many sectors we are into the mini fat stream.
	 * minifatsect_real_sector is the physical section that that corresponds to. */

	/* If we're behind our cache position, start from scratch. */
	if (minifatsect < cfb->minifatcache_sector)
	{
		cfb->minifatcache_real_sector = cfb->mini_fat_sector0;
		cfb->minifatcache_sector = 0;
		cache_valid = 0;
	}

	/* Skip forward until we are at the right position. */
	while (minifatsect != cfb->minifatcache_sector)
	{
		cfb->minifatcache_real_sector = read_fat(ctx, cfb, cfb->minifatcache_real_sector);
		cfb->minifatcache_sector++;
		cache_valid = 0;
	}

	/* Prime the cache if we just moved */
	if (!cache_valid)
	{
		sector_seek(ctx, cfb, cfb->minifatcache_real_sector, 0);
		read(ctx, cfb->super.file, cfb->minifatcache, sector_size);
	}

	return get32(&cfb->minifatcache[index_within_minifatsect*4]);
}

static void drop_cfb_archive(fz_context *ctx, fz_archive *arch)
{
	fz_cfb_archive *cfb = (fz_cfb_archive *) arch;
	int i;
	for (i = 0; i < cfb->count; ++i)
		fz_free(ctx, cfb->entries[i].name);
	fz_free(ctx, cfb->entries);
}

static cfb_entry *lookup_cfb_entry(fz_context *ctx, fz_cfb_archive *cfb, const char *name)
{
	int i;
	for (i = 0; i < cfb->count; i++)
		if (!fz_strcasecmp(name, cfb->entries[i].name))
			return &cfb->entries[i];
	return NULL;
}

typedef struct
{
	fz_cfb_archive *archive;
	uint32_t first_sector;
	uint32_t next_sector;
	uint32_t next_sector_slow;
	uint32_t next_sector_slow_flag;
	uint64_t pos_at_next_sector;
	uint64_t size;
	fz_stream *mini_stream;
	uint8_t buffer[4096];
} cfb_state;

static void
cfb_close(fz_context *ctx, void *state_)
{
	cfb_state *state = (cfb_state *)state_;

	fz_drop_archive(ctx, &state->archive->super);
	fz_drop_stream(ctx, state->mini_stream);
	fz_free(ctx, state);
}

static int
cfb_next(fz_context *ctx, fz_stream *stm, size_t required)
{
	cfb_state *state = stm->state;
	fz_cfb_archive *cfb = state->archive;
	uint64_t sector_size = ((uint64_t)1)<<cfb->sector_shift;
	uint64_t desired_sector_pos;
	uint32_t pos_in_sector;
	uint32_t this_sector;

	if ((uint64_t)stm->pos >= state->size)
		stm->eof = 1;

	if (stm->eof)
	{
		stm->rp = stm->wp = state->buffer;
		return EOF;
	}

	pos_in_sector = stm->pos & (sector_size-1);
	desired_sector_pos = stm->pos & ~(sector_size-1);
	if (desired_sector_pos != state->pos_at_next_sector)
	{
		state->pos_at_next_sector = 0;
		state->next_sector = state->first_sector;
		state->next_sector_slow = state->first_sector;
		state->next_sector_slow_flag = 0;
	}

	this_sector = state->next_sector;
	while (desired_sector_pos >= state->pos_at_next_sector)
	{
		this_sector = state->next_sector;
		state->next_sector = read_fat(ctx, cfb, state->next_sector);
		state->pos_at_next_sector += sector_size;
		if (state->next_sector > MAXREGSECT)
			break;

		state->next_sector_slow_flag = !state->next_sector_slow_flag;
		if (state->next_sector_slow_flag == 0)
			state->next_sector_slow = read_fat(ctx, cfb, state->next_sector_slow);
		if (state->next_sector_slow == state->next_sector)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Loop in FAT chain");
	}
	if (state->next_sector > MAXREGSECT && state->next_sector != ENDOFCHAIN)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Unexpected entry in FAT chain");

	if (this_sector > MAXREGSECT)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Unexpected end of FAT chain");
	sector_seek(ctx, cfb, this_sector, 0);
	read(ctx, cfb->super.file, state->buffer, sector_size);
	stm->rp = state->buffer;
	stm->wp = stm->rp + sector_size;
	stm->pos = state->pos_at_next_sector;
	if ((uint64_t)stm->pos >= state->size)
	{
		stm->wp -= (stm->pos - state->size);
		stm->pos = state->size;
	}
	stm->rp += pos_in_sector;

	return *stm->rp++;
}

#define MINI_SECTOR_SHIFT 6
#define MINI_SECTOR_SIZE (1<<MINI_SECTOR_SHIFT)

static int
cfb_next_mini(fz_context *ctx, fz_stream *stm, size_t required)
{
	cfb_state *state = stm->state;
	fz_cfb_archive *cfb = state->archive;
	uint64_t desired_sector_pos;
	uint32_t pos_in_sector;
	uint32_t this_sector;

	if ((uint64_t)stm->pos >= state->size)
		stm->eof = 1;

	if (stm->eof)
	{
		stm->rp = stm->wp = state->buffer;
		return EOF;
	}

	/* Whenever we say 'sector' here, we mean 'mini sector'. */
	pos_in_sector = stm->pos & (MINI_SECTOR_SIZE-1);
	desired_sector_pos = stm->pos & ~(MINI_SECTOR_SIZE-1);
	if (desired_sector_pos != state->pos_at_next_sector)
	{
		state->pos_at_next_sector = 0;
		state->next_sector = state->first_sector;
		state->next_sector_slow = state->first_sector;
		state->next_sector_slow_flag = 0;
	}

	this_sector = state->next_sector;
	while (desired_sector_pos >= state->pos_at_next_sector)
	{
		this_sector = state->next_sector;
		state->next_sector = read_mini_fat(ctx, cfb, state->next_sector);
		state->pos_at_next_sector += MINI_SECTOR_SIZE;
		if (state->next_sector > MAXREGSECT)
			break;

		state->next_sector_slow_flag = !state->next_sector_slow_flag;
		if (state->next_sector_slow_flag == 0)
			state->next_sector_slow = read_mini_fat(ctx, cfb, state->next_sector_slow);
		if (state->next_sector_slow == state->next_sector)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Loop in FAT chain");
	}
	if (state->next_sector > MAXREGSECT && state->next_sector != ENDOFCHAIN)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Unexpected entry in FAT chain");

	if (this_sector > MAXREGSECT)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Unexpected end of FAT chain");

	fz_seek(ctx, state->mini_stream, ((uint64_t)this_sector) * MINI_SECTOR_SIZE, SEEK_SET);
	read(ctx, state->mini_stream, state->buffer, MINI_SECTOR_SIZE);
	stm->rp = state->buffer;
	stm->wp = stm->rp + MINI_SECTOR_SIZE;
	stm->pos += MINI_SECTOR_SIZE;
	if ((uint64_t)stm->pos >= state->size)
	{
		stm->wp -= (stm->pos - state->size);
		stm->pos = state->size;
	}
	stm->rp += pos_in_sector;

	return *stm->rp++;
}

static void cfb_seek(fz_context *ctx, fz_stream *stm, int64_t offset, int whence)
{
	cfb_state *state = stm->state;
	int64_t pos = stm->pos - (stm->wp - stm->rp);
	/* Convert to absolute pos */
	if (whence == 1)
	{
		offset += pos; /* Was relative to current pos */
	}
	else if (whence == 2)
	{
		offset += stm->pos; /* Was relative to end */
	}

	if (offset < 0)
		offset = 0;
	if ((uint64_t)offset > state->size)
		offset = (int64_t)state->size;
	stm->pos = offset;
	stm->rp = stm->wp = state->buffer;
}

static fz_stream *sector_stream(fz_context *ctx, fz_cfb_archive *cfb, uint32_t sector, uint64_t size)
{
	fz_stream *stm;
	cfb_state *state = fz_malloc_struct(ctx, cfb_state);

	state->archive = (fz_cfb_archive *)fz_keep_archive(ctx, &cfb->super);
	state->pos_at_next_sector = 0;
	state->size = size;
	state->first_sector = sector;
	state->next_sector = state->first_sector;
	state->next_sector_slow = state->first_sector;
	state->next_sector_slow_flag = 0;

	stm = fz_new_stream(ctx, state, cfb_next, cfb_close);
	stm->seek = cfb_seek;

	return stm;
}

static fz_stream *open_cfb_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_cfb_archive *cfb = (fz_cfb_archive *) arch;
	cfb_entry *ent;
	fz_stream *stm;
	cfb_state *state;

	ent = lookup_cfb_entry(ctx, cfb, name);
	if (!ent)
		return NULL;

	if (ent->size >= 0x1000)
	{
		/* Working from entire sectors */
		return sector_stream(ctx, cfb, ent->sector, ent->size);
	}

	/* We're working from the mini stream. */
	state = fz_malloc_struct(ctx, cfb_state);

	fz_try(ctx)
	{
		/* Let's get a stream that gets us the mini stream, and then work from that. */
		state->mini_stream = sector_stream(ctx, cfb, cfb->mini_stream_sector0, cfb->mini_stream_len);
		state->first_sector = ent->sector;
		state->pos_at_next_sector = 0;
		state->size = ent->size;
		state->next_sector = state->first_sector;
		state->next_sector_slow = state->first_sector;
		state->next_sector_slow_flag = 0;
		state->archive = (fz_cfb_archive *)fz_keep_archive(ctx, &cfb->super);

	}
	fz_catch(ctx)
	{
		fz_free(ctx, state);
		fz_rethrow(ctx);
	}

	stm = fz_new_stream(ctx, state, cfb_next_mini, cfb_close);
	stm->seek = cfb_seek;
	return stm;
}

static fz_buffer *read_cfb_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_stream *stm;
	fz_buffer *buf = NULL;

	stm = open_cfb_entry(ctx, arch, name);
	if (!stm)
		return NULL;

	fz_try(ctx)
		buf = fz_read_all(ctx, stm, 1024);
	fz_always(ctx)
		fz_drop_stream(ctx, stm);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return buf;
}

static int has_cfb_entry(fz_context *ctx, fz_archive *arch, const char *name)
{
	fz_cfb_archive *cfb = (fz_cfb_archive *) arch;
	cfb_entry *ent = lookup_cfb_entry(ctx, cfb, name);
	return ent != NULL;
}

static const char *list_cfb_entry(fz_context *ctx, fz_archive *arch, int idx)
{
	fz_cfb_archive *cfb = (fz_cfb_archive *) arch;
	if (idx < 0 || idx >= cfb->count)
		return NULL;
	return cfb->entries[idx].name;
}

static int count_cfb_entries(fz_context *ctx, fz_archive *arch)
{
	fz_cfb_archive *cfb = (fz_cfb_archive *) arch;
	return cfb->count;
}

static const uint8_t sig[8] = { 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };
static const uint8_t zeros[16] = { 0 };

int
fz_is_cfb_archive(fz_context *ctx, fz_stream *file)
{
	uint8_t data[nelem(sig)];
	size_t n;

	fz_seek(ctx, file, 0, SEEK_SET);
	n = fz_read(ctx, file, data, nelem(data));
	if (n != nelem(data))
		return 0;
	if (!memcmp(data, sig, nelem(sig)))
		return 1;

	return 0;
}

static void
expect(fz_context *ctx, fz_stream *file, const uint8_t *pattern, size_t n, const char *msg)
{
	uint8_t buffer[64];

	assert(sizeof(buffer) >= n);
	read(ctx, file, buffer, n);

	if (memcmp(buffer, pattern, n) != 0)
		fz_throw(ctx, FZ_ERROR_FORMAT, "%s in CFB", msg);
}

static void
expect16(fz_context *ctx, fz_stream *file, uint16_t v, const char *msg)
{
	uint16_t u;

	u = fz_read_uint16_le(ctx, file);
	if (u != v)
		fz_throw(ctx, FZ_ERROR_FORMAT, "%s in CFB: 0x%04x != 0x%04x", msg, u, v);
}

static void
expect32(fz_context *ctx, fz_stream *file, uint32_t v, const char *msg)
{
	uint32_t u;

	u = fz_read_uint32_le(ctx, file);
	if (u != v)
		fz_throw(ctx, FZ_ERROR_FORMAT, "%s in CFB: 0x%08x != 0x%08x", msg, u, v);
}

#define REACHED 0xFFFFFFFF
#define REACHED_KEEP 0xFFFFFFFE

static void
make_absolute(fz_context *ctx, fz_cfb_archive *cfb, char *prefix, int node, int depth)
{
	uint32_t type;

	/* To avoid recursion where possible. */
	while (1)
	{
		if (node == (int)NOSTREAM)
			return;

		if (node < 0 || node >= cfb->count)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid tree");

		if (depth >= 32)
			fz_throw(ctx, FZ_ERROR_FORMAT, "CBF Tree too deep");

		type = cfb->entries[node].t;
		if (type == REACHED || type == REACHED_KEEP)
			fz_throw(ctx, FZ_ERROR_FORMAT, "CBF Tree has cycles");
		cfb->entries[node].t = (type == 2) ? REACHED_KEEP : REACHED;

		if (prefix)
		{
			size_t z0 = strlen(prefix);
			size_t z1 = strlen(cfb->entries[node].name);
			char *newname = fz_malloc(ctx, z0+z1+2);
			memcpy(newname, prefix, z0);
			newname[z0] = '/';
			memcpy(newname+z0+1, cfb->entries[node].name, z1+1);
			fz_free(ctx, cfb->entries[node].name);
			cfb->entries[node].name = newname;
		}

		if (cfb->entries[node].d == NOSTREAM && cfb->entries[node].r == NOSTREAM)
		{
			/* Handle 'l' without recursion, because there is no 'r' or 'd'. */
			node = cfb->entries[node].l;
			continue;
		}
		make_absolute(ctx, cfb, prefix, cfb->entries[node].l, depth+1);
		if (cfb->entries[node].d == NOSTREAM)
		{
			/* Handle 'r' without recursion, because there is no 'd'. */
			node = cfb->entries[node].r;
			continue;
		}
		make_absolute(ctx, cfb, prefix, cfb->entries[node].r, depth+1);

		/* Rather than recursing:
		 *	make_absolute(ctx, cfb, node == 0 ? NULL : cfb->entries[node].name, cfb->entries[node].d, depth+1);
		 * instead just loop. */
		prefix = node == 0 ? NULL : cfb->entries[node].name;
		node = cfb->entries[node].d;
	}

}

static void
absolutise_names(fz_context *ctx, fz_cfb_archive *cfb)
{
	make_absolute(ctx, cfb, NULL, 0, 0);
}

static void
strip_unused_names(fz_context *ctx, fz_cfb_archive *cfb)
{
	int i, j;
	int n = cfb->count;

	/* Init i and j so that we always delete the root node. */
	fz_free(ctx, cfb->entries[0].name);
	for (i = 1, j = 0; i < n; i++)
	{
		if (cfb->entries[i].t == REACHED_KEEP)
		{
			if (i != j)
				cfb->entries[j] = cfb->entries[i];
			cfb->entries[j].t = i;
			j++;
		}
		else
			fz_free(ctx, cfb->entries[i].name);
	}
	cfb->count = j;
}

fz_archive *
fz_open_cfb_archive_with_stream(fz_context *ctx, fz_stream *file)
{
	fz_cfb_archive *cfb;
	uint8_t buffer[4096];
	uint32_t sector, slow_sector, slow_sector_flag;
	int i;

	if (!fz_is_cfb_archive(ctx, file))
		fz_throw(ctx, FZ_ERROR_FORMAT, "cannot recognize cfb archive");

	cfb = fz_new_derived_archive(ctx, file, fz_cfb_archive);
	cfb->super.format = "cfb";
	cfb->super.count_entries = count_cfb_entries;
	cfb->super.list_entry = list_cfb_entry;
	cfb->super.has_entry = has_cfb_entry;
	cfb->super.read_entry = read_cfb_entry;
	cfb->super.open_entry = open_cfb_entry;
	cfb->super.drop_archive = drop_cfb_archive;

	fz_try(ctx)
	{
		fz_seek(ctx, file, 0, SEEK_SET);
		/* Read the header */
		expect(ctx, file, sig, 8, "Bad signature");
		expect(ctx, file, zeros, 16, "Bad CLSID");
		/* The minor version is SUPPOSED to be 0x3e, but we don't seem to be
		 * able to rely on this. So just skip it. */
		(void)fz_read_uint16_le(ctx, file);
		cfb->major = fz_read_uint16_le(ctx, file);
		if (cfb->major != 3 && cfb->major != 4)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Bad major version of CFB: %d", cfb->major);
		expect16(ctx, file, 0xfffe, "Bad byte order");
		cfb->sector_shift = fz_read_uint16_le(ctx, file);
		if ((cfb->major == 3 && cfb->sector_shift != 9) ||
			(cfb->major == 4 && cfb->sector_shift != 12))
			fz_throw(ctx, FZ_ERROR_FORMAT, "Bad sector shift: %d", cfb->sector_shift);
		expect16(ctx, file, 6, "Bad mini section shift");
		expect(ctx, file, zeros, 6, "Bad padding");
		cfb->num_dir_sectors = fz_read_uint32_le(ctx, file);
		cfb->num_fat_sectors = fz_read_uint32_le(ctx, file);
		cfb->dir_sector0 = fz_read_uint32_le(ctx, file);
		(void)fz_read_uint32_le(ctx, file); /* Transaction signature number */
		expect32(ctx, file, 0x1000, "Bad mini stream cutoff size");
		cfb->mini_fat_sector0 = fz_read_uint32_le(ctx, file);
		cfb->num_mini_fat_sectors = fz_read_uint32_le(ctx, file);
		cfb->difat_sector0 = fz_read_uint32_le(ctx, file);
		cfb->num_difat_sectors = fz_read_uint32_le(ctx, file);
		for (i = 0; i < 109; i++)
			cfb->difat[i] = fz_read_uint32_le(ctx, file);
		cfb->fatcache_sector = (uint32_t)-1;
		cfb->minifatcache_sector = (uint32_t)-1;

		/* Read the directory entries. */
		/* On our first pass through, EVERYTHING goes into the entries. */
		sector = cfb->dir_sector0;
		slow_sector = sector;
		slow_sector_flag = 0;
		do
		{
			size_t z = ((size_t)1)<<cfb->sector_shift;
			size_t off;

			/* Fetch the sector. */
			fz_seek(ctx, file, ((int64_t)sector+1)<<cfb->sector_shift, SEEK_SET);
			read(ctx, file, buffer, z);

			for (off = 0; off < z; off += 128)
			{
				int count = 0;
				int type;
				int namelen = get16(buffer+off+64);

				if (namelen == 0)
					break;

				/* What flavour of object is this? */
				type = buffer[off+64+2];

				/* Ensure our entries list is long enough. */
				if (cfb->max == cfb->count)
				{
					int newmax = cfb->max * 2;
					if (newmax == 0)
						newmax = 32;
					cfb->entries = fz_realloc_array(ctx, cfb->entries, newmax, cfb_entry);
					cfb->max = newmax;
				}

				/* Count the name length in utf8 encoded bytes, including terminator. */
				for (i = 0; i < 64; i += 2)
				{
					int ucs = get16(buffer+off+i);
					if (ucs == 0)
						break;
					count += fz_runelen(ucs);
				}
				if (i+2 != namelen || i == 64)
					fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed name in CFB directory");

				/* Copy the name. */
				cfb->entries[cfb->count++].name = fz_malloc(ctx, count + 1);
				count = 0;
				for (i = 0; i < 64; i += 2)
				{
					int ucs = buffer[off+i] + (buffer[off+i+1]<<8);
					if (ucs == 0)
						break;
					count += fz_runetochar(&cfb->entries[cfb->count-1].name[count], ucs);
				}
				cfb->entries[cfb->count-1].name[count] = 0;

				cfb->entries[cfb->count-1].sector = get32(buffer+off+128-12);
				cfb->entries[cfb->count-1].size = get_len(ctx, cfb, buffer+off+128-8);
				cfb->entries[cfb->count-1].l = get32(buffer+off+68);
				cfb->entries[cfb->count-1].r = get32(buffer+off+72);
				cfb->entries[cfb->count-1].d = get32(buffer+off+76);
				cfb->entries[cfb->count-1].t = type;

#ifdef DEBUG_DIRENTRIES
				fz_write_printf(ctx, fz_stddbg(ctx), "%d: ", cfb->count-1);
				if (type == 1)
					fz_write_printf(ctx, fz_stddbg(ctx), "(storage) ");
				else if (type == 2)
					fz_write_printf(ctx, fz_stddbg(ctx), "(file) ");
				else if (type == 5)
					fz_write_printf(ctx, fz_stddbg(ctx), "(root) ");
				else
					fz_write_printf(ctx, fz_stddbg(ctx), "(%d?) ", type);

				fz_write_printf(ctx, fz_stddbg(ctx), "%q", cfb->entries[cfb->count-1].name);
				fz_write_printf(ctx, fz_stddbg(ctx), " @%x+%x\n", cfb->entries[cfb->count-1].sector, cfb->entries[cfb->count-1].size );
				if (cfb->entries[cfb->count-1].l <= MAXREGSID)
					fz_write_printf(ctx, fz_stddbg(ctx), "\tleft=%d\n", cfb->entries[cfb->count-1].l);
				if (cfb->entries[cfb->count-1].r <= MAXREGSID)
					fz_write_printf(ctx, fz_stddbg(ctx), "\tright=%d\n", cfb->entries[cfb->count-1].r);
				if (cfb->entries[cfb->count-1].d <= MAXREGSID)
					fz_write_printf(ctx, fz_stddbg(ctx), "\tchild=%d\n", cfb->entries[cfb->count-1].d);
#endif

				/* Type 5 is just for the root. */
				if (type == 5)
				{
					cfb->mini_stream_sector0 = get32(buffer+off+128-12);
					cfb->mini_stream_len = get_len(ctx, cfb, buffer+off+128-8);
				}
			}

			/* To get the next sector, we need to read it from the FAT. */
			sector = read_fat(ctx, cfb, sector);
			slow_sector_flag = !slow_sector_flag;
			if (slow_sector_flag == 0)
				slow_sector = read_fat(ctx, cfb, slow_sector);
			if (slow_sector == sector)
				fz_throw(ctx, FZ_ERROR_FORMAT, "Loop in FAT");
		}
		while (sector <= MAXREGSECT);

		absolutise_names(ctx, cfb);
		strip_unused_names(ctx, cfb);

#ifdef DEBUG_DIRENTRIES
		for (i = 0; i < cfb->count; i++)
			fz_write_printf(ctx, fz_stddbg(ctx), "%d: %s (was %d)\n", i, cfb->entries[i].name, cfb->entries[i].t);
#endif
	}
	fz_catch(ctx)
	{
		fz_drop_archive(ctx, &cfb->super);
		fz_rethrow(ctx);
	}

	return &cfb->super;
}

fz_archive *
fz_open_cfb_archive(fz_context *ctx, const char *filename)
{
	fz_archive *cfb = NULL;
	fz_stream *file;

	file = fz_open_file(ctx, filename);

	fz_try(ctx)
		cfb = fz_open_cfb_archive_with_stream(ctx, file);
	fz_always(ctx)
		fz_drop_stream(ctx, file);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return cfb;
}
