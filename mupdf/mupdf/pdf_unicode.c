#include "fitz.h"
#include "mupdf.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#if ((FREETYPE_MAJOR == 2) && (FREETYPE_MINOR == 1)) || \
	((FREETYPE_MAJOR == 2) && (FREETYPE_MINOR == 2)) || \
	((FREETYPE_MAJOR == 2) && (FREETYPE_MINOR == 3) && (FREETYPE_PATCH < 8))

int FT_Get_Advance(FT_Face face, int gid, int masks, FT_Fixed *out)
{
	int fterr;
	fterr = FT_Load_Glyph(face, gid, masks | FT_LOAD_IGNORE_TRANSFORM);
	if (fterr)
		return fterr;
	*out = face->glyph->advance.x * 1024;
	return 0;
}

#else

#include FT_ADVANCES_H

#endif

/*
 * ToUnicode map for fonts
 */

fz_error
pdf_loadtounicode(pdf_fontdesc *font, pdf_xref *xref,
	char **strings, char *collection, fz_obj *cmapstm)
{
	fz_error error = fz_okay;
	pdf_cmap *cmap;
	int cid;
	int ucs;
	int i;

	if (pdf_isstream(xref, fz_tonum(cmapstm), fz_togen(cmapstm)))
	{
		pdf_logfont("tounicode embedded cmap\n");

		error = pdf_loadembeddedcmap(&cmap, xref, cmapstm);
		if (error)
			return fz_rethrow(error, "cannot load embedded cmap");

		font->tounicode = pdf_newcmap();

		for (i = 0; i < (strings ? 256 : 65536); i++)
		{
			cid = pdf_lookupcmap(font->encoding, i);
			if (cid >= 0) /* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=687 */
			{
				ucs = pdf_lookupcmap(cmap, i);
				if (ucs > 0)
					pdf_maprangetorange(font->tounicode, cid, cid, ucs);
				else if (ucs < -1)
				{
					/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=788 */
					/* copy over a multi-character mapping */
					int j, tbl[7], len = min(cmap->table[-ucs - 2], 7);
					for (j = 0; j < len; j++)
						tbl[j] = cmap->table[-ucs - 1 + j];
					pdf_maponetomany(font->tounicode, cid, tbl, len);
				}
			}
		}

		pdf_sortcmap(font->tounicode);

		pdf_dropcmap(cmap);
		// return fz_okay; // cf. http://code.google.com/p/sumatrapdf/issues/detail?id=787
	}

	else if (collection)
	{
		pdf_logfont("tounicode cid collection (%s)\n", collection);

		error = fz_okay;

		if (!strcmp(collection, "Adobe-CNS1"))
			error = pdf_loadsystemcmap(&font->tounicode, "Adobe-CNS1-UCS2");
		else if (!strcmp(collection, "Adobe-GB1"))
			error = pdf_loadsystemcmap(&font->tounicode, "Adobe-GB1-UCS2");
		else if (!strcmp(collection, "Adobe-Japan1"))
			error = pdf_loadsystemcmap(&font->tounicode, "Adobe-Japan1-UCS2");
		else if (!strcmp(collection, "Adobe-Japan2"))
			error = pdf_loadsystemcmap(&font->tounicode, "Adobe-Japan2-UCS2"); /* where's this? */
			else if (!strcmp(collection, "Adobe-Korea1"))
				error = pdf_loadsystemcmap(&font->tounicode, "Adobe-Korea1-UCS2");

		if (error)
			return fz_rethrow(error, "cannot load tounicode system cmap %s-UCS2", collection);
	}

	if (strings)
	{
		pdf_logfont("tounicode strings\n");

		/* TODO one-to-many mappings */

		font->ncidtoucs = 256;
		font->cidtoucs = fz_malloc(256 * sizeof(unsigned short));

		for (i = 0; i < 256; i++)
		{
			if (strings[i])
				font->cidtoucs[i] = pdf_lookupagl(strings[i]);
			else
				font->cidtoucs[i] = '?';
		}

		return fz_okay;
	}

	if (!font->tounicode && !font->cidtoucs)
	{
		pdf_logfont("tounicode could not be loaded\n");
		/* TODO: synthesize a ToUnicode if it's a freetype font with
		* cmap and/or post tables or if it has glyph names. */
	}

	return fz_okay;
}

/*
 * Extract lines of text from display tree.
 *
 * This extraction needs to be rewritten for the new tree
 * architecture where glyph index and unicode characters are both stored
 * in the text objects.
 */

pdf_textline *
pdf_newtextline(void)
{
	pdf_textline *line;
	line = fz_malloc(sizeof(pdf_textline));
	line->len = 0;
	line->cap = 0;
	line->text = nil;
	line->next = nil;
	return line;
}

void
pdf_droptextline(pdf_textline *line)
{
	if (line->next)
		pdf_droptextline(line->next);
	fz_free(line->text);
	fz_free(line);
}

