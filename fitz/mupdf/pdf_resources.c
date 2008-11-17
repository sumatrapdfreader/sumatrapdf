#include "fitz.h"
#include "mupdf.h"

/*

Go through resource dictionary and resolve some levels of
indirect references so we end up with a stylized structure.
The resources referenced are all pre-loaded which inserts
them into the resource store for later lookup when interpreting
content streams.

All resources except colorspaces are automatically inserted
in the resource store when they are parsed. For colorspaces
named in resource dictionaries, we have to insert them ourselves
since we cannot take the risk of having to resolve objects
while in the middle of parsing a content stream.

<<
	/Font <<
		/F0 1 0 R
		/F1 2 0 R
		/F2 3 0 R
	>>
	/ExtGState <<
		/Gs0 << ... /Font 1 0 R ... >>
		/Gs1 << ... >>
	>>
	/ColorSpace <<
		/Cs0 5 0 R
		/Cs1 [ /ICCBased 5 0 R ]
		/Cs2 [ /CalRGB << ... >> ]
		/CsX [ /Pattern /DeviceRGB ]
	>>
	/Pattern <<
		/Pat0 20 0 R
	>>
	/Shading <<
		/Sh0 30 0 R
	>>
	/XObject <<
		/Im0 10 0 R
		/Fm0 11 0 R
	>>
>>

*/

static fz_error *
preloadcolorspace(pdf_xref *xref, fz_obj *ref)
{
	fz_error *error;
	fz_colorspace *colorspace;
	fz_obj *obj = ref;

	if (pdf_finditem(xref->store, PDF_KCOLORSPACE, ref))
		return fz_okay;

	error = pdf_resolve(&obj, xref);
	if (error)
		return fz_rethrow(error, "cannot resolve colorspace resource (%d %d R)", fz_tonum(ref), fz_togen(ref));
	error = pdf_loadcolorspace(&colorspace, xref, obj);
	fz_dropobj(obj);
	if (error)
		return fz_rethrow(error, "cannot load colorspace resource (%d %d R)", fz_tonum(ref), fz_togen(ref));

	pdf_logrsrc("rsrc colorspace %s\n", colorspace->name);

	error = pdf_storeitem(xref->store, PDF_KCOLORSPACE, ref, colorspace);
	fz_dropcolorspace(colorspace); /* we did this just to fill the store, no need to hold on to it */
	if (error)
		return fz_rethrow(error, "cannot store colorspace resource");

	return fz_okay;
}

static fz_error *
preloadpattern(pdf_xref *xref, fz_obj *ref)
{
	fz_error *error;
	pdf_pattern *pattern;
	fz_shade *shade;
	fz_obj *type;
	fz_obj *obj = ref;

	error = pdf_resolve(&obj, xref);
	if (error)
		return fz_rethrow(error, "cannot resolve pattern/shade resource (%d %d R)", fz_tonum(ref), fz_togen(ref));

	type = fz_dictgets(obj, "PatternType");

	if (fz_toint(type) == 1)
	{
		error = pdf_loadpattern(&pattern, xref, obj, ref);
		pdf_droppattern(pattern); /* we did this just to fill the store, no need to hold on to it */
		fz_dropobj(obj);
		if (error)
			return fz_rethrow(error, "cannot load pattern resource (%d %d R)", fz_tonum(ref), fz_togen(ref));
		return fz_okay;
	}

	else if (fz_toint(type) == 2)
	{
		error = pdf_loadshade(&shade, xref, obj, ref);
		fz_dropshade(shade); /* we did this just to fill the store, no need to hold on to it */
		fz_dropobj(obj);
		if (error)
			return fz_rethrow(error, "cannot load shade resource (%d %d R)", fz_tonum(ref), fz_togen(ref));
		return fz_okay;
	}

	else
	{
		fz_dropobj(obj);
		return fz_throw("unknown pattern resource type");
	}
}

static fz_error *
preloadshading(pdf_xref *xref, fz_obj *ref)
{
	fz_error *error;
	fz_shade *shade;

	fz_obj *obj = ref;
	error = pdf_resolve(&obj, xref);
	if (error)
		return fz_rethrow(error, "cannot resolve shade resource (%d %d R)", fz_tonum(ref), fz_togen(ref));

	error = pdf_loadshade(&shade, xref, obj, ref);
	fz_dropshade(shade); /* we did this just to fill the store, no need to hold on to it */
	fz_dropobj(obj);
	if (error)
		return fz_rethrow(error, "cannot load shade resource (%d %d R)", fz_tonum(ref), fz_togen(ref));
	return fz_okay;
}

