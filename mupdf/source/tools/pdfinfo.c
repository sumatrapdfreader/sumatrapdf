/*
 * Information tool.
 * Print information about the input pdf.
 */

#include "mupdf/pdf.h"

pdf_document *doc;
fz_context *ctx;
int pagecount;

void closexref(void);

void openxref(char *filename, char *password, int dieonbadpass, int loadpages);

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
	pdf_obj *pageref;
	pdf_obj *pageobj;
	union {
		struct {
			pdf_obj *obj;
		} info;
		struct {
			pdf_obj *obj;
		} crypt;
		struct {
			pdf_obj *obj;
			fz_rect *bbox;
		} dim;
		struct {
			pdf_obj *obj;
			pdf_obj *subtype;
			pdf_obj *name;
		} font;
		struct {
			pdf_obj *obj;
			pdf_obj *width;
			pdf_obj *height;
			pdf_obj *bpc;
			pdf_obj *filter;
			pdf_obj *cs;
			pdf_obj *altcs;
		} image;
		struct {
			pdf_obj *obj;
			pdf_obj *type;
		} shading;
		struct {
			pdf_obj *obj;
			pdf_obj *type;
			pdf_obj *paint;
			pdf_obj *tiling;
			pdf_obj *shading;
		} pattern;
		struct {
			pdf_obj *obj;
			pdf_obj *groupsubtype;
			pdf_obj *reference;
		} form;
	} u;
};

static struct info *dim = NULL;
static int dims = 0;
static struct info *font = NULL;
static int fonts = 0;
static struct info *image = NULL;
static int images = 0;
static struct info *shading = NULL;
static int shadings = 0;
static struct info *pattern = NULL;
static int patterns = 0;
static struct info *form = NULL;
static int forms = 0;
static struct info *psobj = NULL;
static int psobjs = 0;

void closexref(void)
{
	int i;
	if (doc)
	{
		pdf_close_document(doc);
		doc = NULL;
	}

	if (dim)
	{
		for (i = 0; i < dims; i++)
			fz_free(ctx, dim[i].u.dim.bbox);
		fz_free(ctx, dim);
		dim = NULL;
		dims = 0;
	}

	if (font)
	{
		fz_free(ctx, font);
		font = NULL;
		fonts = 0;
	}

	if (image)
	{
		fz_free(ctx, image);
		image = NULL;
		images = 0;
	}

	if (shading)
	{
		fz_free(ctx, shading);
		shading = NULL;
		shadings = 0;
	}

	if (pattern)
	{
		fz_free(ctx, pattern);
		pattern = NULL;
		patterns = 0;
	}

	if (form)
	{
		fz_free(ctx, form);
		form = NULL;
		forms = 0;
	}

	if (psobj)
	{
		fz_free(ctx, psobj);
		psobj = NULL;
		psobjs = 0;
	}
}

static void
infousage(void)
{
	fprintf(stderr,
		"usage: mutool info [options] [file.pdf ... ]\n"
		"\t-d -\tpassword for decryption\n"
		"\t-f\tlist fonts\n"
		"\t-i\tlist images\n"
		"\t-m\tlist dimensions\n"
		"\t-p\tlist patterns\n"
		"\t-s\tlist shadings\n"
		"\t-x\tlist form and postscript xobjects\n");
	exit(1);
}

static void
showglobalinfo(void)
{
	pdf_obj *obj;

	printf("\nPDF-%d.%d\n", doc->version / 10, doc->version % 10);

	obj = pdf_dict_gets(pdf_trailer(doc), "Info");
	if (obj)
	{
		printf("Info object (%d %d R):\n", pdf_to_num(obj), pdf_to_gen(obj));
		pdf_fprint_obj(stdout, pdf_resolve_indirect(obj), 0);
	}

	obj = pdf_dict_gets(pdf_trailer(doc), "Encrypt");
	if (obj)
	{
		printf("\nEncryption object (%d %d R):\n", pdf_to_num(obj), pdf_to_gen(obj));
		pdf_fprint_obj(stdout, pdf_resolve_indirect(obj), 0);
	}

	printf("\nPages: %d\n\n", pagecount);
}

