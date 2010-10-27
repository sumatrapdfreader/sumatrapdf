#include "fitz.h"
#include "mupdf.h"

#ifndef WIN32
/* TODO: should build freetype from sources in unix/mac build as well */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_XFREE86_H
#else
/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=687 */
/* get access to the internal Type 1 specific structures (for extracting the embedded encoding table) */
#define FT2_BUILD_LIBRARY
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_XFREE86_H

#include FT_INTERNAL_INTERNAL_H
#include FT_INTERNAL_TYPE1_TYPES_H
#endif

static char *basefontnames[14][7] =
{
	{ "Courier", "CourierNew", "CourierNewPSMT", nil },
	{ "Courier-Bold", "CourierNew,Bold", "Courier,Bold",
		"CourierNewPS-BoldMT", "CourierNew-Bold", nil },
	{ "Courier-Oblique", "CourierNew,Italic", "Courier,Italic",
		"CourierNewPS-ItalicMT", "CourierNew-Italic", nil },
	{ "Courier-BoldOblique", "CourierNew,BoldItalic", "Courier,BoldItalic",
		"CourierNewPS-BoldItalicMT", "CourierNew-BoldItalic", nil },
	{ "Helvetica", "ArialMT", "Arial", nil },
	{ "Helvetica-Bold", "Arial-BoldMT", "Arial,Bold", "Arial-Bold",
		"Helvetica,Bold", nil },
	{ "Helvetica-Oblique", "Arial-ItalicMT", "Arial,Italic", "Arial-Italic",
		"Helvetica,Italic", "Helvetica-Italic", nil },
	{ "Helvetica-BoldOblique", "Arial-BoldItalicMT",
		"Arial,BoldItalic", "Arial-BoldItalic",
		"Helvetica,BoldItalic", "Helvetica-BoldItalic", nil },
	{ "Times-Roman", "TimesNewRomanPSMT", "TimesNewRoman",
		"TimesNewRomanPS", nil },
	{ "Times-Bold", "TimesNewRomanPS-BoldMT", "TimesNewRoman,Bold",
		"TimesNewRomanPS-Bold", "TimesNewRoman-Bold", nil },
	{ "Times-Italic", "TimesNewRomanPS-ItalicMT", "TimesNewRoman,Italic",
		"TimesNewRomanPS-Italic", "TimesNewRoman-Italic", nil },
	{ "Times-BoldItalic", "TimesNewRomanPS-BoldItalicMT",
		"TimesNewRoman,BoldItalic", "TimesNewRomanPS-BoldItalic",
		"TimesNewRoman-BoldItalic", nil },
	{ "Symbol", nil },
	{ "ZapfDingbats", nil }
};

static int strcmpignorespace(char *a, char *b)
{
	while (1)
	{
		while (*a == ' ')
			a++;
		while (*b == ' ')
			b++;
		if (*a != *b)
			return 1;
		if (*a == 0)
			return *a != *b;
		if (*b == 0)
			return *a != *b;
		a++;
		b++;
	}
}

static char *cleanfontname(char *fontname)
{
	int i, k;
	for (i = 0; i < 14; i++)
		for (k = 0; basefontnames[i][k]; k++)
			if (!strcmpignorespace(basefontnames[i][k], fontname))
				return basefontnames[i][0];
	return fontname;
}

/*
 * FreeType and Rendering glue
 */

enum { UNKNOWN, TYPE1, TRUETYPE };

static int ftkind(FT_Face face)
{
	const char *kind = FT_Get_X11_Font_Format(face);
	pdf_logfont("ft font format %s\n", kind);
	if (!strcmp(kind, "TrueType"))
		return TRUETYPE;
	if (!strcmp(kind, "Type 1"))
		return TYPE1;
	if (!strcmp(kind, "CFF"))
		return TYPE1;
	if (!strcmp(kind, "CID Type 1"))
		return TYPE1;
	return UNKNOWN;
}

static int ftcharindex(FT_Face face, int cid)
{
	int gid = FT_Get_Char_Index(face, cid);
	if (gid == 0)
		gid = FT_Get_Char_Index(face, 0xf000 + cid);
	return gid;
}

/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=687 */
/* extract the Type 1 font's embedded encoding table */
static int ftloadt1encoding(FT_Face face, char **estrings)
{
#ifdef WIN32
	T1_Encoding encoding;
	T1_Font font;
	int i;

	/* not (yet) implemented for CFF and CID fonts */
	if (strcmp(FT_Get_X11_Font_Format(face), "Type 1") != 0)
		return 0;

	font = &((T1_Face)face)->type1;
	switch (font->encoding_type)
	{
	case T1_ENCODING_TYPE_STANDARD:
		pdf_loadencoding(estrings, "StandardEncoding");
		return 1;
	case T1_ENCODING_TYPE_ISOLATIN1:
		pdf_loadencoding(estrings, "WinAnsiEncoding");
		return 1;
	case T1_ENCODING_TYPE_EXPERT:
		pdf_loadencoding(estrings, "MacExpertEncoding");
		return 1;
	case T1_ENCODING_TYPE_ARRAY:
		encoding = &font->encoding;
		if (encoding->code_first == encoding->code_last)
			break;
		assert(encoding->code_first < encoding->code_last && encoding->code_last <= 256);
		for (i = encoding->code_first; i < encoding->code_last; i++)
			estrings[i] = encoding->char_name[i];
		return 1;
	}
#endif
	return 0;
}