static fz_error *
preloadxobject(pdf_xref *xref, fz_obj *ref)
{
	fz_error *error;
	pdf_xobject *xobject;
	pdf_image *image;
	fz_obj *obj = ref;
	fz_obj *subtype;

	error = pdf_resolve(&obj, xref);
	if (error)
		return fz_rethrow(error, "cannot resolve xobject/image resource (%d %d R)", fz_tonum(ref), fz_togen(ref));

	subtype = fz_dictgets(obj, "Subtype");

	if (!strcmp(fz_toname(subtype), "Form"))
	{
		error = pdf_loadxobject(&xobject, xref, obj, ref);
		pdf_dropxobject(xobject); /* we did this just to fill the store, no need to hold on to it */
		fz_dropobj(obj);
		if (error)
			return fz_rethrow(error, "cannot load xobject resource (%d %d R)", fz_tonum(ref), fz_togen(ref));
		return fz_okay;
	}

	else if (!strcmp(fz_toname(subtype), "Image"))
	{
		error = pdf_loadimage(&image, xref, obj, ref);
		fz_dropimage((fz_image*)image); /* we did this just to fill the store, no need to hold on to it */
		fz_dropobj(obj);
		if (error)
			return fz_rethrow(error, "cannot load image resource (%d %d R)", fz_tonum(ref), fz_togen(ref));
		return fz_okay;
	}

	else
	{
		fz_dropobj(obj);
		return fz_throw("unknown xobject resource type");
	}
}

static fz_error *
preloadfont(pdf_xref *xref, fz_obj *ref)
{
	fz_error *error;
	pdf_font *font;
	fz_obj *obj = ref;
	error = pdf_resolve(&obj, xref);
	if (error)
		return fz_rethrow(error, "cannot resolve font resource (%d %d R)", fz_tonum(ref), fz_togen(ref));
	error = pdf_loadfont(&font, xref, obj, ref);
	fz_dropfont((fz_font*)font); /* we did this just to fill the store, no need to hold on to it */
	if (error)
		return fz_rethrow(error, "cannot load font resource (%d %d R)", fz_tonum(ref), fz_togen(ref));
	return fz_okay;
}

static fz_error *
preloadmask(pdf_xref *xref, fz_obj *ref)
{
	fz_error *error;
	fz_obj *obj = ref;
	fz_obj *grp;
	error = pdf_resolve(&obj, xref);
	if (error)
		return fz_rethrow(error, "cannot resolve mask dictionary (%d %d R)", fz_tonum(ref), fz_togen(ref));

	if (fz_isdict(obj))
	{
		grp = fz_dictgets(obj, "G");
		if (grp)
		{
		    error = preloadxobject(xref, grp);
		    if (error)
			return fz_rethrow(error, "cannot resolve mask xobject");
		}
	}

	return fz_okay;
}

static fz_error *
scanfontsandmasks(pdf_xref *xref, fz_obj *rdb)
{
	fz_error *error;
	fz_obj *dict;
	fz_obj *obj;
	int i;

	dict = fz_dictgets(rdb, "ExtGState");
	if (dict)
	{
		for (i = 0; i < fz_dictlen(dict); i++)
		{
			obj = fz_dictgetval(dict, i);
			obj = fz_dictgets(obj, "Font");
			if (obj)
			{
				pdf_logrsrc("extgstate font\n");
				error = preloadfont(xref, fz_arrayget(obj, 0));
				if (error)
					return fz_rethrow(error, "cannot preload font listed in ExtGState");
			}

			obj = fz_dictgetval(dict, i);
			obj = fz_dictgets(obj, "SMask");
			if (obj)
			{
				pdf_logrsrc("extgstate smask\n");
				error = preloadmask(xref, obj);
				if (error)
					return fz_rethrow(error, "cannot preload mask listed in ExtGState");
			}
		}
	}

	dict = fz_dictgets(rdb, "Font");
	if (dict)
	{
		for (i = 0; i < fz_dictlen(dict); i++)
		{
			obj = fz_dictgetval(dict, i);
			error = preloadfont(xref, obj);
			if (error)
				return fz_rethrow(error, "cannot preload font resource");
		}
	}

	return fz_okay;
}

