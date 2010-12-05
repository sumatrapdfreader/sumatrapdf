#include "fitz.h"

static void
fz_tracematrix(fz_matrix ctm)
{
	printf("matrix=\"%g %g %g %g %g %g\" ",
		ctm.a, ctm.b, ctm.c, ctm.d, ctm.e, ctm.f);
}

static void
fz_tracecolor(fz_colorspace *colorspace, float *color, float alpha)
{
	int i;
	printf("colorspace=\"%s\" color=\"", colorspace->name);
	for (i = 0; i < colorspace->n; i++)
		printf("%s%g", i == 0 ? "" : " ", color[i]);
	printf("\" ");
	if (alpha < 1)
		printf("alpha=\"%g\" ", alpha);
}

static void
fz_tracepath(fz_path *path, int indent)
{
	float x, y;
	int i = 0;
	int n;
	while (i < path->len)
	{
		for (n = 0; n < indent; n++)
			putchar(' ');
		switch (path->els[i++].k)
		{
		case FZ_MOVETO:
			x = path->els[i++].v;
			y = path->els[i++].v;
			printf("<moveto x=\"%g\" y=\"%g\" />\n", x, y);
			break;
		case FZ_LINETO:
			x = path->els[i++].v;
			y = path->els[i++].v;
			printf("<lineto x=\"%g\" y=\"%g\" />\n", x, y);
			break;
		case FZ_CURVETO:
			x = path->els[i++].v;
			y = path->els[i++].v;
			printf("<curveto x1=\"%g\" y1=\"%g\" ", x, y);
			x = path->els[i++].v;
			y = path->els[i++].v;
			printf("x2=\"%g\" y2=\"%g\" ", x, y);
			x = path->els[i++].v;
			y = path->els[i++].v;
			printf("x3=\"%g\" y3=\"%g\" />\n", x, y);
			break;
		case FZ_CLOSEPATH:
			printf("<closepath />\n");
		}
	}
}

static void
fz_tracefillpath(void *user, fz_path *path, int evenodd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	printf("<fillpath ");
	if (evenodd)
		printf("winding=\"eofill\" ");
	else
		printf("winding=\"nonzero\" ");
	fz_tracecolor(colorspace, color, alpha);
	fz_tracematrix(ctm);
	printf(">\n");
	fz_tracepath(path, 0);
	printf("</fillpath>\n");
}

static void
fz_tracestrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	int i;

	printf("<strokepath ");
	printf("linewidth=\"%g\" ", stroke->linewidth);
	printf("miterlimit=\"%g\" ", stroke->miterlimit);
	printf("linecap=\"%d\" ", stroke->linecap);
	printf("linejoin=\"%d\" ", stroke->linejoin);

	if (stroke->dashlen)
	{
		printf("dashphase=\"%g\" dash=\"", stroke->dashphase);
		for (i = 0; i < stroke->dashlen; i++)
			printf("%g ", stroke->dashlist[i]);
		printf("\"");
	}

	fz_tracecolor(colorspace, color, alpha);
	fz_tracematrix(ctm);
	printf(">\n");

	fz_tracepath(path, 0);

	printf("</strokepath>\n");
}

static void
fz_traceclippath(void *user, fz_path *path, int evenodd, fz_matrix ctm)
{
	printf("<clippath ");
	if (evenodd)
		printf("winding=\"eofill\" ");
	else
		printf("winding=\"nonzero\" ");
	fz_tracematrix(ctm);
	printf(">\n");
	fz_tracepath(path, 0);
	printf("</clippath>\n");
}

static void
fz_traceclipstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm)
{
	printf("<clipstrokepath ");
	fz_tracematrix(ctm);
	printf(">\n");
	fz_tracepath(path, 0);
	printf("</clipstrokepath>\n");
}

static void
fz_tracefilltext(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	printf("<filltext font=\"%s\" wmode=\"%d\" ", text->font->name, text->wmode);
	fz_tracecolor(colorspace, color, alpha);
	fz_tracematrix(fz_concat(ctm, text->trm));
	printf(">\n");
	fz_debugtext(text, 0);
	printf("</filltext>\n");
}

static void
fz_tracestroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	printf("<stroketext font=\"%s\" wmode=\"%d\" ", text->font->name, text->wmode);
	fz_tracecolor(colorspace, color, alpha);
	fz_tracematrix(fz_concat(ctm, text->trm));
	printf(">\n");
	fz_debugtext(text, 0);
	printf("</stroketext>\n");
}

