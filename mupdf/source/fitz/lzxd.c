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

// This file is a conversion to C from the MIT/BSD licensed decoder found at
// https://github.com/Lonami/lzxd

#include "mupdf/fitz.h"

#include "lzxd-imp.h"

/* bitstream.rs */

// > An LZXD bitstream is encoded as a sequence of aligned 16-bit integers stored in the
// > least-significant- byte to most-significant-byte order, also known as byte-swapped,
// > or little-endian, words. Given an input stream of bits named a, b, c,..., x, y, z,
// > A, B, C, D, E, F, the output byte stream MUST be as [ 0| 1| 2| 3|...|30|31].
//
// It is worth mentioning that older revisions of the document explain this better:
//
// > Given an input stream of bits named a, b, c, ..., x, y, z, A, B, C, D, E, F, the output
// > byte stream (with byte boundaries highlighted) would be as follows:
// > [i|j|k|l|m|n|o|p#a|b|c|d|e|f|g|h#y|z|A|B|C|D|E|F#q|r|s|t|u|v|w|x]
typedef struct {
	const uint8_t *buffer;
	size_t buffer_len;
	const uint8_t *rp;
	const uint8_t *limit;

	uint16_t n; // Next number in the bitstream.
	int remaining; // How many bits left in the current `n`.
	int exhausted;
} bitstream_t;

static void
init_bitstream(fz_context *ctx, bitstream_t *self, const uint8_t *buffer, size_t buffer_len)
{
	if (buffer_len & 1)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Odd length buffer");
	self->remaining = 0;
	self->buffer = buffer;
	self->buffer_len = buffer_len;
	self->rp = buffer;
	self->limit = buffer + buffer_len;
	self->n = 0;
	self->exhausted = 0;
}

static uint16_t
read16(fz_context *ctx, bitstream_t *self)
{
	uint16_t ret;

	if (self->rp == self->limit)
	{
		self->exhausted = 1;
		return 0;
	}

	ret = self->rp[0] | (self->rp[1]<<8);
	self->rp += 2;

	return ret;
}

static void
bitstream_advance(fz_context *ctx, bitstream_t *self)
{
	self->remaining = 16;
	self->n = read16(ctx, self);
}

static uint32_t
bitstream_read_bit(fz_context *ctx, bitstream_t *self)
{
	uint32_t i;

	if (self->remaining == 0)
		bitstream_advance(ctx, self);

	self->remaining--;
	i = self->n>>15;
	self->n <<= 1;

	return i;
}

// Read from the bitstream, no more than 16 bits (one word).
static uint32_t
bitstream_read_bits(fz_context *ctx, bitstream_t *self, int bits)
{
	uint32_t lo, hi;
	assert(bits <= 16);
	assert(self->remaining <= 16);

	if (bits <= self->remaining)
	{
		self->remaining -= bits;
		lo = self->n >> (16-bits);
		self->n <<= bits;
		return lo;
	}

	hi = self->n >> (16-self->remaining);
	bits -= self->remaining;
	bitstream_advance(ctx, self);

	self->remaining -= bits;
	lo = self->n >> (16-bits);
	self->n <<= bits;

	return (hi<<bits) | lo;
}

// Read from the bitstream, no more than 16 bits (one word).
static uint32_t
bitstream_peek_bits(fz_context *ctx, bitstream_t *self, int bits)
{
	uint32_t lo, hi;
	assert(bits <= 16);
	assert(self->remaining <= 16);

	if (bits <= self->remaining)
		return self->n >> (16-bits);

	hi = self->n >> (16-self->remaining);
	bits -= self->remaining;

	if (self->rp == self->limit)
		lo = 0;
	else
		lo = self->rp[0] | (self->rp[1]<<8);
	lo = lo >> (16-bits);

	return (hi<<bits) | lo;
}

static uint16_t
bitstream_read_u16_le(fz_context *ctx, bitstream_t *self)
{
	uint16_t i = bitstream_read_bits(ctx, self, 16);

	return (i>>8) | (i<<8);
}

static uint32_t
bitstream_read_u32_le(fz_context *ctx, bitstream_t *self)
{
	uint32_t lo = bitstream_read_u16_le(ctx, self);
	uint32_t hi = bitstream_read_u16_le(ctx, self);

	return (hi<<16) | lo;
}

uint32_t
bitstream_read_u24_be(fz_context *ctx, bitstream_t *self)
{
	uint32_t hi = bitstream_read_bits(ctx, self, 16);
	uint32_t lo = bitstream_read_bits(ctx, self, 8);

	return (hi<<8) | lo;
}

static int
bitstream_align(fz_context *ctx, bitstream_t *self)
{
	if (self->remaining == 0)
		return 0;

	self->remaining = 0;
	return 1;
}

// Copies from the current buffer to the destination output ignoring the representation.
//
// The buffer should be aligned beforehand, otherwise bits may be discarded.
//
// If the output length is not evenly divisible, such padding byte will be discarded.
static void
bitstream_read_raw(fz_context *ctx, bitstream_t *self, uint8_t *out, size_t len)
{
	size_t advance = (len+1) & ~1;
	size_t read = self->limit - self->rp;

	if (read < advance)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated lzwd bitstream");
	memcpy(out, self->rp, len);
	self->rp += advance;
}

static size_t
bitstream_remaining_bytes(fz_context *ctx, bitstream_t *self)
{
	return self->limit - self->rp;
}

/* tree.rs */

typedef struct
{
	uint16_t len;
	uint8_t v[32];
} vec8_t;

typedef struct
{
	uint32_t len;
	uint16_t v[32];
} vec16_t;

