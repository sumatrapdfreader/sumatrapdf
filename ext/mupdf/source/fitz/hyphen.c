// Copyright (C) 2025 Artifex Software, Inc.
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

#ifdef HAVE_OBJCOPY

extern const unsigned char _binary_resources_hyphen_hyph_std_zip_start;
extern const unsigned char _binary_resources_hyphen_hyph_std_zip_end;
#define HYPH_STD &_binary_resources_hyphen_hyph_std_zip_start, &_binary_resources_hyphen_hyph_std_zip_end-&_binary_resources_hyphen_hyph_std_zip_start

extern const unsigned char _binary_resources_hyphen_hyph_all_zip_start;
extern const unsigned char _binary_resources_hyphen_hyph_all_zip_end;
#define HYPH_ALL &_binary_resources_hyphen_hyph_all_zip_start, &_binary_resources_hyphen_hyph_all_zip_end-&_binary_resources_hyphen_hyph_all_zip_start

#else

extern const unsigned char _binary_hyph_std_zip[];
extern const unsigned int _binary_hyph_std_zip_size;
#define HYPH_STD _binary_hyph_std_zip, _binary_hyph_std_zip_size

extern const unsigned char _binary_hyph_all_zip[];
extern const unsigned int _binary_hyph_all_zip_size;
#define HYPH_ALL _binary_hyph_all_zip, _binary_hyph_all_zip_size

#endif

#if FZ_ENABLE_HYPHEN
	#if FZ_ENABLE_HYPHEN_ALL
		#define HYPH_ZIP HYPH_ALL
	#else
		#define HYPH_ZIP HYPH_STD
	#endif
#endif

struct fz_hyph_context
{
	int refs;
	fz_archive *arch;
	struct fz_hyph_lang *head;
};

struct fz_hyph_lang
{
	fz_text_language lang;
	fz_hyphenator *hyph;
	struct fz_hyph_lang *next;
};

void
fz_new_hyph_context(fz_context *ctx)
{
	if (ctx)
	{
		ctx->hyph = fz_malloc_struct(ctx, fz_hyph_context);
		ctx->hyph->refs = 1;
		ctx->hyph->head = NULL;
	}
}

fz_hyph_context *
fz_keep_hyph_context(fz_context *ctx)
{
	if (!ctx)
		return NULL;
	return fz_keep_imp(ctx, ctx->hyph, &ctx->hyph->refs);
}

void
fz_drop_hyph_context(fz_context *ctx)
{
	struct fz_hyph_lang *node, *next;
	if (!ctx)
		return;
	if (fz_drop_imp(ctx, ctx->hyph, &ctx->hyph->refs))
	{
		for (node = ctx->hyph->head; node; node = next)
		{
			next = node->next;
			fz_drop_hyphenator(ctx, node->hyph);
			fz_free(ctx, node);
		}
		fz_drop_archive(ctx, ctx->hyph->arch);
		fz_free(ctx, ctx->hyph);
	}
}

void
fz_register_hyphenator(fz_context *ctx, fz_text_language lang, fz_hyphenator *hyph)
{
	struct fz_hyph_lang *node = fz_malloc_struct(ctx, struct fz_hyph_lang);
	node->lang = lang;
	node->hyph = hyph;
	node->next = ctx->hyph->head;
	ctx->hyph->head = node;
}

