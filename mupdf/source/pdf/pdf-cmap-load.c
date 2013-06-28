#include "mupdf/pdf.h"

unsigned int
pdf_cmap_size(fz_context *ctx, pdf_cmap *cmap)
{
	if (cmap == NULL)
		return 0;
	if (cmap->storable.refs < 0)
		return 0;

	return cmap->rcap * sizeof(pdf_range) + cmap->tcap * sizeof(short) + pdf_cmap_size(ctx, cmap->usecmap);
}

/*
 * Load CMap stream in PDF file
 */
pdf_cmap *
pdf_load_embedded_cmap(pdf_document *doc, pdf_obj *stmobj)
{
	fz_stream *file = NULL;
	pdf_cmap *cmap = NULL;
	pdf_cmap *usecmap;
	pdf_obj *wmode;
	pdf_obj *obj = NULL;
	fz_context *ctx = doc->ctx;
	int phase = 0;

	fz_var(phase);
	fz_var(obj);
	fz_var(file);
	fz_var(cmap);

	if (pdf_obj_marked(stmobj))
		fz_throw(ctx, FZ_ERROR_GENERIC, "Recursion in embedded cmap");

	if ((cmap = pdf_find_item(ctx, pdf_free_cmap_imp, stmobj)))
	{
		return cmap;
	}

	fz_try(ctx)
	{
		file = pdf_open_stream(doc, pdf_to_num(stmobj), pdf_to_gen(stmobj));
		phase = 1;
		cmap = pdf_load_cmap(ctx, file);
		phase = 2;
		fz_close(file);
		file = NULL;

		wmode = pdf_dict_gets(stmobj, "WMode");
		if (pdf_is_int(wmode))
			pdf_set_cmap_wmode(ctx, cmap, pdf_to_int(wmode));
		obj = pdf_dict_gets(stmobj, "UseCMap");
		if (pdf_is_name(obj))
		{
			usecmap = pdf_load_system_cmap(ctx, pdf_to_name(obj));
			pdf_set_usecmap(ctx, cmap, usecmap);
			pdf_drop_cmap(ctx, usecmap);
		}
		else if (pdf_is_indirect(obj))
		{
			phase = 3;
			pdf_mark_obj(obj);
			usecmap = pdf_load_embedded_cmap(doc, obj);
			pdf_unmark_obj(obj);
			phase = 4;
			pdf_set_usecmap(ctx, cmap, usecmap);
			pdf_drop_cmap(ctx, usecmap);
		}

		pdf_store_item(ctx, stmobj, cmap, pdf_cmap_size(ctx, cmap));
	}
	fz_catch(ctx)
	{
		if (file)
			fz_close(file);
		if (cmap)
			pdf_drop_cmap(ctx, cmap);
		if (phase < 1)
			fz_rethrow_message(ctx, "cannot open cmap stream (%d %d R)", pdf_to_num(stmobj), pdf_to_gen(stmobj));
		else if (phase < 2)
			fz_rethrow_message(ctx, "cannot parse cmap stream (%d %d R)", pdf_to_num(stmobj), pdf_to_gen(stmobj));
		else if (phase < 3)
			fz_rethrow_message(ctx, "cannot load system usecmap '%s'", pdf_to_name(obj));
		else
		{
			if (phase == 3)
				pdf_unmark_obj(obj);
			fz_rethrow_message(ctx, "cannot load embedded usecmap (%d %d R)", pdf_to_num(obj), pdf_to_gen(obj));
		}
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
		pdf_set_cmap_wmode(ctx, cmap, wmode);
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

	cmap = pdf_load_builtin_cmap(ctx, cmap_name);
	if (!cmap)
		fz_throw(ctx, FZ_ERROR_GENERIC, "no builtin cmap file: %s", cmap_name);

	if (cmap->usecmap_name[0] && !cmap->usecmap)
	{
		usecmap = pdf_load_builtin_cmap(ctx, cmap->usecmap_name);
		if (!usecmap)
			fz_throw(ctx, FZ_ERROR_GENERIC, "nu builtin cmap file: %s", cmap->usecmap_name);
		pdf_set_usecmap(ctx, cmap, usecmap);
	}

	return cmap;
}