static vec8_t *
new_vec8(fz_context *ctx, uint16_t len)
{
	vec8_t *v = fz_malloc(ctx, sizeof(vec8_t) + (((size_t)len) - 32)*sizeof(uint8_t));

	v->len = len;

	return v;
}

static void
drop_vec8(fz_context *ctx, vec8_t *vec)
{
	fz_free(ctx, vec);
}

static vec16_t *
new_vec16(fz_context *ctx, uint32_t len)
{
	vec16_t *v = fz_malloc(ctx, sizeof(vec16_t) + (((size_t)len) - 32)*sizeof(uint16_t));

	v->len = len;

	return v;
}

static void
drop_vec16(fz_context *ctx, vec16_t *vec)
{
	fz_free(ctx, vec);
}

static uint8_t
vec8_max(fz_context *ctx, const vec8_t *v)
{
	size_t i, n;
	uint8_t max;

	if (v == NULL || (n = v->len) == 0)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Unexpected empty vector!");

	max = v->v[0];
	for (i = 1; i < n; i++)
		if (v->v[i] > max)
			max = v->v[i];

	return max;
}

// The canonical tree cannot be used to decode elements. Instead, it behaves like a builder for
// instances of the actual tree that can decode elements efficiently.
//
// Rather than having a canonical_tree_t be a structure that contains a vec, just have
// it be the vec.
// > Each tree element can have a path length of [0, 16], where a zero path length indicates
// > that the element has a zero frequency and is not present in the tree.
//
// We represent them as `u8` due to their very short range.
typedef vec8_t canonical_tree_t;

typedef struct
{
	int drop_path_lengths;
	vec8_t *path_lengths;
	uint8_t largest_length;
	vec16_t *huffman_tree;
} tree_t;

static canonical_tree_t *
new_canonical_tree(fz_context *ctx, size_t count)
{
	vec8_t *vec;
	// > In the case of the very first such tree, the delta is calculated against a tree
	// > in which all elements have a zero path length.
	vec = new_vec8(ctx, (uint16_t)count);
	memset(vec->v, 0, count);
	return vec;
}

static void
drop_canonical_tree(fz_context *ctx, canonical_tree_t *self)
{
	drop_vec8(ctx, self);
}

static void
drop_tree(fz_context *ctx, tree_t *self)
{
	if (!self)
		return;

	drop_vec16(ctx, self->huffman_tree);
	if (self->drop_path_lengths)
		drop_vec8(ctx, self->path_lengths);
	fz_free(ctx, self);
}

// Create a new `Tree` instance from this cast that can be used to decode elements.
//
// This method transforms the canonical Huffman tree into a different structure that can
// be used to better decode elements.
// > an LZXD decoder uses only the path lengths of the Huffman tree to reconstruct the
// > identical tree,
static tree_t *
canonical_tree_create_instance(fz_context *ctx, canonical_tree_t *self, int own_canonical_tree, int allow_empty)
{
	tree_t *tree = NULL;

	fz_var(tree);

	fz_try(ctx)
	{
		// The ideas implemented by this method are heavily inspired from LeonBlade's xnbcli
		// on GitHub.
		//
		// The path lengths contains the bit indices or zero if its not present, so find the
		// highest path length to determine how big our tree needs to be.
		uint8_t largest_length = vec8_max(ctx, self);
		uint32_t pos = 0;
		uint16_t bit;
		uint32_t huffsize;
		vec16_t *huffman_tree;

		assert(largest_length <= 16);
		huffsize = (uint32_t)(1<<largest_length);

		tree = fz_malloc_struct(ctx, tree_t);

		tree->largest_length = largest_length;
		if (tree->largest_length == 0)
		{
			/* The tree is empty! */
			if (allow_empty)
				break;
			fz_throw(ctx, FZ_ERROR_FORMAT, "Empty huffman tree");
		}
		tree->huffman_tree = huffman_tree = new_vec16(ctx, huffsize);

		// > a zero path length indicates that the element has a zero frequency and is not
		// > present in the tree. Tree elements are output in sequential order starting with the
		// > first element
		//
		// We start at the MSB, 1, and write the tree elements in sequential order from index 0.
		for (bit = 1; bit <= largest_length; bit++)
		{
			uint16_t amount = 1 << (largest_length - bit);

			// The codes correspond with the indices of the path length (because
			// `path_lengths[code]` is its path length).
			uint16_t code;
			for (code = 0; code < self->len; code++)
			{
				// As soon as a code's path length matches with our bit index write the code as
				// many times as the bit index itself represents.
				if (self->v[code] == bit)
				{
					uint16_t i;

					if (pos + amount > huffsize)
						fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid huffman tree (overrun)");

					for (i = amount; i > 0; i--)
						huffman_tree->v[pos++] = code;
				}
			}
		}

		// If we didn't fill the entire table, the path lengths were wrong.
		if (pos != huffsize)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid huffman tree (underrun)");
	}
	fz_catch(ctx)
	{
		drop_tree(ctx, tree);
		if (own_canonical_tree)
			drop_canonical_tree(ctx, self);
		fz_rethrow(ctx);
	}

	// Move self into tree only after nothing can throw to avoid double frees.
	tree->path_lengths = self;
	tree->drop_path_lengths = own_canonical_tree;

	return tree;
}

static uint16_t
tree_decode_element(fz_context *ctx, tree_t *self, bitstream_t *bstm)
{
	// Perform the inverse translation, peeking as many bits as our tree is...
	uint16_t code = self->huffman_tree->v[bitstream_peek_bits(ctx, bstm, self->largest_length)];

	// ...and advancing the stream for as many bits this code actually takes (read to seek).
	(void)bitstream_read_bits(ctx, bstm, self->path_lengths->v[code]);

	return code;
}