static void
gatherdimensions(int page, pdf_obj *pageref, pdf_obj *pageobj)
{
	fz_rect bbox;
	pdf_obj *obj;
	int j;

	obj = pdf_dict_gets(pageobj, "MediaBox");
	if (!pdf_is_array(obj))
		return;

	pdf_to_rect(ctx, obj, &bbox);

	obj = pdf_dict_gets(pageobj, "UserUnit");
	if (pdf_is_real(obj))
	{
		float unit = pdf_to_real(obj);
		bbox.x0 *= unit;
		bbox.y0 *= unit;
		bbox.x1 *= unit;
		bbox.y1 *= unit;
	}

	for (j = 0; j < dims; j++)
		if (!memcmp(dim[j].u.dim.bbox, &bbox, sizeof (fz_rect)))
			break;

	if (j < dims)
		return;

	dim = fz_resize_array(ctx, dim, dims+1, sizeof(struct info));
	dims++;

	dim[dims - 1].page = page;
	dim[dims - 1].pageref = pageref;
	dim[dims - 1].pageobj = pageobj;
	dim[dims - 1].u.dim.bbox = fz_malloc(ctx, sizeof(fz_rect));
	memcpy(dim[dims - 1].u.dim.bbox, &bbox, sizeof (fz_rect));

	return;
}

static void
gatherfonts(int page, pdf_obj *pageref, pdf_obj *pageobj, pdf_obj *dict)
{
	int i, n;

	n = pdf_dict_len(dict);
	for (i = 0; i < n; i++)
	{
		pdf_obj *fontdict = NULL;
		pdf_obj *subtype = NULL;
		pdf_obj *basefont = NULL;
		pdf_obj *name = NULL;
		int k;

		fontdict = pdf_dict_get_val(dict, i);
		if (!pdf_is_dict(fontdict))
		{
			fz_warn(ctx, "not a font dict (%d %d R)", pdf_to_num(fontdict), pdf_to_gen(fontdict));
			continue;
		}

		subtype = pdf_dict_gets(fontdict, "Subtype");
		basefont = pdf_dict_gets(fontdict, "BaseFont");
		if (!basefont || pdf_is_null(basefont))
			name = pdf_dict_gets(fontdict, "Name");

		for (k = 0; k < fonts; k++)
			if (!pdf_objcmp(font[k].u.font.obj, fontdict))
				break;

		if (k < fonts)
			continue;

		font = fz_resize_array(ctx, font, fonts+1, sizeof(struct info));
		fonts++;

		font[fonts - 1].page = page;
		font[fonts - 1].pageref = pageref;
		font[fonts - 1].pageobj = pageobj;
		font[fonts - 1].u.font.obj = fontdict;
		font[fonts - 1].u.font.subtype = subtype;
		font[fonts - 1].u.font.name = basefont ? basefont : name;
	}
}

static void
gatherimages(int page, pdf_obj *pageref, pdf_obj *pageobj, pdf_obj *dict)
{
	int i, n;

	n = pdf_dict_len(dict);
	for (i = 0; i < n; i++)
	{
		pdf_obj *imagedict;
		pdf_obj *type;
		pdf_obj *width;
		pdf_obj *height;
		pdf_obj *bpc = NULL;
		pdf_obj *filter = NULL;
		pdf_obj *cs = NULL;
		pdf_obj *altcs;
		int k;

		imagedict = pdf_dict_get_val(dict, i);
		if (!pdf_is_dict(imagedict))
		{
			fz_warn(ctx, "not an image dict (%d %d R)", pdf_to_num(imagedict), pdf_to_gen(imagedict));
			continue;
		}

		type = pdf_dict_gets(imagedict, "Subtype");
		if (strcmp(pdf_to_name(type), "Image"))
			continue;

		filter = pdf_dict_gets(imagedict, "Filter");

		altcs = NULL;
		cs = pdf_dict_gets(imagedict, "ColorSpace");
		if (pdf_is_array(cs))
		{
			pdf_obj *cses = cs;

			cs = pdf_array_get(cses, 0);
			if (pdf_is_name(cs) && (!strcmp(pdf_to_name(cs), "DeviceN") || !strcmp(pdf_to_name(cs), "Separation")))
			{
				altcs = pdf_array_get(cses, 2);
				if (pdf_is_array(altcs))
					altcs = pdf_array_get(altcs, 0);
			}
		}

		width = pdf_dict_gets(imagedict, "Width");
		height = pdf_dict_gets(imagedict, "Height");
		bpc = pdf_dict_gets(imagedict, "BitsPerComponent");

		for (k = 0; k < images; k++)
			if (!pdf_objcmp(image[k].u.image.obj, imagedict))
				break;

		if (k < images)
			continue;

		image = fz_resize_array(ctx, image, images+1, sizeof(struct info));
		images++;

		image[images - 1].page = page;
		image[images - 1].pageref = pageref;
		image[images - 1].pageobj = pageobj;
		image[images - 1].u.image.obj = imagedict;
		image[images - 1].u.image.width = width;
		image[images - 1].u.image.height = height;
		image[images - 1].u.image.bpc = bpc;
		image[images - 1].u.image.filter = filter;
		image[images - 1].u.image.cs = cs;
		image[images - 1].u.image.altcs = altcs;
	}
}