static void
fz_tracecliptext(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	printf("<cliptext font=\"%s\" wmode=\"%d\" ", text->font->name, text->wmode);
	printf("accumulate=\"%d\" ", accumulate);
	fz_tracematrix(fz_concat(ctm, text->trm));
	printf(">\n");
	fz_debugtext(text, 0);
	printf("</cliptext>\n");
}

static void
fz_traceclipstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm)
{
	printf("<clipstroketext font=\"%s\" wmode=\"%d\" ", text->font->name, text->wmode);
	fz_tracematrix(fz_concat(ctm, text->trm));
	printf(">\n");
	fz_debugtext(text, 0);
	printf("</clipstroketext>\n");
}

static void
fz_traceignoretext(void *user, fz_text *text, fz_matrix ctm)
{
	printf("<ignoretext font=\"%s\" wmode=\"%d\" ", text->font->name, text->wmode);
	fz_tracematrix(fz_concat(ctm, text->trm));
	printf(">\n");
	fz_debugtext(text, 0);
	printf("</ignoretext>\n");
}

static void
fz_tracefillimage(void *user, fz_pixmap *image, fz_matrix ctm, float alpha)
{
	printf("<fillimage alpha=\"%g\" ", alpha);
	fz_tracematrix(ctm);
	printf("/>\n");
}

static void
fz_tracefillshade(void *user, fz_shade *shade, fz_matrix ctm, float alpha)
{
	printf("<fillshade alpha=\"%g\" ", alpha);
	fz_tracematrix(ctm);
	printf("/>\n");
}

static void
fz_tracefillimagemask(void *user, fz_pixmap *image, fz_matrix ctm,
fz_colorspace *colorspace, float *color, float alpha)
{
	printf("<fillimagemask ");
	fz_tracematrix(ctm);
	fz_tracecolor(colorspace, color, alpha);
	printf("/>\n");
}

static void
fz_traceclipimagemask(void *user, fz_pixmap *image, fz_matrix ctm)
{
	printf("<clipimagemask ");
	fz_tracematrix(ctm);
	printf("/>\n");
}

static void
fz_tracepopclip(void *user)
{
	printf("<popclip />\n");
}

static void
fz_tracebeginmask(void *user, fz_rect bbox, int luminosity, fz_colorspace *colorspace, float *color)
{
	printf("<mask bbox=\"%g %g %g %g\" s=\"%s\" ",
		bbox.x0, bbox.y0, bbox.x1, bbox.y1,
		luminosity ? "luminosity" : "alpha");
//	fz_tracecolor(colorspace, color, 1);
	printf(">\n");
}

static void
fz_traceendmask(void *user)
{
	printf("</mask>\n");
}

static void
fz_tracebegingroup(void *user, fz_rect bbox, int isolated, int knockout, fz_blendmode blendmode, float alpha)
{
	printf("<group bbox=\"%g %g %g %g\" isolated=\"%d\" knockout=\"%d\" blendmode=\"%s\" alpha=\"%g\">\n",
		bbox.x0, bbox.y0, bbox.x1, bbox.y1,
		isolated, knockout, fz_blendnames[blendmode], alpha);
}

static void
fz_traceendgroup(void *user)
{
	printf("</group>\n");
}

fz_device *fz_newtracedevice(void)
{
	fz_device *dev = fz_newdevice(nil);

	dev->fillpath = fz_tracefillpath;
	dev->strokepath = fz_tracestrokepath;
	dev->clippath = fz_traceclippath;
	dev->clipstrokepath = fz_traceclipstrokepath;

	dev->filltext = fz_tracefilltext;
	dev->stroketext = fz_tracestroketext;
	dev->cliptext = fz_tracecliptext;
	dev->clipstroketext = fz_traceclipstroketext;
	dev->ignoretext = fz_traceignoretext;

	dev->fillshade = fz_tracefillshade;
	dev->fillimage = fz_tracefillimage;
	dev->fillimagemask = fz_tracefillimagemask;
	dev->clipimagemask = fz_traceclipimagemask;

	dev->popclip = fz_tracepopclip;

	dev->beginmask = fz_tracebeginmask;
	dev->endmask = fz_traceendmask;
	dev->begingroup = fz_tracebegingroup;
	dev->endgroup = fz_traceendgroup;

	return dev;
}
