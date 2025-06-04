// Copyright (C) 2004-2025 Artifex Software, Inc.
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

/* Android font functions */

static fz_font *load_noto(fz_context *ctx, const char *a, const char *b, const char *c, int idx)
{
	char buf[500];
	fz_font *font = NULL;
	fz_try(ctx)
	{
		fz_snprintf(buf, sizeof buf, "/system/fonts/%s%s%s.ttf", a, b, c);
		if (!fz_file_exists(ctx, buf))
			fz_snprintf(buf, sizeof buf, "/system/fonts/%s%s%s.otf", a, b, c);
		if (!fz_file_exists(ctx, buf))
			fz_snprintf(buf, sizeof buf, "/system/fonts/%s%s%s.ttc", a, b, c);
		if (fz_file_exists(ctx, buf))
			font = fz_new_font_from_file(ctx, NULL, buf, idx, 0);
	}
	fz_catch(ctx)
		return NULL;
	return font;
}

static fz_font *load_noto_cjk(fz_context *ctx, int lang)
{
	fz_font *font = load_noto(ctx, "NotoSerif", "CJK", "-Regular", lang);
	if (!font) font = load_noto(ctx, "NotoSans", "CJK", "-Regular", lang);
	if (!font) font = load_noto(ctx, "DroidSans", "Fallback", "", 0);
	return font;
}

static fz_font *load_noto_arabic(fz_context *ctx)
{
	fz_font *font = load_noto(ctx, "Noto", "Naskh", "-Regular", 0);
	if (!font) font = load_noto(ctx, "Noto", "NaskhArabic", "-Regular", 0);
	if (!font) font = load_noto(ctx, "Droid", "Naskh", "-Regular", 0);
	if (!font) font = load_noto(ctx, "NotoSerif", "Arabic", "-Regular", 0);
	if (!font) font = load_noto(ctx, "NotoSans", "Arabic", "-Regular", 0);
	if (!font) font = load_noto(ctx, "DroidSans", "Arabic", "-Regular", 0);
	return font;
}

static fz_font *load_noto_try(fz_context *ctx, const char *stem)
{
	fz_font *font = load_noto(ctx, "NotoSerif", stem, "-Regular", 0);
	if (!font) font = load_noto(ctx, "NotoSerif", stem, "-VF", 0);
	if (!font) font = load_noto(ctx, "NotoSans", stem, "-Regular", 0);
	if (!font) font = load_noto(ctx, "NotoSans", stem, "-VF", 0);
	if (!font) font = load_noto(ctx, "DroidSans", stem, "-Regular", 0);
	return font;
}

enum { JP, KR, SC, TC };

fz_font *load_droid_fallback_font(fz_context *ctx, int script, int language, int serif, int bold, int italic)
{
	const char *stem = NULL;
	switch (script)
	{
	case UCDN_SCRIPT_HANGUL: return load_noto_cjk(ctx, KR);
	case UCDN_SCRIPT_HIRAGANA: return load_noto_cjk(ctx, JP);
	case UCDN_SCRIPT_KATAKANA: return load_noto_cjk(ctx, JP);
	case UCDN_SCRIPT_BOPOMOFO: return load_noto_cjk(ctx, TC);
	case UCDN_SCRIPT_HAN:
		switch (language)
		{
		case FZ_LANG_ja: return load_noto_cjk(ctx, JP);
		case FZ_LANG_ko: return load_noto_cjk(ctx, KR);
		case FZ_LANG_zh_Hans: return load_noto_cjk(ctx, SC);
		default:
		case FZ_LANG_zh_Hant: return load_noto_cjk(ctx, TC);
		}
	case UCDN_SCRIPT_ARABIC:
		return load_noto_arabic(ctx);
	default:
		stem = fz_lookup_noto_stem_from_script(ctx, script, language);
		if (stem)
			return load_noto_try(ctx, stem);
	}
	return NULL;
}

fz_font *load_droid_cjk_font(fz_context *ctx, const char *name, int ros, int serif)
{
	switch (ros)
	{
	case FZ_ADOBE_CNS: return load_noto_cjk(ctx, TC);
	case FZ_ADOBE_GB: return load_noto_cjk(ctx, SC);
	case FZ_ADOBE_JAPAN: return load_noto_cjk(ctx, JP);
	case FZ_ADOBE_KOREA: return load_noto_cjk(ctx, KR);
	}
	return NULL;
}

fz_font *load_droid_font(fz_context *ctx, const char *name, int bold, int italic, int needs_exact_metrics)
{
	return NULL;
}
