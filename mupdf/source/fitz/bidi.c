/*
 * Bidirectional text processing.
 *
 * Processes unicode text by arranging the characters into an order suitable
 * for display. E.g. Hebrew text will be arranged from right-to-left and
 * any English within the text will remain in the left-to-right order.
 * Characters such as parenthesis will be substituted for their mirrored
 * equivalents if they are part of text which must be reversed.
 *
 * This is an implementation of the unicode Bidirectional Algorithm which
 * can be found here: http://www.unicode.org/reports/tr9/ and is based
 * on the reference implementation of the algorithm found on that page.
 *
 * For a nice overview of how it works, read this...
 * http://www.w3.org/TR/REC-html40/struct/dirlang.html
 *
 * Extracted from the SmartOffice code, where it was modified by Ian
 * Beveridge.
 *
 * Copyright (C) Picsel, 2004. All Rights Reserved.
 */

/*
 * Original copyright notice from unicode reference implementation.
 * ----------------------------------------------------------------
 * Written by: Asmus Freytag
 *	C++ and Windows dependencies removed, and
 *	command line interface added by: Rick McGowan
 *
 *	Copyright (C) 1999, ASMUS, Inc. All Rights Reserved
 */

/*
 * Includes...
 */

#include "mupdf/fitz.h"
#include "mupdf/ucdn.h"
#include "bidi-imp.h" /* standard bidi code interface */
#include <assert.h>

/*
 * Macros...
 */

#define ODD(x) ((x) & 1)

#define REPLACEABLE_TYPE(t) ( \
		((t)==BDI_ES) || ((t)==BDI_ET) || ((t)==BDI_CS) || \
		((t)==BDI_NSM) || ((t)==BDI_PDF) || ((t)==BDI_BN) || \
		((t)==BDI_S) || ((t)==BDI_WS) || ((t)==BDI_N) )

#ifdef DEBUG_BIDI_VERBOSE
#define DBUGVF(params) do { fz_warn params; } while (0)
#else
#define DBUGVF(params) do {} while (0)
#endif

#ifdef DEBUG_BIDI_OUTLINE
#define DBUGH(params) do { fz_warn params; } while (0)
#else
#define DBUGH(params) do {} while (0)
#endif

#define UNICODE_EOS					0
#define UNICODE_DIGIT_ZERO				0x0030
#define UNICODE_DIGIT_NINE				0x0039
#define UNICODE_SUPERSCRIPT_TWO				0x00B2
#define UNICODE_SUPERSCRIPT_THREE			0x00B3
#define UNICODE_SUPERSCRIPT_ONE				0x00B9
#define UNICODE_RTL_START				0x0590
#define UNICODE_RTL_END					0x07BF
#define UNICODE_ARABIC_INDIC_DIGIT_ZERO			0x0660
#define UNICODE_ARABIC_INDIC_DIGIT_NINE			0x0669
#define UNICODE_EXTENDED_ARABIC_INDIC_DIGIT_ZERO	0x06F0
#define UNICODE_EXTENDED_ARABIC_INDIC_DIGIT_NINE	0x06F9
#define UNICODE_ZERO_WIDTH_NON_JOINER			0x200C
#define UNICODE_SUPERSCRIPT_ZERO			0x2070
#define UNICODE_SUPERSCRIPT_FOUR			0x2074
#define UNICODE_SUPERSCRIPT_NINE			0x2079
#define UNICODE_SUBSCRIPT_ZERO				0x2080
#define UNICODE_SUBSCRIPT_NINE				0x2089
#define UNICODE_CIRCLED_DIGIT_ONE			0x2460
#define UNICODE_NUMBER_TWENTY_FULL_STOP			0x249B
#define UNICODE_CIRCLED_DIGIT_ZERO			0x24EA
#define UNICODE_FULLWIDTH_DIGIT_ZERO			0xFF10
#define UNICODE_FULLWIDTH_DIGIT_NINE			0xFF19

#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif

/*
 * Enumerations...
 */

#ifdef DEBUG_BIDI_VERBOSE
/* display support: */
static const char char_from_types[] =
{
	' ',	/* ON */
	'>',	/* L */
	'<',	/* R */
	'9',	/* AN */
	'1',	/* EN */
	'a',	/* AL */
	'@',	/* NSM */
	'.',	/* CS */
	',',	/* ES */
	'$',	/* ET */
	':',	/* BN */
	'X',	/* S */
	'_',	/* WS */
	'B',	/* B */
	'+',	/* RLO */
	'+',	/* RLE */
	'+',	/* LRO */
	'+',	/* LRE */
	'-',	/* PDF */
	'='	/* LS */
};
#endif