// Note: the tree already exists and is used to apply the deltas.
static void
tree_update_range_with_pretree(fz_context *ctx, canonical_tree_t *self, bitstream_t *bstm, uint16_t start, uint16_t end)
{
	// > Each of the 17 possible values of (len[x] - prev_len[x]) mod 17, plus three
	// > additional codes used for run-length encoding, are not output directly as 5-bit
	// > numbers but are instead encoded via a Huffman tree called the pretree. The pretree
	// > is generated dynamically according to the frequencies of the 20 allowable tree
	// > codes. The structure of the pretree is encoded in a total of 80 bits by using 4 bits
	// > to output the path length of each of the 20 pretree elements. Once again, a zero
	// > path length indicates a zero-frequency element.
	tree_t *pretree = NULL;
	// Get a static path_lengths. This relies on our standard vec8 being defined as having
	// at least 20 elements. And it does.
	vec8_t path_lengths = { 20 };
	uint16_t i;
	for (i = 0; i < 20; i++)
	{
		path_lengths.v[i] = bitstream_read_bits(ctx, bstm, 4);
	}

	pretree = canonical_tree_create_instance(ctx, &path_lengths, 0, 0);

	fz_try(ctx)
	{
		// > Tree elements are output in sequential order starting with the first element.
		i = start;
		while (i < end)
		{
			// > The "real" tree is then encoded using the pretree Huffman codes.
			uint16_t code = tree_decode_element(ctx, pretree, bstm);

			// > Elements can be encoded in one of two ways: if several consecutive elements have
			// > the same path length, run-length encoding is employed; otherwise, the element is
			// > output by encoding the difference between the current path length and the
			// > previous path length of the tree, mod 17.
			switch (code)
			{
			case 0: case 1: case 2: case 3:
			case 4: case 5: case 6: case 7:
			case 8: case 9: case 10: case 11:
			case 12: case 13: case 14: case 15:
			case 16:
			{
				self->v[i] = (17 + self->v[i] - code) % 17;
				i += 1;
				break;
			}
			// > Codes 17, 18, and 19 are used to represent consecutive elements that have the
			// > same path length.
			case 17:
			{
				uint16_t zeros = bitstream_read_bits(ctx, bstm, 4) + 4;

				if (i + zeros > self->len)
					fz_throw(ctx, FZ_ERROR_FORMAT, "Overrun with pretree codes");

				do
					self->v[i++] = 0;
				while (--zeros);
				break;
			}
			case 18:
			{
				uint16_t zeros = bitstream_read_bits(ctx, bstm, 5) + 20;

				if (i + zeros > self->len)
					fz_throw(ctx, FZ_ERROR_FORMAT, "Overrun with pretree codes");

				do
					self->v[i++] = 0;
				while (--zeros);
				break;
			}
			case 19:
			{
				uint8_t value;
				uint16_t same = bitstream_read_bits(ctx, bstm, 1) + 4;
				// "Decode new code" is used to parse the next code from the bitstream, which
				// has a value range of [0, 16].
				uint16_t code = tree_decode_element(ctx, pretree, bstm);

				if (i + same > self->len || code > 16)
					fz_throw(ctx, FZ_ERROR_FORMAT, "Overrun with pretree codes");

				value = (17 + self->v[i] - code) % 17;
				do
				{
					self->v[i++] = value;
				}
				while (--same);
				break;
			}
			default:
				fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid code in pretree updating.");
			}
		}
	}
	fz_always(ctx)
		drop_tree(ctx, pretree);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/* window.rs */

// The window size is not stored in the compressed data stream and must be known before
// decoding begins.
//
// The window size should be the smallest power of two between 2^17 and 2^25 that is greater
// than or equal to the sum of the size of the reference data rounded up to a multiple of
// 32_768 and the size of the subject data. However, some implementations also seem to support
// a window size of less than 2^17, and this one is no exception.

// A sliding window of a certain size.
typedef struct
{
	size_t pos;
	size_t len;
	uint8_t buffer[32];
} window_t;

static int
window_size_position_slots(fz_context *ctx, fz_lzxd_window_size_t ws)
{
	// The window size determines the number of window subdivisions, or position slots.
	switch (ws)
	{
	case KB32: return 30;
	case KB64: return 32;
	case KB128: return 34;
	case KB256: return 36;
	case KB512: return 38;
	case MB1: return 42;
	case MB2: return 50;
	case MB4: return 66;
	case MB8: return 98;
	case MB16: return 162;
	case MB32: return 290;
	default:
		fz_throw(ctx, FZ_ERROR_FORMAT, "Illegal window size");
	}
}

static int
is_power_of_two(fz_lzxd_window_size_t ws)
{
	while ((ws & 1) == 0)
		ws >>= 1;

	return ws == 1;
}

// A chunk represents exactly 32 KB of uncompressed data until the last chunk in the stream,
// which can represent less than 32 KB.
#define MAX_CHUNK_SIZE (32 * 1024)

// Decoder state needed for new blocks.
typedef struct
{
	/// The window size we're working with.
	fz_lzxd_window_size_t window_size;

	// This tree cannot be used directly, it exists only to apply the delta of upcoming trees
	// to its path lengths.
	canonical_tree_t *main_tree;

	// This tree cannot be used directly, it exists only to apply the delta of upcoming trees
	// to its path lengths.
	canonical_tree_t *length_tree;
} decoder_state_t;

static window_t *
window_create(fz_context *ctx, fz_lzxd_window_size_t ws)
{
	window_t *w;

	// The window must be at least as big as the smallest chunk, or else we can't possibly
	// contain an entire chunk inside of the sliding window.
	if (ws < MAX_CHUNK_SIZE)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Illegal LZXD window size");

	// We can use bit operations if we rely on this assumption so make sure it holds.
	if (!is_power_of_two(ws))
		fz_throw(ctx, FZ_ERROR_FORMAT, "LZXD Window size must be a power of 2");

	w = fz_malloc(ctx, sizeof(window_t) - sizeof(w->buffer) + sizeof(uint8_t) * ws);
	w->pos = 0;
	w->len = ws;

	return w;
}

static void
drop_window(fz_context *ctx, window_t *w)
{
	fz_free(ctx, w);
}

static void
window_advance(window_t *self, size_t delta)
{
	self->pos = (self->pos + delta) & (self->len-1);
}

static void
window_push(window_t *self, uint8_t value)
{
	self->buffer[self->pos] = value;
	self->pos = (self->pos + 1) & (self->len-1);
}

static void
window_zero_extend(window_t *self, size_t len)
{
	size_t end = (self->pos + len) & (self->len-1);

	if (end > len)
		memset(&self->buffer[self->pos], 0, len);
	else
	{
		memset(&self->buffer[self->pos], 0, self->len - self->pos);
		memset(self->buffer, 0, end);
	}
	self->pos = end & (self->len-1);
}

void
window_copy_from_self(window_t *self, size_t offset, size_t length)
{
	// For the fast path:
	// * Source cannot wrap around
	// * `copy_within` won't overwrite as we go but we need that
	// * Destination cannot wrap around
	if (offset <= self->pos && length <= offset && self->pos + length < self->len)
	{
		// Best case: neither source or destination wrap around
		size_t start = self->pos - offset;
		memmove(&self->buffer[self->pos], &self->buffer[start], length);
	}
	else
	{
		// Either source or destination wrap around. We could expand this case into three
		// (one for only source wrapping, one for only destination wrapping, one for both)
		// but it's not really worth the effort.
		//
		// We could work out the ranges for use in `copy_within` but this is a lot simpler.
		size_t mask = self->len - 1; // relying on power of two assumption
		size_t i;
		size_t src = (self->pos - offset) & mask;
		size_t dst = self->pos;

		for (i = 0; i < length; i++)
		{
			self->buffer[dst++] = self->buffer[src++];
			dst &= mask;
			src &= mask;
		}
	}

	window_advance(self, length);
}

static void
window_copy_from_bitstream(fz_context *ctx, window_t *self, bitstream_t *bstm, size_t len)
{
	if (len > self->len)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Window Too Small");

	// This seems inefficient. Couldn't we just raw_read in 2 hits and
	// wraparound?
	if (self->pos + len > self->len)
	{
		size_t shift = self->pos + len - self->len;
		self->pos -= shift;

		memmove(&self->buffer[0], &self->buffer[shift], self->pos);
	}

	bitstream_read_raw(ctx, bstm, &self->buffer[self->pos], len);
	window_advance(self, len);
}

static void
window_past_view(fz_context *ctx, window_t *self, size_t len, uint8_t *uncompressed)
{
	if (len > MAX_CHUNK_SIZE)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Attempt to past_view too much");

	// The old code used to shuffle data within the window. We don't do that.
	// We just copy out the last 32k.
	if (self->pos >= len)
	{
		/* Simples! */
		memcpy(uncompressed, &self->buffer[self->pos-len], len);
	}
	else
	{
		size_t start = (self->pos - len) & (self->len-1);
		memcpy(uncompressed, &self->buffer[start], self->len - start);
		memcpy(&uncompressed[self->len-start], self->buffer, len - (self->len - start));
	}
}

/* block.rs */

static const uint8_t FOOTER_BITS[289] = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13,
	13, 14, 14, 15, 15, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
	17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
	17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
	17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
	17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
	17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
	17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
	17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
	17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
	17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
	17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17
};

