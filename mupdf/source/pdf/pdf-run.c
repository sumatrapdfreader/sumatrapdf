// Copyright (C) 2004-2025 Artifex Software, Inc.
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
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"
#include "pdf-annot-imp.h"

static void
pdf_run_annot_with_usage(fz_context *ctx, pdf_document *doc, pdf_page *page, pdf_annot *annot, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie)
{
	fz_matrix page_ctm;
	fz_rect mediabox;
	pdf_processor *proc = NULL;
	fz_default_colorspaces *default_cs = NULL;
	int resources_pushed = 0;
	int struct_parent_num;
	pdf_obj *struct_parent;

	fz_var(proc);
	fz_var(default_cs);
	fz_var(resources_pushed);

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

		ctm = fz_concat(page_ctm, ctm);

		struct_parent = pdf_dict_getl(ctx, page->obj, PDF_NAME(StructParent), NULL);
		struct_parent_num = pdf_to_int_default(ctx, struct_parent, -1);

		proc = pdf_new_run_processor(ctx, page->doc, dev, ctm, struct_parent_num, usage, NULL, default_cs, cookie, NULL, NULL);
		pdf_processor_push_resources(ctx, proc, pdf_page_resources(ctx, annot->page));
		resources_pushed = 1;
		pdf_process_annot(ctx, proc, annot, cookie);
		pdf_close_processor(ctx, proc);
	}
	fz_always(ctx)
	{
		if (resources_pushed)
			pdf_drop_obj(ctx, pdf_processor_pop_resources(ctx, proc));
		pdf_drop_processor(ctx, proc);
		fz_drop_default_colorspaces(ctx, default_cs);
		pdf_annot_pop_local_xref(ctx, annot);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static fz_rect pdf_page_cropbox(fz_context *ctx, pdf_page *page)
{
	pdf_obj *obj = pdf_dict_get_inheritable(ctx, page->obj, PDF_NAME(CropBox));
	if (!obj)
		obj = pdf_dict_get_inheritable(ctx, page->obj, PDF_NAME(MediaBox));
	return pdf_to_rect(ctx, obj);
}

static fz_rect pdf_page_mediabox(fz_context *ctx, pdf_page *page)
{
	return pdf_dict_get_inheritable_rect(ctx, page->obj, PDF_NAME(MediaBox));
}

static void
pdf_run_page_contents_with_usage_imp(fz_context *ctx, pdf_document *doc, pdf_page *page, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie)
{
	fz_matrix page_ctm;
	pdf_obj *resources;
	pdf_obj *contents;
	fz_rect fitzbox;
	fz_rect mediabox, cropbox;
	pdf_processor *proc = NULL;
	fz_default_colorspaces *default_cs = NULL;
	fz_colorspace *colorspace = NULL;
	fz_path *path = NULL;
	int struct_parent_num;
	pdf_obj *struct_parent;

	fz_var(proc);
	fz_var(colorspace);
	fz_var(default_cs);
	fz_var(path);

	if (cookie && page->super.incomplete)
		cookie->incomplete = 1;

	fz_try(ctx)
	{
		default_cs = pdf_load_default_colorspaces(ctx, doc, page);
		if (default_cs)
			fz_set_default_colorspaces(ctx, dev, default_cs);

		pdf_page_transform(ctx, page, &fitzbox, &page_ctm);
		ctm = fz_concat(page_ctm, ctm);
		fitzbox = fz_transform_rect(fitzbox, ctm);

		resources = pdf_page_resources(ctx, page);
		contents = pdf_page_contents(ctx, page);

		mediabox = pdf_page_mediabox(ctx, page);
		cropbox = pdf_page_cropbox(ctx, page);

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
						fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
						fz_report_error(ctx);
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

			fz_begin_group(ctx, dev, fitzbox, colorspace, 1, 0, 0, 1);
		}

		struct_parent = pdf_dict_get(ctx, page->obj, PDF_NAME(StructParents));
		struct_parent_num = pdf_to_int_default(ctx, struct_parent, -1);

		/* Clip content to CropBox if it is smaller than the MediaBox */
		if (cropbox.x0 > mediabox.x0 || cropbox.x1 < mediabox.x1 || cropbox.y0 > mediabox.y0 || cropbox.y1 < mediabox.y1)
		{
			path = fz_new_path(ctx);
			fz_rectto(ctx, path, cropbox.x0, cropbox.y0, cropbox.x1, cropbox.y1);
			fz_clip_path(ctx, dev, path, 1, ctm, fz_infinite_rect);
		}

		proc = pdf_new_run_processor(ctx, page->doc, dev, ctm, struct_parent_num, usage, NULL, default_cs, cookie, NULL, NULL);
		pdf_process_contents(ctx, proc, doc, resources, contents, cookie, NULL);
		pdf_close_processor(ctx, proc);

		if (cropbox.x0 > mediabox.x0 || cropbox.x1 < mediabox.x1 || cropbox.y0 > mediabox.y0 || cropbox.y1 < mediabox.y1)
		{
			fz_pop_clip(ctx, dev);
		}

		if (page->transparency)
		{
			fz_end_group(ctx, dev);
		}
	}
	fz_always(ctx)
	{
		fz_drop_path(ctx, path);
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
	pdf_document *doc;
	int nocache;

	if (!page)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "annotation not bound to any page");

	pdf_update_page(ctx, page);

	doc = page->doc;

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

	pdf_update_page(ctx, page);

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

	pdf_update_page(ctx, page);

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

	pdf_update_page(ctx, page);

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
pdf_run_glyph(fz_context *ctx, pdf_document *doc, pdf_obj *resources, fz_buffer *contents, fz_device *dev, fz_matrix ctm, void *gstate, fz_default_colorspaces *default_cs, void *fill_gstate, void *stroke_gstate)
{
	pdf_processor *proc;

	proc = pdf_new_run_processor(ctx, doc, dev, ctm, -1, "View", gstate, default_cs, NULL, fill_gstate, stroke_gstate);
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

fz_structure
pdf_structure_type(fz_context *ctx, pdf_obj *role_map, pdf_obj *tag)
{
	/* Perform Structure mapping to go from tag to standard. */
	if (role_map)
	{
		pdf_obj *o = pdf_dict_get(ctx, role_map, tag);
		if (o)
			tag = o;
	}

	if (pdf_name_eq(ctx, tag, PDF_NAME(Document)))
		return FZ_STRUCTURE_DOCUMENT;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Part)))
		return FZ_STRUCTURE_PART;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Art)))
		return FZ_STRUCTURE_ART;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Sect)))
		return FZ_STRUCTURE_SECT;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Div)))
		return FZ_STRUCTURE_DIV;
	if (pdf_name_eq(ctx, tag, PDF_NAME(BlockQuote)))
		return FZ_STRUCTURE_BLOCKQUOTE;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Caption)))
		return FZ_STRUCTURE_CAPTION;
	if (pdf_name_eq(ctx, tag, PDF_NAME(TOC)))
		return FZ_STRUCTURE_TOC;
	if (pdf_name_eq(ctx, tag, PDF_NAME(TOCI)))
		return FZ_STRUCTURE_TOCI;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Index)))
		return FZ_STRUCTURE_INDEX;
	if (pdf_name_eq(ctx, tag, PDF_NAME(NonStruct)))
		return FZ_STRUCTURE_NONSTRUCT;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Private)))
		return FZ_STRUCTURE_PRIVATE;
	/* Grouping elements (PDF 2.0 - Table 364) */
	if (pdf_name_eq(ctx, tag, PDF_NAME(DocumentFragment)))
		return FZ_STRUCTURE_DOCUMENTFRAGMENT;
	/* Grouping elements (PDF 2.0 - Table 365) */
	if (pdf_name_eq(ctx, tag, PDF_NAME(Aside)))
		return FZ_STRUCTURE_ASIDE;
	/* Grouping elements (PDF 2.0 - Table 366) */
	if (pdf_name_eq(ctx, tag, PDF_NAME(Title)))
		return FZ_STRUCTURE_TITLE;
	if (pdf_name_eq(ctx, tag, PDF_NAME(FENote)))
		return FZ_STRUCTURE_FENOTE;
	/* Grouping elements (PDF 2.0 - Table 367) */
	if (pdf_name_eq(ctx, tag, PDF_NAME(Sub)))
		return FZ_STRUCTURE_SUB;

	/* Paragraphlike elements (PDF 1.7 - Table 10.21) */
	if (pdf_name_eq(ctx, tag, PDF_NAME(P)))
		return FZ_STRUCTURE_P;
	if (pdf_name_eq(ctx, tag, PDF_NAME(H)))
		return FZ_STRUCTURE_H;
	if (pdf_name_eq(ctx, tag, PDF_NAME(H1)))
		return FZ_STRUCTURE_H1;
	if (pdf_name_eq(ctx, tag, PDF_NAME(H2)))
		return FZ_STRUCTURE_H2;
	if (pdf_name_eq(ctx, tag, PDF_NAME(H3)))
		return FZ_STRUCTURE_H3;
	if (pdf_name_eq(ctx, tag, PDF_NAME(H4)))
		return FZ_STRUCTURE_H4;
	if (pdf_name_eq(ctx, tag, PDF_NAME(H5)))
		return FZ_STRUCTURE_H5;
	if (pdf_name_eq(ctx, tag, PDF_NAME(H6)))
		return FZ_STRUCTURE_H6;

	/* List elements (PDF 1.7 - Table 10.23) */
	if (pdf_name_eq(ctx, tag, PDF_NAME(L)))
		return FZ_STRUCTURE_LIST;
	if (pdf_name_eq(ctx, tag, PDF_NAME(LI)))
		return FZ_STRUCTURE_LISTITEM;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Lbl)))
		return FZ_STRUCTURE_LABEL;
	if (pdf_name_eq(ctx, tag, PDF_NAME(LBody)))
		return FZ_STRUCTURE_LISTBODY;

	/* Table elements (PDF 1.7 - Table 10.24) */
	if (pdf_name_eq(ctx, tag, PDF_NAME(Table)))
		return FZ_STRUCTURE_TABLE;
	if (pdf_name_eq(ctx, tag, PDF_NAME(TR)))
		return FZ_STRUCTURE_TR;
	if (pdf_name_eq(ctx, tag, PDF_NAME(TH)))
		return FZ_STRUCTURE_TH;
	if (pdf_name_eq(ctx, tag, PDF_NAME(TD)))
		return FZ_STRUCTURE_TD;
	if (pdf_name_eq(ctx, tag, PDF_NAME(THead)))
		return FZ_STRUCTURE_THEAD;
	if (pdf_name_eq(ctx, tag, PDF_NAME(TBody)))
		return FZ_STRUCTURE_TBODY;
	if (pdf_name_eq(ctx, tag, PDF_NAME(TFoot)))
		return FZ_STRUCTURE_TFOOT;

	/* Inline elements (PDF 1.7 - Table 10.25) */
	if (pdf_name_eq(ctx, tag, PDF_NAME(Span)))
		return FZ_STRUCTURE_SPAN;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Quote)))
		return FZ_STRUCTURE_QUOTE;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Note)))
		return FZ_STRUCTURE_NOTE;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Reference)))
		return FZ_STRUCTURE_REFERENCE;
	if (pdf_name_eq(ctx, tag, PDF_NAME(BibEntry)))
		return FZ_STRUCTURE_BIBENTRY;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Code)))
		return FZ_STRUCTURE_CODE;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Link)))
		return FZ_STRUCTURE_LINK;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Annot)))
		return FZ_STRUCTURE_ANNOT;
	/* Inline elements (PDF 2.0 - Table 368) */
	if (pdf_name_eq(ctx, tag, PDF_NAME(Em)))
		return FZ_STRUCTURE_EM;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Strong)))
		return FZ_STRUCTURE_STRONG;

	/* Ruby inline element (PDF 1.7 - Table 10.26) */
	if (pdf_name_eq(ctx, tag, PDF_NAME(Ruby)))
		return FZ_STRUCTURE_RUBY;
	if (pdf_name_eq(ctx, tag, PDF_NAME(RB)))
		return FZ_STRUCTURE_RB;
	if (pdf_name_eq(ctx, tag, PDF_NAME(RT)))
		return FZ_STRUCTURE_RT;
	if (pdf_name_eq(ctx, tag, PDF_NAME(RP)))
		return FZ_STRUCTURE_RP;

	/* Warichu inline element (PDF 1.7 - Table 10.26) */
	if (pdf_name_eq(ctx, tag, PDF_NAME(Warichu)))
		return FZ_STRUCTURE_WARICHU;
	if (pdf_name_eq(ctx, tag, PDF_NAME(WT)))
		return FZ_STRUCTURE_WT;
	if (pdf_name_eq(ctx, tag, PDF_NAME(WP)))
		return FZ_STRUCTURE_WP;

	/* Illustration elements (PDF 1.7 - Table 10.27) */
	if (pdf_name_eq(ctx, tag, PDF_NAME(Figure)))
		return FZ_STRUCTURE_FIGURE;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Formula)))
		return FZ_STRUCTURE_FORMULA;
	if (pdf_name_eq(ctx, tag, PDF_NAME(Form)))
		return FZ_STRUCTURE_FORM;

	/* Artifact structure type (PDF 2.0 - Table 375) */
	if (pdf_name_eq(ctx, tag, PDF_NAME(Artifact)))
		return FZ_STRUCTURE_ARTIFACT;

	return FZ_STRUCTURE_INVALID;
}