/*
 * Functions and static functions...
 */

/* UCDN uses a different ordering than Bidi does. We cannot
 * change to the UCDN ordering, as the bidi-std.c code relies
 * on the exact ordering (at least that N = ON = 0). We
 * therefore map between the two using this small table. It
 * also takes care of fudging LRI, RLI, FSI and PDI, that this
 * code does not currently support. */
static const uint8_t ucdn_to_bidi[] =
{
	BDI_L,		/* UCDN_BIDI_CLASS_L = 0 */
	BDI_LRE,	/* UCDN_BIDI_CLASS_LRE = 1 */
	BDI_LRO,	/* UCDN_BIDI_CLASS_LRO = 2 */
	BDI_R,		/* UCDN_BIDI_CLASS_R = 3 */
	BDI_AL,		/* UCDN_BIDI_CLASS_AL = 4 */
	BDI_RLE,	/* UCDN_BIDI_CLASS_RLE = 5 */
	BDI_RLO,	/* UCDN_BIDI_CLASS_RLO = 6 */
	BDI_PDF,	/* UCDN_BIDI_CLASS_PDF = 7 */
	BDI_EN,		/* UCDN_BIDI_CLASS_EN = 8 */
	BDI_ES,		/* UCDN_BIDI_CLASS_ES = 9 */
	BDI_ET,		/* UCDN_BIDI_CLASS_ET = 10 */
	BDI_AN,		/* UCDN_BIDI_CLASS_AN = 11 */
	BDI_CS,		/* UCDN_BIDI_CLASS_CS = 12 */
	BDI_NSM,	/* UCDN_BIDI_CLASS_NSM = 13 */
	BDI_BN,		/* UCDN_BIDI_CLASS_BN = 14 */
	BDI_B,		/* UCDN_BIDI_CLASS_B = 15 */
	BDI_S,		/* UCDN_BIDI_CLASS_S = 16 */
	BDI_WS,		/* UCDN_BIDI_CLASS_WS = 17 */
	BDI_ON,		/* UCDN_BIDI_CLASS_ON = 18 */
	BDI_LRE,	/* UCDN_BIDI_CLASS_LRI = 19 */
	BDI_RLE,	/* UCDN_BIDI_CLASS_RLI = 20 */
	BDI_N,		/* UCDN_BIDI_CLASS_FSI = 21 */
	BDI_N,		/* UCDN_BIDI_CLASS_PDI = 22 */
};

#define class_from_ch_ws(ch) (ucdn_to_bidi[ucdn_get_bidi_class(ch)])

/* Return a direction for white-space on the second pass of the algorithm. */
static fz_bidi_chartype class_from_ch_n(uint32_t ch)
{
	fz_bidi_chartype from_ch_ws = class_from_ch_ws(ch);
	if (from_ch_ws == BDI_S || from_ch_ws == BDI_WS)
		return BDI_N;
	return from_ch_ws;
}

/* Split fragments into single scripts (or punctuation + single script) */
static void
split_at_script(const uint32_t *fragment,
		size_t fragment_len,
		int level,
		void *arg,
		fz_bidi_fragment_fn *callback)
{
	int script = UCDN_SCRIPT_COMMON;
	size_t script_start, i;

	script_start = 0;
	for (i = 0; i < fragment_len; i++)
	{
		int s = ucdn_get_script(fragment[i]);
		if (s == UCDN_SCRIPT_COMMON || s == UCDN_SCRIPT_INHERITED)
		{
			/* Punctuation etc. This is fine. */
		}
		else if (s == script)
		{
			/* Same script. Still fine. */
		}
		else if (script == UCDN_SCRIPT_COMMON || script == UCDN_SCRIPT_INHERITED)
		{
			/* First non punctuation thing. Set the script. */
			script = s;
		}
		else
		{
			/* Change of script. Break the fragment. */
			(*callback)(&fragment[script_start], i - script_start, level, script, arg);
			script_start = i;
			script = s;
		}
	}
	if (script_start != fragment_len)
	{
		(*callback)(&fragment[script_start], fragment_len - script_start, level, script, arg);
	}
}

/* Determines the character classes for all following
 * passes of the algorithm. A character class is basically the type of Bidi
 * behaviour that the character exhibits.
 */
