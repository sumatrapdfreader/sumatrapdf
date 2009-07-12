/*
 * Information tool.
 * Print information about the input pdf.
 */

#include "pdftool.h"

enum
{
	DIMENSIONS = 0x01,
	FONTS = 0x02,
	IMAGES = 0x04,
	SHADINGS = 0x08,
	PATTERNS = 0x10,
	XOBJS = 0x20,
	ALL = DIMENSIONS | FONTS | IMAGES | SHADINGS | PATTERNS | XOBJS
};

struct info
{
	int page;
	fz_obj *pageobj;
	fz_obj *ref;
	union {
		struct {
			fz_obj *obj;
		} info;
		struct {
			fz_obj *obj;
		} crypt;
		struct {
			fz_rect *bbox;
		} dim;
		struct {
			fz_obj *subtype;
			fz_obj *name;
		} font;
		struct {
			fz_obj *width;
			fz_obj *height;
			fz_obj *bpc;
			fz_obj *filter;
			fz_obj *cs;
			fz_obj *altcs;
		} image;
		struct {
			fz_obj *type;
		} shading;
		struct {
			fz_obj *pattern;
			fz_obj *paint;
			fz_obj *tiling;
		} pattern;
		struct {
			fz_obj *group;
			fz_obj *reference;
		} form;
	} u;
};

static struct info *info = nil;
static struct info *cryptinfo = nil;
static struct info **dim = nil;
static int dims = 0;
static struct info **font = nil;
static int fonts = 0;
static struct info **image = nil;
static int images = 0;
static struct info **shading = nil;
static int shadings = 0;
static struct info **pattern = nil;
static int patterns = 0;
static struct info **form = nil;
static int forms = 0;
static struct info **psobj = nil;
static int psobjs = 0;

static void local_cleanup(void)
{
	int i;

	if (info)
	{
		free(info);
		info = nil;
	}

	if (dim)
	{
		for (i = 0; i < dims; i++)
			free(dim[i]);
		free(dim);
		dim = nil;
	}

	if (font)
	{
		for (i = 0; i < fonts; i++)
			free(font[i]);
		free(font);
		font = nil;
	}

	if (image)
	{
		for (i = 0; i < images; i++)
			free(image[i]);
		free(image);
		image = nil;
	}

	if (shading)
	{
		for (i = 0; i < shadings; i++)
			free(shading[i]);
		free(shading);
		shading = nil;
	}

	if (pattern)
	{
		for (i = 0; i < patterns; i++)
			free(pattern[i]);
		free(pattern);
		pattern = nil;
	}

	if (form)
	{
		for (i = 0; i < forms; i++)
			free(form[i]);
		free(form);
		form = nil;
	}

	if (psobj)
	{
		for (i = 0; i < psobjs; i++)
			free(psobj[i]);
		free(psobj);
		psobj = nil;
	}

	if (xref && xref->store)
	{
		pdf_dropstore(xref->store);
		xref->store = nil;
	}
}

static void
infousage(void)
{
	fprintf(stderr,
			"usage: pdfinfo [options] [file.pdf ... ]\n"
			"  -d -\tpassword for decryption\n"
			"  -f -\tlist fonts\n"
			"  -i -\tlist images\n"
			"  -m -\tlist dimensions\n"
			"  -p -\tlist patterns\n"
			"  -s -\tlist shadings\n"
			"  -x -\tlist form and postscript xobjects\n"
			"  example:\n"
			"    pdfinfo -p mypassword a.pdf\n");
	exit(1);
}

