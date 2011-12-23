#include "fitz.h"
#include "mupdf.h"

unsigned int
pdf_cmap_size(pdf_cmap *cmap)
{
	if (cmap == NULL)
		return 0;
	if (cmap->storable.refs < 0)
		return 0;

	return cmap->rcap * sizeof(pdf_range) + cmap->tcap * sizeof(short) + pdf_cmap_size(cmap->usecmap);
}

/*
 * Load CMap stream in PDF file
 */
pdf_cmap *
pdf_load_embedded_cmap(pdf_xref *xref, fz_obj *stmobj)
{
	fz_stream *file = NULL;
	pdf_cmap *cmap = NULL;
	pdf_cmap *usecmap;
	fz_obj *wmode;
	fz_obj *obj;
	fz_context *ctx = xref->ctx;
	int phase = 0;

	fz_var(phase);

	if ((cmap = fz_find_item(ctx, pdf_free_cmap_imp, stmobj)))
	{
		return cmap;
	}

	fz_try(ctx)
	{

		file = pdf_open_stream(xref, fz_to_num(stmobj), fz_to_gen(stmobj));
		phase = 1;
		cmap = pdf_parse_cmap(file);
		phase = 2;
		fz_close(file);
		file = NULL;

		wmode = fz_dict_gets(stmobj, "WMode");
		if (fz_is_int(wmode))
			pdf_set_wmode(cmap, fz_to_int(wmode));
		obj = fz_dict_gets(stmobj, "UseCMap");
		if (fz_is_name(obj))
		{
			usecmap = pdf_load_system_cmap(ctx, fz_to_name(obj));
			pdf_set_usecmap(ctx, cmap, usecmap);
			pdf_drop_cmap(ctx, usecmap);
		}
		else if (fz_is_indirect(obj))
		{
			phase = 3;
			usecmap = pdf_load_embedded_cmap(xref, obj);
			pdf_set_usecmap(ctx, cmap, usecmap);
			pdf_drop_cmap(ctx, usecmap);
		}

		fz_store_item(ctx, stmobj, cmap, pdf_cmap_size(cmap));
	}
	fz_catch(ctx)
	{
		if (file)
			fz_close(file);
		if (cmap)
			pdf_drop_cmap(ctx, cmap);
		if (phase < 1)
			fz_throw(ctx, "cannot open cmap stream (%d %d R)", fz_to_num(stmobj), fz_to_gen(stmobj));
		else if (phase < 2)
			fz_throw(ctx, "cannot parse cmap stream (%d %d R)", fz_to_num(stmobj), fz_to_gen(stmobj));
		else if (phase < 3)
			fz_throw(ctx, "cannot load system usecmap '%s'", fz_to_name(obj));
		else
			fz_throw(ctx, "cannot load embedded usecmap (%d %d R)", fz_to_num(obj), fz_to_gen(obj));
	}

	return cmap;
}

/*
 * Create an Identity-* CMap (for both 1 and 2-byte encodings)
 */
pdf_cmap *
pdf_new_identity_cmap(fz_context *ctx, int wmode, int bytes)
{
	pdf_cmap *cmap = pdf_new_cmap(ctx);
	fz_try(ctx)
	{
		sprintf(cmap->cmap_name, "Identity-%c", wmode ? 'V' : 'H');
		pdf_add_codespace(ctx, cmap, 0x0000, 0xffff, bytes);
		pdf_map_range_to_range(ctx, cmap, 0x0000, 0xffff, 0);
		pdf_sort_cmap(ctx, cmap);
		pdf_set_wmode(cmap, wmode);
	}
	fz_catch(ctx)
	{
		pdf_drop_cmap(ctx, cmap);
		fz_rethrow(ctx);
	}
	return cmap;
}

/*
 * Load predefined CMap from system.
 */
pdf_cmap *
pdf_load_system_cmap(fz_context *ctx, char *cmap_name)
{
	pdf_cmap *usecmap;
	pdf_cmap *cmap;

	cmap = pdf_find_builtin_cmap(cmap_name);
	if (!cmap)
		fz_throw(ctx, "no builtin cmap file: %s", cmap_name);

	if (cmap->usecmap_name[0] && !cmap->usecmap)
	{
		usecmap = pdf_find_builtin_cmap(cmap->usecmap_name);
		if (!usecmap)
			fz_throw(ctx, "nu builtin cmap file: %s", cmap->usecmap_name);
		pdf_set_usecmap(ctx, cmap, usecmap);
	}

	return cmap;
}
