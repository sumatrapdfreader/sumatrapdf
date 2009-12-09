#include "fitz.h"
#include "mupdf.h"

extern const unsigned char pdf_font_Dingbats_cff_buf[];
extern const unsigned int  pdf_font_Dingbats_cff_len;
extern const unsigned char pdf_font_NimbusMonL_Bold_cff_buf[];
extern const unsigned int  pdf_font_NimbusMonL_Bold_cff_len;
extern const unsigned char pdf_font_NimbusMonL_BoldObli_cff_buf[];
extern const unsigned int  pdf_font_NimbusMonL_BoldObli_cff_len;
extern const unsigned char pdf_font_NimbusMonL_Regu_cff_buf[];
extern const unsigned int  pdf_font_NimbusMonL_Regu_cff_len;
extern const unsigned char pdf_font_NimbusMonL_ReguObli_cff_buf[];
extern const unsigned int  pdf_font_NimbusMonL_ReguObli_cff_len;
extern const unsigned char pdf_font_NimbusRomNo9L_Medi_cff_buf[];
extern const unsigned int  pdf_font_NimbusRomNo9L_Medi_cff_len;
extern const unsigned char pdf_font_NimbusRomNo9L_MediItal_cff_buf[];
extern const unsigned int  pdf_font_NimbusRomNo9L_MediItal_cff_len;
extern const unsigned char pdf_font_NimbusRomNo9L_Regu_cff_buf[];
extern const unsigned int  pdf_font_NimbusRomNo9L_Regu_cff_len;
extern const unsigned char pdf_font_NimbusRomNo9L_ReguItal_cff_buf[];
extern const unsigned int  pdf_font_NimbusRomNo9L_ReguItal_cff_len;
extern const unsigned char pdf_font_NimbusSanL_Bold_cff_buf[];
extern const unsigned int  pdf_font_NimbusSanL_Bold_cff_len;
extern const unsigned char pdf_font_NimbusSanL_BoldItal_cff_buf[];
extern const unsigned int  pdf_font_NimbusSanL_BoldItal_cff_len;
extern const unsigned char pdf_font_NimbusSanL_Regu_cff_buf[];
extern const unsigned int  pdf_font_NimbusSanL_Regu_cff_len;
extern const unsigned char pdf_font_NimbusSanL_ReguItal_cff_buf[];
extern const unsigned int  pdf_font_NimbusSanL_ReguItal_cff_len;
extern const unsigned char pdf_font_StandardSymL_cff_buf[];
extern const unsigned int  pdf_font_StandardSymL_cff_len;
extern const unsigned char pdf_font_URWChanceryL_MediItal_cff_buf[];
extern const unsigned int  pdf_font_URWChanceryL_MediItal_cff_len;

#ifndef NOCJK
extern const unsigned char pdf_font_DroidSansFallback_ttf_buf[];
extern const unsigned int  pdf_font_DroidSansFallback_ttf_len;
#endif

enum
{
	FD_FIXED = 1 << 0,
	FD_SERIF = 1 << 1,
	FD_SYMBOLIC = 1 << 2,
	FD_SCRIPT = 1 << 3,
	FD_NONSYMBOLIC = 1 << 5,
	FD_ITALIC = 1 << 6,
	FD_ALLCAP = 1 << 16,
	FD_SMALLCAP = 1 << 17,
	FD_FORCEBOLD = 1 << 18
};

enum { CNS, GB, Japan, Korea };
enum { MINCHO, GOTHIC };

static const struct
{
	const char *name;
	const unsigned char *cff;
	const unsigned int *len;
	} basefonts[] = {
	{ "Courier",
		pdf_font_NimbusMonL_Regu_cff_buf,
		&pdf_font_NimbusMonL_Regu_cff_len },
	{ "Courier-Bold",
		pdf_font_NimbusMonL_Bold_cff_buf,
		&pdf_font_NimbusMonL_Bold_cff_len },
	{ "Courier-Oblique",
		pdf_font_NimbusMonL_ReguObli_cff_buf,
		&pdf_font_NimbusMonL_ReguObli_cff_len },
	{ "Courier-BoldOblique",
		pdf_font_NimbusMonL_BoldObli_cff_buf,
		&pdf_font_NimbusMonL_BoldObli_cff_len },
	{ "Helvetica",
		pdf_font_NimbusSanL_Regu_cff_buf,
		&pdf_font_NimbusSanL_Regu_cff_len },
	{ "Helvetica-Bold",
		pdf_font_NimbusSanL_Bold_cff_buf,
		&pdf_font_NimbusSanL_Bold_cff_len },
	{ "Helvetica-Oblique",
		pdf_font_NimbusSanL_ReguItal_cff_buf,
		&pdf_font_NimbusSanL_ReguItal_cff_len },
	{ "Helvetica-BoldOblique",
		pdf_font_NimbusSanL_BoldItal_cff_buf,
		&pdf_font_NimbusSanL_BoldItal_cff_len },
	{ "Times-Roman",
		pdf_font_NimbusRomNo9L_Regu_cff_buf,
		&pdf_font_NimbusRomNo9L_Regu_cff_len },
	{ "Times-Bold",
		pdf_font_NimbusRomNo9L_Medi_cff_buf,
		&pdf_font_NimbusRomNo9L_Medi_cff_len },
	{ "Times-Italic",
		pdf_font_NimbusRomNo9L_ReguItal_cff_buf,
		&pdf_font_NimbusRomNo9L_ReguItal_cff_len },
	{ "Times-BoldItalic",
		pdf_font_NimbusRomNo9L_MediItal_cff_buf,
		&pdf_font_NimbusRomNo9L_MediItal_cff_len },
	{ "Symbol",
		pdf_font_StandardSymL_cff_buf,
		&pdf_font_StandardSymL_cff_len },
	{ "ZapfDingbats",
		pdf_font_Dingbats_cff_buf,
		&pdf_font_Dingbats_cff_len },
	{ "Chancery",
		pdf_font_URWChanceryL_MediItal_cff_buf,
		&pdf_font_URWChanceryL_MediItal_cff_len },
	{ nil, nil, nil }
};