static void
addtextchar(pdf_textline *line, fz_irect bbox, int c)
{
	if (line->len + 1 >= line->cap)
	{
		line->cap = line->cap ? (line->cap * 3) / 2 : 80;
		line->text = fz_realloc(line->text, sizeof(pdf_textchar) * line->cap);
	}

	line->text[line->len].bbox = bbox;
	line->text[line->len].c = c;
	line->len ++;
}

static fz_error
extracttext(pdf_textline **line, fz_node *node, fz_matrix ctm, fz_point *oldpt)
{
	fz_error error;

	if (fz_istextnode(node))
	{
		fz_textnode *text = (fz_textnode*)node;
		fz_font *font = text->font;
		fz_matrix tm = text->trm;
		fz_matrix inv = fz_invertmatrix(text->trm);
		fz_matrix trm;
		float dx, dy;
		fz_point p;
		float adv;
		int i, x, y, fterr;

		// TODO: this is supposed to calculate font bbox, but doesn't seem
		// to be right at all
		fz_irect bbox;
		int fontdx, fontdy;
		fz_point fontp1, fontp2;
		fz_matrix fontmtx;
		fz_irect fontbbox;

		fontmtx = inv;
		fontbbox = font->bbox;
		fontdx = fontbbox.x1 - fontbbox.x0;
		fontdy = fontbbox.y1 - fontbbox.y0;
		fontp1.x = fontbbox.x0;
		fontp1.y = fontbbox.y0;
		fontp1 = fz_transformpoint(fontmtx, fontp1);
		fontp2.x = fontbbox.x1;
		fontp2.y = fontbbox.y1;
		fontp2 = fz_transformpoint(fontmtx, fontp2);
		fontdx = fontp2.x - fontp1.x;
		fontdy = fontp2.y - fontp1.y;

		// TODO: magically divide by 10 because that's what it looks like
		// we should be doing. Doesn't work well for all fonts
		fontdx = fontdx / 10;
		fontdy = fontdy / 10;

		if (font->ftface)
		{
			FT_Set_Transform(font->ftface, NULL, NULL);
			fterr = FT_Set_Char_Size(font->ftface, 64, 64, 72, 72);
			if (fterr)
				return fz_throw("freetype set character size: %s", ft_errorstring(fterr));
		}

		for (i = 0; i < text->len; i++)
		{
			tm.e = text->els[i].x;
			tm.f = text->els[i].y;
			trm = fz_concat(tm, ctm);
			x = trm.e;
			y = trm.f;
			trm.e = 0;
			trm.f = 0;

			p.x = text->els[i].x;
			p.y = text->els[i].y;
			p = fz_transformpoint(inv, p);
			dx = oldpt->x - p.x;
			dy = oldpt->y - p.y;
			*oldpt = p;

			/* TODO: flip advance and test for vertical writing */

			if (font->ftface)
			{
				FT_Fixed ftadv;
				fterr = FT_Get_Advance(font->ftface, text->els[i].gid,
					FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING,
					&ftadv);
				if (fterr)
					return fz_throw("freetype get advance (gid %d): %s", text->els[i].gid, ft_errorstring(fterr));
				adv = ftadv / 65536.0;
				oldpt->x += adv;
			}
			else
			{
				adv = font->t3widths[text->els[i].gid];
				oldpt->x += adv;
			}

			bbox.x0 = x;
			bbox.x1 = x + fontdx;
			bbox.y0 = y;
			bbox.y1 = y + fontdy;

			if (fabs(dy) > 0.27) /* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=687 */
			{
				pdf_textline *newline = pdf_newtextline();
				(*line)->next = newline;
				*line = newline;
			}
			else if (fabs(dx) > 0.15 && (*line)->len > 0 && (*line)->text[(*line)->len - 1].c != ' ') /* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=687 */
			{
				addtextchar(*line, bbox, ' ');
			}

			/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=788 */
			/* add one or several characters */
			for (x = 1; x <= text->els[i].ucs[0]; x++)
				addtextchar(*line, bbox, text->els[i].ucs[x]);
		}
	}

	if (fz_istransformnode(node))
		ctm = fz_concat(((fz_transformnode*)node)->m, ctm);

	for (node = node->first; node; node = node->next)
	{
		error = extracttext(line, node, ctm, oldpt);
		if (error)
			return fz_rethrow(error, "cannot extract text from display node");
	}

	return fz_okay;
}

/***** various string fixups *****/
static void
insertcharacter(pdf_textline *line, int i, int c)
{
	if (line->len + 1 >= line->cap)
	{
		line->cap = line->cap ? (line->cap * 3) / 2 : 80;
		line->text = fz_realloc(line->text, sizeof(pdf_textchar) * line->cap);
	}

	memmove(&line->text[i + 1], &line->text[i], (line->len - i) * sizeof(pdf_textchar));
	line->text[i].c = c;
	line->text[i].bbox = line->text[i + 1].bbox;
	line->len++;
}

