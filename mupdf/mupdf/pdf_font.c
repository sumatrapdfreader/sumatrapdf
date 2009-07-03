#include "fitz.h"
#include "mupdf.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_XFREE86_H

static char *basefontnames[14][7] =
{
	{ "Courier", "CourierNew", "CourierNewPSMT", 0 },
	{ "Courier-Bold", "CourierNew,Bold", "Courier,Bold",
		"CourierNewPS-BoldMT", "CourierNew-Bold", 0 },
	{ "Courier-Oblique", "CourierNew,Italic", "Courier,Italic",
		"CourierNewPS-ItalicMT", "CourierNew-Italic", 0 },
	{ "Courier-BoldOblique", "CourierNew,BoldItalic", "Courier,BoldItalic",
		"CourierNewPS-BoldItalicMT", "CourierNew-BoldItalic", 0 },
	{ "Helvetica", "ArialMT", "Arial", 0 },
	{ "Helvetica-Bold", "Arial-BoldMT", "Arial,Bold", "Arial-Bold",
		"Helvetica,Bold", 0 },
	{ "Helvetica-Oblique", "Arial-ItalicMT", "Arial,Italic", "Arial-Italic",
		"Helvetica,Italic", "Helvetica-Italic", 0 },
	{ "Helvetica-BoldOblique", "Arial-BoldItalicMT",
		"Arial,BoldItalic", "Arial-BoldItalic",
		"Helvetica,BoldItalic", "Helvetica-BoldItalic", 0 },
	{ "Times-Roman", "TimesNewRomanPSMT", "TimesNewRoman",
		"TimesNewRomanPS", 0 },
	{ "Times-Bold", "TimesNewRomanPS-BoldMT", "TimesNewRoman,Bold",
		"TimesNewRomanPS-Bold", "TimesNewRoman-Bold", 0 },
	{ "Times-Italic", "TimesNewRomanPS-ItalicMT", "TimesNewRoman,Italic",
		"TimesNewRomanPS-Italic", "TimesNewRoman-Italic", 0 },
	{ "Times-BoldItalic", "TimesNewRomanPS-BoldItalicMT",
		"TimesNewRoman,BoldItalic", "TimesNewRomanPS-BoldItalic",
		"TimesNewRoman-BoldItalic", 0 },
	{ "Symbol", 0 },
	{ "ZapfDingbats", 0 }
};

/*
 * FreeType and Rendering glue
 */

enum { UNKNOWN, TYPE1, TRUETYPE, CID };

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

int pdf_fontcidtogid(pdf_fontdesc *fontdesc, int cid)
{
    if (fontdesc->font->ftface)
	return ftcidtogid(fontdesc, cid);
    return cid;
}

static int ftwidth(pdf_fontdesc *fontdesc, int cid)
{
	int gid, e;

	gid = ftcidtogid(fontdesc, cid);

	e = FT_Load_Glyph(fontdesc->font->ftface, gid,
			FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM);
	if (e)
		return 0;
	return ((FT_Face)fontdesc->font->ftface)->glyph->advance.x;
}

/*
 * Basic encoding tables
 */

