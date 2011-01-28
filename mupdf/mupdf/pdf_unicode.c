#include "fitz.h"
#include "mupdf.h"

/* Load or synthesize ToUnicode map for fonts */

fz_error
pdf_loadtounicode(pdf_fontdesc *font, pdf_xref *xref,
	char **strings, char *collection, fz_obj *cmapstm)
{
	fz_error error = fz_okay;
	pdf_cmap *cmap;
	int cid;
	int ucsbuf[8];
	int ucslen;
	int i;

	if (pdf_isstream(xref, fz_tonum(cmapstm), fz_togen(cmapstm)))
	{
		pdf_logfont("tounicode embedded cmap\n");

		error = pdf_loadembeddedcmap(&cmap, xref, cmapstm);
		if (error)
			return fz_rethrow(error, "cannot load embedded cmap (%d %d R)", fz_tonum(cmapstm), fz_togen(cmapstm));

		font->tounicode = pdf_newcmap();

		for (i = 0; i < (strings ? 256 : 65536); i++)
		{
			cid = pdf_lookupcmap(font->encoding, i);
			if (cid >= 0)
			{
				ucslen = pdf_lookupcmapfull(cmap, i, ucsbuf);
				if (ucslen == 1)
					pdf_maprangetorange(font->tounicode, cid, cid, ucsbuf[0]);
				if (ucslen > 1)
					pdf_maponetomany(font->tounicode, cid, ucsbuf, ucslen);
			}
		}

		pdf_sortcmap(font->tounicode);

		pdf_dropcmap(cmap);
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
		font->cidtoucs = fz_calloc(256, sizeof(unsigned short));

		for (i = 0; i < 256; i++)
		{
			if (strings[i])
				font->cidtoucs[i] = pdf_lookupagl(strings[i]);
			else
				font->cidtoucs[i] = '?';
		}
	}

	if (!font->tounicode && !font->cidtoucs)
	{
		pdf_logfont("tounicode could not be loaded\n");
		/* TODO: synthesize a ToUnicode if it's a freetype font with
		* cmap and/or post tables or if it has glyph names. */
	}

	return fz_okay;
}