static inline int ftcidtogid(pdf_fontdesc *fontdesc, int cid)
{
	if (fontdesc->tottfcmap)
	{
		cid = pdf_lookupcmap(fontdesc->tottfcmap, cid);
		return ftcharindex(fontdesc->font->ftface, cid);
	}

	if (fontdesc->cidtogid)
		return fontdesc->cidtogid[cid];

	return cid;
}

int
pdf_fontcidtogid(pdf_fontdesc *fontdesc, int cid)
{
	if (fontdesc->font->ftface)
		return ftcidtogid(fontdesc, cid);
	return cid;
}

static int ftwidth(pdf_fontdesc *fontdesc, int cid)
{
	int gid, fterr;

	gid = ftcidtogid(fontdesc, cid);

	fterr = FT_Load_Glyph(fontdesc->font->ftface, gid,
			FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM);
	if (fterr)
	{
		fz_warn("freetype load glyph (gid %d): %s", gid, ft_errorstring(fterr));
		return 0;
	}
	return ((FT_Face)fontdesc->font->ftface)->glyph->advance.x;
}

/*
 * Basic encoding tables
 */

static int mrecode(char *name)
{
	int i;
	for (i = 0; i < 256; i++)
		if (pdf_macroman[i] && !strcmp(name, pdf_macroman[i]))
			return i;
	return -1;
}

/*
 * Create and destroy
 */

pdf_fontdesc *
pdf_keepfont(pdf_fontdesc *fontdesc)
{
	fontdesc->refs ++;
	return fontdesc;
}

void
pdf_dropfont(pdf_fontdesc *fontdesc)
{
	if (fontdesc && --fontdesc->refs == 0)
	{
		if (fontdesc->font)
			fz_dropfont(fontdesc->font);
		if (fontdesc->buffer)
			fz_free(fontdesc->buffer);
		if (fontdesc->encoding)
			pdf_dropcmap(fontdesc->encoding);
		if (fontdesc->tottfcmap)
			pdf_dropcmap(fontdesc->tottfcmap);
		if (fontdesc->tounicode)
			pdf_dropcmap(fontdesc->tounicode);
		fz_free(fontdesc->cidtogid);
		fz_free(fontdesc->cidtoucs);
		fz_free(fontdesc->hmtx);
		fz_free(fontdesc->vmtx);
		fz_free(fontdesc);
	}
}

pdf_fontdesc *
pdf_newfontdesc(void)
{
	pdf_fontdesc *fontdesc;

	fontdesc = fz_malloc(sizeof (pdf_fontdesc));
	fontdesc->refs = 1;

	fontdesc->font = nil;
	fontdesc->buffer = nil;

	fontdesc->flags = 0;
	fontdesc->italicangle = 0;
	fontdesc->ascent = 0;
	fontdesc->descent = 0;
	fontdesc->capheight = 0;
	fontdesc->xheight = 0;
	fontdesc->missingwidth = 0;

	fontdesc->encoding = nil;
	fontdesc->tottfcmap = nil;
	fontdesc->ncidtogid = 0;
	fontdesc->cidtogid = nil;

	fontdesc->tounicode = nil;
	fontdesc->ncidtoucs = 0;
	fontdesc->cidtoucs = nil;

	fontdesc->wmode = 0;

	fontdesc->hmtxcap = 0;
	fontdesc->vmtxcap = 0;
	fontdesc->nhmtx = 0;
	fontdesc->nvmtx = 0;
	fontdesc->hmtx = nil;
	fontdesc->vmtx = nil;

	fontdesc->dhmtx.lo = 0x0000;
	fontdesc->dhmtx.hi = 0xFFFF;
	fontdesc->dhmtx.w = 1000;

	fontdesc->dvmtx.lo = 0x0000;
	fontdesc->dvmtx.hi = 0xFFFF;
	fontdesc->dvmtx.x = 0;
	fontdesc->dvmtx.y = 880;
	fontdesc->dvmtx.w = -1000;

	fontdesc->isembedded = 0;

	return fontdesc;
}

/*
 * Simple fonts (Type1 and TrueType)
 */