static const uint32_t BASE_POSITION[290] = {
	0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536,
	2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576, 32768, 49152, 65536, 98304, 131072, 196608,
	262144, 393216, 524288, 655360, 786432, 917504, 1048576, 1179648, 1310720, 1441792, 1572864,
	1703936, 1835008, 1966080, 2097152, 2228224, 2359296, 2490368, 2621440, 2752512, 2883584,
	3014656, 3145728, 3276800, 3407872, 3538944, 3670016, 3801088, 3932160, 4063232, 4194304,
	4325376, 4456448, 4587520, 4718592, 4849664, 4980736, 5111808, 5242880, 5373952, 5505024,
	5636096, 5767168, 5898240, 6029312, 6160384, 6291456, 6422528, 6553600, 6684672, 6815744,
	6946816, 7077888, 7208960, 7340032, 7471104, 7602176, 7733248, 7864320, 7995392, 8126464,
	8257536, 8388608, 8519680, 8650752, 8781824, 8912896, 9043968, 9175040, 9306112, 9437184,
	9568256, 9699328, 9830400, 9961472, 10092544, 10223616, 10354688, 10485760, 10616832, 10747904,
	10878976, 11010048, 11141120, 11272192, 11403264, 11534336, 11665408, 11796480, 11927552,
	12058624, 12189696, 12320768, 12451840, 12582912, 12713984, 12845056, 12976128, 13107200,
	13238272, 13369344, 13500416, 13631488, 13762560, 13893632, 14024704, 14155776, 14286848,
	14417920, 14548992, 14680064, 14811136, 14942208, 15073280, 15204352, 15335424, 15466496,
	15597568, 15728640, 15859712, 15990784, 16121856, 16252928, 16384000, 16515072, 16646144,
	16777216, 16908288, 17039360, 17170432, 17301504, 17432576, 17563648, 17694720, 17825792,
	17956864, 18087936, 18219008, 18350080, 18481152, 18612224, 18743296, 18874368, 19005440,
	19136512, 19267584, 19398656, 19529728, 19660800, 19791872, 19922944, 20054016, 20185088,
	20316160, 20447232, 20578304, 20709376, 20840448, 20971520, 21102592, 21233664, 21364736,
	21495808, 21626880, 21757952, 21889024, 22020096, 22151168, 22282240, 22413312, 22544384,
	22675456, 22806528, 22937600, 23068672, 23199744, 23330816, 23461888, 23592960, 23724032,
	23855104, 23986176, 24117248, 24248320, 24379392, 24510464, 24641536, 24772608, 24903680,
	25034752, 25165824, 25296896, 25427968, 25559040, 25690112, 25821184, 25952256, 26083328,
	26214400, 26345472, 26476544, 26607616, 26738688, 26869760, 27000832, 27131904, 27262976,
	27394048, 27525120, 27656192, 27787264, 27918336, 28049408, 28180480, 28311552, 28442624,
	28573696, 28704768, 28835840, 28966912, 29097984, 29229056, 29360128, 29491200, 29622272,
	29753344, 29884416, 30015488, 30146560, 30277632, 30408704, 30539776, 30670848, 30801920,
	30932992, 31064064, 31195136, 31326208, 31457280, 31588352, 31719424, 31850496, 31981568,
	32112640, 32243712, 32374784, 32505856, 32636928, 32768000, 32899072, 33030144, 33161216,
	33292288, 33423360
};