static void
classify_characters(const uint32_t *text,
		fz_bidi_chartype *types,
		size_t len,
		fz_bidi_flags flags)
{
	size_t i;

	if ((flags & FZ_BIDI_CLASSIFY_WHITE_SPACE)!=0)
	{
		for (i = 0; i < len; i++)
		{
			types[i] = class_from_ch_ws(text[i]);
		}
	}
	else
	{
#ifdef DEBUG_BIDI_VERBOSE
		fprintf(stderr, "Text:  ");
		for (i = 0; i < len; i++)
		{
			/* So that we can actually sort of read the debug string, any
			 * non-ascii characters are replaced with a 1-digit hash
			 * value from 0-9, making non-english characters appear
			 * as numbers
			 */
			fprintf(stderr, "%c", (text[i] <= 127 && text[i] >= 32) ?
					text[i] : text[i] % 9 + '0');
		}
		fprintf(stderr, "\nTypes: ");
#endif
		for (i = 0; i < len; i++)
		{
			types[i] = class_from_ch_n(text[i]);
#ifdef DEBUG_BIDI_VERBOSE
			fprintf(stderr, "%c", char_from_types[(int)types[i]]);
#endif
		}
#ifdef DEBUG_BIDI_VERBOSE
		fprintf(stderr, "\n");
#endif
	}
}

/* Determines the base level of the text.
 * Implements rule P2 of the Unicode Bidi Algorithm.
 * Note: Ignores explicit embeddings
 */
static fz_bidi_level base_level_from_text(fz_bidi_chartype *types, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
	{
		switch (types[i])
		{
		/* strong left */
		case BDI_L:
			return FZ_BIDI_LTR;

		/* strong right */
		case BDI_R:
		case BDI_AL:
			return FZ_BIDI_RTL;
		}
	}
	return FZ_BIDI_LTR;
}

static fz_bidi_direction direction_from_type(fz_bidi_chartype type)
{
	switch (type)
	{
	case BDI_L:
	case BDI_EN:
		return FZ_BIDI_LTR;

	case BDI_R:
	case BDI_AL:
		return FZ_BIDI_RTL;

	default:
		return FZ_BIDI_NEUTRAL;
	}
}

static void
classify_quoted_blocks(const uint32_t *text,
		fz_bidi_chartype *types,
		size_t len)
{
	size_t i;
	int inQuote = FALSE;
	int pdfNeeded = FALSE;
	int ltrFound = FALSE;
	int rtlFound = FALSE;

	/* Only do anything special here if there is mixed content
	 * (LTR *and* RTL) in the text.
	 */
	for (i = 0; i < len; i++)
	{
		switch (direction_from_type(types[i]))
		{
		case FZ_BIDI_LTR:
			ltrFound = TRUE;
			break;

		case FZ_BIDI_RTL:
			rtlFound = TRUE;
			break;

		default:
			break;
		}
	}

	/* Only make any changes if *both* LTR and RTL characters exist
	 * in this text.
	 */
	if (!ltrFound || !rtlFound)
	{
		return;
	}

	for (i = 0; i < len; i++)
	{
		if (text[i]=='"')
		{
			/* If we're already in a quote then terminate it,
			 * else start a new block.
			 */
			if (inQuote)
			{
				inQuote = FALSE;
				if (pdfNeeded)
				{
					pdfNeeded = FALSE;
					types[i] = BDI_PDF;
				}
			}
			else
			{
				size_t j;
				int done = FALSE;

				inQuote = TRUE;

				/* Find the first strong right or left type and
				 * use that to determine whether we should classify
				 * the quote as LRE or RLE. Or neither, if we
				 * hit another quote before any strongly-directional
				 * character.
				 */
				for (j = i + 1; !done && (j < len) && text[j] != '"'; ++j)
				{
					switch(types[j])
					{
					case BDI_RLE:
					case BDI_LRE:
						done = TRUE;
						break;

					case BDI_L:
					case BDI_EN:
						types[i] = BDI_LRE;
						pdfNeeded = TRUE;
						done = TRUE;
						break;

					case BDI_R:
					case BDI_AL:
						types[i] = BDI_RLE;
						pdfNeeded = TRUE;
						done = TRUE;
						break;

					default:
						break;
					}
				}
			}
		}
	}
}

/* Creates a buffer with an embedding level for every character in the
 * given text. Also determines the base level and returns it in
 * *baseDir if *baseDir does not initially contain a valid direction.
 */