static fz_error *
copyresolved(fz_obj **outp, pdf_xref *xref, fz_obj *dict)
{
	fz_error *error;
	fz_obj *key, *val, *obj;
	fz_obj *copy;
	int i;

	error = fz_newdict(&copy, fz_dictlen(dict));
	if (error)
		return fz_rethrow(error, "cannot create dictionary");

	for (i = 0; i < fz_dictlen(dict); i++)
	{
		key = fz_dictgetkey(dict, i);
		val = fz_dictgetval(dict, i);

		if (fz_isindirect(val))
		{
			error = pdf_loadindirect(&obj, xref, val);
			if (error)
			{
				fz_dropobj(copy);
				return fz_rethrow(error, "cannot load object");
			}
			error = fz_dictput(copy, key, obj);
			fz_dropobj(obj);
			if (error)
			{
				fz_dropobj(copy);
				return fz_rethrow(error, "cannot save object");
			}
		}
		else
		{
			error = fz_dictput(copy, key, val);
			if (error)
			{
				fz_dropobj(copy);
				return fz_rethrow(error, "cannot copy object");
			}
		}
	}

	*outp = copy;
	return fz_okay;
}

fz_error *
pdf_loadresources(fz_obj **rdbp, pdf_xref *xref, fz_obj *orig)
{
	fz_error *error;
	fz_obj *copy;
	fz_obj *old;
	fz_obj *new;
	fz_obj *dict;
	fz_obj *obj;
	int i;

	/*
	 * We need a store for resources.
	 */

	if (!xref->store)
	{
		error = pdf_newstore(&xref->store);
		if (error)
			return fz_rethrow(error, "cannot create resource store");
	}

	pdf_logrsrc("load resources {\n");

	/*
	 * Resolve indirect objects
	 */

	error = copyresolved(&copy, xref, orig);
	if (error)
		return fz_rethrow(error, "cannot resolve indirect objects");

	old = fz_dictgets(copy, "ExtGState");
	if (old)
	{
		error = copyresolved(&new, xref, old);
		if (error)
		{
			fz_dropobj(copy);
			return fz_rethrow(error, "cannot resolve indirect objects");
		}
		error = fz_dictputs(copy, "ExtGState", new);
		fz_dropobj(new);
		if (error)
		{
			fz_dropobj(copy);
			return fz_rethrow(error, "cannot copy extgstate");
		}
	}

	/*
	 * Load ColorSpace objects
	 */

	dict = fz_dictgets(copy, "ColorSpace");
	if (dict)
	{
		for (i = 0; i < fz_dictlen(dict); i++)
		{
			obj = fz_dictgetval(dict, i);
			error = preloadcolorspace(xref, obj);
			if (error)
			{
				fz_dropobj(copy);
				return fz_rethrow(error, "cannot load colorspace resource");
			}
		}
	}

	/*
	 * Load Patterns (and Shadings)
	 */

	dict = fz_dictgets(copy, "Pattern");
	if (dict)
	{
		for (i = 0; i < fz_dictlen(dict); i++)
		{
			obj = fz_dictgetval(dict, i);
			error = preloadpattern(xref, obj);
			if (error)
			{
				fz_dropobj(copy);
				return fz_rethrow(error, "cannot load pattern resource");
			}
		}
	}

	dict = fz_dictgets(copy, "Shading");
	if (dict)
	{
		for (i = 0; i < fz_dictlen(dict); i++)
		{
			obj = fz_dictgetval(dict, i);
			error = preloadshading(xref, obj);
			if (error)
			{
				fz_dropobj(copy);
				return fz_rethrow(error, "cannot load shade resource");
			}
		}
	}

	/*
	 * Load XObjects and Images
	 */

	dict = fz_dictgets(copy, "XObject");
	if (dict)
	{
		for (i = 0; i < fz_dictlen(dict); i++)
		{
			obj = fz_dictgetval(dict, i);
			error = preloadxobject(xref, obj);
			if (error)
			{
				fz_dropobj(copy);
				return fz_rethrow(error, "cannot load xobject resource");
			}
		}
	}

	/*
	 * Load Font objects
	 */

	error = scanfontsandmasks(xref, copy);
	if (error)
	{
		fz_dropobj(copy);
		return fz_rethrow(error, "cannot load font resources");
	}

	pdf_logrsrc("}\n");

	*rdbp = copy;
	return fz_okay;
}