static void
gatherforms(int page, pdf_obj *pageref, pdf_obj *pageobj, pdf_obj *dict)
{
	int i, n;

	n = pdf_dict_len(dict);
	for (i = 0; i < n; i++)
	{
		pdf_obj *xobjdict;
		pdf_obj *type;
		pdf_obj *subtype;
		pdf_obj *group;
		pdf_obj *groupsubtype;
		pdf_obj *reference;
		int k;

		xobjdict = pdf_dict_get_val(dict, i);
		if (!pdf_is_dict(xobjdict))
		{
			fz_warn(ctx, "not a xobject dict (%d %d R)", pdf_to_num(xobjdict), pdf_to_gen(xobjdict));
			continue;
		}

		type = pdf_dict_gets(xobjdict, "Subtype");
		if (strcmp(pdf_to_name(type), "Form"))
			continue;

		subtype = pdf_dict_gets(xobjdict, "Subtype2");
		if (!strcmp(pdf_to_name(subtype), "PS"))
			continue;

		group = pdf_dict_gets(xobjdict, "Group");
		groupsubtype = pdf_dict_gets(group, "S");
		reference = pdf_dict_gets(xobjdict, "Ref");

		for (k = 0; k < forms; k++)
			if (!pdf_objcmp(form[k].u.form.obj, xobjdict))
				break;

		if (k < forms)
			continue;

		form = fz_resize_array(ctx, form, forms+1, sizeof(struct info));
		forms++;

		form[forms - 1].page = page;
		form[forms - 1].pageref = pageref;
		form[forms - 1].pageobj = pageobj;
		form[forms - 1].u.form.obj = xobjdict;
		form[forms - 1].u.form.groupsubtype = groupsubtype;
		form[forms - 1].u.form.reference = reference;
	}
}

static void
gatherpsobjs(int page, pdf_obj *pageref, pdf_obj *pageobj, pdf_obj *dict)
{
	int i, n;

	n = pdf_dict_len(dict);
	for (i = 0; i < n; i++)
	{
		pdf_obj *xobjdict;
		pdf_obj *type;
		pdf_obj *subtype;
		int k;

		xobjdict = pdf_dict_get_val(dict, i);
		if (!pdf_is_dict(xobjdict))
		{
			fz_warn(ctx, "not a xobject dict (%d %d R)", pdf_to_num(xobjdict), pdf_to_gen(xobjdict));
			continue;
		}

		type = pdf_dict_gets(xobjdict, "Subtype");
		subtype = pdf_dict_gets(xobjdict, "Subtype2");
		if (strcmp(pdf_to_name(type), "PS") &&
			(strcmp(pdf_to_name(type), "Form") || strcmp(pdf_to_name(subtype), "PS")))
			continue;

		for (k = 0; k < psobjs; k++)
			if (!pdf_objcmp(psobj[k].u.form.obj, xobjdict))
				break;

		if (k < psobjs)
			continue;

		psobj = fz_resize_array(ctx, psobj, psobjs+1, sizeof(struct info));
		psobjs++;

		psobj[psobjs - 1].page = page;
		psobj[psobjs - 1].pageref = pageref;
		psobj[psobjs - 1].pageobj = pageobj;
		psobj[psobjs - 1].u.form.obj = xobjdict;
	}
}