typedef struct {
	tree_t *aligned_offset_tree;
	tree_t *main_tree;
	tree_t *length_tree;
}
decode_info_t;

typedef enum
{
	decoded_single,
	decoded_match,
	decoded_read
} decoded_type_t;

typedef struct
{
	decoded_type_t type;
	union
	{
		uint8_t single;
		struct
		{
			size_t offset;
			size_t length;
		} match;
		size_t read;
	} u;
} decoded_t;

typedef enum
{
	kind_verbatim,
	kind_alignedoffset,
	kind_uncompressed
} kind_type_t;

typedef struct
{
	kind_type_t type;
	union
	{
		struct
		{
			tree_t *main_tree;
			tree_t *length_tree;
		} verbatim;
		struct
		{
			tree_t *main_tree;
			tree_t *length_tree;
			tree_t *aligned_offset_tree;
		} aligned_offset;
		struct
		{
			uint32_t r[3];
		} uncompressed;
	} u;
} kind_t;

// Note that this is not the block header, but the head of the block's body, which includes
// everything except the tail of the block data (either uncompressed data or token sequence).
typedef struct
{
	// Only 24 bits may be used.
	size_t size;
	kind_t kind;
} block_t;

static void
drop_block(fz_context *ctx, block_t *block)
{
	switch (block->kind.type)
	{
	case kind_verbatim:
		drop_tree(ctx, block->kind.u.verbatim.main_tree);
		drop_tree(ctx, block->kind.u.verbatim.length_tree);
		break;
	case kind_alignedoffset:
		drop_tree(ctx, block->kind.u.aligned_offset.main_tree);
		drop_tree(ctx, block->kind.u.aligned_offset.length_tree);
		drop_tree(ctx, block->kind.u.aligned_offset.aligned_offset_tree);
		break;
	case kind_uncompressed:
		break;
	}
	block->kind.type = kind_uncompressed;
}

// Read the pretrees for the main and length tree, and with those also read the trees
// themselves, using the path lengths from a previous tree if any.
//
// This is used when reading a verbatim or aligned block.
static void
read_main_and_length_trees(fz_context *ctx, bitstream_t *bstm, decoder_state_t *state)
{
	// Verbatim block
	// Entry						Comments
	// Pretree for first 256 elements of main tree		20 elements, 4 bits each
	// Path lengths of first 256 elements of main tree	Encoded using pretree
	// Pretree for remainder of main tree			20 elements, 4 bits each
	// Path lengths of remaining elements of main tree	Encoded using pretree
	// Pretree for length tree				20 elements, 4 bits each
	// Path lengths of elements in length tree		Encoded using pretree
	// Token sequence (matches and literals)		Specified in section 2.6
	tree_update_range_with_pretree(ctx, state->main_tree, bstm, 0, 256);
	tree_update_range_with_pretree(ctx, state->main_tree, bstm, 256, 256 + 8 * window_size_position_slots(ctx, state->window_size));
	tree_update_range_with_pretree(ctx, state->length_tree, bstm, 0, 249);
}

