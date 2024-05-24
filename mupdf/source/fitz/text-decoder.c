// Copyright (C) 2024 Artifex Software, Inc.
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
#include "mupdf/pdf.h"

static int simple_text_decode_bound(fz_text_decoder *dec, unsigned char *s, int n)
{
	return n * 4 + 1;
}

static int simple_text_decode_size(fz_text_decoder *dec, unsigned char *s, int n)
{
	const unsigned short *table = dec->table1;
	unsigned char *e = s + n;
	int len = 1;
	while (s < e)
		len += fz_runelen(table[*s++]);
	return len;
}

static void simple_text_decode(fz_text_decoder *dec, char *p, unsigned char *s, int n)
{
	const unsigned short *table = dec->table1;
	unsigned char *e = s + n;
	while (s < e)
		p += fz_runetochar(p, table[*s++]);
	*p = 0;
}

static int utf16be_text_decode_bound(fz_text_decoder *dec, unsigned char *s, int n)
{
	return n * 2 + 1;
}

static int utf16le_text_decode_bound(fz_text_decoder *dec, unsigned char *s, int n)
{
	return n * 2 + 1;
}

static int utf16be_text_decode_size(fz_text_decoder *dec, unsigned char *s, int n)
{
	unsigned char *e = s + n;
	int len = 1;
	while (s + 1 < e) {
		len += fz_runelen(s[0] << 8 | s[1]);
		s += 2;
	}
	return len;
}

static int utf16le_text_decode_size(fz_text_decoder *dec, unsigned char *s, int n)
{
	unsigned char *e = s + n;
	int len = 1;
	while (s + 1 < e) {
		len += fz_runelen(s[0] | s[1] << 8);
		s += 2;
	}
	return len;
}

static void utf16be_text_decode(fz_text_decoder *dec, char *p, unsigned char *s, int n)
{
	unsigned char *e = s + n;
	while (s + 1 < e) {
		p += fz_runetochar(p, s[0] << 8 | s[1]);
		s += 2;
	}
	*p = 0;
}

static void utf16le_text_decode(fz_text_decoder *dec, char *p, unsigned char *s, int n)
{
	unsigned char *e = s + n;
	while (s + 1 < e) {
		p += fz_runetochar(p, s[0] | s[1] << 8);
		s += 2;
	}
	*p = 0;
}

static int cjk_text_decode_bound(fz_text_decoder *dec, unsigned char *s, int n)
{
	return n * 4 + 1;
}

static int cjk_text_decode_size(fz_text_decoder *dec, unsigned char *s, int n)
{
	unsigned char *e = s + n;
	pdf_cmap *to_cid = dec->table1;
	pdf_cmap *to_uni = dec->table2;
	unsigned int raw;
	int cid, uni;
	int len = 1;
	while (s < e) {
		s += pdf_decode_cmap(to_cid, s, e, &raw);
		cid = pdf_lookup_cmap(to_cid, raw);
		uni = pdf_lookup_cmap(to_uni, cid);
		if (uni < 0) {
			// ASCII control characters are missing in the CMaps
			if (raw < 32)
				uni = raw;
			else
				uni = FZ_REPLACEMENT_CHARACTER;
		}
		len += fz_runelen(uni);
	}
	return len;
}

static void cjk_text_decode(fz_text_decoder *dec, char *p, unsigned char *s, int n)
{
	unsigned char *e = s + n;
	pdf_cmap *to_cid = dec->table1;
	pdf_cmap *to_uni = dec->table2;
	unsigned int raw;
	int cid, uni;
	while (s < e) {
		s += pdf_decode_cmap(to_cid, s, e, &raw);
		cid = pdf_lookup_cmap(to_cid, raw);
		uni = pdf_lookup_cmap(to_uni, cid);
		if (uni < 0) {
			// ASCII control characters are missing in the CMaps
			if (raw < 32)
				uni = raw;
			else
				uni = FZ_REPLACEMENT_CHARACTER;
		}
		p += fz_runetochar(p, uni);
	}
	*p = 0;
}

static void fz_init_simple_text_decoder(fz_context *ctx, fz_text_decoder *dec, const unsigned short *table)
{
	dec->decode_bound = simple_text_decode_bound;
	dec->decode_size = simple_text_decode_size;
	dec->decode = simple_text_decode;
	dec->table1 = (void*)table;
}