static void
gathershadings(int page, pdf_obj *pageref, pdf_obj *pageobj, pdf_obj *dict)
{
	int i, n;

	n = pdf_dict_len(dict);
	for (i = 0; i < n; i++)
	{
		pdf_obj *shade;
		pdf_obj *type;
		int k;

		shade = pdf_dict_get_val(dict, i);
		if (!pdf_is_dict(shade))
		{
			fz_warn(ctx, "not a shading dict (%d %d R)", pdf_to_num(shade), pdf_to_gen(shade));
			continue;
		}

		type = pdf_dict_gets(shade, "ShadingType");
		if (!pdf_is_int(type) || pdf_to_int(type) < 1 || pdf_to_int(type) > 7)
		{
			fz_warn(ctx, "not a shading type (%d %d R)", pdf_to_num(shade), pdf_to_gen(shade));
			type = NULL;
		}

		for (k = 0; k < shadings; k++)
			if (!pdf_objcmp(shading[k].u.shading.obj, shade))
				break;

		if (k < shadings)
			continue;

		shading = fz_resize_array(ctx, shading, shadings+1, sizeof(struct info));
		shadings++;

		shading[shadings - 1].page = page;
		shading[shadings - 1].pageref = pageref;
		shading[shadings - 1].pageobj = pageobj;
		shading[shadings - 1].u.shading.obj = shade;
		shading[shadings - 1].u.shading.type = type;
	}
}

static void
gatherpatterns(int page, pdf_obj *pageref, pdf_obj *pageobj, pdf_obj *dict)
{
	int i, n;

	n = pdf_dict_len(dict);
	for (i = 0; i < n; i++)
	{
		pdf_obj *patterndict;
		pdf_obj *type;
		pdf_obj *paint = NULL;
		pdf_obj *tiling = NULL;
		pdf_obj *shading = NULL;
		int k;

		patterndict = pdf_dict_get_val(dict, i);
		if (!pdf_is_dict(patterndict))
		{
			fz_warn(ctx, "not a pattern dict (%d %d R)", pdf_to_num(patterndict), pdf_to_gen(patterndict));
			continue;
		}

		type = pdf_dict_gets(patterndict, "PatternType");
		if (!pdf_is_int(type) || pdf_to_int(type) < 1 || pdf_to_int(type) > 2)
		{
			fz_warn(ctx, "not a pattern type (%d %d R)", pdf_to_num(patterndict), pdf_to_gen(patterndict));
			type = NULL;
		}

		if (pdf_to_int(type) == 1)
		{
			paint = pdf_dict_gets(patterndict, "PaintType");
			if (!pdf_is_int(paint) || pdf_to_int(paint) < 1 || pdf_to_int(paint) > 2)
			{
				fz_warn(ctx, "not a pattern paint type (%d %d R)", pdf_to_num(patterndict), pdf_to_gen(patterndict));
				paint = NULL;
			}

			tiling = pdf_dict_gets(patterndict, "TilingType");
			if (!pdf_is_int(tiling) || pdf_to_int(tiling) < 1 || pdf_to_int(tiling) > 3)
			{
				fz_warn(ctx, "not a pattern tiling type (%d %d R)", pdf_to_num(patterndict), pdf_to_gen(patterndict));
				tiling = NULL;
			}
		}
		else
		{
			shading = pdf_dict_gets(patterndict, "Shading");
		}

		for (k = 0; k < patterns; k++)
			if (!pdf_objcmp(pattern[k].u.pattern.obj, patterndict))
				break;

		if (k < patterns)
			continue;

		pattern = fz_resize_array(ctx, pattern, patterns+1, sizeof(struct info));
		patterns++;

		pattern[patterns - 1].page = page;
		pattern[patterns - 1].pageref = pageref;
		pattern[patterns - 1].pageobj = pageobj;
		pattern[patterns - 1].u.pattern.obj = patterndict;
		pattern[patterns - 1].u.pattern.type = type;
		pattern[patterns - 1].u.pattern.paint = paint;
		pattern[patterns - 1].u.pattern.tiling = tiling;
		pattern[patterns - 1].u.pattern.shading = shading;
	}
}