static decoded_t
block_decode_element_aux(fz_context *ctx, bitstream_t *bstm, uint32_t r[3], decode_info_t *di)
{
	// Decoding Matches and Literals (Aligned and Verbatim Blocks)
	uint16_t main_element = tree_decode_element(ctx, di->main_tree, bstm);
	decoded_t ret;

	// Check if it is a literal character.
	if (main_element < 256)
	{
		// It is a literal, so copy the literal to output.
		ret.type = decoded_single;
		ret.u.single = main_element;
	}
	else
	{
		// Decode the match. For a match, there are two components, offset and length.
		uint16_t length_header = main_element & 7;
		uint16_t match_length;
		uint16_t position_slot;
		uint32_t match_offset;

		if (length_header == 7)
		{
			// Length of the footer.
			match_length = tree_decode_element(ctx, di->length_tree, bstm) + 7 + 2;
		}
		else
		{
			match_length = length_header + 2; // no length footer
							// Decoding a match length (if a match length < 257).
		}
		assert(match_length != 0);

		position_slot = (main_element - 256) >> 3;

		// Check for repeated offsets (positions 0, 1, 2).
		if (position_slot == 0)
		{
			match_offset = r[0];
		}
		else if (position_slot == 1)
		{
			match_offset = r[1];
			r[1] = r[0];
			r[0] = match_offset;
		}
		else if (position_slot == 2)
		{
			match_offset = r[2];
			r[2] = r[0];
			r[0] = match_offset;
		}
		else
		{
			// Not a repeated offset.
			uint8_t offset_bits = FOOTER_BITS[position_slot];

			uint32_t formatted_offset;

			if (di->aligned_offset_tree)
			{
				uint32_t verbatim_bits;
				uint16_t aligned_bits;

				// This means there are some aligned bits.
				if (offset_bits >= 3) {
					verbatim_bits = bitstream_read_bits(ctx, bstm, offset_bits - 3) << 3;
					aligned_bits = tree_decode_element(ctx, di->aligned_offset_tree, bstm);
				}
				else
				{
					// 0, 1, or 2 verbatim bits
					verbatim_bits = bitstream_read_bits(ctx, bstm, offset_bits);
					aligned_bits = 0;
				}

				formatted_offset = BASE_POSITION[position_slot] + verbatim_bits + aligned_bits;
			}
			else
			{
				// Block_type is a verbatim_block.
				uint32_t verbatim_bits = bitstream_read_bits(ctx, bstm, offset_bits);
				formatted_offset = BASE_POSITION[position_slot] + verbatim_bits;
			}

			// Decoding a match offset.
			match_offset = formatted_offset - 2;

			// Update repeated offset least recently used queue.
			r[2] = r[1];
			r[1] = r[0];
			r[0] = match_offset;
		}

		// Check for extra length.
		// > If the match length is 257 or larger, the encoded match length token
		// > (or match length, as specified in section 2.6) value is 257, and an
		// > encoded Extra Length field follows the other match encoding components,
		// > as specified in section 2.6.7, in the bitstream.

		// TODO for some reason, if we do this, parsing .xnb files with window size
		//      64KB, it breaks and stops decompressing correctly, but no idea why.
		/*
		let match_length = if match_length == 257 {
			// Decode the extra length.
			let extra_len = if bitstream.read_bit() != 0 {
				if bitstream.read_bit() != 0 {
					if bitstream.read_bit() != 0 {
						// > Prefix 0b111; Number of bits to decode 15;
						bitstream.read_bits(15)
					} else {
						// > Prefix 0b110; Number of bits to decode 12;
						bitstream.read_bits(12) + 1024 + 256
					}
				} else {
					// > Prefix 0b10; Number of bits to decode 10;
					bitstream.read_bits(10) + 256
				}
			} else {
				// > Prefix 0b0; Number of bits to decode 8;
				bitstream.read_bits(8)
			};

			// Get the match length (if match length >= 257).
			// In all cases,
			// > Base value to add to decoded value 257 + …
			257 + extra_len
		} else {
			match_length as u16
		};
		*/

		// Get match length and offset. Perform copy and paste work.
		ret.type = decoded_match;
		ret.u.match.offset = match_offset;
		ret.u.match.length = match_length;
	}

	return ret;
}

// This structure stores the required state to process the compressed chunks of data in a
// sequential order.
//
// ```no_run
// # fn get_compressed_chunk() -> Option<Vec<u8>> { unimplemented!() }
// # fn write_data(a: &[u8]) { unimplemented!() }
// use ::lzxd::{Lzxd, WindowSize};
//
// let mut lzxd = Lzxd::new(WindowSize::KB64);
//
// while let Some(chunk) = get_compressed_chunk() {
//     let decompressed = lzxd.decompress_next(&chunk);
//     write_data(decompressed.unwrap());
// }
// ```
typedef struct fz_lzxd_t
{
	// Sliding window into which data is decompressed.
	window_t *window;

	// Current decoder state.
	decoder_state_t state;

	// > The three most recent real match offsets are kept in a list.
	uint32_t r[3];

	// Has the very first chunk been read yet? Unlike the rest, it has additional data.
	int first_chunk_read;

	// This field will update after the first chunk is read, but will remain being 0
	// if the E8 Call Translation is not enabled for this stream.
	//size_t e8_translation_size;

	// Current block.
	block_t current_block;

	// Which chunk we are decoding;
	size_t chunk_num;

	// Reset interval (in chunks)
	size_t reset_interval;
} fz_lzxd_t;

static void
block_read(fz_context *ctx, bitstream_t *bstm, fz_lzxd_t *self)
{
	decoder_state_t *state = &self->state;
	block_t *block = &self->current_block;

	// > Each block of compressed data begins with a 3-bit Block Type field.
	// > Of the eight possible values, only three are valid values for the Block Type
	// > field.
	uint8_t kind = bitstream_read_bits(ctx, bstm, 3);

	drop_block(ctx, block);

	block->size = bitstream_read_u24_be(ctx, bstm);
	if (block->size == 0)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid Block Size %zd", block->size);

	switch(kind)
	{
	case 1:
		read_main_and_length_trees(ctx, bstm, state);
		block->kind.u.verbatim.length_tree = NULL;
		block->kind.u.verbatim.main_tree = canonical_tree_create_instance(ctx, state->main_tree, 0, 0);
		block->kind.type = kind_verbatim;
		block->kind.u.verbatim.length_tree = canonical_tree_create_instance(ctx, state->length_tree, 0, 1);
		break;
	case 2:
	{
		// > encoding only the delta path lengths between the current and previous trees
		//
		// This means we don't need to worry about deltas on this tree.
		int i;
		vec8_t *path_lengths = new_vec8(ctx, 8);

		for (i = 0; i < 8; i++)
			path_lengths->v[i] = bitstream_read_bits(ctx, bstm, 3);

		block->kind.type = kind_alignedoffset;
		block->kind.u.aligned_offset.main_tree = NULL;
		block->kind.u.aligned_offset.length_tree = NULL;
		block->kind.u.aligned_offset.aligned_offset_tree = canonical_tree_create_instance(ctx, path_lengths, 1, 0);

		// > An aligned offset block is identical to the verbatim block except for the
		// > presence of the aligned offset tree preceding the other trees.
		read_main_and_length_trees(ctx, bstm, state);

		block->kind.u.aligned_offset.main_tree = canonical_tree_create_instance(ctx, state->main_tree, 0, 0);
		block->kind.u.aligned_offset.length_tree = canonical_tree_create_instance(ctx, state->length_tree, 0, 1);
		break;
	}
	case 3:
	{
		if (bitstream_align(ctx, bstm))
			bitstream_read_bits(ctx, bstm, 16); // padding will be 1..=16, not 0

		block->kind.type = kind_uncompressed;
		block->kind.u.uncompressed.r[0] = bitstream_read_u32_le(ctx, bstm);
		block->kind.u.uncompressed.r[1] = bitstream_read_u32_le(ctx, bstm);
		block->kind.u.uncompressed.r[2] = bitstream_read_u32_le(ctx, bstm);
		break;
	}
	default:
		fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid Block type");
	}
}

