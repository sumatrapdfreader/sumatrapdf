// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

#include "mupdf/fitz.h"
#include "pdf-annot-imp.h"

static void
pdf_run_annot_with_usage(fz_context *ctx, pdf_document *doc, pdf_page *page, pdf_annot *annot, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie)
{
	fz_matrix page_ctm;
	fz_rect mediabox;
	pdf_processor *proc = NULL;
	fz_default_colorspaces *default_cs = NULL;
	int flags;

	fz_var(proc);
	fz_var(default_cs);

	if (cookie && page->super.incomplete)
		cookie->incomplete = 1;

	pdf_annot_push_local_xref(ctx, annot);

	/* Widgets only get displayed if they have both a T and a TF flag,
	 * apparently */
	if (pdf_name_eq(ctx, pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype)), PDF_NAME(Widget)))
	{
		pdf_obj *ft = pdf_dict_get_inheritable(ctx, annot->obj, PDF_NAME(FT));
		pdf_obj *t = pdf_dict_get_inheritable(ctx, annot->obj, PDF_NAME(T));

		if (ft == NULL || t == NULL)
		{
			pdf_annot_pop_local_xref(ctx, annot);
			return;
		}
	}

	fz_try(ctx)
	{
		default_cs = pdf_load_default_colorspaces(ctx, doc, page);
		if (default_cs)
			fz_set_default_colorspaces(ctx, dev, default_cs);

		pdf_page_transform(ctx, page, &mediabox, &page_ctm);

		flags = pdf_dict_get_int(ctx, annot->obj, PDF_NAME(F));
		if (flags & PDF_ANNOT_IS_NO_ROTATE)
		{
			int rotate = pdf_to_int(ctx, pdf_dict_get_inheritable(ctx, page->obj, PDF_NAME(Rotate)));
			fz_rect rect = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
			fz_point tp = fz_transform_point_xy(rect.x0, rect.y1, page_ctm);
			page_ctm = fz_concat(page_ctm, fz_translate(-tp.x, -tp.y));
			page_ctm = fz_concat(page_ctm, fz_rotate(-rotate));
			page_ctm = fz_concat(page_ctm, fz_translate(tp.x, tp.y));
		}

		ctm = fz_concat(page_ctm, ctm);

		proc = pdf_new_run_processor(ctx, dev, ctm, usage, NULL, default_cs, cookie);
		pdf_process_annot(ctx, proc, annot, cookie);
		pdf_close_processor(ctx, proc);
	}
	fz_always(ctx)
	{
		pdf_drop_processor(ctx, proc);
		fz_drop_default_colorspaces(ctx, default_cs);
		pdf_annot_pop_local_xref(ctx, annot);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_run_page_contents_with_usage_imp(fz_context *ctx, pdf_document *doc, pdf_page *page, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie)
{
	fz_matrix page_ctm;
	pdf_obj *resources;
	pdf_obj *contents;
	fz_rect mediabox;
	pdf_processor *proc = NULL;
	fz_default_colorspaces *default_cs = NULL;
	fz_colorspace *colorspace = NULL;

	fz_var(proc);
	fz_var(colorspace);
	fz_var(default_cs);

	if (cookie && page->super.incomplete)
		cookie->incomplete = 1;

	fz_try(ctx)
	{
		default_cs = pdf_load_default_colorspaces(ctx, doc, page);
		if (default_cs)
			fz_set_default_colorspaces(ctx, dev, default_cs);

		pdf_page_transform(ctx, page, &mediabox, &page_ctm);
		ctm = fz_concat(page_ctm, ctm);
		mediabox = fz_transform_rect(mediabox, ctm);

		resources = pdf_page_resources(ctx, page);
		contents = pdf_page_contents(ctx, page);

		if (page->transparency)
		{
			pdf_obj *group = pdf_page_group(ctx, page);

			if (group)
			{
				pdf_obj *cs = pdf_dict_get(ctx, group, PDF_NAME(CS));
				if (cs)
				{
					fz_try(ctx)
						colorspace = pdf_load_colorspace(ctx, cs);
					fz_catch(ctx)
					{
						fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
						fz_warn(ctx, "Ignoring Page blending colorspace.");
					}
					if (!fz_is_valid_blend_colorspace(ctx, colorspace))
					{
						fz_warn(ctx, "Ignoring invalid Page blending colorspace: %s.", colorspace->name);
						fz_drop_colorspace(ctx, colorspace);
						colorspace = NULL;
					}
				}
			}
			else
				colorspace = fz_keep_colorspace(ctx, fz_default_output_intent(ctx, default_cs));

			fz_begin_group(ctx, dev, mediabox, colorspace, 1, 0, 0, 1);
		}

		proc = pdf_new_run_processor(ctx, dev, ctm, usage, NULL, default_cs, cookie);
		pdf_process_contents(ctx, proc, doc, resources, contents, cookie);
		pdf_close_processor(ctx, proc);

		if (page->transparency)
		{
			fz_end_group(ctx, dev);
		}
	}
	fz_always(ctx)
	{
		pdf_drop_processor(ctx, proc);
		fz_drop_colorspace(ctx, colorspace);
		fz_drop_default_colorspaces(ctx, default_cs);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_run_page_contents_with_usage(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie)
{
	pdf_document *doc = page->doc;
	int nocache;

	nocache = !!(dev->hints & FZ_NO_CACHE);
	if (nocache)
		pdf_mark_xref(ctx, doc);

	fz_try(ctx)
	{
		pdf_run_page_contents_with_usage_imp(ctx, doc, page, dev, ctm, usage, cookie);
	}
	fz_always(ctx)
	{
		if (nocache)
			pdf_clear_xref_to_mark(ctx, doc);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_run_page_contents(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie)
{
	pdf_run_page_contents_with_usage(ctx, page, dev, ctm, "View", cookie);
}

void pdf_run_annot(fz_context *ctx, pdf_annot *annot, fz_device *dev, fz_matrix ctm, fz_cookie *cookie)
{
	pdf_page *page = annot->page;
	pdf_document *doc = page->doc;
	int nocache;

	nocache = !!(dev->hints & FZ_NO_CACHE);
	if (nocache)
		pdf_mark_xref(ctx, doc);
	fz_try(ctx)
	{
		pdf_run_annot_with_usage(ctx, doc, page, annot, dev, ctm, "View", cookie);
	}
	fz_always(ctx)
	{
		if (nocache)
			pdf_clear_xref_to_mark(ctx, doc);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
pdf_run_page_widgets_with_usage_imp(fz_context *ctx, pdf_document *doc, pdf_page *page, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie)
{
	pdf_annot *widget;

	if (cookie && cookie->progress_max != (size_t)-1)
	{
		int count = 1;
		for (widget = page->widgets; widget; widget = widget->next)
			count++;
		cookie->progress_max += count;
	}

	for (widget = page->widgets; widget; widget = widget->next)
	{
		/* Check the cookie for aborting */
		if (cookie)
		{
			if (cookie->abort)
				break;
			cookie->progress++;
		}

		pdf_run_annot_with_usage(ctx, doc, page, widget, dev, ctm, usage, cookie);
	}
}

static void
pdf_run_page_annots_with_usage_imp(fz_context *ctx, pdf_document *doc, pdf_page *page, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie)
{
	pdf_annot *annot;

	if (cookie && cookie->progress_max != (size_t)-1)
	{
		int count = 1;
		for (annot = page->annots; annot; annot = annot->next)
			count++;
		cookie->progress_max += count;
	}

	for (annot = page->annots; annot; annot = annot->next)
	{
		/* Check the cookie for aborting */
		if (cookie)
		{
			if (cookie->abort)
				break;
			cookie->progress++;
		}

		pdf_run_annot_with_usage(ctx, doc, page, annot, dev, ctm, usage, cookie);
	}
}

void pdf_run_page_annots_with_usage(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie)
{
	pdf_document *doc = page->doc;
	int nocache;

	nocache = !!(dev->hints & FZ_NO_CACHE);
	if (nocache)
		pdf_mark_xref(ctx, doc);

	fz_try(ctx)
	{
		pdf_run_page_annots_with_usage_imp(ctx, doc, page, dev, ctm, usage, cookie);
	}
	fz_always(ctx)
	{
		if (nocache)
			pdf_clear_xref_to_mark(ctx, doc);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_run_page_annots(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie)
{
	pdf_run_page_annots_with_usage(ctx, page, dev, ctm, "View", cookie);
}

void pdf_run_page_widgets_with_usage(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie)
{
	pdf_document *doc = page->doc;
	int nocache;

	nocache = !!(dev->hints & FZ_NO_CACHE);
	if (nocache)
		pdf_mark_xref(ctx, doc);

	fz_try(ctx)
	{
		pdf_run_page_widgets_with_usage_imp(ctx, doc, page, dev, ctm, usage, cookie);
	}
	fz_always(ctx)
	{
		if (nocache)
			pdf_clear_xref_to_mark(ctx, doc);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_run_page_widgets(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie)
{
	pdf_run_page_widgets_with_usage(ctx, page, dev, ctm, "View", cookie);
}

void
pdf_run_page_with_usage(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie)
{
	pdf_document *doc = page->doc;
	int nocache = !!(dev->hints & FZ_NO_CACHE);

	if (nocache)
		pdf_mark_xref(ctx, doc);
	fz_try(ctx)
	{
		pdf_run_page_contents_with_usage_imp(ctx, doc, page, dev, ctm, usage, cookie);
		pdf_run_page_annots_with_usage_imp(ctx, doc, page, dev, ctm, usage, cookie);
		pdf_run_page_widgets_with_usage_imp(ctx, doc, page, dev, ctm, usage, cookie);
	}
	fz_always(ctx)
	{
		if (nocache)
			pdf_clear_xref_to_mark(ctx, doc);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void
pdf_run_page(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie)
{
	pdf_run_page_with_usage(ctx, page, dev, ctm, "View", cookie);
}

void
pdf_run_glyph(fz_context *ctx, pdf_document *doc, pdf_obj *resources, fz_buffer *contents, fz_device *dev, fz_matrix ctm, void *gstate, fz_default_colorspaces *default_cs)
{
	pdf_processor *proc;

	proc = pdf_new_run_processor(ctx, dev, ctm, "View", gstate, default_cs, NULL);
	fz_try(ctx)
	{
		pdf_process_glyph(ctx, proc, doc, resources, contents);
		pdf_close_processor(ctx, proc);
	}
	fz_always(ctx)
		pdf_drop_processor(ctx, proc);
	fz_catch(ctx)
		fz_rethrow(ctx);
}