static fz_error
loadsimplefont(pdf_fontdesc **fontdescp, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	fz_obj *descriptor;
	fz_obj *encoding;
	fz_obj *widths;
	unsigned short *etable = nil;
	pdf_fontdesc *fontdesc;
	fz_bbox bbox;
	FT_Face face;
	FT_CharMap cmap;
	int kind;
	int symbolic;

	char *basefont;
	char *fontname;
	char *estrings[256];
	char ebuffer[256][32];
	int i, k, n;
	int fterr;

	basefont = fz_toname(fz_dictgets(dict, "BaseFont"));
	fontname = cleanfontname(basefont);

	/* Load font file */

	fontdesc = pdf_newfontdesc();

	pdf_logfont("load simple font (%d %d R) ptr=%p {\n", fz_tonum(dict), fz_togen(dict), fontdesc);
	pdf_logfont("basefont %s -> %s\n", basefont, fontname);

	descriptor = fz_dictgets(dict, "FontDescriptor");
	if (descriptor)
		error = pdf_loadfontdescriptor(fontdesc, xref, descriptor, nil, basefont);
	else
		error = pdf_loadbuiltinfont(fontdesc, fontname);
	if (error)
		goto cleanup;

	face = fontdesc->font->ftface;
	kind = ftkind(face);

	pdf_logfont("ft name '%s' '%s'\n", face->family_name, face->style_name);

	bbox.x0 = (face->bbox.xMin * 1000) / face->units_per_EM;
	bbox.y0 = (face->bbox.yMin * 1000) / face->units_per_EM;
	bbox.x1 = (face->bbox.xMax * 1000) / face->units_per_EM;
	bbox.y1 = (face->bbox.yMax * 1000) / face->units_per_EM;

	pdf_logfont("ft bbox [%d %d %d %d]\n", bbox.x0, bbox.y0, bbox.x1, bbox.y1);

	if (bbox.x0 == bbox.x1)
		fz_setfontbbox(fontdesc->font, -1000, -1000, 2000, 2000);
	else
		fz_setfontbbox(fontdesc->font, bbox.x0, bbox.y0, bbox.x1, bbox.y1);

	/* Encoding */

	symbolic = fontdesc->flags & 4;

	if (face->num_charmaps > 0)
		cmap = face->charmaps[0];
	else
		cmap = nil;

	for (i = 0; i < face->num_charmaps; i++)
	{
		FT_CharMap test = face->charmaps[i];

		if (kind == TYPE1)
		{
			if (test->platform_id == 7)
				cmap = test;
		}

		if (kind == TRUETYPE)
		{
			if (test->platform_id == 1 && test->encoding_id == 0)
				cmap = test;
			if (test->platform_id == 3 && test->encoding_id == 1)
				cmap = test;
		}
	}

	if (cmap)
	{
		fterr = FT_Set_Charmap(face, cmap);
		if (fterr)
			fz_warn("freetype could not set cmap: %s", ft_errorstring(fterr));
	}
	else
		fz_warn("freetype could not find any cmaps");

	etable = fz_malloc(sizeof(unsigned short) * 256);
	for (i = 0; i < 256; i++)
	{
		estrings[i] = nil;
		etable[i] = 0;
	}

	encoding = fz_dictgets(dict, "Encoding");
	if (encoding)
	{
		if (fz_isname(encoding))
			pdf_loadencoding(estrings, fz_toname(encoding));

		if (fz_isdict(encoding))
		{
			fz_obj *base, *diff, *item;

			base = fz_dictgets(encoding, "BaseEncoding");
			if (fz_isname(base))
				pdf_loadencoding(estrings, fz_toname(base));
			else if (!fontdesc->isembedded && !symbolic)
				pdf_loadencoding(estrings, "StandardEncoding");
			/* cf. http://bugs.ghostscript.com/show_bug.cgi?id=690615 and http://code.google.com/p/sumatrapdf/issues/detail?id=687 */
			/* try to extract an encoding from the font or synthesize a likely one */
			/* note: FT_Get_Name_Index fails for symbolic CFF fonts, so let them be encoded by index */
			else if (!fontdesc->encoding && !ftloadt1encoding(face, estrings) && !(symbolic && !strcmp(FT_Get_X11_Font_Format(face), "CFF")))
				pdf_loadencoding(estrings, "StandardEncoding");

			diff = fz_dictgets(encoding, "Differences");
			if (fz_isarray(diff))
			{
				n = fz_arraylen(diff);
				k = 0;
				for (i = 0; i < n; i++)
				{
					item = fz_arrayget(diff, i);
					if (fz_isint(item))
						k = fz_toint(item);
					if (fz_isname(item))
						estrings[k++] = fz_toname(item);
					if (k < 0) k = 0;
					if (k > 255) k = 255;
				}
			}
		}
	}

	/* start with the builtin encoding */
	for (i = 0; i < 256; i++)
		etable[i] = ftcharindex(face, i);

	/* encode by glyph name where we can */
	if (kind == TYPE1)
	{
		pdf_logfont("encode type1/cff by strings\n");
		for (i = 0; i < 256; i++)
		{
			if (estrings[i])
			{
				etable[i] = FT_Get_Name_Index(face, estrings[i]);
				if (etable[i] == 0)
				{
					int aglcode = pdf_lookupagl(estrings[i]);
					char **aglnames = pdf_lookupaglnames(aglcode);
					while (*aglnames)
					{
						etable[i] = FT_Get_Name_Index(face, *aglnames);
						if (etable[i])
							break;
						aglnames++;
					}
				}
			}
		}
	}

	/* encode by glyph name where we can */
	if (kind == TRUETYPE)
	{
		/* Unicode cmap */
		if (!symbolic && face->charmap && face->charmap->platform_id == 3)
		{
			pdf_logfont("encode truetype via unicode\n");
			for (i = 0; i < 256; i++)
			{
				if (estrings[i])
				{
					int aglcode = pdf_lookupagl(estrings[i]);
					if (!aglcode)
						etable[i] = FT_Get_Name_Index(face, estrings[i]);
					else
						etable[i] = ftcharindex(face, aglcode);
				}
			}
		}

		/* MacRoman cmap */
		else if (!symbolic && face->charmap && face->charmap->platform_id == 1)
		{
			pdf_logfont("encode truetype via macroman\n");
			for (i = 0; i < 256; i++)
			{
				if (estrings[i])
				{
					k = mrecode(estrings[i]);
					if (k <= 0)
						etable[i] = FT_Get_Name_Index(face, estrings[i]);
					else
						etable[i] = ftcharindex(face, k);
				}
			}
		}

		/* Symbolic cmap */
		else
		{
			pdf_logfont("encode truetype symbolic\n");
			for (i = 0; i < 256; i++)
			{
				if (estrings[i])
				{
					etable[i] = FT_Get_Name_Index(face, estrings[i]);
					if (etable[i] == 0)
						etable[i] = ftcharindex(face, i);
				}
			}
		}
	}

	/* try to reverse the glyph names from the builtin encoding */
	for (i = 0; i < 256; i++)
	{
		if (etable[i] && !estrings[i])
		{
			if (FT_HAS_GLYPH_NAMES(face))
			{
				fterr = FT_Get_Glyph_Name(face, etable[i], ebuffer[i], 32);
				if (fterr)
					fz_warn("freetype get glyph name (gid %d): %s", etable[i], ft_errorstring(fterr));
				if (ebuffer[i][0])
					estrings[i] = ebuffer[i];
			}
			else
			{
				estrings[i] = (char*) pdf_winansi[i]; /* discard const */
			}
		}
	}

	/* Prevent encoding Differences from being overwritten by reloading them */
	/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=115 */
	if (fz_isdict(encoding))
	{
		fz_obj *diff, *item;

		diff = fz_dictgets(encoding, "Differences");
		if (fz_isarray(diff))
		{
			n = fz_arraylen(diff);
			k = 0;
			for (i = 0; i < n; i++)
			{
				item = fz_arrayget(diff, i);
				if (fz_isint(item))
					k = fz_toint(item);
				if (fz_isname(item))
					estrings[k++] = fz_toname(item);
				if (k < 0) k = 0;
				if (k > 255) k = 255;
			}
		}
	}

	fontdesc->encoding = pdf_newidentitycmap(0, 1);
	fontdesc->ncidtogid = 256;
	fontdesc->cidtogid = etable;

	error = pdf_loadtounicode(fontdesc, xref, estrings, nil, fz_dictgets(dict, "ToUnicode"));
	if (error)
		goto cleanup;

	/* Widths */

	pdf_setdefaulthmtx(fontdesc, fontdesc->missingwidth);

	widths = fz_dictgets(dict, "Widths");
	if (widths)
	{
		int first, last;

		first = fz_toint(fz_dictgets(dict, "FirstChar"));
		last = fz_toint(fz_dictgets(dict, "LastChar"));

		if (first < 0 || last > 255 || first > last)
			first = last = 0;

		for (i = 0; i < last - first + 1; i++)
		{
			int wid = fz_toint(fz_arrayget(widths, i));
			pdf_addhmtx(fontdesc, i + first, i + first, wid);
		}
	}
	else
	{
		fterr = FT_Set_Char_Size(face, 1000, 1000, 72, 72);
		if (fterr)
			fz_warn("freetype set character size: %s", ft_errorstring(fterr));
		for (i = 0; i < 256; i++)
		{
			pdf_addhmtx(fontdesc, i, i, ftwidth(fontdesc, i));
		}
	}

	pdf_endhmtx(fontdesc);

	pdf_logfont("}\n");

	*fontdescp = fontdesc;
	return fz_okay;

cleanup:
	if (etable != fontdesc->cidtogid)
		fz_free(etable);
	pdf_dropfont(fontdesc);
	return fz_rethrow(error, "cannot load simple font (%d %d R)", fz_tonum(dict), fz_togen(dict));
}