static void
gatherresourceinfo(int page, pdf_obj *rsrc, int show)
{
	pdf_obj *pageobj;
	pdf_obj *pageref;
	pdf_obj *font;
	pdf_obj *xobj;
	pdf_obj *shade;
	pdf_obj *pattern;
	pdf_obj *subrsrc;
	int i;

	pageref = pdf_lookup_page_obj(doc, page-1);
	pageobj = pdf_resolve_indirect(pageref);

	if (!pageobj)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot retrieve info from page %d", page);

	font = pdf_dict_gets(rsrc, "Font");
	if (show & FONTS && font)
	{
		int n;

		gatherfonts(page, pageref, pageobj, font);
		n = pdf_dict_len(font);
		for (i = 0; i < n; i++)
		{
			pdf_obj *obj = pdf_dict_get_val(font, i);

			subrsrc = pdf_dict_gets(obj, "Resources");
			if (subrsrc && pdf_objcmp(rsrc, subrsrc))
				gatherresourceinfo(page, subrsrc, show);
		}
	}

	xobj = pdf_dict_gets(rsrc, "XObject");
	if (show & XOBJS && xobj)
	{
		int n;

		gatherimages(page, pageref, pageobj, xobj);
		gatherforms(page, pageref, pageobj, xobj);
		gatherpsobjs(page, pageref, pageobj, xobj);
		n = pdf_dict_len(xobj);
		for (i = 0; i < n; i++)
		{
			pdf_obj *obj = pdf_dict_get_val(xobj, i);
			subrsrc = pdf_dict_gets(obj, "Resources");
			if (subrsrc && pdf_objcmp(rsrc, subrsrc))
				gatherresourceinfo(page, subrsrc, show);
		}
	}

	shade = pdf_dict_gets(rsrc, "Shading");
	if (show & SHADINGS && shade)
		gathershadings(page, pageref, pageobj, shade);

	pattern = pdf_dict_gets(rsrc, "Pattern");
	if (show & PATTERNS && pattern)
	{
		int n;
		gatherpatterns(page, pageref, pageobj, pattern);
		n = pdf_dict_len(pattern);
		for (i = 0; i < n; i++)
		{
			pdf_obj *obj = pdf_dict_get_val(pattern, i);
			subrsrc = pdf_dict_gets(obj, "Resources");
			if (subrsrc && pdf_objcmp(rsrc, subrsrc))
				gatherresourceinfo(page, subrsrc, show);
		}
	}
}

static void
gatherpageinfo(int page, int show)
{
	pdf_obj *pageobj;
	pdf_obj *pageref;
	pdf_obj *rsrc;

	pageref = pdf_lookup_page_obj(doc, page-1);
	pageobj = pdf_resolve_indirect(pageref);

	if (!pageobj)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot retrieve info from page %d", page);

	gatherdimensions(page, pageref, pageobj);

	rsrc = pdf_dict_gets(pageobj, "Resources");
	gatherresourceinfo(page, rsrc, show);
}