static void
gatherglobalinfo(void)
{
	info = fz_malloc(sizeof (struct info));
	if (!info)
		die(fz_throw("out of memory"));

	info->page = -1;
	info->pageobj = nil;
	info->ref = nil;
	info->u.info.obj = nil;

	if (xref->info)
	{
		info->ref = fz_dictgets(xref->trailer, "Info");
		if (!fz_isdict(info->ref) && !fz_isindirect(info->ref))
			die(fz_throw("not an indirect info object"));

		info->u.info.obj = xref->info;
	}

	cryptinfo = fz_malloc(sizeof (struct info));

	cryptinfo->page = -1;
	cryptinfo->pageobj = nil;
	cryptinfo->ref = nil;
	cryptinfo->u.crypt.obj = nil;

	if (xref->crypt)
	{
		cryptinfo->ref = fz_dictgets(xref->trailer, "Encrypt");
		if (!fz_isdict(cryptinfo->ref) && !fz_isindirect(cryptinfo->ref))
			die(fz_throw("not an indirect crypt object"));

		cryptinfo->u.crypt.obj = xref->crypt->encrypt;
	}
}

static fz_error
gatherdimensions(int page, fz_obj *pageobj)
{
	fz_obj *ref;
	fz_rect bbox;
	fz_obj *obj;
	int j;

	obj = ref = fz_dictgets(pageobj, "MediaBox");
	if (!fz_isarray(obj))
		return fz_throw("cannot find page bounds (%d %d R)", fz_tonum(ref), fz_togen(ref));

	bbox = pdf_torect(obj);

	for (j = 0; j < dims; j++)
		if (!memcmp(dim[j]->u.dim.bbox, &bbox, sizeof (fz_rect)))
			break;

	if (j < dims)
		return fz_okay;

	dims++;

	dim = fz_realloc(dim, dims * sizeof (struct info *));
	if (!dim)
		return fz_throw("out of memory");

	dim[dims - 1] = fz_malloc(sizeof (struct info));
	if (!dim[dims - 1])
		return fz_throw("out of memory");

	dim[dims - 1]->u.dim.bbox = fz_malloc(sizeof (fz_rect));
	if (!dim[dims - 1]->u.dim.bbox)
		return fz_throw("out of memory");

	dim[dims - 1]->page = page;
	dim[dims - 1]->pageobj = pageobj;
	dim[dims - 1]->ref = nil;
	memcpy(dim[dims - 1]->u.dim.bbox, &bbox, sizeof (fz_rect));

	return fz_okay;
}