/*
 * CID Fonts
 */

static fz_error
loadcidfont(pdf_fontdesc **fontdescp, pdf_xref *xref, fz_obj *dict, fz_obj *encoding, fz_obj *tounicode)
{
	fz_error error;
	fz_obj *widths;
	fz_obj *descriptor;
	pdf_fontdesc *fontdesc;
	FT_Face face;
	fz_bbox bbox;
	int kind;
	char collection[256];
	char *basefont;
	int i, k, fterr;
	fz_obj *obj;
	int dw;

	/* Get font name and CID collection */

	basefont = fz_toname(fz_dictgets(dict, "BaseFont"));

	{
		fz_obj *cidinfo;
		char tmpstr[64];
		int tmplen;

		cidinfo = fz_dictgets(dict, "CIDSystemInfo");
		if (!cidinfo)
			return fz_throw("cid font is missing info");

		obj = fz_dictgets(cidinfo, "Registry");
		tmplen = MIN(sizeof tmpstr - 1, fz_tostrlen(obj));
		memcpy(tmpstr, fz_tostrbuf(obj), tmplen);
		tmpstr[tmplen] = '\0';
		fz_strlcpy(collection, tmpstr, sizeof collection);

		fz_strlcat(collection, "-", sizeof collection);

		obj = fz_dictgets(cidinfo, "Ordering");
		tmplen = MIN(sizeof tmpstr - 1, fz_tostrlen(obj));
		memcpy(tmpstr, fz_tostrbuf(obj), tmplen);
		tmpstr[tmplen] = '\0';
		fz_strlcat(collection, tmpstr, sizeof collection);
	}

	/* Load font file */

	fontdesc = pdf_newfontdesc();

	pdf_logfont("load cid font (%d %d R) ptr=%p {\n", fz_tonum(dict), fz_togen(dict), fontdesc);
	pdf_logfont("basefont %s\n", basefont);
	pdf_logfont("collection %s\n", collection);

	descriptor = fz_dictgets(dict, "FontDescriptor");
	if (descriptor)
		error = pdf_loadfontdescriptor(fontdesc, xref, descriptor, collection, basefont);
	else
		error = fz_throw("syntaxerror: missing font descriptor");
	if (error)
		goto cleanup;

	face = fontdesc->font->ftface;
	kind = ftkind(face);

	bbox.x0 = (face->bbox.xMin * 1000) / face->units_per_EM;
	bbox.y0 = (face->bbox.yMin * 1000) / face->units_per_EM;
	bbox.x1 = (face->bbox.xMax * 1000) / face->units_per_EM;
	bbox.y1 = (face->bbox.yMax * 1000) / face->units_per_EM;

	pdf_logfont("ft bbox [%d %d %d %d]\n", bbox.x0, bbox.y0, bbox.x1, bbox.y1);

	if (bbox.x0 == bbox.x1)
		fz_setfontbbox(fontdesc->font, -1000, -1000, 2000, 2000);
	else
		fz_setfontbbox(fontdesc->font, bbox.x0, bbox.y0, bbox.x1, bbox.y1);

	/* Encoding */

	error = fz_okay;
	if (fz_isname(encoding))
	{
		pdf_logfont("encoding /%s\n", fz_toname(encoding));
		if (!strcmp(fz_toname(encoding), "Identity-H"))
			fontdesc->encoding = pdf_newidentitycmap(0, 2);
		else if (!strcmp(fz_toname(encoding), "Identity-V"))
			fontdesc->encoding = pdf_newidentitycmap(1, 2);
		else
			error = pdf_loadsystemcmap(&fontdesc->encoding, fz_toname(encoding));
	}
	else if (fz_isindirect(encoding))
	{
		pdf_logfont("encoding %d %d R\n", fz_tonum(encoding), fz_togen(encoding));
		error = pdf_loadembeddedcmap(&fontdesc->encoding, xref, encoding);
	}
	else
	{
		error = fz_throw("syntaxerror: font missing encoding");
	}
	if (error)
		goto cleanup;

	pdf_setfontwmode(fontdesc, pdf_getwmode(fontdesc->encoding));
	pdf_logfont("wmode %d\n", pdf_getwmode(fontdesc->encoding));

	if (kind == TRUETYPE)
	{
		fz_obj *cidtogidmap;

		cidtogidmap = fz_dictgets(dict, "CIDToGIDMap");
		if (fz_isindirect(cidtogidmap))
		{
			fz_buffer *buf;

			pdf_logfont("cidtogidmap stream\n");

			error = pdf_loadstream(&buf, xref, fz_tonum(cidtogidmap), fz_togen(cidtogidmap));
			if (error)
				goto cleanup;

			fontdesc->ncidtogid = (buf->len) / 2;
			fontdesc->cidtogid = fz_malloc(fontdesc->ncidtogid * sizeof(unsigned short));
			for (i = 0; i < fontdesc->ncidtogid; i++)
				fontdesc->cidtogid[i] = (buf->data[i * 2] << 8) + buf->data[i * 2 + 1];

			fz_dropbuffer(buf);
		}

		/* if truetype font is external, cidtogidmap should not be identity */
		/* so we map from cid to unicode and then map that through the (3 1) */
		/* unicode cmap to get a glyph id */
		else if (fontdesc->font->ftsubstitute)
		{
			pdf_logfont("emulate ttf cidfont\n");

			fterr = FT_Select_Charmap(face, ft_encoding_unicode);
			if (fterr)
			{
				error = fz_throw("fonterror: no unicode cmap when emulating CID font: %s", ft_errorstring(fterr));
				goto cleanup;
			}

			if (!strcmp(collection, "Adobe-CNS1"))
				error = pdf_loadsystemcmap(&fontdesc->tottfcmap, "Adobe-CNS1-UCS2");
			else if (!strcmp(collection, "Adobe-GB1"))
				error = pdf_loadsystemcmap(&fontdesc->tottfcmap, "Adobe-GB1-UCS2");
			else if (!strcmp(collection, "Adobe-Japan1"))
				error = pdf_loadsystemcmap(&fontdesc->tottfcmap, "Adobe-Japan1-UCS2");
			else if (!strcmp(collection, "Adobe-Japan2"))
				error = pdf_loadsystemcmap(&fontdesc->tottfcmap, "Adobe-Japan2-UCS2");
			else if (!strcmp(collection, "Adobe-Korea1"))
				error = pdf_loadsystemcmap(&fontdesc->tottfcmap, "Adobe-Korea1-UCS2");
			else
				error = fz_okay;

			if (error)
			{
				error = fz_rethrow(error, "cannot load system cmap %s", collection);
				goto cleanup;
			}
		}
	}

	error = pdf_loadtounicode(fontdesc, xref, nil, collection, tounicode);
	if (error)
		goto cleanup;

	/* Horizontal */

	dw = 1000;
	obj = fz_dictgets(dict, "DW");
	if (obj)
		dw = fz_toint(obj);
	pdf_setdefaulthmtx(fontdesc, dw);

	widths = fz_dictgets(dict, "W");
	if (widths)
	{
		int c0, c1, w;

		for (i = 0; i < fz_arraylen(widths); )
		{
			c0 = fz_toint(fz_arrayget(widths, i));
			obj = fz_arrayget(widths, i + 1);
			if (fz_isarray(obj))
			{
				for (k = 0; k < fz_arraylen(obj); k++)
				{
					w = fz_toint(fz_arrayget(obj, k));
					pdf_addhmtx(fontdesc, c0 + k, c0 + k, w);
				}
				i += 2;
			}
			else
			{
				c1 = fz_toint(obj);
				w = fz_toint(fz_arrayget(widths, i + 2));
				pdf_addhmtx(fontdesc, c0, c1, w);
				i += 3;
			}
		}
	}

	pdf_endhmtx(fontdesc);

	/* Vertical */

	if (pdf_getwmode(fontdesc->encoding) == 1)
	{
		int dw2y = 880;
		int dw2w = -1000;

		obj = fz_dictgets(dict, "DW2");
		if (obj)
		{
			dw2y = fz_toint(fz_arrayget(obj, 0));
			dw2w = fz_toint(fz_arrayget(obj, 1));
		}

		pdf_setdefaultvmtx(fontdesc, dw2y, dw2w);

		widths = fz_dictgets(dict, "W2");
		if (widths)
		{
			int c0, c1, w, x, y;

			for (i = 0; i < fz_arraylen(widths); )
			{
				c0 = fz_toint(fz_arrayget(widths, i));
				obj = fz_arrayget(widths, i + 1);
				if (fz_isarray(obj))
				{
					for (k = 0; k < fz_arraylen(obj); k += 3)
					{
						w = fz_toint(fz_arrayget(obj, k + 0));
						x = fz_toint(fz_arrayget(obj, k + 1));
						y = fz_toint(fz_arrayget(obj, k + 2));
						pdf_addvmtx(fontdesc, c0 + k, c0 + k, x, y, w);
					}
					i += 2;
				}
				else
				{
					c1 = fz_toint(obj);
					w = fz_toint(fz_arrayget(widths, i + 2));
					x = fz_toint(fz_arrayget(widths, i + 3));
					y = fz_toint(fz_arrayget(widths, i + 4));
					pdf_addvmtx(fontdesc, c0, c1, x, y, w);
					i += 5;
				}
			}
		}

		pdf_endvmtx(fontdesc);
	}

	pdf_logfont("}\n");

	*fontdescp = fontdesc;
	return fz_okay;

cleanup:
	pdf_dropfont(fontdesc);
	return fz_rethrow(error, "cannot load cid font (%d %d R)", fz_tonum(dict), fz_togen(dict));
}