/* The recursive descent of the structure tree uses an fz_try at each level.
 * At the risk of creating a foot cannon... "no one will need more than ~64
 * levels of structure tree". */
static void
run_ds(fz_context *ctx, fz_device *dev, pdf_obj *role_map, pdf_obj *obj, int idx, fz_cookie *cookie)
{
	pdf_obj *k;
	int i, n;

	/* Check the cookie for aborting */
	if (cookie)
	{
		if (cookie->abort)
			return;
		cookie->progress++;
	}

	if (pdf_is_number(ctx, obj))
	{
		/* A marked-content identifier denoting a marked content sequence. WHAT? */
		return;
	}

	if (pdf_mark_obj(ctx, obj))
		return;

	fz_try(ctx)
	{
		fz_structure standard;
		pdf_obj *tag = pdf_dict_get(ctx, obj, PDF_NAME(S));
		if (!tag)
			break;

		standard = pdf_structure_type(ctx, role_map, tag);
		if (standard == FZ_STRUCTURE_INVALID)
			break;
		fz_begin_structure(ctx, dev, standard, pdf_to_name(ctx, tag), idx);
		k = pdf_dict_get(ctx, obj, PDF_NAME(K));
		if (k)
		{
			n = pdf_array_len(ctx, k);
			if (n == 0)
				run_ds(ctx, dev, role_map, k, 0, cookie);
			else
			{
				for (i = 0; i < n; i++)
					run_ds(ctx, dev, role_map, pdf_array_get(ctx, k, i), i, cookie);
			}
		}
		fz_end_structure(ctx, dev);
	}
	fz_always(ctx)
		pdf_unmark_obj(ctx, obj);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void pdf_run_document_structure(fz_context *ctx, pdf_document *doc, fz_device *dev, fz_cookie *cookie)
{
	int nocache;
	int marked = 0;
	pdf_obj *st, *rm, *k;

	fz_var(marked);

	nocache = !!(dev->hints & FZ_NO_CACHE);
	if (nocache)
		pdf_mark_xref(ctx, doc);

	fz_try(ctx)
	{
		st = pdf_dict_get(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root)), PDF_NAME(StructTreeRoot));
		rm = pdf_dict_get(ctx, st, PDF_NAME(RoleMap));

		if (pdf_mark_obj(ctx, st))
			break;
		marked = 1;

		k = pdf_dict_get(ctx, st, PDF_NAME(K));
		if (k)
		{
			int n = pdf_array_len(ctx, k);
			if (n == 0)
				run_ds(ctx, dev, rm, k, 0, cookie);
			else
			{
				int i;
				for (i = 0; i < n; i++)
					run_ds(ctx, dev, rm, pdf_array_get(ctx, k, i), i, cookie);
			}
		}
	}
	fz_always(ctx)
	{
		if (marked)
			pdf_unmark_obj(ctx, st);
		if (nocache)
			pdf_clear_xref_to_mark(ctx, doc);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}