static fz_bidi_level *
create_levels(fz_context *ctx,
		const uint32_t *text,
		size_t len,
		fz_bidi_direction *baseDir,
		int resolveWhiteSpace,
		int flags)
{
	fz_bidi_level *levels, *plevels;
	fz_bidi_chartype *types = NULL;
	fz_bidi_chartype *ptypes;
	fz_bidi_level baseLevel;
	const uint32_t *ptext;
	size_t plen, remaining;

	levels = Memento_label(fz_malloc(ctx, len * sizeof(*levels)), "bidi_levels");

	fz_var(types);

	fz_try(ctx)
	{
		types = fz_malloc(ctx, len * sizeof(fz_bidi_chartype));

		classify_characters(text, types, len, flags);

		if (*baseDir != FZ_BIDI_LTR && *baseDir != FZ_BIDI_RTL)
		{
			/* Derive the base level from the text and
			 * update *baseDir in case the caller wants to know.
			 */
			baseLevel = base_level_from_text(types, len);
			*baseDir = ODD(baseLevel)==1 ? FZ_BIDI_RTL : FZ_BIDI_LTR;
		}
		else
		{
			baseLevel = (fz_bidi_level)*baseDir;
		}

		{
			/* Replace tab with base direction, i.e. make tab appear as
			 * 'strong left' if the base direction is left-to-right and
			 * 'strong right' if base direction is right-to-left. This
			 * allows Layout to implicitly treat tabs as 'segment separators'.
			 */
			size_t i;

			for (i = 0u; i < len; i++)
			{
				if (text[i]=='\t')
				{
					types[i] = (*baseDir == FZ_BIDI_RTL) ? BDI_R : BDI_L;
				}
			}
		}

		/* Look for quotation marks. Classify them as RLE or LRE
		 * or leave them alone, depending on what follows them.
		 */
		classify_quoted_blocks(text, types, len);

		/* Work one paragraph at a time. */
		plevels = levels;
		ptypes = types;
		ptext = text;
		remaining = len;
		while (remaining)
		{
			plen = fz_bidi_resolve_paragraphs(ptypes, remaining);

			/* Work out the levels and character types... */
			(void)fz_bidi_resolve_explicit(baseLevel, BDI_N, ptypes, plevels, plen, 0);
			fz_bidi_resolve_weak(ctx, baseLevel, ptypes, plevels, plen);
			fz_bidi_resolve_neutrals(baseLevel, ptypes, plevels, plen);
			fz_bidi_resolve_implicit(ptypes, plevels, plen);

			classify_characters(ptext, ptypes, plen, FZ_BIDI_CLASSIFY_WHITE_SPACE);

			if (resolveWhiteSpace)
			{
				/* resolve whitespace */
				fz_bidi_resolve_whitespace(baseLevel, ptypes, plevels, plen);
			}

			plevels += plen;
			ptypes += plen;
			ptext += plen;
			remaining -= plen;
		}

		/* The levels buffer now has odd and even numbers indicating
		 * rtl or ltr characters, respectively.
		 */
#ifdef DEBUG_BIDI_VERBOSE
		fprintf(stderr, "Levels: ");
		{
			size_t i;
			for (i = 0; i < len; i++)
			{
				fprintf(stderr, "%d", levels[i]>9?0:levels[i]);
			}
			fprintf(stderr, "\n");
		}
#endif
	}
	fz_always(ctx)
	{
		fz_free(ctx, types);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, levels);
		fz_rethrow(ctx);
	}
	return levels;
}

/* Partitions the given character sequence into one or more unidirectional
 * fragments and invokes the given callback function for each fragment.
 */
void fz_bidi_fragment_text(fz_context *ctx,
		const uint32_t *text,
		size_t textlen,
		fz_bidi_direction *baseDir,
		fz_bidi_fragment_fn *callback,
		void *arg,
		int flags)
{
	size_t startOfFragment;
	size_t i;
	fz_bidi_level *levels;

	if (text == NULL || callback == NULL || textlen == 0)
		return;

	DBUGH(("fz_bidi_fragment_text('%S', len = %d)\n", text, textlen));

	levels = create_levels(ctx, text, textlen, baseDir, FALSE, flags);

	/* We now have an array with an embedding level
	 * for each character in text.
	 */
	assert(levels != NULL);

	fz_try(ctx)
	{
		startOfFragment = 0;
		for (i = 1; i < textlen; i++)
		{
			if (levels[i] != levels[i-1])
			{
				/* We've gone past the end of the fragment.
				 * Create a text object for it, then start
				 * a new fragment.
				 */
				split_at_script(&text[startOfFragment],
						i - startOfFragment,
						levels[startOfFragment],
						arg,
						callback);
				startOfFragment = i;
			}
		}
		/* Now i == textlen. Deal with the final (or maybe only) fragment. */
		/* otherwise create 1 fragment */
		split_at_script(&text[startOfFragment],
				i - startOfFragment,
				levels[startOfFragment],
				arg,
				callback);
	}
	fz_always(ctx)
	{
		fz_free(ctx, levels);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}