static fz_error
gatherfonts(int page, fz_obj *pageobj, fz_obj *dict)
{
	int i;

	for (i = 0; i < fz_dictlen(dict); i++)
	{
		fz_obj *ref;
		fz_obj *fontdict;
		fz_obj *subtype;
		fz_obj *basefont;
		fz_obj *name;
		int k;

		fontdict = ref = fz_dictgetval(dict, i);
		if (!fz_isdict(fontdict))
			return fz_throw("not a font dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		subtype = fz_dictgets(fontdict, "Subtype");
		if (!fz_isname(subtype))
			return fz_throw("not a font dict subtype (%d %d R)", fz_tonum(ref), fz_togen(ref));

		basefont = fz_dictgets(fontdict, "BaseFont");
		if (basefont)
		{
			if (!fz_isname(basefont))
				return fz_throw("not a font dict basefont (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		else
		{
			name = fz_dictgets(fontdict, "Name");
			if (name && !fz_isname(name))
				return fz_throw("not a font dict name (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}

		for (k = 0; k < fonts; k++)
			if (fz_tonum(font[k]->ref) == fz_tonum(ref) &&
					fz_togen(font[k]->ref) == fz_togen(ref))
				break;

		if (k < fonts)
			continue;

		fonts++;

		font = fz_realloc(font, fonts * sizeof (struct info *));
		if (!font)
			return fz_throw("out of memory");

		font[fonts - 1] = fz_malloc(sizeof (struct info));
		if (!font[fonts - 1])
			return fz_throw("out of memory");

		font[fonts - 1]->page = page;
		font[fonts - 1]->pageobj = pageobj;
		font[fonts - 1]->ref = ref;
		font[fonts - 1]->u.font.subtype = subtype;
		font[fonts - 1]->u.font.name = basefont ? basefont : name;
	}

	return fz_okay;
}

static fz_error
gatherimages(int page, fz_obj *pageobj, fz_obj *dict)
{
	int i;

	for (i = 0; i < fz_dictlen(dict); i++)
	{
		fz_obj *ref;
		fz_obj *imagedict;
		fz_obj *type;
		fz_obj *width;
		fz_obj *height;
		fz_obj *bpc = nil;
		fz_obj *filter = nil;
		fz_obj *mask;
		fz_obj *cs = nil;
		fz_obj *altcs;
		int k;

		imagedict = ref = fz_dictgetval(dict, i);
		if (!fz_isdict(imagedict))
			return fz_throw("not an image dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		type = fz_dictgets(imagedict, "Subtype");
		if (!fz_isname(type))
			return fz_throw("not an image subtype (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (strcmp(fz_toname(type), "Image"))
			continue;

		filter = fz_dictgets(imagedict, "Filter");
		if (filter && !fz_isname(filter) && !fz_isarray(filter))
			return fz_throw("not an image filter (%d %d R)", fz_tonum(ref), fz_togen(ref));

		mask = fz_dictgets(imagedict, "ImageMask");

		altcs = nil;
		cs = fz_dictgets(imagedict, "ColorSpace");
		if (fz_isarray(cs))
		{
			fz_obj *cses = cs;

			cs = fz_arrayget(cses, 0);
			if (fz_isname(cs) && (!strcmp(fz_toname(cs), "DeviceN") || !strcmp(fz_toname(cs), "Separation")))
			{
				altcs = fz_arrayget(cses, 2);
				if (fz_isarray(altcs))
					altcs = fz_arrayget(altcs, 0);
			}
		}

		if (fz_isbool(mask) && fz_tobool(mask))
		{
			if (cs)
				fz_warn("image mask (%d %d R) may not have colorspace", fz_tonum(ref), fz_togen(ref));
		}
		if (cs && !fz_isname(cs))
			return fz_throw("not an image colorspace (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (altcs && !fz_isname(altcs))
			return fz_throw("not an image alternate colorspace (%d %d R)", fz_tonum(ref), fz_togen(ref));

		width = fz_dictgets(imagedict, "Width");
		if (!fz_isint(width))
			return fz_throw("not an image width (%d %d R)", fz_tonum(ref), fz_togen(ref));

		height = fz_dictgets(imagedict, "Height");
		if (!fz_isint(height))
			return fz_throw("not an image height (%d %d R)", fz_tonum(ref), fz_togen(ref));

		bpc = fz_dictgets(imagedict, "BitsPerComponent");
		if (!fz_tobool(mask) && !fz_isint(bpc))
			return fz_throw("not an image bits per component (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (fz_tobool(mask) && fz_isint(bpc) && fz_toint(bpc) != 1)
			return fz_throw("not an image mask bits per component (%d %d R)", fz_tonum(ref), fz_togen(ref));

		for (k = 0; k < images; k++)
			if (fz_tonum(image[k]->ref) == fz_tonum(ref) &&
					fz_togen(image[k]->ref) == fz_togen(ref))
				break;

		if (k < images)
			continue;

		images++;

		image = fz_realloc(image, images * sizeof (struct info *));
		if (!image)
			return fz_throw("out of memory");

		image[images - 1] = fz_malloc(sizeof (struct info));
		if (!image[images - 1])
			return fz_throw("out of memory");

		image[images - 1]->page = page;
		image[images - 1]->pageobj = pageobj;
		image[images - 1]->ref = ref;
		image[images - 1]->u.image.width = width;
		image[images - 1]->u.image.height = height;
		image[images - 1]->u.image.bpc = bpc;
		image[images - 1]->u.image.filter = filter;
		image[images - 1]->u.image.cs = cs;
		image[images - 1]->u.image.altcs = altcs;
	}

	return fz_okay;
}

static fz_error
gatherforms(int page, fz_obj *pageobj, fz_obj *dict)
{
	int i;

	for (i = 0; i < fz_dictlen(dict); i++)
	{
		fz_obj *ref;
		fz_obj *xobjdict;
		fz_obj *type;
		fz_obj *subtype;
		fz_obj *group;
		fz_obj *reference;
		int k;

		xobjdict = ref = fz_dictgetval(dict, i);
		if (!fz_isdict(xobjdict))
			return fz_throw("not a xobject dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		type = fz_dictgets(xobjdict, "Subtype");
		if (!fz_isname(type))
			return fz_throw("not a xobject type (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (strcmp(fz_toname(type), "Form"))
			continue;

		subtype = fz_dictgets(xobjdict, "Subtype2");
		if (subtype && !fz_isname(subtype))
			return fz_throw("not a xobject subtype (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (strcmp(fz_toname(subtype), "PS"))
			continue;

		group = fz_dictgets(xobjdict, "Group");
		if (group && !fz_isdict(group))
			return fz_throw("not a form xobject group dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		reference = fz_dictgets(xobjdict, "Ref");
		if (reference && !fz_isdict(reference))
			return fz_throw("not a form xobject reference dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		for (k = 0; k < forms; k++)
			if (fz_tonum(form[k]->ref) == fz_tonum(ref) &&
					fz_togen(form[k]->ref) == fz_togen(ref))
				break;

		if (k < forms)
			continue;

		forms++;

		form = fz_realloc(form, forms * sizeof (struct info *));
		if (!form)
			return fz_throw("out of memory");

		form[forms - 1] = fz_malloc(sizeof (struct info));
		if (!form[forms - 1])
			return fz_throw("out of memory");

		form[forms - 1]->page = page;
		form[forms - 1]->pageobj = pageobj;
		form[forms - 1]->ref = ref;
		form[forms - 1]->u.form.group = group;
		form[forms - 1]->u.form.reference = reference;
	}

	return fz_okay;
}

static fz_error
gatherpsobjs(int page, fz_obj *pageobj, fz_obj *dict)
{
	int i;

	for (i = 0; i < fz_dictlen(dict); i++)
	{
		fz_obj *ref;
		fz_obj *xobjdict;
		fz_obj *type;
		fz_obj *subtype;
		int k;

		xobjdict = ref = fz_dictgetval(dict, i);
		if (!fz_isdict(xobjdict))
			return fz_throw("not a xobject dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		type = fz_dictgets(xobjdict, "Subtype");
		if (!fz_isname(type))
			return fz_throw("not a xobject type (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (strcmp(fz_toname(type), "Form"))
			continue;

		subtype = fz_dictgets(xobjdict, "Subtype2");
		if (subtype && !fz_isname(subtype))
			return fz_throw("not a xobject subtype (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (strcmp(fz_toname(type), "PS") &&
				(strcmp(fz_toname(type), "Form") || strcmp(fz_toname(subtype), "PS")))
			continue;

		for (k = 0; k < psobjs; k++)
			if (fz_tonum(psobj[k]->ref) == fz_tonum(ref) &&
					fz_togen(psobj[k]->ref) == fz_togen(ref))
				break;

		if (k < psobjs)
			continue;

		psobjs++;

		psobj = fz_realloc(psobj, psobjs * sizeof (struct info *));
		if (!psobj)
			return fz_throw("out of memory");

		psobj[psobjs - 1] = fz_malloc(sizeof (struct info));
		if (!psobj[psobjs - 1])
			return fz_throw("out of memory");

		psobj[psobjs - 1]->page = page;
		psobj[psobjs - 1]->pageobj = pageobj;
		psobj[psobjs - 1]->ref = ref;
	}

	return fz_okay;
}

static fz_error
gathershadings(int page, fz_obj *pageobj, fz_obj *dict)
{
	int i;

	for (i = 0; i < fz_dictlen(dict); i++)
	{
		fz_obj *ref;
		fz_obj *shade;
		fz_obj *type;
		int k;

		shade = ref = fz_dictgetval(dict, i);
		if (!fz_isdict(shade))
			return fz_throw("not a shading dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		type = fz_dictgets(shade, "ShadingType");
		if (!fz_isint(type) || fz_toint(type) < 1 || fz_toint(type) > 7)
			return fz_throw("not a shading type (%d %d R)", fz_tonum(ref), fz_togen(ref));

		for (k = 0; k < shadings; k++)
			if (fz_tonum(shading[k]->ref) == fz_tonum(ref) &&
					fz_togen(shading[k]->ref) == fz_togen(ref))
				break;

		if (k < shadings)
			continue;

		shadings++;

		shading = fz_realloc(shading, shadings * sizeof (struct info *));
		if (!shading)
			return fz_throw("out of memory");

		shading[shadings - 1] = fz_malloc(sizeof (struct info));
		if (!shading[shadings - 1])
			return fz_throw("out of memory");

		shading[shadings - 1]->page = page;
		shading[shadings - 1]->pageobj = pageobj;
		shading[shadings - 1]->ref = ref;
		shading[shadings - 1]->u.shading.type = type;
	}

	return fz_okay;
}

static fz_error
gatherpatterns(int page, fz_obj *pageobj, fz_obj *dict)
{
	int i;

	for (i = 0; i < fz_dictlen(dict); i++)
	{
		fz_obj *ref;
		fz_obj *patterndict;
		fz_obj *type;
		fz_obj *paint = nil;
		fz_obj *tiling = nil;
		int k;

		patterndict = ref = fz_dictgetval(dict, i);
		if (!fz_isdict(patterndict))
			return fz_throw("not a pattern dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		type = fz_dictgets(patterndict, "PatternType");
		if (!fz_isint(type) || fz_toint(type) < 1 || fz_toint(type) > 2)
			return fz_throw("not a pattern type (%d %d R)", fz_tonum(ref), fz_togen(ref));

		if (fz_toint(type) == 1)
		{
			paint = fz_dictgets(patterndict, "PaintType");
			if (!fz_isint(paint) || fz_toint(paint) < 1 || fz_toint(paint) > 2)
				return fz_throw("not a pattern paint type (%d %d R)", fz_tonum(ref), fz_togen(ref));

			tiling = fz_dictgets(patterndict, "TilingType");
			if (!fz_isint(tiling) || fz_toint(tiling) < 1 || fz_toint(tiling) > 3)
				return fz_throw("not a pattern tiling type (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}

		for (k = 0; k < patterns; k++)
			if (fz_tonum(pattern[k]->ref) == fz_tonum(ref) &&
					fz_togen(pattern[k]->ref) == fz_togen(ref))
				break;

		if (k < patterns)
			continue;

		patterns++;

		pattern = fz_realloc(pattern, patterns * sizeof (struct info *));
		if (!pattern)
			return fz_throw("out of memory");

		pattern[patterns - 1] = fz_malloc(sizeof (struct info));
		if (!pattern[patterns - 1])
			return fz_throw("out of memory");

		pattern[patterns - 1]->page = page;
		pattern[patterns - 1]->pageobj = pageobj;
		pattern[patterns - 1]->ref = ref;
		pattern[patterns - 1]->u.pattern.pattern = type;
		pattern[patterns - 1]->u.pattern.paint = paint;
		pattern[patterns - 1]->u.pattern.tiling = tiling;
	}

	return fz_okay;
}

static void
gatherinfo(int show, int page)
{
	fz_error error;
	fz_obj *pageobj;
	fz_obj *rsrc;
	fz_obj *font;
	fz_obj *xobj;
	fz_obj *shade;
	fz_obj *pattern;

	error = pdf_getpageobject(xref, page, &pageobj);
	if (error)
		die(error);

	if (!pageobj)
		die(fz_throw("cannot retrieve info from page %d", page));

	if (show & DIMENSIONS)
	{
		error = gatherdimensions(page, pageobj);
		if (error)
			die(fz_rethrow(error, "gathering dimensions at page %d (%d %d R)", page, fz_tonum(pageobj), fz_togen(pageobj)));
	}

	rsrc = fz_dictgets(pageobj, "Resources");

	if (show & FONTS)
	{
		font = fz_dictgets(rsrc, "Font");
		if (font)
		{
			error = gatherfonts(page, pageobj, font);
			if (error)
				die(fz_rethrow(error, "gathering fonts at page %d (%d %d R)", page, fz_tonum(pageobj), fz_togen(pageobj)));
		}
	}

	if (show & IMAGES || show & XOBJS)
	{
		xobj = fz_dictgets(rsrc, "XObject");
		if (xobj)
		{
			error = gatherimages(page, pageobj, xobj);
			if (error)
				die(fz_rethrow(error, "gathering images at page %d (%d %d R)", page, fz_tonum(pageobj), fz_togen(pageobj)));
			error = gatherforms(page, pageobj, xobj);
			if (error)
				die(fz_rethrow(error, "gathering forms at page %d (%d %d R)", page, fz_tonum(pageobj), fz_togen(pageobj)));
			error = gatherpsobjs(page, pageobj, xobj);
			if (error)
				die(fz_rethrow(error, "gathering postscript objects at page %d (%d %d R)", page, fz_tonum(pageobj), fz_togen(pageobj)));
		}
	}

	if (show & SHADINGS)
	{
		shade = fz_dictgets(rsrc, "Shading");
		if (shade)
		{
			error = gathershadings(page, pageobj, shade);
			if (error)
				die(fz_rethrow(error, "gathering shadings at page %d (%d %d R)", page, fz_tonum(pageobj), fz_togen(pageobj)));
		}
	}

	if (show & PATTERNS)
	{
		pattern = fz_dictgets(rsrc, "Pattern");
		if (pattern)
		{
			error = gatherpatterns(page, pageobj, pattern);
			if (error)
				die(fz_rethrow(error, "gathering shadings at page %d (%d %d R)", page, fz_tonum(pageobj), fz_togen(pageobj)));
		}
	}
}

static void
printglobalinfo(void)
{
	printf("\nPDF-%d.%d\n", xref->version / 10, xref->version % 10);

	if (info->u.info.obj)
	{
		printf("Info object (%d %d R):\n", fz_tonum(info->ref), fz_togen(info->ref));
		fz_debugobj(info->u.info.obj);
	}

	if (cryptinfo->u.crypt.obj)
	{
		printf("\nEncryption object (%d %d R):\n", fz_tonum(cryptinfo->ref), fz_togen(cryptinfo->ref));
		fz_debugobj(cryptinfo->u.crypt.obj);
	}

	printf("\nPages: %d\n\n", xref->pagecount);
}

static void
printinfo(char *filename, int show, int page)
{
	int i;
	int j;

#define PAGE_FMT "\t% 6d (% 6d %1d R): "

	if (show & DIMENSIONS && dims > 0)
	{
		printf("MediaBox: ");
		printf("\n");
		for (i = 0; i < dims; i++)
			printf(PAGE_FMT "[ %g %g %g %g ]\n",
					dim[i]->page,
					fz_tonum(dim[i]->pageobj), fz_togen(dim[i]->pageobj),
					dim[i]->u.dim.bbox->x0,
					dim[i]->u.dim.bbox->y0,
					dim[i]->u.dim.bbox->x1,
					dim[i]->u.dim.bbox->y1);
		printf("\n");

		for (i = 0; i < dims; i++)
		{
			fz_free(dim[i]->u.dim.bbox);
			fz_free(dim[i]);
		}
		fz_free(dim);
		dim = nil;
		dims = 0;
	}

	if (show & FONTS && fonts > 0)
	{
		printf("Fonts (%d):\n", fonts);
		for (i = 0; i < fonts; i++)
		{
			printf(PAGE_FMT "%s '%s' (%d %d R)\n",
					font[i]->page,
					fz_tonum(font[i]->pageobj), fz_togen(font[i]->pageobj),
					fz_toname(font[i]->u.font.subtype),
					fz_toname(font[i]->u.font.name),
					fz_tonum(font[i]->ref), fz_togen(font[i]->ref));
		}
		printf("\n");

		for (i = 0; i < fonts; i++)
			fz_free(font[i]);
		fz_free(font);
		font = nil;
		fonts = 0;
	}

	if (show & IMAGES && images > 0)
	{
		printf("Images (%d):\n", images);
		for (i = 0; i < images; i++)
		{
			printf(PAGE_FMT "[ ",
					image[i]->page,
					fz_tonum(image[i]->pageobj), fz_togen(image[i]->pageobj));

			if (fz_isarray(image[i]->u.image.filter))
				for (j = 0; j < fz_arraylen(image[i]->u.image.filter); j++)
				{
					printf("%s%s",
							fz_toname(fz_arrayget(image[i]->u.image.filter, j)),
							j == fz_arraylen(image[i]->u.image.filter) - 1 ? "" : " ");
				}
			else if (image[i]->u.image.filter)
				printf("%s", fz_toname(image[i]->u.image.filter));
			else
				printf("Raw");

			printf(" ] %dx%d %dbpc %s%s%s (%d %d R)\n",
					fz_toint(image[i]->u.image.width),
					fz_toint(image[i]->u.image.height),
					image[i]->u.image.bpc ? fz_toint(image[i]->u.image.bpc) : 1,
					image[i]->u.image.cs ? fz_toname(image[i]->u.image.cs) : "ImageMask",
					image[i]->u.image.altcs ? " " : "",
					image[i]->u.image.altcs ? fz_toname(image[i]->u.image.altcs) : "",
					fz_tonum(image[i]->ref), fz_togen(image[i]->ref));
		}
		printf("\n");

		for (i = 0; i < images; i++)
			fz_free(image[i]);
		fz_free(image);
		image = nil;
		images = 0;
	}

	if (show & SHADINGS && shadings > 0)
	{
		printf("Shading patterns (%d):\n", shadings);
		for (i = 0; i < shadings; i++)
		{
			char *shadingtype[] =
			{
				"",
				"Function",
				"Axial",
				"Radial",
				"Free-form triangle mesh",
				"Lattice-form triangle mesh",
				"Coons patch mesh",
				"Tendor-product patch mesh",
			};

			printf(PAGE_FMT "%s (%d %d R)\n",
					shading[i]->page,
					fz_tonum(shading[i]->pageobj), fz_togen(shading[i]->pageobj),
					shadingtype[fz_toint(shading[i]->u.shading.type)],
					fz_tonum(shading[i]->ref), fz_togen(shading[i]->ref));
		}
		printf("\n");

		for (i = 0; i < shadings; i++)
			fz_free(shading[i]);
		fz_free(shading);
		shading = nil;
		shadings = 0;
	}

	if (show & PATTERNS && patterns > 0)
	{
		printf("Patterns (%d):\n", patterns);
		for (i = 0; i < patterns; i++)
		{
			char *patterntype[] =
			{
				"",
				"Tiling",
				"Shading",
			};
			char *painttype[] =
			{
				"",
				"Colored",
				"Uncolored",
			};
			char *tilingtype[] =
			{
				"",
				"Constant spacing",
				"No distortion",
				"Constant space/fast tiling",
			};

			printf(PAGE_FMT "%s %s %s (%d %d R)\n",
					pattern[i]->page,
					fz_tonum(pattern[i]->pageobj), fz_togen(pattern[i]->pageobj),
					patterntype[fz_toint(pattern[i]->u.pattern.pattern)],
					painttype[fz_toint(pattern[i]->u.pattern.paint)],
					tilingtype[fz_toint(pattern[i]->u.pattern.tiling)],
					fz_tonum(pattern[i]->ref), fz_togen(pattern[i]->ref));
		}
		printf("\n");

		for (i = 0; i < patterns; i++)
			fz_free(pattern[i]);
		fz_free(pattern);
		pattern = nil;
		patterns = 0;
	}

	if (show & XOBJS && forms > 0)
	{
		printf("Form xobjects (%d):\n", forms);
		for (i = 0; i < forms; i++)
		{
			printf(PAGE_FMT "%s%s (%d %d R)\n",
					form[i]->page,
					fz_tonum(form[i]->pageobj), fz_togen(form[i]->pageobj),
					form[i]->u.form.group ? "Group" : "",
					form[i]->u.form.reference ? "Reference" : "",
					fz_tonum(form[i]->ref), fz_togen(form[i]->ref));
		}
		printf("\n");

		for (i = 0; i < forms; i++)
			fz_free(form[i]);
		fz_free(form);
		form = nil;
		forms = 0;
	}

	if (show & XOBJS && psobjs > 0)
	{
		printf("Postscript xobjects (%d):\n", psobjs);
		for (i = 0; i < psobjs; i++)
		{
			printf(PAGE_FMT "(%d %d R)\n",
					psobj[i]->page,
					fz_tonum(psobj[i]->pageobj), fz_togen(psobj[i]->pageobj),
					fz_tonum(psobj[i]->ref), fz_togen(psobj[i]->ref));
		}
		printf("\n");

		for (i = 0; i < psobjs; i++)
			fz_free(psobj[i]);
		fz_free(psobj);
		psobj = nil;
		psobjs = 0;
	}
}

static void
showinfo(char *filename, int show, char *pagelist)
{
	int page, spage, epage;
	char *spec, *dash;
	int allpages;

	if (!xref)
		infousage();

	allpages = !strcmp(pagelist, "1-");

	spec = strsep(&pagelist, ",");
	while (spec)
	{
		dash = strchr(spec, '-');

		if (dash == spec)
			spage = epage = 1;
		else
			spage = epage = atoi(spec);

		if (dash)
		{
			if (strlen(dash) > 1)
				epage = atoi(dash + 1);
			else
				epage = xref->pagecount;
		}

		if (spage > epage)
			page = spage, spage = epage, epage = page;

		if (spage < 1)
			spage = 1;
		if (epage > xref->pagecount)
			epage = xref->pagecount;
		if (spage > xref->pagecount)
			spage = xref->pagecount;

		if (allpages)
			printf("Retrieving info from pages %d-%d...\n", spage, epage);
		if (spage >= 1)
		{
			for (page = spage; page <= epage; page++)
			{
				gatherinfo(show, page);
				if (!allpages)
				{
					printf("Page %05d:\n", page);
					printinfo(filename, show, page);
					printf("\n");
				}
			}
		}

		spec = strsep(&pagelist, ",");
	}

	if (allpages)
		printinfo(filename, show, -1);
}

int main(int argc, char **argv)
{
	enum { NO_FILE_OPENED, NO_INFO_GATHERED, INFO_SHOWN } state;
	char *filename = "";
	char *password = "";
	int show = ALL;
	int c;

	while ((c = fz_getopt(argc, argv, "mfispxd:")) != -1)
	{
		switch (c)
		{
			case 'm': if (show == ALL) show = DIMENSIONS; else show |= DIMENSIONS; break;
			case 'f': if (show == ALL) show = FONTS; else show |= FONTS; break;
			case 'i': if (show == ALL) show = IMAGES; else show |= IMAGES; break;
			case 's': if (show == ALL) show = SHADINGS; else show |= SHADINGS; break;
			case 'p': if (show == ALL) show = PATTERNS; else show |= PATTERNS; break;
			case 'x': if (show == ALL) show = XOBJS; else show |= XOBJS; break;
			case 'd': password = fz_optarg; break;
			default:
				infousage();
				break;
		}
	}

	if (fz_optind == argc)
		infousage();

	setcleanup(local_cleanup);

	state = NO_FILE_OPENED;
	while (fz_optind < argc)
	{
		if (strstr(argv[fz_optind], ".pdf") || strstr(argv[fz_optind], ".PDF"))
		{
			if (state == NO_INFO_GATHERED)
			{
				printglobalinfo();
				showinfo(filename, show, "1-");
				closexref();
			}

			closexref();
			filename = argv[fz_optind];
			openxref(filename, password, 0);
			gatherglobalinfo();
			state = NO_INFO_GATHERED;
		}
		else
		{
			if (state == NO_INFO_GATHERED)
				printglobalinfo();
			showinfo(filename, show, argv[fz_optind]);
			state = INFO_SHOWN;
		}

		fz_optind++;
	}

	if (state == NO_INFO_GATHERED)
	{
		printglobalinfo();
		showinfo(filename, show, "1-");
	}

	closexref();
}

