/*
 * Information tool.
 * Print information about the input pdf.
 */

#include "fitz.h"
#include "mupdf.h"

/* put these up here so we can clean up in die() */
fz_renderer *drawgc = nil;
void closesrc(void);

/*
 * Common operations.
 * Parse page selectors.
 * Load and decrypt a PDF file.
 * Select pages.
 */

char *srcname = "(null)";
pdf_xref *src = nil;
pdf_outline *srcoutline = nil;
pdf_pagetree *srcpages = nil;

void die(fz_error eo)
{
    fz_catch(eo, "aborting");
    if (drawgc)
	fz_droprenderer(drawgc);
    closesrc();
    exit(-1);
}

void closesrc(void)
{
	if (srcpages)
	{
		pdf_droppagetree(srcpages);
		srcpages = nil;
	}

	if (src)
	{
		if (src->store)
		{
			pdf_dropstore(src->store);
			src->store = nil;
		}
		pdf_closexref(src);
		src = nil;
	}

	srcname = nil;
}

void opensrc(char *filename, char *password, int loadpages)
{
	fz_error error;
	fz_obj *obj;

	closesrc();

	srcname = filename;

	error = pdf_newxref(&src);
	if (error)
		die(error);

	error = pdf_loadxref(src, filename);
	if (error)
	{
		fz_catch(error, "trying to repair");
		error = pdf_repairxref(src, filename);
		if (error)
			die(error);
	}

	error = pdf_decryptxref(src);
	if (error)
		die(error);

	if (src->crypt)
	{
		int okay = pdf_setpassword(src->crypt, password);
		if (!okay)
			die(fz_throw("invalid password"));
	}

	if (loadpages)
	{
		error = pdf_loadpagetree(&srcpages, src);
		if (error)
			die(error);
	}

	/* TODO: move into mupdf lib, see pdfapp_open in pdfapp.c */
	obj = fz_dictgets(src->trailer, "Root");
	if (!obj)
		die(error);

	error = pdf_loadindirect(&src->root, src, obj);
	if (error)
		die(error);

	obj = fz_dictgets(src->trailer, "Info");
	if (obj)
	{
		error = pdf_loadindirect(&src->info, src, obj);
		if (error)
			die(error);
	}

	error = pdf_loadnametrees(src);
	if (error)
		die(error);

	error = pdf_loadoutline(&srcoutline, src);
	if (error)
		die(error);
}

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
	fz_obj *pageref;
	fz_obj *ref;
	union {
		struct {
			fz_obj *obj;
		} info;
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

struct info *info = nil;
struct info **dim = nil;
int dims = 0;
struct info **font = nil;
int fonts = 0;
struct info **image = nil;
int images = 0;
struct info **shading = nil;
int shadings = 0;
struct info **pattern = nil;
int patterns = 0;
struct info **form = nil;
int forms = 0;
struct info **psobj = nil;
int psobjs = 0;

void
infousage(void)
{
	fprintf(stderr,
			"usage: mupdftool info [options] [file.pdf ... ]\n"
			"  -d -\tpassword for decryption\n"
			"  -f -\tlist fonts\n"
			"  -i -\tlist images\n"
			"  -m -\tlist dimensions\n"
			"  -p -\tlist pattners\n"
			"  -s -\tlist shadings\n"
			"  -x -\tlist form and postscript xobjects\n"
			"  example:\n"
			"    mupdftool info -p mypassword a.pdf\n");
	exit(1);
}

void
gatherglobalinfo()
{
	info = malloc(sizeof (struct info));
	if (!info)
		die(fz_throw("out of memory"));

	info->page = -1;
	info->pageref = nil;
	info->ref = fz_dictgets(src->trailer, "Info");

	info->u.info.obj = nil;

	if (!info->ref)
		return;

	if (!fz_isdict(info->ref) && !fz_isindirect(info->ref))
		die(fz_throw("not an indirect info object"));

	info->u.info.obj = src->info;
}

fz_error
gatherdimensions(int page, fz_obj *pageref, fz_obj *pageobj)
{
	fz_error error;
	fz_obj *ref;
	fz_rect bbox;
	fz_obj *obj;
	int j;

	obj = ref = fz_dictgets(pageobj, "MediaBox");
	if (obj)
	{
		error = pdf_resolve(&obj, src);
		if (error)
			return error;
	}
	if (!fz_isarray(obj))
		return fz_throw("cannot find page bounds (%d %d R)", fz_tonum(ref), fz_togen(ref));

	bbox = pdf_torect(obj);

	for (j = 0; j < dims; j++)
		if (!memcmp(dim[j]->u.dim.bbox, &bbox, sizeof (fz_rect)))
			break;

	if (j < dims)
		return fz_okay;

	dims++;

	dim = realloc(dim, dims * sizeof (struct info *));
	if (!dim)
		return fz_throw("out of memory");

	dim[dims - 1] = malloc(sizeof (struct info));
	if (!dim[dims - 1])
		return fz_throw("out of memory");

	dim[dims - 1]->u.dim.bbox = malloc(sizeof (fz_rect));
	if (!dim[dims - 1]->u.dim.bbox)
		return fz_throw("out of memory");

	dim[dims - 1]->page = page;
	dim[dims - 1]->pageref = pageref;
	dim[dims - 1]->ref = nil;
	memcpy(dim[dims - 1]->u.dim.bbox, &bbox, sizeof (fz_rect));

	return fz_okay;
}

fz_error
gatherfonts(int page, fz_obj *pageref, fz_obj *pageobj, fz_obj *dict)
{
	fz_error error;
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
		if (fontdict)
		{
			error = pdf_resolve(&fontdict, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect font dict (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isdict(fontdict))
			return fz_throw("not a font dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		subtype = fz_dictgets(fontdict, "Subtype");
		if (subtype)
		{
			error = pdf_resolve(&subtype, src);
			if (error)
				return fz_rethrow(error, "cannot find font dict subtype (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isname(subtype))
			return fz_throw("not a font dict subtype (%d %d R)", fz_tonum(ref), fz_togen(ref));

		basefont = fz_dictgets(fontdict, "BaseFont");
		if (basefont)
		{
		    error = pdf_resolve(&basefont, src);
		    if (error)
			return fz_rethrow(error, "cannot find font dict basefont (%d %d R)", fz_tonum(ref), fz_togen(ref));
		    if (!fz_isname(basefont))
			return fz_throw("not a font dict basefont (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		else
		{
		    name = fz_dictgets(fontdict, "Name");
		    if (name)
			error = pdf_resolve(&name, src);
		    else
			error = fz_newnull(&name);
		    if (error)
			return fz_rethrow(error, "cannot find font dict name (%d %d R)", fz_tonum(ref), fz_togen(ref));
		    if (!fz_isnull(name) && !fz_isname(name))
			return fz_throw("not a font dict name (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}

		for (k = 0; k < fonts; k++)
			if (fz_tonum(font[k]->ref) == fz_tonum(ref) &&
					fz_togen(font[k]->ref) == fz_togen(ref))
				break;

		if (k < fonts)
			return fz_okay;

		fonts++;

		font = realloc(font, fonts * sizeof (struct info *));
		if (!font)
			return fz_throw("out of memory");

		font[fonts - 1] = malloc(sizeof (struct info));
		if (!font[fonts - 1])
			return fz_throw("out of memory");

		font[fonts - 1]->page = page;
		font[fonts - 1]->pageref = pageref;
		font[fonts - 1]->ref = ref;
		font[fonts - 1]->u.font.subtype = subtype;
		font[fonts - 1]->u.font.name = basefont ? basefont : name;
	}

	return fz_okay;
}

fz_error
gatherimages(int page, fz_obj *pageref, fz_obj *pageobj, fz_obj *dict)
{
	fz_error error;
	int i;

	for (i = 0; i < fz_dictlen(dict); i++)
	{
		fz_obj *ref;
		fz_obj *imagedict;
		fz_obj *type;
		fz_obj *width;
		fz_obj *height;
		fz_obj *bpc;
		fz_obj *filter;
		fz_obj *mask;
		fz_obj *cs;
		fz_obj *altcs;
		int k;

		imagedict = ref = fz_dictgetval(dict, i);
		if (imagedict)
		{
			error = pdf_resolve(&imagedict, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect image dict (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isdict(imagedict))
			return fz_throw("not an image dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		type = fz_dictgets(imagedict, "Subtype");
		if (type)
		{
			error = pdf_resolve(&type, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect image subtype (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isname(type))
			return fz_throw("not an image subtype (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (strcmp(fz_toname(type), "Image"))
			continue;

		filter = fz_dictgets(imagedict, "Filter");
		if (filter)
		{
			error = pdf_resolve(&filter, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect image filter (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		else
		{
			error = fz_newname(&filter, "Raw");
			if (error)
				return fz_rethrow(error, "cannot create fake raw image filter (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isname(filter) && !fz_isarray(filter))
			return fz_throw("not an image filter (%d %d R)", fz_tonum(ref), fz_togen(ref));

		mask = fz_dictgets(imagedict, "ImageMask");
		if (mask)
		{
			error = pdf_resolve(&mask, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect image mask (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}

		altcs = nil;
		cs = fz_dictgets(imagedict, "ColorSpace");
		if (cs)
		{
			error = pdf_resolve(&cs, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect image colorspace (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (fz_isarray(cs))
		{
			fz_obj *cses = cs;

			cs = fz_arrayget(cses, 0);
			if (cs)
			{
				error = pdf_resolve(&cs, src);
				if (error)
					return fz_rethrow(error, "cannot resolve indirect image colorspace name (%d %d R)", fz_tonum(ref), fz_togen(ref));
			}

			if (fz_isname(cs) && (!strcmp(fz_toname(cs), "DeviceN") || !strcmp(fz_toname(cs), "Separation")))
			{
				altcs = fz_arrayget(cses, 2);
				if (altcs)
				{
					error = pdf_resolve(&altcs, src);
					if (error)
						return fz_rethrow(error, "cannot resolve indirect image alternate colorspace name (%d %d R)", fz_tonum(ref), fz_togen(ref));
				}

				if (fz_isarray(altcs))
				{
					altcs = fz_arrayget(altcs, 0);
					if (altcs)
					{
						error = pdf_resolve(&altcs, src);
						if (error)
							return fz_rethrow(error, "cannot resolve indirect image alternate colorspace name (%d %d R)", fz_tonum(ref), fz_togen(ref));
					}
				}
			}
		}

		if (fz_isbool(mask) && fz_tobool(mask))
		{
			if (cs)
				fz_warn("image mask (%d %d R) may not have colorspace", fz_tonum(ref), fz_togen(ref));
			error = fz_newname(&cs, "ImageMask");
			if (error)
				return fz_rethrow(error, "cannot create fake image mask colorspace (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isname(cs))
			return fz_throw("not an image colorspace (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (altcs && !fz_isname(altcs))
			return fz_throw("not an image alternate colorspace (%d %d R)", fz_tonum(ref), fz_togen(ref));

		width = fz_dictgets(imagedict, "Width");
		if (width)
		{
			error = pdf_resolve(&type, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect image width (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isint(width))
			return fz_throw("not an image width (%d %d R)", fz_tonum(ref), fz_togen(ref));

		height = fz_dictgets(imagedict, "Height");
		if (height)
		{
			error = pdf_resolve(&height, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect image height (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isint(height))
			return fz_throw("not an image height (%d %d R)", fz_tonum(ref), fz_togen(ref));

		bpc = fz_dictgets(imagedict, "BitsPerComponent");
		if (bpc)
		{
			error = pdf_resolve(&bpc, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect image bits per component (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_tobool(mask) && !fz_isint(bpc))
			return fz_throw("not an image bits per component (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (fz_tobool(mask) && fz_isint(bpc) && fz_toint(bpc) != 1)
			return fz_throw("not an image mask bits per component (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (fz_tobool(mask) && !bpc)
		{
			error = fz_newint(&bpc, 1);
			if (error)
				return fz_rethrow(error, "cannot create fake image mask bits per components (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}

		for (k = 0; k < images; k++)
			if (fz_tonum(image[k]->ref) == fz_tonum(ref) &&
					fz_togen(image[k]->ref) == fz_togen(ref))
				break;

		if (k < images)
			continue;

		images++;

		image = realloc(image, images * sizeof (struct info *));
		if (!image)
			return fz_throw("out of memory");

		image[images - 1] = malloc(sizeof (struct info));
		if (!image[images - 1])
			return fz_throw("out of memory");

		image[images - 1]->page = page;
		image[images - 1]->pageref = pageref;
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

fz_error
gatherforms(int page, fz_obj *pageref, fz_obj *pageobj, fz_obj *dict)
{
	fz_error error;
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
		if (xobjdict)
		{
			error = pdf_resolve(&xobjdict, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect image dict (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isdict(xobjdict))
			return fz_throw("not a xobject dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		type = fz_dictgets(xobjdict, "Subtype");
		if (type)
		{
			error = pdf_resolve(&type, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect xobject type (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isname(type))
			return fz_throw("not a xobject type (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (strcmp(fz_toname(type), "Form"))
			return fz_okay;

		subtype = fz_dictgets(xobjdict, "Subtype2");
		if (subtype)
		{
			error = pdf_resolve(&subtype, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect xobject subtype (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (subtype && !fz_isname(subtype))
			return fz_throw("not a xobject subtype (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (strcmp(fz_toname(subtype), "PS"))
			return fz_okay;

		group = fz_dictgets(xobjdict, "Group");
		if (group)
		{
			error = pdf_resolve(&group, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect form xobject group dict (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (group && !fz_isdict(group))
			return fz_throw("not a form xobject group dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		reference = fz_dictgets(xobjdict, "Ref");
		if (reference)
		{
			error = pdf_resolve(&reference, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect form xobject reference dict (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (reference && !fz_isdict(reference))
			return fz_throw("not a form xobject reference dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		for (k = 0; k < forms; k++)
			if (fz_tonum(form[k]->ref) == fz_tonum(ref) &&
					fz_togen(form[k]->ref) == fz_togen(ref))
				break;

		if (k < forms)
			return fz_okay;

		forms++;

		form = realloc(form, forms * sizeof (struct info *));
		if (!form)
			return fz_throw("out of memory");

		form[forms - 1] = malloc(sizeof (struct info));
		if (!form[forms - 1])
			return fz_throw("out of memory");

		form[forms - 1]->page = page;
		form[forms - 1]->pageref = pageref;
		form[forms - 1]->ref = ref;
		form[forms - 1]->u.form.group = group;
		form[forms - 1]->u.form.reference = reference;
	}

	return fz_okay;
}

fz_error
gatherpsobjs(int page, fz_obj *pageref, fz_obj *pageobj, fz_obj *dict)
{
	fz_error error;
	int i;

	for (i = 0; i < fz_dictlen(dict); i++)
	{
		fz_obj *ref;
		fz_obj *xobjdict;
		fz_obj *type;
		fz_obj *subtype;
		int k;

		xobjdict = ref = fz_dictgetval(dict, i);
		if (xobjdict)
		{
			error = pdf_resolve(&xobjdict, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect image dict (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isdict(xobjdict))
			return fz_throw("not a xobject dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		type = fz_dictgets(xobjdict, "Subtype");
		if (type)
		{
			error = pdf_resolve(&type, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect xobject type (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isname(type))
			return fz_throw("not a xobject type (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (strcmp(fz_toname(type), "Form"))
			return fz_okay;

		subtype = fz_dictgets(xobjdict, "Subtype2");
		if (subtype)
		{
			error = pdf_resolve(&subtype, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect xobject subtype (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (subtype && !fz_isname(subtype))
			return fz_throw("not a xobject subtype (%d %d R)", fz_tonum(ref), fz_togen(ref));
		if (strcmp(fz_toname(type), "PS") &&
				(strcmp(fz_toname(type), "Form") || strcmp(fz_toname(subtype), "PS")))
			return fz_okay;

		for (k = 0; k < psobjs; k++)
			if (fz_tonum(psobj[k]->ref) == fz_tonum(ref) &&
					fz_togen(psobj[k]->ref) == fz_togen(ref))
				break;

		if (k < psobjs)
			return fz_okay;

		psobjs++;

		psobj = realloc(psobj, psobjs * sizeof (struct info *));
		if (!psobj)
			return fz_throw("out of memory");

		psobj[psobjs - 1] = malloc(sizeof (struct info));
		if (!psobj[psobjs - 1])
			return fz_throw("out of memory");

		psobj[psobjs - 1]->page = page;
		psobj[psobjs - 1]->pageref = pageref;
		psobj[psobjs - 1]->ref = ref;
	}

	return fz_okay;
}

fz_error
gathershadings(int page, fz_obj *pageref, fz_obj *pageobj, fz_obj *dict)
{
	fz_error error;
	int i;

	for (i = 0; i < fz_dictlen(dict); i++)
	{
		fz_obj *ref;
		fz_obj *shade;
		fz_obj *type;
		int k;

		shade = ref = fz_dictgetval(dict, i);
		if (shade)
		{
			error = pdf_resolve(&shade, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect shading dict (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isdict(shade))
			return fz_throw("not a shading dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		type = fz_dictgets(shade, "ShadingType");
		if (type)
		{
			error = pdf_resolve(&type, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect shading type (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isint(type) || fz_toint(type) < 1 || fz_toint(type) > 7)
			return fz_throw("not a shading type (%d %d R)", fz_tonum(ref), fz_togen(ref));

		for (k = 0; k < shadings; k++)
			if (fz_tonum(shading[k]->ref) == fz_tonum(ref) &&
					fz_togen(shading[k]->ref) == fz_togen(ref))
				break;

		if (k < shadings)
			return fz_okay;

		shadings++;

		shading = realloc(shading, shadings * sizeof (struct info *));
		if (!shading)
			return fz_throw("out of memory");

		shading[shadings - 1] = malloc(sizeof (struct info));
		if (!shading[shadings - 1])
			return fz_throw("out of memory");

		shading[shadings - 1]->page = page;
		shading[shadings - 1]->pageref = pageref;
		shading[shadings - 1]->ref = ref;
		shading[shadings - 1]->u.shading.type = type;
	}

	return fz_okay;
}

fz_error
gatherpatterns(int page, fz_obj *pageref, fz_obj *pageobj, fz_obj *dict)
{
	fz_error error;
	int i;

	for (i = 0; i < fz_dictlen(dict); i++)
	{
		fz_obj *ref;
		fz_obj *patterndict;
		fz_obj *type;
		fz_obj *paint;
		fz_obj *tiling;
		int k;

		patterndict = ref = fz_dictgetval(dict, i);
		if (patterndict)
		{
			error = pdf_resolve(&patterndict, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect pattern dict (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isdict(patterndict))
			return fz_throw("not a pattern dict (%d %d R)", fz_tonum(ref), fz_togen(ref));

		type = fz_dictgets(patterndict, "PatternType");
		if (type)
		{
			error = pdf_resolve(&type, src);
			if (error)
				return fz_rethrow(error, "cannot resolve indirect pattern type (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		if (!fz_isint(type) || fz_toint(type) < 1 || fz_toint(type) > 2)
			return fz_throw("not a pattern type (%d %d R)", fz_tonum(ref), fz_togen(ref));

		if (fz_toint(type) == 1)
		{
			paint = fz_dictgets(patterndict, "PaintType");
			if (paint)
			{
				error = pdf_resolve(&paint, src);
				if (error)
					return fz_rethrow(error, "cannot resolve indirect pattern paint type (%d %d R)", fz_tonum(ref), fz_togen(ref));
			}
			if (!fz_isint(paint) || fz_toint(paint) < 1 || fz_toint(paint) > 2)
				return fz_throw("not a pattern paint type (%d %d R)", fz_tonum(ref), fz_togen(ref));

			tiling = fz_dictgets(patterndict, "TilingType");
			if (tiling)
			{
				error = pdf_resolve(&tiling, src);
				if (error)
					return fz_rethrow(error, "cannot resolve indirect pattern tiling type (%d %d R)", fz_tonum(ref), fz_togen(ref));
			}
			if (!fz_isint(tiling) || fz_toint(tiling) < 1 || fz_toint(tiling) > 3)
				return fz_throw("not a pattern tiling type (%d %d R)", fz_tonum(ref), fz_togen(ref));
		}
		else
		{
			error = fz_newint(&paint, 0);
			if (error)
				return fz_throw("cannot create fake pattern paint type");
			error = fz_newint(&tiling, 0);
			if (error)
				return fz_throw("cannot create fake pattern tiling type");
		}

		for (k = 0; k < patterns; k++)
			if (fz_tonum(pattern[k]->ref) == fz_tonum(ref) &&
					fz_togen(pattern[k]->ref) == fz_togen(ref))
				break;

		if (k < patterns)
			return fz_okay;

		patterns++;

		pattern = realloc(pattern, patterns * sizeof (struct info *));
		if (!pattern)
			return fz_throw("out of memory");

		pattern[patterns - 1] = malloc(sizeof (struct info));
		if (!pattern[patterns - 1])
			return fz_throw("out of memory");

		pattern[patterns - 1]->page = page;
		pattern[patterns - 1]->pageref = pageref;
		pattern[patterns - 1]->ref = ref;
		pattern[patterns - 1]->u.pattern.pattern = type;
		pattern[patterns - 1]->u.pattern.paint = paint;
		pattern[patterns - 1]->u.pattern.tiling = tiling;
	}

	return fz_okay;
}

void
gatherinfo(int show, int page)
{
	fz_error error;
	fz_obj *pageref;
	fz_obj *pageobj;
	fz_obj *rsrc;
	fz_obj *font;
	fz_obj *xobj;
	fz_obj *shade;
	fz_obj *pattern;

	pageref = pdf_getpagereference(srcpages, page - 1);
	pageobj = pdf_getpageobject(srcpages, page - 1);

	if (!pageref || !pageobj)
		die(fz_throw("cannot retrieve info from page %d", page));

	if (show & DIMENSIONS)
	{
		error = gatherdimensions(page, pageref, pageobj);
		if (error)
			die(fz_rethrow(error, "gathering dimensions at page %d (%d %d R)", page, fz_tonum(pageref), fz_togen(pageref)));
	}

	rsrc = fz_dictgets(pageobj, "Resources");
	if (rsrc)
	{
		error = pdf_resolve(&rsrc, src);
		if (error)
			die(fz_rethrow(error, "retrieving resources at page %d (%d %d R)", page, fz_tonum(pageref), fz_togen(pageref)));
	}

	if (show & FONTS)
	{
		font = fz_dictgets(rsrc, "Font");
		if (font)
		{
			error = pdf_resolve(&font, src);
			if (error)
				die(fz_rethrow(error, "resolving font dict at page %d (%d %d R)", page, fz_tonum(pageref), fz_togen(pageref)));

			error = gatherfonts(page, pageref, pageobj, font);
			if (error)
				die(fz_rethrow(error, "gathering fonts at page %d (%d %d R)", page, fz_tonum(pageref), fz_togen(pageref)));
		}
	}

	if (show & IMAGES || show & XOBJS)
	{
		xobj = fz_dictgets(rsrc, "XObject");
		if (xobj)
		{
			error = pdf_resolve(&xobj, src);
			if (error)
				die(fz_rethrow(error, "resolving xobject dict at page %d (%d %d R)", page, fz_tonum(pageref), fz_togen(pageref)));

			error = gatherimages(page, pageref, pageobj, xobj);
			if (error)
				die(fz_rethrow(error, "gathering images at page %d (%d %d R)", page, fz_tonum(pageref), fz_togen(pageref)));
			error = gatherforms(page, pageref, pageobj, xobj);
			if (error)
				die(fz_rethrow(error, "gathering forms at page %d (%d %d R)", page, fz_tonum(pageref), fz_togen(pageref)));
			error = gatherpsobjs(page, pageref, pageobj, xobj);
			if (error)
				die(fz_rethrow(error, "gathering postscript objects at page %d (%d %d R)", page, fz_tonum(pageref), fz_togen(pageref)));
		}
	}

	if (show & SHADINGS)
	{
		shade = fz_dictgets(rsrc, "Shading");
		if (shade)
		{
			error = pdf_resolve(&shade, src);
			if (error)
				die(fz_rethrow(error, "resolving shading dict at page %d (%d %d R)", page, fz_tonum(pageref), fz_togen(pageref)));

			error = gathershadings(page, pageref, pageobj, shade);
			if (error)
				die(fz_rethrow(error, "gathering shadings at page %d (%d %d R)", page, fz_tonum(pageref), fz_togen(pageref)));
		}
	}

	if (show & PATTERNS)
	{
		pattern = fz_dictgets(rsrc, "Pattern");
		if (pattern)
		{
			error = pdf_resolve(&pattern, src);
			if (error)
				die(fz_rethrow(error, "resolving pattern dict at page %d (%d %d R)", page, fz_tonum(pageref), fz_togen(pageref)));

			error = gathershadings(page, pageref, pageobj, shade);
			if (error)
				die(fz_rethrow(error, "gathering shadings at page %d (%d %d R)", page, fz_tonum(pageref), fz_togen(pageref)));
		}
	}
}

void
printglobalinfo(char *filename)
{
	printf("%s:\n\n", filename);
	printf("PDF-%d.%d\n\n", src->version / 10, src->version % 10);

	if (info->u.info.obj)
	{
		printf("Info object (%d %d R):\n", fz_tonum(info->ref), fz_togen(info->ref));
		fz_debugobj(info->u.info.obj);
	}

	printf("\nPages: %d\n\n", pdf_getpagecount(srcpages));
}

void
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
					fz_tonum(dim[i]->pageref), fz_togen(dim[i]->pageref),
					dim[i]->u.dim.bbox->x0,
					dim[i]->u.dim.bbox->y0,
					dim[i]->u.dim.bbox->x1,
					dim[i]->u.dim.bbox->y1);
		printf("\n");

		for (i = 0; i < dims; i++)
		{
			free(dim[i]->u.dim.bbox);
			free(dim[i]);
		}
		free(dim);
		dim = nil;
		dims = 0;
	}

	if (show & FONTS && fonts > 0)
	{
		printf("Fonts (%d):\n", fonts);
		for (i = 0; i < fonts; i++)
		{
			printf(PAGE_FMT "%s %s (%d %d R)\n",
					font[i]->page,
					fz_tonum(font[i]->pageref), fz_togen(font[i]->pageref),
					fz_toname(font[i]->u.font.subtype),
					fz_toname(font[i]->u.font.name),
					fz_tonum(font[i]->ref), fz_togen(font[i]->ref));
		}
		printf("\n");

		for (i = 0; i < fonts; i++)
			free(font[i]);
		free(font);
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
					fz_tonum(image[i]->pageref), fz_togen(image[i]->pageref));

			if (fz_isarray(image[i]->u.image.filter))
				for (j = 0; j < fz_arraylen(image[i]->u.image.filter); j++)
				{
					printf("%s%s",
							fz_toname(fz_arrayget(image[i]->u.image.filter, j)),
							j == fz_arraylen(image[i]->u.image.filter) - 1 ? "" : " ");
				}
			else
				printf("%s", fz_toname(image[i]->u.image.filter));

			printf(" ] %dx%d %dbpc %s%s%s (%d %d R)\n",
					fz_toint(image[i]->u.image.width),
					fz_toint(image[i]->u.image.height),
					fz_toint(image[i]->u.image.bpc),
					fz_toname(image[i]->u.image.cs),
					image[i]->u.image.altcs ? " " : "",
					image[i]->u.image.altcs ? fz_toname(image[i]->u.image.altcs) : "",
					fz_tonum(image[i]->ref), fz_togen(image[i]->ref));
		}
		printf("\n");

		for (i = 0; i < images; i++)
			free(image[i]);
		free(image);
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
					fz_tonum(shading[i]->pageref), fz_togen(shading[i]->pageref),
					shadingtype[fz_toint(shading[i]->u.shading.type)],
					fz_tonum(shading[i]->ref), fz_togen(shading[i]->ref));
		}
		printf("\n");

		for (i = 0; i < shadings; i++)
			free(shading[i]);
		free(shading);
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
					fz_tonum(pattern[i]->pageref), fz_togen(pattern[i]->pageref),
					patterntype[fz_toint(pattern[i]->u.pattern.pattern)],
					painttype[fz_toint(pattern[i]->u.pattern.paint)],
					tilingtype[fz_toint(pattern[i]->u.pattern.tiling)],
					fz_tonum(pattern[i]->ref), fz_togen(pattern[i]->ref));
		}
		printf("\n");

		for (i = 0; i < patterns; i++)
			free(pattern[i]);
		free(pattern);
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
					fz_tonum(form[i]->pageref), fz_togen(form[i]->pageref),
					form[i]->u.form.group ? "Group" : "",
					form[i]->u.form.reference ? "Reference" : "",
					fz_tonum(form[i]->ref), fz_togen(form[i]->ref));
		}
		printf("\n");

		for (i = 0; i < forms; i++)
			free(form[i]);
		free(form);
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
					fz_tonum(psobj[i]->pageref), fz_togen(psobj[i]->pageref),
					fz_tonum(psobj[i]->ref), fz_togen(psobj[i]->ref));
		}
		printf("\n");

		for (i = 0; i < psobjs; i++)
			free(psobj[i]);
		free(psobj);
		psobj = nil;
		psobjs = 0;
	}
}

void
showinfo(char *filename, int show, char *pagelist)
{
	int page, spage, epage;
	char *spec, *dash;
	int allpages;

	if (!src)
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
				epage = pdf_getpagecount(srcpages);
		}

		if (spage > epage)
			page = spage, spage = epage, epage = page;

		if (spage < 1)
			spage = 1;
		if (epage > pdf_getpagecount(srcpages))
			epage = pdf_getpagecount(srcpages);
		if (spage > pdf_getpagecount(srcpages))
			spage = pdf_getpagecount(srcpages);

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

	while ((c = getopt(argc, argv, "mfispxd:")) != -1)
	{
		switch (c)
		{
			case 'm': if (show == ALL) show = DIMENSIONS; else show |= DIMENSIONS; break;
			case 'f': if (show == ALL) show = FONTS; else show |= FONTS; break;
			case 'i': if (show == ALL) show = IMAGES; else show |= IMAGES; break;
			case 's': if (show == ALL) show = SHADINGS; else show |= SHADINGS; break;
			case 'p': if (show == ALL) show = PATTERNS; else show |= PATTERNS; break;
			case 'x': if (show == ALL) show = XOBJS; else show |= XOBJS; break;
			case 'd': password = optarg; break;
			default:
				  infousage();
				  break;
		}
	}

	if (optind == argc)
		infousage();

	state = NO_FILE_OPENED;
	while (optind < argc)
	{
		if (strstr(argv[optind], ".pdf") || strstr(argv[optind], ".PDF"))
		{
			if (state == NO_INFO_GATHERED)
			{
				printglobalinfo(filename);
				showinfo(filename, show, "1-");
			}

			filename = argv[optind];
			opensrc(filename, password, 1);
			gatherglobalinfo();
			state = NO_INFO_GATHERED;
		}
		else
		{
			if (state == NO_INFO_GATHERED)
			printglobalinfo(filename);
			showinfo(filename, show, argv[optind]);
			state = INFO_SHOWN;
		}

		optind++;
	}

	if (state == NO_INFO_GATHERED)
	{
		printglobalinfo(filename);
		showinfo(filename, show, "1-");
	}

	closesrc();
}