static void
deletecharacter(pdf_textline *line, int i)
{
	memmove(&line->text[i], &line->text[i + 1], (line->len - (i + 1)) * sizeof(pdf_textchar));
	line->len--;
}

static void
reversecharacters(pdf_textline *line, int i, int j)
{
	while (i < j)
	{
		pdf_textchar tc = line->text[i];
		line->text[i] = line->text[j];
		line->text[j] = tc;
		i++; j--;
	}
}

static int
ornatecharacter(int ornate, int character)
{
	static wchar_t *ornates[] = {
#ifdef WIN32
		/* TODO: those must be encoded with hex encoding, because gcc doesn't understand this file encoding */
		L" ®¥`^",
		L"a‰·‡‚", L"Aƒ¡¿¬",
		L"eÎÈËÍ", L"EÀ…» ",
		L"iÔÌÏÓ", L"IœÕÃŒ",
		L"oˆÛÚÙ", L"O÷”“‘",
		L"u¸˙˘˚", L"U‹⁄Ÿ€",
#endif
		nil
	};
	int i, j;

	for (i = 1; ornates[0][i] && ornates[0][i] != (wchar_t)ornate; i++);
	for (j = 1; ornates[j] && ornates[j][0] != (wchar_t)character; j++);
	return ornates[0][i] && ornates[j] ? ornates[j][i] : 0;
}

/* TODO: Complete these lists... */
#define ISLEFTTORIGHTCHAR(c) ((0x0041 <= (c) && (c) <= 0x005A) || (0x0061 <= (c) && (c) <= 0x007A) || (0xFB00 <= (c) && (c) <= 0xFB06))
#define ISRIGHTTOLEFTCHAR(c) ((0x0590 <= (c) && (c) <= 0x05FF) || (0x0600 <= (c) && (c) <= 0x06FF) || (0x0750 <= (c) && (c) <= 0x077F) || (0xFB50 <= (c) && (c) <= 0xFDFF) || (0xFE70 <= (c) && (c) <= 0xFEFF))

static void
fixuptextlines(pdf_textline *root)
{
	pdf_textline *line;
	for (line = root; line; line = line->next)
	{
		int i;
		for (i = 0; i < line->len; i++)
		{
			switch (line->text[i].c)
			{
			/* recombine characters and their accents */
			case 0x00A8: /* ® */
			case 0x00B4: /* ¥ */
			case 0x0060: /* ` */
			case 0x005E: /* ^ */
				if (i + 2 < line->len && line->text[i + 1].c == 32)
				{
					int newC = ornatecharacter(line->text[i].c, line->text[i + 2].c);
					if (newC)
					{
						deletecharacter(line, i);
						deletecharacter(line, i);
						line->text[i].c = newC;
					}
				}
				break;
			/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=400 */
			/* copy ligatures as individual characters */
			case 0xFB00: /* ff */
				insertcharacter(line, i++, 'f'); line->text[i].c = 'f'; break;
			case 0xFB03: /* ffi */
				insertcharacter(line, i++, 'f');
			case 0xFB01: /* fi */
				insertcharacter(line, i++, 'f'); line->text[i].c = 'i'; break;
			case 0xFB04: /* ffl */
				insertcharacter(line, i++, 'f');
			case 0xFB02: /* fl */
				insertcharacter(line, i++, 'f'); line->text[i].c = 'l'; break;
			case 0xFB05: case 0xFB06: /* st */
				insertcharacter(line, i++, 's'); line->text[i].c = 't'; break;
			default:
				/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=733 */
				/* reverse words written in RTL languages */
				if (ISRIGHTTOLEFTCHAR(line->text[i].c))
				{
					int j = i + 1;
					while (j < line->len && line->text[j - 1].bbox.x0 <= line->text[j].bbox.x0 && !ISLEFTTORIGHTCHAR(line->text[i].c))
						j++;
					reversecharacters(line, i, j - 1);
					i = j;
				}
			}
		}
	}
}
/***** various string fixups *****/

fz_error
pdf_loadtextfromtree(pdf_textline **outp, fz_tree *tree, fz_matrix ctm)
{
	pdf_textline *root;
	pdf_textline *line;
	fz_error error;
	fz_point oldpt;

	oldpt.x = -1;
	oldpt.y = -1;

	root = pdf_newtextline();
	line = root;

	error = extracttext(&line, tree->root, ctm, &oldpt);
	if (error)
	{
		pdf_droptextline(root);
		return fz_rethrow(error, "cannot extract text from display tree");
	}

	fixuptextlines(root);
	*outp = root;
	return fz_okay;
}

void
pdf_debugtextline(pdf_textline *line)
{
	char buf[10];
	int c, n, k, i;

	for (i = 0; i < line->len; i++)
	{
		c = line->text[i].c;
		if (c < 128)
			putchar(c);
		else
		{
			n = runetochar(buf, &c);
			for (k = 0; k < n; k++)
				putchar(buf[k]);
		}
	}
	putchar('\n');

	if (line->next)
		pdf_debugtextline(line->next);
}