static char *cleanfontname(char *fontname)
{
	int i, k;
	for (i = 0; i < 14; i++)
		for (k = 0; basefontnames[i][k]; k++)
			if (!strcmp(basefontnames[i][k], fontname))
				return basefontnames[i][0];
	return fontname;
}

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
	if (!fontdesc)
		return nil;

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
	fontdesc->tottfcmap = 0;
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
	fontdesc->dhmtx.w = 0;

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
loadsimplefont(pdf_fontdesc **fontdescp, pdf_xref *xref, fz_obj *dict, fz_obj *ref)
{
	fz_error error;
	fz_obj *descriptor = nil;
	fz_obj *encoding = nil;
	fz_obj *widths = nil;
	unsigned short *etable = nil;
	pdf_fontdesc *fontdesc;
	fz_irect bbox;
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

	/*
	 * Load font file
	 */

	fontdesc = pdf_newfontdesc();
	if (!fontdesc)
		return fz_rethrow(-1, "out of memory");

	pdf_logfont("load simple font (%d %d R) ptr=%p {\n", fz_tonum(ref), fz_togen(ref), fontdesc);
	pdf_logfont("basefont0 %s\n", basefont);
	pdf_logfont("basefont1 %s\n", fontname);

	descriptor = fz_dictgets(dict, "FontDescriptor");
	if (descriptor && basefont == fontname)
		error = pdf_loadfontdescriptor(fontdesc, xref, descriptor, nil);
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

	/*
	 * Encoding
	 */

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
		{
			error = fz_throw("freetype could not set cmap: %s", ft_errorstring(fterr));
			goto cleanup;
		}
	}
	else
		fz_warn("freetype could not find any cmaps");

	etable = fz_malloc(sizeof(unsigned short) * 256);
	if (!etable)
		goto cleanup;

	for (i = 0; i < 256; i++)
	{
		estrings[i] = nil;
		etable[i] = 0;
	}

	encoding = fz_dictgets(dict, "Encoding");
	if (encoding && !(kind == TRUETYPE && symbolic))
	{
		if (fz_isname(encoding))
			pdf_loadencoding(estrings, fz_toname(encoding));

		if (fz_isdict(encoding))
		{
			fz_obj *base, *diff, *item;

			base = fz_dictgets(encoding, "BaseEncoding");
			if (fz_isname(base))
				pdf_loadencoding(estrings, fz_toname(base));
			else if (!fontdesc->isembedded)
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

		if (kind == TYPE1)
		{
			pdf_logfont("encode type1/cff by strings\n");
			for (i = 0; i < 256; i++)
				if (estrings[i])
					etable[i] = FT_Get_Name_Index(face, estrings[i]);
				else
					etable[i] = ftcharindex(face, i);
		}

		if (kind == TRUETYPE)
		{
			/* Unicode cmap */
			if (face->charmap && face->charmap->platform_id == 3)
			{
				pdf_logfont("encode truetype via unicode\n");
				for (i = 0; i < 256; i++)
					if (estrings[i])
					{
						int aglbuf[256];
						int aglnum;
						aglnum = pdf_lookupagl(estrings[i], aglbuf, nelem(aglbuf));
						if (aglnum != 1)
							etable[i] = FT_Get_Name_Index(face, estrings[i]);
						else
							etable[i] = ftcharindex(face, aglbuf[0]);
					}
					else
						etable[i] = ftcharindex(face, i);
			}

			/* MacRoman cmap */
			else if (face->charmap && face->charmap->platform_id == 1)
			{
				pdf_logfont("encode truetype via macroman\n");
				for (i = 0; i < 256; i++)
					if (estrings[i])
					{
						k = mrecode(estrings[i]);
						if (k <= 0)
							etable[i] = FT_Get_Name_Index(face, estrings[i]);
						else
							etable[i] = ftcharindex(face, k);
					}
					else
						etable[i] = ftcharindex(face, i);
			}

			/* Symbolic cmap */
			else
			{
				pdf_logfont("encode truetype symbolic\n");
				for (i = 0; i < 256; i++)
				{
					etable[i] = ftcharindex(face, i);
					FT_Get_Glyph_Name(face, etable[i], ebuffer[i], 32);
					if (ebuffer[i][0])
						estrings[i] = ebuffer[i];
				}
			}
		}
	}

	else
	{
		pdf_logfont("encode builtin\n");
		for (i = 0; i < 256; i++)
		{
			etable[i] = ftcharindex(face, i);
			FT_Get_Glyph_Name(face, etable[i], ebuffer[i], 32);
			if (ebuffer[i][0])
				estrings[i] = ebuffer[i];
		}
	}

	error = pdf_newidentitycmap(&fontdesc->encoding, 0, 1);
	if (error)
		goto cleanup;

	fontdesc->ncidtogid = 256;
	fontdesc->cidtogid = etable;

	error = pdf_loadtounicode(fontdesc, xref, estrings, nil, fz_dictgets(dict, "ToUnicode"));
	if (error)
		goto cleanup;

	/*
	 * Widths
	 */

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
			error = pdf_addhmtx(fontdesc, i + first, i + first, wid);
			if (error)
				goto cleanup;
		}
	}
	else
	{
		FT_Set_Char_Size(face, 1000, 1000, 72, 72);
		for (i = 0; i < 256; i++)
		{
			error = pdf_addhmtx(fontdesc, i, i, ftwidth(fontdesc, i));
			if (error)
				goto cleanup;
		}
	}

	error = pdf_endhmtx(fontdesc);
	if (error)
		goto cleanup;

	pdf_logfont("}\n");

	*fontdescp = fontdesc;
	return fz_okay;