static void
printinfo(char *filename, int show, int page)
{
	int i;
	int j;

#define PAGE_FMT "\t% 5d (% 7d %1d R): "

	if (show & DIMENSIONS && dims > 0)
	{
		printf("Mediaboxes (%d):\n", dims);
		for (i = 0; i < dims; i++)
		{
			printf(PAGE_FMT "[ %g %g %g %g ]\n",
				dim[i].page,
				pdf_to_num(dim[i].pageref), pdf_to_gen(dim[i].pageref),
				dim[i].u.dim.bbox->x0,
				dim[i].u.dim.bbox->y0,
				dim[i].u.dim.bbox->x1,
				dim[i].u.dim.bbox->y1);
		}
		printf("\n");
	}

	if (show & FONTS && fonts > 0)
	{
		printf("Fonts (%d):\n", fonts);
		for (i = 0; i < fonts; i++)
		{
			printf(PAGE_FMT "%s '%s' (%d %d R)\n",
				font[i].page,
				pdf_to_num(font[i].pageref), pdf_to_gen(font[i].pageref),
				pdf_to_name(font[i].u.font.subtype),
				pdf_to_name(font[i].u.font.name),
				pdf_to_num(font[i].u.font.obj), pdf_to_gen(font[i].u.font.obj));
		}
		printf("\n");
	}

	if (show & IMAGES && images > 0)
	{
		printf("Images (%d):\n", images);
		for (i = 0; i < images; i++)
		{
			char *cs = NULL;
			char *altcs = NULL;

			printf(PAGE_FMT "[ ",
				image[i].page,
				pdf_to_num(image[i].pageref), pdf_to_gen(image[i].pageref));

			if (pdf_is_array(image[i].u.image.filter))
			{
				int n = pdf_array_len(image[i].u.image.filter);
				for (j = 0; j < n; j++)
				{
					pdf_obj *obj = pdf_array_get(image[i].u.image.filter, j);
					char *filter = fz_strdup(ctx, pdf_to_name(obj));

					if (strstr(filter, "Decode"))
						*(strstr(filter, "Decode")) = '\0';

					printf("%s%s",
							filter,
							j == pdf_array_len(image[i].u.image.filter) - 1 ? "" : " ");
					fz_free(ctx, filter);
				}
			}
			else if (image[i].u.image.filter)
			{
				pdf_obj *obj = image[i].u.image.filter;
				char *filter = fz_strdup(ctx, pdf_to_name(obj));

				if (strstr(filter, "Decode"))
					*(strstr(filter, "Decode")) = '\0';

				printf("%s", filter);
				fz_free(ctx, filter);
			}
			else
				printf("Raw");

			if (image[i].u.image.cs)
			{
				cs = fz_strdup(ctx, pdf_to_name(image[i].u.image.cs));

				if (!strncmp(cs, "Device", 6))
				{
					int len = strlen(cs + 6);
					memmove(cs + 3, cs + 6, len + 1);
					cs[3 + len + 1] = '\0';
				}
				if (strstr(cs, "ICC"))
					fz_strlcpy(cs, "ICC", 4);
				if (strstr(cs, "Indexed"))
					fz_strlcpy(cs, "Idx", 4);
				if (strstr(cs, "Pattern"))
					fz_strlcpy(cs, "Pat", 4);
				if (strstr(cs, "Separation"))
					fz_strlcpy(cs, "Sep", 4);
			}
			if (image[i].u.image.altcs)
			{
				altcs = fz_strdup(ctx, pdf_to_name(image[i].u.image.altcs));

				if (!strncmp(altcs, "Device", 6))
				{
					int len = strlen(altcs + 6);
					memmove(altcs + 3, altcs + 6, len + 1);
					altcs[3 + len + 1] = '\0';
				}
				if (strstr(altcs, "ICC"))
					fz_strlcpy(altcs, "ICC", 4);
				if (strstr(altcs, "Indexed"))
					fz_strlcpy(altcs, "Idx", 4);
				if (strstr(altcs, "Pattern"))
					fz_strlcpy(altcs, "Pat", 4);
				if (strstr(altcs, "Separation"))
					fz_strlcpy(altcs, "Sep", 4);
			}

			printf(" ] %dx%d %dbpc %s%s%s (%d %d R)\n",
				pdf_to_int(image[i].u.image.width),
				pdf_to_int(image[i].u.image.height),
				image[i].u.image.bpc ? pdf_to_int(image[i].u.image.bpc) : 1,
				image[i].u.image.cs ? cs : "ImageMask",
				image[i].u.image.altcs ? " " : "",
				image[i].u.image.altcs ? altcs : "",
				pdf_to_num(image[i].u.image.obj), pdf_to_gen(image[i].u.image.obj));

			fz_free(ctx, cs);
			fz_free(ctx, altcs);
		}
		printf("\n");
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
				"Triangle mesh",
				"Lattice",
				"Coons patch",
				"Tensor patch",
			};

			printf(PAGE_FMT "%s (%d %d R)\n",
				shading[i].page,
				pdf_to_num(shading[i].pageref), pdf_to_gen(shading[i].pageref),
				shadingtype[pdf_to_int(shading[i].u.shading.type)],
				pdf_to_num(shading[i].u.shading.obj), pdf_to_gen(shading[i].u.shading.obj));
		}
		printf("\n");
	}

	if (show & PATTERNS && patterns > 0)
	{
		printf("Patterns (%d):\n", patterns);
		for (i = 0; i < patterns; i++)
		{
			if (pdf_to_int(pattern[i].u.pattern.type) == 1)
			{
				char *painttype[] =
				{
					"",
					"Colored",
					"Uncolored",
				};
				char *tilingtype[] =
				{
					"",
					"Constant",
					"No distortion",
					"Constant/fast tiling",
				};

				printf(PAGE_FMT "Tiling %s %s (%d %d R)\n",
						pattern[i].page,
						pdf_to_num(pattern[i].pageref), pdf_to_gen(pattern[i].pageref),
						painttype[pdf_to_int(pattern[i].u.pattern.paint)],
						tilingtype[pdf_to_int(pattern[i].u.pattern.tiling)],
						pdf_to_num(pattern[i].u.pattern.obj), pdf_to_gen(pattern[i].u.pattern.obj));
			}
			else
			{
				printf(PAGE_FMT "Shading %d %d R (%d %d R)\n",
						pattern[i].page,
						pdf_to_num(pattern[i].pageref), pdf_to_gen(pattern[i].pageref),
						pdf_to_num(pattern[i].u.pattern.shading), pdf_to_gen(pattern[i].u.pattern.shading),
						pdf_to_num(pattern[i].u.pattern.obj), pdf_to_gen(pattern[i].u.pattern.obj));
			}
		}
		printf("\n");
	}

	if (show & XOBJS && forms > 0)
	{
		printf("Form xobjects (%d):\n", forms);
		for (i = 0; i < forms; i++)
		{
			printf(PAGE_FMT "Form%s%s%s%s (%d %d R)\n",
				form[i].page,
				pdf_to_num(form[i].pageref), pdf_to_gen(form[i].pageref),
				form[i].u.form.groupsubtype ? " " : "",
				form[i].u.form.groupsubtype ? pdf_to_name(form[i].u.form.groupsubtype) : "",
				form[i].u.form.groupsubtype ? " Group" : "",
				form[i].u.form.reference ? " Reference" : "",
				pdf_to_num(form[i].u.form.obj), pdf_to_gen(form[i].u.form.obj));
		}
		printf("\n");
	}

	if (show & XOBJS && psobjs > 0)
	{
		printf("Postscript xobjects (%d):\n", psobjs);
		for (i = 0; i < psobjs; i++)
		{
			printf(PAGE_FMT "(%d %d R)\n",
				psobj[i].page,
				pdf_to_num(psobj[i].pageref), pdf_to_gen(psobj[i].pageref),
				pdf_to_num(psobj[i].u.form.obj), pdf_to_gen(psobj[i].u.form.obj));
		}
		printf("\n");
	}
}