fz_hyphenator *
fz_lookup_hyphenator(fz_context *ctx, fz_text_language lang)
{
	struct fz_hyph_lang *node;

	// try to find an already registered hyphenator
	for (node = ctx->hyph->head; node; node = node->next)
		if (node->lang == lang)
			return node->hyph;

#if FZ_ENABLE_HYPHEN
	{
		char lang_name[8];
		fz_hyphenator *hyph;
		fz_stream *stm;

		if (!ctx->hyph->arch)
			ctx->hyph->arch = fz_open_zip_archive_with_memory(ctx, HYPH_ZIP);

		// language code aliases (nn and nb to no, etc.)
		if (lang == FZ_LANG_TAG2('n', 'n') || lang == FZ_LANG_TAG2('n', 'b'))
			lang = FZ_LANG_TAG2('n', 'o');

		fz_string_from_text_language(lang_name, lang);

		if (fz_has_archive_entry(ctx, ctx->hyph->arch, lang_name))
		{
			stm = fz_open_archive_entry(ctx, ctx->hyph->arch, lang_name);
			fz_try(ctx)
			{
				hyph = fz_new_hyphenator_from_stream(ctx, stm);
				fz_register_hyphenator(ctx, lang, hyph);
			}
			fz_always(ctx)
				fz_drop_stream(ctx, stm);
			fz_catch(ctx)
				fz_rethrow(ctx);
			return hyph;
		}
	}
#endif

	return NULL;
}

fz_hyphenator *fz_new_hyphenator(fz_context *ctx)
{
	fz_hyphenator *hyph;
	fz_pool *pool;

	pool = fz_new_pool(ctx);
	fz_try(ctx)
	{
		hyph = fz_pool_alloc(ctx, pool, sizeof *hyph);
		hyph->pool = pool;
		hyph->node_count = 0;
		hyph->pattern_count = 0;
		hyph->trie = fz_pool_alloc(ctx, pool, sizeof *hyph->trie);
	}
	fz_catch(ctx)
	{
		fz_drop_pool(ctx, pool);
		fz_rethrow(ctx);
	}
	return hyph;
}

void fz_drop_hyphenator(fz_context *ctx, fz_hyphenator *hyph)
{
	fz_drop_pool(ctx, hyph->pool);
}

#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static void
integrate_pattern(fz_context *ctx, fz_hyphenator *hyph, fz_hyph_trie *trie, int *patstr, char *patval, int patlen, int idx)
{
	fz_hyph_trie *arc;
	if (idx >= patlen) {
		trie->patlen = patlen;
		trie->patval = patval;
	} else {
		for (arc = trie->child; arc; arc = arc->next)
			if (arc->ch == patstr[idx])
				break;
		if (!arc) {
			hyph->node_count++;
			arc = fz_pool_alloc(ctx, hyph->pool, sizeof (fz_hyph_trie));
			arc->ch = patstr[idx];
			arc->next = trie->child;
			trie->child = arc;
		}
		integrate_pattern(ctx, hyph, arc, patstr, patval, patlen, idx+1);
	}
}

static void
fz_hyphenate_insert_pattern(fz_context *ctx, fz_hyphenator *hyph, const char *s)
{
	int k, i, r, n;
	int patlen;
	int patstr[256];
	char *patval;

	hyph->pattern_count++;

	/* count number of chars in pattern */
	n = 0;
	patlen = 0;
	for (i = 0; s[i] != 0; i += fz_chartorune(&r, s+i)) {
		n++;
		if (!ISDIGIT(s[i]))
			patlen++;
	}

	/* zero values */
	patval = fz_pool_alloc(ctx, hyph->pool, patlen + 1);

	/* extract interleaved values and chars */
	for (k = i = 0; i < n;) {
		i += fz_chartorune(&r, s+i);
		if (ISDIGIT(r)) {
			patval[k] *= 10;
			patval[k] += r - '0';
		} else {
			patstr[k++] = r;
		}
	}

#if 0
	printf("INSERT PATTERN (%.*s)\n", n, s);
	for (i = 0; i < patlen+1; ++i)
		printf("  %d: (%c) %d\n", i, i<patlen?patstr[i]:' ', patval[i]);
#endif

	integrate_pattern(ctx, hyph, hyph->trie, patstr, patval, patlen, 0);
}