static void fz_init_utf16be_text_decoder(fz_context *ctx, fz_text_decoder *dec)
{
	dec->decode_bound = utf16be_text_decode_bound;
	dec->decode_size = utf16be_text_decode_size;
	dec->decode = utf16be_text_decode;
}

static void fz_init_utf16le_text_decoder(fz_context *ctx, fz_text_decoder *dec)
{
	dec->decode_bound = utf16le_text_decode_bound;
	dec->decode_size = utf16le_text_decode_size;
	dec->decode = utf16le_text_decode;
}

static void fz_init_cjk_text_decoder(fz_context *ctx, fz_text_decoder *dec, const char *to_cid, const char *to_uni)
{
	dec->decode_bound = cjk_text_decode_bound;
	dec->decode_size = cjk_text_decode_size;
	dec->decode = cjk_text_decode;
	dec->table1 = pdf_load_builtin_cmap(ctx, to_cid);
	if (!dec->table1)
		fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "unknown CMap: %s", to_cid);
	dec->table2 = pdf_load_builtin_cmap(ctx, to_uni);
	if (!dec->table2)
		fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "unknown CMap: %s", to_uni);
}

void fz_init_text_decoder(fz_context *ctx, fz_text_decoder *dec, const char *enc)
{
	// Recognize IANA character set identifiers (case insensitive).
	// https://www.iana.org/assignments/character-sets/character-sets.xhtml

	if (!fz_strcasecmp(enc, "utf-16"))
		fz_init_utf16le_text_decoder(ctx, dec);
	else if (!fz_strcasecmp(enc, "utf-16be"))
		fz_init_utf16be_text_decoder(ctx, dec);
	else if (!fz_strcasecmp(enc, "utf-16le"))
		fz_init_utf16le_text_decoder(ctx, dec);

	else if (!fz_strcasecmp(enc, "euc-jp"))
		fz_init_cjk_text_decoder(ctx, dec, "EUC-H", "Adobe-Japan1-UCS2");
	else if (!fz_strcasecmp(enc, "shift_jis") || !fz_strcasecmp(enc, "sjis"))
		fz_init_cjk_text_decoder(ctx, dec, "90msp-H", "Adobe-Japan1-UCS2");

	else if (!fz_strcasecmp(enc, "euc-kr"))
		fz_init_cjk_text_decoder(ctx, dec, "KSCms-UHC-H", "Adobe-Korea1-UCS2");

	else if (!fz_strcasecmp(enc, "euc-cn"))
		fz_init_cjk_text_decoder(ctx, dec, "GB-EUC-H", "Adobe-GB1-UCS2");
	else if (!fz_strcasecmp(enc, "gbk") || !fz_strcasecmp(enc, "gb2312") || !fz_strcasecmp(enc, "gb18030"))
		fz_init_cjk_text_decoder(ctx, dec, "GBK2K-H", "Adobe-GB1-UCS2");

	else if (!fz_strcasecmp(enc, "euc-tw"))
		fz_init_cjk_text_decoder(ctx, dec, "CNS-EUC-H", "Adobe-CNS1-UCS2");
	else if (!fz_strcasecmp(enc, "big5"))
		fz_init_cjk_text_decoder(ctx, dec, "ETen-B5-H", "Adobe-CNS1-UCS2");
	else if (!fz_strcasecmp(enc, "big5-hkscs"))
		fz_init_cjk_text_decoder(ctx, dec, "HKscs-B5-H", "Adobe-CNS1-UCS2");

	else if (!fz_strcasecmp(enc, "iso-8859-1"))
		fz_init_simple_text_decoder(ctx, dec, fz_unicode_from_iso8859_1);
	else if (!fz_strcasecmp(enc, "iso-8859-7"))
		fz_init_simple_text_decoder(ctx, dec, fz_unicode_from_iso8859_7);
	else if (!fz_strcasecmp(enc, "koi8-r"))
		fz_init_simple_text_decoder(ctx, dec, fz_unicode_from_koi8u);
	else if (!fz_strcasecmp(enc, "windows-1250"))
		fz_init_simple_text_decoder(ctx, dec, fz_unicode_from_windows_1250);
	else if (!fz_strcasecmp(enc, "windows-1251"))
		fz_init_simple_text_decoder(ctx, dec, fz_unicode_from_windows_1251);
	else if (!fz_strcasecmp(enc, "windows-1252"))
		fz_init_simple_text_decoder(ctx, dec, fz_unicode_from_windows_1252);

	else
		fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "unknown text encoding: %s", enc);
}