cleanup:
	fz_free(etable);
	fz_dropfont(fontdesc->font);
	fz_free(fontdesc);
	return fz_rethrow(error, "cannot load simple font");
}

/*
 * CID Fonts
 */

static fz_error
loadcidfont(pdf_fontdesc **fontdescp, pdf_xref *xref, fz_obj *dict, fz_obj *ref, fz_obj *encoding, fz_obj *tounicode)
{
	fz_error error;
	fz_obj *widths = nil;
	fz_obj *descriptor;
	pdf_fontdesc *fontdesc;
	FT_Face face;
	fz_irect bbox;
	int kind;
	char collection[256];
	char *basefont;
	int i, k;

	/*
	 * Get font name and CID collection
	 */

	basefont = fz_toname(fz_dictgets(dict, "BaseFont"));

	{
		fz_obj *cidinfo;
		fz_obj *obj;
		char tmpstr[64];
		int tmplen;

		cidinfo = fz_dictgets(dict, "CIDSystemInfo");
		if (!cidinfo)
			return fz_throw("cid font is missing info");

		obj = fz_dictgets(cidinfo, "Registry");
		tmplen = MIN(sizeof tmpstr - 1, fz_tostrlen(obj));
		memcpy(tmpstr, fz_tostrbuf(obj), tmplen);
		tmpstr[tmplen] = '\0';
		strlcpy(collection, tmpstr, sizeof collection);

		strlcat(collection, "-", sizeof collection);

		obj = fz_dictgets(cidinfo, "Ordering");
		tmplen = MIN(sizeof tmpstr - 1, fz_tostrlen(obj));
		memcpy(tmpstr, fz_tostrbuf(obj), tmplen);
		tmpstr[tmplen] = '\0';
		strlcat(collection, tmpstr, sizeof collection);
	}

	/*
	 * Load font file
	 */

	fontdesc = pdf_newfontdesc();
	if (!fontdesc)
		return fz_rethrow(-1, "out of memory");

	pdf_logfont("load cid font (%d %d R) ptr=%p {\n", fz_tonum(ref), fz_togen(ref), fontdesc);
	pdf_logfont("basefont %s\n", basefont);
	pdf_logfont("collection %s\n", collection);

	descriptor = fz_dictgets(dict, "FontDescriptor");
	if (descriptor)
		error = pdf_loadfontdescriptor(fontdesc, xref, descriptor, collection);
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

	/*
	 * Encoding
	 */

	if (fz_isname(encoding))
	{
		pdf_logfont("encoding /%s\n", fz_toname(encoding));
		if (!strcmp(fz_toname(encoding), "Identity-H"))
			error = pdf_newidentitycmap(&fontdesc->encoding, 0, 2);
		else if (!strcmp(fz_toname(encoding), "Identity-V"))
			error = pdf_newidentitycmap(&fontdesc->encoding, 1, 2);
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
			unsigned short *map;
			fz_buffer *buf;
			int len;

			pdf_logfont("cidtogidmap stream\n");

			error = pdf_loadstream(&buf, xref, fz_tonum(cidtogidmap), fz_togen(cidtogidmap));
			if (error)
				goto cleanup;

			len = (buf->wp - buf->rp) / 2;

			map = fz_malloc(len * sizeof(unsigned short));
			if (!map) {
				fz_dropbuffer(buf);
				error = fz_rethrow(-1, "out of memory: cidtogidmap");
				goto cleanup;
			}

			for (i = 0; i < len; i++)
				map[i] = (buf->rp[i * 2] << 8) + buf->rp[i * 2 + 1];

			fontdesc->ncidtogid = len;
			fontdesc->cidtogid = map;

			fz_dropbuffer(buf);
		}

		/* if truetype font is external, cidtogidmap should not be identity */
		/* so we map from cid to unicode and then map that through the (3 1) */
		/* unicode cmap to get a glyph id */
		else if (fontdesc->font->ftsubstitute)
		{
			pdf_logfont("emulate ttf cidfont\n");

			error = FT_Select_Charmap(face, ft_encoding_unicode);
			if (error)
			{
				error = fz_throw("fonterror: no unicode cmap when emulating CID font");
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

	/*
	 * Horizontal
	 */

	pdf_setdefaulthmtx(fontdesc, fz_toint(fz_dictgets(dict, "DW")));

	widths = fz_dictgets(dict, "W");
	if (widths)
	{
		int c0, c1, w;
		fz_obj *obj;

		for (i = 0; i < fz_arraylen(widths); )
		{
			c0 = fz_toint(fz_arrayget(widths, i));
			obj = fz_arrayget(widths, i + 1);
			if (fz_isarray(obj))
			{
				for (k = 0; k < fz_arraylen(obj); k++)
				{
					w = fz_toint(fz_arrayget(obj, k));
					error = pdf_addhmtx(fontdesc, c0 + k, c0 + k, w);
					if (error)
						goto cleanup;
				}
				i += 2;
			}
			else
			{
				c1 = fz_toint(obj);
				w = fz_toint(fz_arrayget(widths, i + 2));
				error = pdf_addhmtx(fontdesc, c0, c1, w);
				if (error)
					goto cleanup;
				i += 3;
			}
		}
	}

	error = pdf_endhmtx(fontdesc);
	if (error)
		goto cleanup;

	/*
	 * Vertical
	 */

	if (pdf_getwmode(fontdesc->encoding) == 1)
	{
		fz_obj *obj;
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
			int c0, c1, w, x, y, k;

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
						error = pdf_addvmtx(fontdesc, c0 + k, c0 + k, x, y, w);
						if (error)
							goto cleanup;
					}
					i += 2;
				}
				else
				{
					c1 = fz_toint(obj);
					w = fz_toint(fz_arrayget(widths, i + 2));
					x = fz_toint(fz_arrayget(widths, i + 3));
					y = fz_toint(fz_arrayget(widths, i + 4));
					error = pdf_addvmtx(fontdesc, c0, c1, x, y, w);
					if (error)
						goto cleanup;
					i += 5;
				}
			}
		}

		error = pdf_endvmtx(fontdesc);
		if (error)
			goto cleanup;
	}

	pdf_logfont("}\n");

	*fontdescp = fontdesc;
	return fz_okay;