static fz_error
loadtype0(pdf_fontdesc **fontdescp, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	fz_obj *dfonts;
	fz_obj *dfont;
	fz_obj *subtype;
	fz_obj *encoding;
	fz_obj *tounicode;

	dfonts = fz_dictgets(dict, "DescendantFonts");
	if (!dfonts)
		return fz_throw("cid font is missing descendant fonts");

	dfont = fz_arrayget(dfonts, 0);

	subtype = fz_dictgets(dfont, "Subtype");
	encoding = fz_dictgets(dict, "Encoding");
	tounicode = fz_dictgets(dict, "ToUnicode");

	if (fz_isname(subtype) && !strcmp(fz_toname(subtype), "CIDFontType0"))
		error = loadcidfont(fontdescp, xref, dfont, encoding, tounicode);
	else if (fz_isname(subtype) && !strcmp(fz_toname(subtype), "CIDFontType2"))
		error = loadcidfont(fontdescp, xref, dfont, encoding, tounicode);
	else
		error = fz_throw("syntaxerror: unknown cid font type");
	if (error)
		return fz_rethrow(error, "cannot load descendant font (%d %d R)", fz_tonum(dfont), fz_togen(dfont));

	return fz_okay;
}

/*
 * FontDescriptor
 */

fz_error
pdf_loadfontdescriptor(pdf_fontdesc *fontdesc, pdf_xref *xref, fz_obj *dict, char *collection, char *basefont)
{
	fz_error error;
	fz_obj *obj1, *obj2, *obj3, *obj;
	fz_rect bbox;
	char *fontname;
	char *origname;

	pdf_logfont("load fontdescriptor {\n");

	/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1014 */
	if (!strchr(basefont, ',') || strchr(basefont, '+'))
		origname = fz_toname(fz_dictgets(dict, "FontName"));
	else
		origname = basefont;
	fontname = cleanfontname(origname);

	pdf_logfont("fontname %s -> %s\n", origname, fontname);

	fontdesc->flags = fz_toint(fz_dictgets(dict, "Flags"));
	fontdesc->italicangle = fz_toreal(fz_dictgets(dict, "ItalicAngle"));
	fontdesc->ascent = fz_toreal(fz_dictgets(dict, "Ascent"));
	fontdesc->descent = fz_toreal(fz_dictgets(dict, "Descent"));
	fontdesc->capheight = fz_toreal(fz_dictgets(dict, "CapHeight"));
	fontdesc->xheight = fz_toreal(fz_dictgets(dict, "XHeight"));
	fontdesc->missingwidth = fz_toreal(fz_dictgets(dict, "MissingWidth"));

	bbox = pdf_torect(fz_dictgets(dict, "FontBBox"));
	pdf_logfont("bbox [%g %g %g %g]\n", bbox.x0, bbox.y0, bbox.x1, bbox.y1);

	pdf_logfont("flags %d\n", fontdesc->flags);

	obj1 = fz_dictgets(dict, "FontFile");
	obj2 = fz_dictgets(dict, "FontFile2");
	obj3 = fz_dictgets(dict, "FontFile3");
	obj = obj1 ? obj1 : obj2 ? obj2 : obj3;

	if (getenv("NOFONT"))
		obj = nil;

	if (fz_isindirect(obj))
	{
		error = pdf_loadembeddedfont(fontdesc, xref, obj);
		if (error)
		{
			fz_catch(error, "ignored error when loading embedded font, attempting to load system font");
			if (origname != fontname)
				error = pdf_loadbuiltinfont(fontdesc, fontname);
			else
				error = pdf_loadsystemfont(fontdesc, fontname, collection);
			if (error)
				return fz_rethrow(error, "cannot load font descriptor (%d %d R)", fz_tonum(dict), fz_togen(dict));
		}
	}
	else
	{
		if (origname != fontname && 0 /* SumatraPDF: prefer system fonts to the built-in ones */)
			error = pdf_loadbuiltinfont(fontdesc, fontname);
		else
			error = pdf_loadsystemfont(fontdesc, fontname, collection);
		if (error)
			return fz_rethrow(error, "cannot load font descriptor (%d %d R)", fz_tonum(dict), fz_togen(dict));
	}

	fz_strlcpy(fontdesc->font->name, fontname, sizeof fontdesc->font->name);

	pdf_logfont("}\n");

	return fz_okay;

}