static decoded_t
block_decode_element(fz_context *ctx, block_t *self, bitstream_t *bstm, uint32_t r[3])
{
	decoded_t decoded;

	switch (self->kind.type)
	{
	case kind_verbatim:
	{
		decode_info_t di = { NULL, self->kind.u.verbatim.main_tree, self->kind.u.verbatim.length_tree };
		decoded = block_decode_element_aux(ctx, bstm, r, &di);
		break;
	}
	case kind_alignedoffset:
	{
		decode_info_t di = { self->kind.u.aligned_offset.aligned_offset_tree, self->kind.u.aligned_offset.main_tree, self->kind.u.aligned_offset.length_tree };
		decoded = block_decode_element_aux(ctx, bstm, r, &di);
		break;
	}
	case kind_uncompressed:
	{
		r[0] = self->kind.u.uncompressed.r[0];
		r[1] = self->kind.u.uncompressed.r[1];
		r[2] = self->kind.u.uncompressed.r[2];
		decoded.type = decoded_read;
		decoded.u.read = self->size;
		break;
	}
	default:
		assert("This never happens" == NULL);
	}
	return decoded;
}

/* lib.rs */

//! This library implements the LZX compression format as described in
//! [LZX DELTA Compression and Decompression], revision 9.0.
//!
//! Lempel-Ziv Extended (LZX) is an LZ77-based compression engine, as described in [UASDC],
//! that is a universal lossless data compression algorithm. It performs no analysis on the
//! data.
//!
//! Lempel-Ziv Extended Delta (LZXD) is a derivative of the Lempel-Ziv Extended (LZX) format with
//! some modifications to facilitate efficient delta compression.
//!
//! In order to use this module, refer to the main [`Lzxd`] type and its methods.
//!
//! [LZX DELTA Compression and Decompression]: https://docs.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-patch/cc78752a-b4af-4eee-88cb-01f4d8a4c2bf
//! [UASDC]: https://ieeexplore.ieee.org/document/1055714
//! [`Lzxd`]: struct.Lzxd.html

// The main interface to perform LZXD decompression.

void
fz_drop_lzxd(fz_context *ctx, fz_lzxd_t *self)
{
	if (self == NULL)
		return;

	drop_canonical_tree(ctx, self->state.main_tree);
	drop_canonical_tree(ctx, self->state.length_tree);
	drop_window(ctx, self->window);
	drop_block(ctx, &self->current_block);
	fz_free(ctx, self);
}

// Creates a new instance of the LZXD decoder state. The [`WindowSize`] must be obtained
// from elsewhere (e.g. it may be predetermined to a certain value), and if it's wrong,
// the decompressed values won't be those expected.
//
// [`WindowSize`]: enum.WindowSize.html
fz_lzxd_t *
fz_new_lzxd(fz_context *ctx, fz_lzxd_window_size_t window_size, size_t reset_interval)
{
	fz_lzxd_t *self = fz_malloc_struct(ctx, fz_lzxd_t);

	fz_try(ctx)
	{
		// > The main tree comprises 256 elements that correspond to all possible 8-bit
		// > characters, plus 8 * NUM_POSITION_SLOTS elements that correspond to matches.
		self->state.main_tree = new_canonical_tree(ctx, 256 + 8 * window_size_position_slots(ctx, window_size));

		// > The length tree comprises 249 elements.
		self->state.length_tree = new_canonical_tree(ctx, 249);

		self->window = window_create(ctx, window_size);
		// > Because trees are output several times during compression of large amounts of
		// > data (multiple blocks), LZXD optimizes compression by encoding only the delta
		// > path lengths lengths between the current and previous trees.
		//
		// Because it uses deltas, we need to store the previous value across blocks.
		self->state.window_size = window_size;
		// > The initial state of R0, R1, R2 is (1, 1, 1).
		self->r[0] = 1;
		self->r[1] = 1;
		self->r[2] = 1;
		self->first_chunk_read = 0;
		//self->e8_translation_size = 0;
		// Start with some dummy value.
		self->current_block.size = 0;
		self->current_block.kind.type = kind_uncompressed;
		self->current_block.kind.u.uncompressed.r[0] = 1;
		self->current_block.kind.u.uncompressed.r[1] = 1;
		self->current_block.kind.u.uncompressed.r[2] = 1;
		if (reset_interval & 0x7fff)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Illegal LZXD reset interval");
		self->reset_interval = reset_interval / 0x8000;
		self->chunk_num = 0;
	}
	fz_catch(ctx)
	{
		fz_drop_lzxd(ctx, self);
		fz_rethrow(ctx);
	}

	return self;
}