cleanup:
	if (widths)
		fz_dropobj(widths);
	fz_dropfont(fontdesc->font);
	fz_free(fontdesc);
	return fz_rethrow(error, "cannot load cid font");
}

static fz_error
loadtype0(pdf_fontdesc **fontdescp, pdf_xref *xref, fz_obj *dict, fz_obj *ref)
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
		error = loadcidfont(fontdescp, xref, dfont, ref, encoding, tounicode);
	else if (fz_isname(subtype) && !strcmp(fz_toname(subtype), "CIDFontType2"))
		error = loadcidfont(fontdescp, xref, dfont, ref, encoding, tounicode);
	else
		error = fz_throw("syntaxerror: unknown cid font type");
	if (error)
		return fz_rethrow(error, "cannot load descendant font");

	return fz_okay;
}

/*
 * FontDescriptor
 */

fz_error
pdf_loadfontdescriptor(pdf_fontdesc *fontdesc, pdf_xref *xref, fz_obj *dict, char *collection)
{
	fz_error error;
	fz_obj *obj1, *obj2, *obj3, *obj;
	fz_rect bbox;
	char *fontname;

	pdf_logfont("load fontdescriptor {\n");

	fontname = fz_toname(fz_dictgets(dict, "FontName"));

	pdf_logfont("fontname '%s'\n", fontname);

	fontdesc->flags = fz_toint(fz_dictgets(dict, "Flags"));
	fontdesc->italicangle = fz_toreal(fz_dictgets(dict, "ItalicAngle"));
	fontdesc->ascent = fz_toreal(fz_dictgets(dict, "Ascent"));
	fontdesc->descent = fz_toreal(fz_dictgets(dict, "Descent"));
	fontdesc->capheight = fz_toreal(fz_dictgets(dict, "CapHeight"));
	fontdesc->xheight = fz_toreal(fz_dictgets(dict, "XHeight"));
	fontdesc->missingwidth = fz_toreal(fz_dictgets(dict, "MissingWidth"));

	bbox = pdf_torect(fz_dictgets(dict, "FontBBox"));
	pdf_logfont("bbox [%g %g %g %g]\n",
			bbox.x0, bbox.y0,
			bbox.x1, bbox.y1);

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
                        error = pdf_loadsystemfont(fontdesc, fontname, collection);
                        if (error)
				return fz_rethrow(error, "cannot load font descriptor");
                }
	}
	else
	{
		error = pdf_loadsystemfont(fontdesc, fontname, collection);
		if (error)
			return fz_rethrow(error, "cannot load font descriptor");
	}

	pdf_logfont("}\n");

	return fz_okay;

}