fz_error
pdf_loadbuiltinfont(pdf_fontdesc *font, char *fontname)
{
	fz_error error;
	unsigned char *data;
	unsigned int len;
	int i;

	for (i = 0; basefonts[i].name; i++)
		if (!strcmp(fontname, basefonts[i].name))
			goto found;

	return fz_throw("cannot find font: '%s'", fontname);

found:
	pdf_logfont("load builtin font %s\n", fontname);

	data = (unsigned char *) basefonts[i].cff;
	len = *basefonts[i].len;

	error = fz_newfontfrombuffer(&font->font, data, len, 0);
	if (error)
		return fz_rethrow(error, "cannot load freetype font from buffer");

	return fz_okay;
}

static fz_error
loadsystemcidfont(pdf_fontdesc *font, int ros, int kind)
{
#ifndef NOCJK
	fz_error error;
	/* We only have one builtin fallback font, we'd really like
	 * to have one for each combination of ROS and Kind.
	 */
	pdf_logfont("loading builtin CJK font\n");
	error = fz_newfontfrombuffer(&font->font,
		(unsigned char *)pdf_font_DroidSansFallback_ttf_buf,
		pdf_font_DroidSansFallback_ttf_len, 0);
	if (error)
		return fz_rethrow(error, "cannot load builtin CJK font");
	font->font->ftsubstitute = 1; /* substitute font */
	return fz_okay;
#else
	return fz_throw("no builtin CJK font file");
#endif
}

fz_error
pdf_loadsystemfont(pdf_fontdesc *font, char *fontname, char *collection)
{
	fz_error error;
	char *name;

	int isbold = 0;
	int isitalic = 0;
	int isserif = 0;
	int isscript = 0;
	int isfixed = 0;

	if (strstr(fontname, "Bold"))
		isbold = 1;
	if (strstr(fontname, "Italic"))
		isitalic = 1;
	if (strstr(fontname, "Oblique"))
		isitalic = 1;

	if (font->flags & FD_FIXED)
		isfixed = 1;
	if (font->flags & FD_SERIF)
		isserif = 1;
	if (font->flags & FD_ITALIC)
		isitalic = 1;
	if (font->flags & FD_SCRIPT)
		isscript = 1;
	if (font->flags & FD_FORCEBOLD)
		isbold = 1;

	pdf_logfont("fixed-%d serif-%d italic-%d script-%d bold-%d\n",
		isfixed, isserif, isitalic, isscript, isbold);

	if (collection)
	{
		int kind;

		if (isserif)
			kind = MINCHO;
		else
			kind = GOTHIC;

		if (!strcmp(collection, "Adobe-CNS1"))
			return loadsystemcidfont(font, CNS, kind);
		else if (!strcmp(collection, "Adobe-GB1"))
			return loadsystemcidfont(font, GB, kind);
		else if (!strcmp(collection, "Adobe-Japan1"))
			return loadsystemcidfont(font, Japan, kind);
		else if (!strcmp(collection, "Adobe-Japan2"))
			return loadsystemcidfont(font, Japan, kind);
		else if (!strcmp(collection, "Adobe-Korea1"))
			return loadsystemcidfont(font, Korea, kind);

		fz_warn("unknown cid collection: %s", collection);
	}

	if (isscript)
		name = "Chancery";

	else if (isfixed)
	{
		if (isitalic) {
			if (isbold) name = "Courier-BoldOblique";
			else name = "Courier-Oblique";
		}
		else {
			if (isbold) name = "Courier-Bold";
			else name = "Courier";
		}
	}

	else if (isserif)
	{
		if (isitalic) {
			if (isbold) name = "Times-BoldItalic";
			else name = "Times-Italic";
		}
		else {
			if (isbold) name = "Times-Bold";
			else name = "Times-Roman";
		}
	}

	else
	{
		if (isitalic) {
			if (isbold) name = "Helvetica-BoldOblique";
			else name = "Helvetica-Oblique";
		}
		else {
			if (isbold) name = "Helvetica-Bold";
			else name = "Helvetica";
		}
	}

	error = pdf_loadbuiltinfont(font, name);
	if (error)
		return fz_throw("cannot load builtin substitute font: %s", name);

	/* it's a substitute font: override the metrics */
	font->font->ftsubstitute = 1;

	return fz_okay;
}

fz_error
pdf_loadembeddedfont(pdf_fontdesc *font, pdf_xref *xref, fz_obj *stmref)
{
	fz_error error;
	fz_buffer *buf;

	pdf_logfont("load embedded font\n");

	error = pdf_loadstream(&buf, xref, fz_tonum(stmref), fz_togen(stmref));
	if (error)
		return fz_rethrow(error, "cannot load font stream");

	error = fz_newfontfrombuffer(&font->font, buf->rp, buf->wp - buf->rp, 0);
	if (error)
	{
		fz_dropbuffer(buf);
		return fz_rethrow(error, "cannot load embedded font (%d %d R)", fz_tonum(stmref), fz_togen(stmref));
	}

	font->buffer = buf->rp; /* save the buffer so we can free it later */
	fz_free(buf); /* only free the fz_buffer struct, not the contained data */

	font->isembedded = 1;

	return fz_okay;
}