// Try reading the header for the first chunk.
static void
try_read_first_chunk(fz_context *ctx, fz_lzxd_t *self, bitstream_t *bstm)
{
	// > The first bit in the first chunk in the LZXD bitstream (following the 2-byte,
	// > chunk-size prefix described in section 2.2.1) indicates the presence or absence of
	// > two 16-bit fields immediately following the single bit. If the bit is set, E8
	// > translation is enabled.
	if (!self->first_chunk_read)
	{
		/* E8 translation is a hangover from when this code used to load 8086
		 * code and then 'fixup' relocations within it. We don't support that
		 * as it's not important for our usecase. */
		int e8_translation = bitstream_read_bit(ctx, bstm) != 0;
		self->first_chunk_read = 1;
		if (e8_translation)
		{
			//int high = bitstream_read_u16_le(ctx, bstm);
			//int low = bitstream_read_u16_le(ctx, bstm);
			//self->e8_translation_size = (high << 16) | low;
			fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "E8 translation unsupported");
		}
		else
		{
			//self->e8_translation_size = 0;
		}
	}
}

// Decompresses the next compressed `chunk` from the LZXD data stream.
void
fz_lzxd_decompress_chunk(fz_context *ctx, fz_lzxd_t *self, const uint8_t *chunk, size_t chunk_len, uint8_t *uncompressed)
{
	// > A chunk represents exactly 32 KB of uncompressed data until the last chunk in the
	// > stream, which can represent less than 32 KB.
	//
	// > The LZXD engine encodes a compressed, chunk-size prefix field preceding each
	// > compressed chunk in the compressed byte stream. The compressed, chunk-size prefix
	// > field is a byte aligned, little-endian, 16-bit field.
	//
	// However, this doesn't seem to be part of LZXD itself? At least when testing with
	// `.xnb` files, every chunk comes with a compressed chunk size unless it has the flag
	// set to 0xff where it also includes the uncompressed chunk size.
	//
	// TODO maybe the docs could clarify whether this length is compressed or not
	size_t decoded_len = 0;
	bitstream_t bstm;

	if (chunk_len & 1)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Odd lengthed chunk");

	init_bitstream(ctx, &bstm, chunk, chunk_len);

	try_read_first_chunk(ctx, self, &bstm);

	do
	{
		decoded_t decoded;
		size_t advance;

		if (self->current_block.size == 0)
		{
			block_read(ctx, &bstm, self);
			assert(self->current_block.size != 0); /* Guaranteed by exception handling within block_read */
		}
		assert((((1<<(16-bstm.remaining))-1) & bstm.n ) == 0);

		decoded = block_decode_element(ctx, &self->current_block, &bstm, self->r);

		switch (decoded.type)
		{
		case decoded_single:
			window_push(self->window, decoded.u.single);
			advance = 1;
			break;
		case decoded_match:
			window_copy_from_self(self->window, decoded.u.match.offset, decoded.u.match.length);
			advance = decoded.u.match.length;
			break;
		default:
			/* default never happens by exception handling within block_decode_element,
			 * but cope with it here and fall through to decoded_read to avoid the
			 * compiler complaining that advance may be used uninitialised. */
		case decoded_read:
		{
			// Read up to end of chunk, to allow for larger blocks.
			advance = bitstream_remaining_bytes(ctx, &bstm);
			if (advance > decoded.u.read)
				advance = decoded.u.read;
			else
				bstm.exhausted = 1;
			window_copy_from_bitstream(ctx, self->window, &bstm, advance);
			break;
		}
		}

		assert(advance != 0);
		decoded_len += advance;

		if (self->current_block.size < advance)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Block overread");
		self->current_block.size -= advance;
	}
	while ((self->window->pos & 0x7fff) && !bstm.exhausted);

	// > To ensure that an exact number of input bytes represent an exact number of
	// > output bytes for each chunk, after each 32 KB of uncompressed data is
	// > represented in the output compressed bitstream, the output bitstream is padded
	// > with up to 15 bits of zeros to realign the bitstream on a 16-bit boundary
	// > (even byte boundary) for the next 32 KB of data. This results in a compressed
	// > chunk of a byte-aligned size. The compressed chunk could be smaller than 32 KB
	// > or larger than 32 KB if the data is incompressible when the chunk is not the
	// > last one.
	//
	// That's the input chunk parsed which aligned to a byte-boundary already. There is
	// no need to align the bitstream because on the next call it will be aligned.

	// TODO last chunk may misalign this and on the next iteration we wouldn't be able
	// to return a continous slice. if we're called on non-aligned, we could shift things
	// and align it.

	// FIXME: Why is the last block's size observed to be one?
	if (self->current_block.size > 1)
	{
		decoded_len = decoded_len;

		// Align the window up to 32KB.
		// See https://github.com/Lonami/lzxd/issues/7 for details.
		if (decoded_len < 0x8000)
		{
			window_zero_extend(self->window, 0x8000 - decoded_len);
			decoded_len = 0x8000;
		}
	}

	// No postprocessing. Just copy the decoded chunk out.
	window_past_view(ctx, self->window, decoded_len, uncompressed);

	self->chunk_num++;
	if (self->chunk_num % self->reset_interval == 0)
	{
		self->first_chunk_read = 0;
		self->window->pos = 0;
		drop_block(ctx, &self->current_block);
		self->r[0] = 1;
		self->r[1] = 1;
		self->r[2] = 1;
		drop_canonical_tree(ctx, self->state.main_tree);
		self->state.main_tree = NULL;
		self->state.main_tree = new_canonical_tree(ctx, 256 + 8 * window_size_position_slots(ctx, self->state.window_size));
		drop_canonical_tree(ctx, self->state.length_tree);
		self->state.length_tree = NULL;
		self->state.length_tree = new_canonical_tree(ctx, 249);
		self->current_block.size = 0;
	}
}