static void
pdf_makewidthtable(pdf_fontdesc *fontdesc)
{
	fz_font *font = fontdesc->font;
	int i, k, cid, gid;

	font->widthcount = 0;
	for (i = 0; i < fontdesc->nhmtx; i++)
	{
		for (k = fontdesc->hmtx[i].lo; k <= fontdesc->hmtx[i].hi; k++)
		{
			cid = pdf_lookupcmap(fontdesc->encoding, k);
			gid = pdf_fontcidtogid(fontdesc, cid);
			if (gid > font->widthcount)
				font->widthcount = gid;
		}
	}
	font->widthcount ++;

	font->widthtable = fz_malloc(sizeof(int) * font->widthcount);
	memset(font->widthtable, 0, sizeof(int) * font->widthcount);

	for (i = 0; i < fontdesc->nhmtx; i++)
	{
		for (k = fontdesc->hmtx[i].lo; k <= fontdesc->hmtx[i].hi; k++)
		{
			cid = pdf_lookupcmap(fontdesc->encoding, k);
			gid = pdf_fontcidtogid(fontdesc, cid);
			font->widthtable[gid] = fontdesc->hmtx[i].w;
		}
	}
}

fz_error
pdf_loadfont(pdf_fontdesc **fontdescp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict)
{
	fz_error error;
	char *subtype;
	fz_obj *dfonts;
	fz_obj *charprocs;

	if ((*fontdescp = pdf_finditem(xref->store, pdf_dropfont, dict)))
	{
		pdf_keepfont(*fontdescp);
		return fz_okay;
	}

	subtype = fz_toname(fz_dictgets(dict, "Subtype"));
	dfonts = fz_dictgets(dict, "DescendantFonts");
	charprocs = fz_dictgets(dict, "CharProcs");

	if (subtype && !strcmp(subtype, "Type0"))
		error = loadtype0(fontdescp, xref, dict);
	else if (subtype && !strcmp(subtype, "Type1"))
		error = loadsimplefont(fontdescp, xref, dict);
	else if (subtype && !strcmp(subtype, "MMType1"))
		error = loadsimplefont(fontdescp, xref, dict);
	else if (subtype && !strcmp(subtype, "TrueType"))
		error = loadsimplefont(fontdescp, xref, dict);
	else if (subtype && !strcmp(subtype, "Type3"))
		error = pdf_loadtype3font(fontdescp, xref, rdb, dict);
	else if (charprocs)
	{
		fz_warn("unknown font format, guessing type3.");
		error = pdf_loadtype3font(fontdescp, xref, rdb, dict);
	}
	else if (dfonts)
	{
		fz_warn("unknown font format, guessing type0.");
		error = loadtype0(fontdescp, xref, dict);
	}
	else
	{
		fz_warn("unknown font format, guessing type1 or truetype.");
		error = loadsimplefont(fontdescp, xref, dict);
	}
	if (error)
		return fz_rethrow(error, "cannot load font (%d %d R)", fz_tonum(dict), fz_togen(dict));

	/* Save the widths to stretch non-CJK substitute fonts */
	if ((*fontdescp)->font->ftsubstitute && !(*fontdescp)->tottfcmap)
		pdf_makewidthtable(*fontdescp);

	/* SumatraPDF: this font renders wrong without hinting */
	if (strstr((*fontdescp)->font->name, "MingLiU"))
		(*fontdescp)->font->fthint = 1;

	pdf_storeitem(xref->store, pdf_keepfont, pdf_dropfont, dict, *fontdescp);

	return fz_okay;
}