fz_error
pdf_loadfont(pdf_fontdesc **fontdescp, pdf_xref *xref, fz_obj *dict, fz_obj *ref)
{
	fz_error error;
	char *subtype;
	fz_obj *dfonts;
	fz_obj *charprocs;

	if ((*fontdescp = pdf_finditem(xref->store, PDF_KFONT, ref)))
	{
		pdf_keepfont(*fontdescp);
		return fz_okay;
	}

	subtype = fz_toname(fz_dictgets(dict, "Subtype"));
	dfonts = fz_dictgets(dict, "DescendantFonts");
	charprocs = fz_dictgets(dict, "CharProcs");

	if (subtype && !strcmp(subtype, "Type0"))
		error = loadtype0(fontdescp, xref, dict, ref);
	else if (subtype && !strcmp(subtype, "Type1"))
		error = loadsimplefont(fontdescp, xref, dict, ref);
	else if (subtype && !strcmp(subtype, "MMType1"))
		error = loadsimplefont(fontdescp, xref, dict, ref);
	else if (subtype && !strcmp(subtype, "TrueType"))
		error = loadsimplefont(fontdescp, xref, dict, ref);
	else if (subtype && !strcmp(subtype, "Type3"))
		error = pdf_loadtype3font(fontdescp, xref, dict, ref);
	else if (charprocs)
	{
		fz_warn("unknown font format, guessing type3.");
		error = pdf_loadtype3font(fontdescp, xref, dict, ref);
	}
	else if (dfonts)
	{
		fz_warn("unknown font format, guessing type0.");
		error = loadtype0(fontdescp, xref, dict, ref);
	}
	else
	{
		fz_warn("unknown font format, guessing type1 or truetype.");
		error = loadsimplefont(fontdescp, xref, dict, ref);
	}
	if (error)
	    return fz_rethrow(error, "cannot load font");

	error = pdf_storeitem(xref->store, PDF_KFONT, ref, *fontdescp);
	if (error)
	    return fz_rethrow(error, "cannot store font resource");

	return fz_okay;
}

void
pdf_debugfont(pdf_fontdesc *fontdesc)
{
    int i;

    printf("fontdesc {\n");

    if (fontdesc->font->ftface)
	printf("  freetype font\n");
    if (fontdesc->font->t3procs)
	printf("  type3 font\n");

    printf("  wmode %d\n", fontdesc->wmode);
    printf("  DW %d\n", fontdesc->dhmtx.w);

    printf("  W {\n");
    for (i = 0; i < fontdesc->nhmtx; i++)
	printf("    <%04x> <%04x> %d\n",
		fontdesc->hmtx[i].lo, fontdesc->hmtx[i].hi, fontdesc->hmtx[i].w);
    printf("  }\n");

    if (fontdesc->wmode)
    {
	printf("  DW2 [%d %d]\n", fontdesc->dvmtx.y, fontdesc->dvmtx.w);
	printf("  W2 {\n");
	for (i = 0; i < fontdesc->nvmtx; i++)
	    printf("    <%04x> <%04x> %d %d %d\n", fontdesc->vmtx[i].lo, fontdesc->vmtx[i].hi,
		    fontdesc->vmtx[i].x, fontdesc->vmtx[i].y, fontdesc->vmtx[i].w);
	printf("  }\n");
    }
}