static void
fz_hyphenate_insert_exception(fz_context *ctx, fz_hyphenator *hyph, const char *s)
{
	int k, i, r;
	int patlen;
	int patstr[256];
	char *patval;

	hyph->pattern_count++;

	/* convert exception into pattern: "ta-ble" -> ".t8a9b8l8e8." */

	/* count number of chars in pattern */
	patlen = 2;
	for (i = 0; s[i]; i += fz_chartorune(&r, s+i))
		if (s[i] != '-')
			patlen++;

	/* zero values */
	patval = fz_pool_alloc(ctx, hyph->pool, patlen + 1);
	for (i = 1; i < patlen; ++i)
		patval[i] = 64;
	patstr[0] = '.';
	patstr[patlen-1] = '.';

	/* create interleaved values and chars */
	for (k = 1, i = 0; s[i];) {
		i += fz_chartorune(&r, s+i);
		if (r == '-') {
			patval[k] = 63;
		} else {
			patstr[k++] = r;
		}
	}

#if 0
	printf("INSERT EXCEPTION (%s)\n", s);
	for (i = 0; i < patlen+1; ++i)
		printf("  %d: (%c) %d\n", i, i<patlen?patstr[i]:' ', patval[i]);
#endif

	integrate_pattern(ctx, hyph, hyph->trie, patstr, patval, patlen, 0);
}

fz_hyphenator *
fz_new_hyphenator_from_stream(fz_context *ctx, fz_stream *stm)
{
	fz_hyphenator *hyph;
	char *line, buf[100];

	hyph = fz_new_hyphenator(ctx);
	fz_try(ctx)
	{
		// read and integrate patterns (stop on blank line)
		for (;;)
		{
			line = fz_read_line(ctx, stm, buf, sizeof buf);
			if (!line)
				break;
			if (line[0] == 0)
				break;
			fz_hyphenate_insert_pattern(ctx, hyph, line);
		}

		// read and integrate exceptions
		for (;;)
		{
			line = fz_read_line(ctx, stm, buf, sizeof buf);
			if (!line)
				break;
			if (line[0] == 0)
				break;
			fz_hyphenate_insert_exception(ctx, hyph, line);
		}
	}
	fz_catch(ctx)
	{
		fz_drop_hyphenator(ctx, hyph);
		fz_rethrow(ctx);
	}

	return hyph;
}

void
fz_hyphenate_word(fz_context *ctx, fz_hyphenator *hyph, const char *input, int input_size, char *output, int output_size)
{
	int word[64], orig[64];
	char vals[64];
	int n, i, j, k;
	fz_hyph_trie *node, *m;

	/* clear value buffer and decode utf-8 */
	word[0] = '.';
	for (i = 0, n = 1; n < 63 && i < input_size && input[i]; ++n) {
		i += fz_chartorune(orig + n, input + i);
		word[n] = fz_tolower(orig[n]);
		vals[n] = 0;
	}
	word[n++] = '.';

	if (n == 64)
	{
		if (input_size + 1 > output_size)
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "hyphenated word is too large for output buffer");
		memcpy(output, input, input_size);
		output[input_size] = 0;
		return;
	}

	/* for all chars in word */
	for (i = 0; i < n; i++)
	{
		/* follow trie finding matching pattern */
		node = hyph->trie;
		j = 0;
		while (node && i + j < n)
		{
			/* copy pattern values we have so far */
			if (node->patval) {
				for (k = 0; k < node->patlen + 1; k++) {
					vals[i+k] = MAX(vals[i+k], node->patval[k]);
				}
			}

			/* find next trie node for current char */
			m = node->child;
			node = NULL;
			while (m) {
				if (m->ch == word[i+j]) {
					node = m;
					break;
				}
				m = m->next;
			}

			/* next char */
			j++;
		}
	}

	/* output word with soft hyphens inserted */
	for (k = 0, i = 1; i < n-1; ++i) {
		// hyphen + char + zero-terminator
		if (k + 7 > output_size)
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "hyphenated word is too large for output buffer");
		if ((vals[i] & 1) && (i > 2) && (i < n - 2))
			k += fz_runetochar(output + k, 0xad);
		k += fz_runetochar(output + k, orig[i]);
	}
	output[k] = 0;
}