void
pdf_debugfont(pdf_fontdesc *fontdesc)
{
	int i;

	printf("fontdesc {\n");

	if (fontdesc->font->ftface)
		printf("\tfreetype font\n");
	if (fontdesc->font->t3procs)
		printf("\ttype3 font\n");

	printf("\twmode %d\n", fontdesc->wmode);
	printf("\tDW %d\n", fontdesc->dhmtx.w);

	printf("\tW {\n");
	for (i = 0; i < fontdesc->nhmtx; i++)
		printf("\t\t<%04x> <%04x> %d\n",
			fontdesc->hmtx[i].lo, fontdesc->hmtx[i].hi, fontdesc->hmtx[i].w);
	printf("\t}\n");

	if (fontdesc->wmode)
	{
		printf("\tDW2 [%d %d]\n", fontdesc->dvmtx.y, fontdesc->dvmtx.w);
		printf("\tW2 {\n");
		for (i = 0; i < fontdesc->nvmtx; i++)
			printf("\t\t<%04x> <%04x> %d %d %d\n", fontdesc->vmtx[i].lo, fontdesc->vmtx[i].hi,
				fontdesc->vmtx[i].x, fontdesc->vmtx[i].y, fontdesc->vmtx[i].w);
		printf("\t}\n");
	}
}