static void
showinfo(char *filename, int show, char *pagelist)
{
	int page, spage, epage;
	char *spec, *dash;
	int allpages;
	int pagecount;

	if (!doc)
		infousage();

	allpages = !strcmp(pagelist, "1-");

	pagecount = pdf_count_pages(doc);
	spec = fz_strsep(&pagelist, ",");
	while (spec && pagecount)
	{
		dash = strchr(spec, '-');

		if (dash == spec)
			spage = epage = pagecount;
		else
			spage = epage = atoi(spec);

		if (dash)
		{
			if (strlen(dash) > 1)
				epage = atoi(dash + 1);
			else
				epage = pagecount;
		}

		if (spage > epage)
			page = spage, spage = epage, epage = page;

		spage = fz_clampi(spage, 1, pagecount);
		epage = fz_clampi(epage, 1, pagecount);

		if (allpages)
			printf("Retrieving info from pages %d-%d...\n", spage, epage);
		for (page = spage; page <= epage; page++)
		{
			gatherpageinfo(page, show);
			if (!allpages)
			{
				printf("Page %d:\n", page);
				printinfo(filename, show, page);
				printf("\n");
			}
		}

		spec = fz_strsep(&pagelist, ",");
	}

	if (allpages)
		printinfo(filename, show, -1);
}

static int arg_is_page_range(const char *arg)
{
	int c;

	while ((c = *arg++) != 0)
	{
		if ((c < '0' || c > '9') && (c != '-') && (c != ','))
			return 0;
	}
	return 1;
}

int pdfinfo_main(int argc, char **argv)
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

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	state = NO_FILE_OPENED;
	while (fz_optind < argc)
	{
		if (state == NO_FILE_OPENED || !arg_is_page_range(argv[fz_optind]))
		{
			if (state == NO_INFO_GATHERED)
			{
				showinfo(filename, show, "1-");
			}

			closexref();

			filename = argv[fz_optind];
			printf("%s:\n", filename);
			doc = pdf_open_document_no_run(ctx, filename);
			if (pdf_needs_password(doc))
				if (!pdf_authenticate_password(doc, password))
					fz_throw(ctx, FZ_ERROR_GENERIC, "cannot authenticate password: %s", filename);
			pagecount = pdf_count_pages(doc);

			showglobalinfo();
			state = NO_INFO_GATHERED;
		}
		else
		{
			showinfo(filename, show, argv[fz_optind]);
			state = INFO_SHOWN;
		}

		fz_optind++;
	}

	if (state == NO_INFO_GATHERED)
		showinfo(filename, show, "1-");

	closexref();
	fz_free_context(ctx);
	return 0;
}
