#ifndef MUPDF_PDF_ANNOT_H
#define MUPDF_PDF_ANNOT_H

/*
	pdf_first_annot: Return the first annotation on a page.

	Does not throw exceptions.
*/
pdf_annot *pdf_first_annot(pdf_document *doc, pdf_page *page);

/*
	pdf_next_annot: Return the next annotation on a page.

	Does not throw exceptions.
*/
pdf_annot *pdf_next_annot(pdf_document *doc, pdf_annot *annot);

/*
	pdf_bound_annot: Return the rectangle for an annotation on a page.

	Does not throw exceptions.
*/
fz_rect *pdf_bound_annot(pdf_document *doc, pdf_annot *annot, fz_rect *rect);

/*
	pdf_annot_type: Return the type of an annotation
*/
fz_annot_type pdf_annot_type(pdf_annot *annot);

/*
	pdf_run_annot: Interpret an annotation and render it on a device.

	page: A page loaded by pdf_load_page.

	annot: an annotation.

	dev: Device used for rendering, obtained from fz_new_*_device.

	ctm: A transformation matrix applied to the objects on the page,
	e.g. to scale or rotate the page contents as desired.
*/
void pdf_run_annot(pdf_document *doc, pdf_page *page, pdf_annot *annot, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie);

struct pdf_annot_s
{
	pdf_page *page;
	pdf_obj *obj;
	fz_rect rect;
	fz_rect pagerect;
	pdf_xobject *ap;
	int ap_iteration;
	fz_matrix matrix;
	pdf_annot *next;
	pdf_annot *next_changed;
	int annot_type;
	int widget_type;
};

fz_link_dest pdf_parse_link_dest(pdf_document *doc, pdf_obj *dest);
fz_link_dest pdf_parse_action(pdf_document *doc, pdf_obj *action);
pdf_obj *pdf_lookup_dest(pdf_document *doc, pdf_obj *needle);
pdf_obj *pdf_lookup_name(pdf_document *doc, char *which, pdf_obj *needle);
pdf_obj *pdf_load_name_tree(pdf_document *doc, char *which);

/* SumatraPDF: parse full file specifications */
char *pdf_file_spec_to_str(pdf_document *doc, pdf_obj *file_spec);

fz_link *pdf_load_link_annots(pdf_document *, pdf_obj *annots, const fz_matrix *page_ctm);

pdf_annot *pdf_load_annots(pdf_document *, pdf_obj *annots, pdf_page *page);
void pdf_update_annot(pdf_document *, pdf_annot *annot);
void pdf_free_annot(fz_context *ctx, pdf_annot *link);

/*
	pdf_create_annot: create a new annotation of the specified type on the
	specified page. The returned pdf_annot structure is owned by the page
	and does not need to be freed.
*/
pdf_annot *pdf_create_annot(pdf_document *doc, pdf_page *page, fz_annot_type type);

/*
	pdf_delete_annot: delete an annotation
*/
void pdf_delete_annot(pdf_document *doc, pdf_page *page, pdf_annot *annot);

/*
	pdf_set_markup_annot_quadpoints: set the quadpoints for a text-markup annotation.
*/
void pdf_set_markup_annot_quadpoints(pdf_document *doc, pdf_annot *annot, fz_point *qp, int n);

/*
	pdf_set_ink_annot_list: set the details of an ink annotation. All the points of the multiple arcs
	are carried in a single array, with the counts for each arc held in a secondary array.
*/
void pdf_set_ink_annot_list(pdf_document *doc, pdf_annot *annot, fz_point *pts, int *counts, int ncount, float color[3], float thickness);

/*
	pdf_set_free_text_details: set the position, text, font and color for a free text annotation.
	Only base 14 fonts are supported and are specified by name.
*/
void pdf_set_free_text_details(pdf_document *doc, pdf_annot *annot, fz_point *pos, char *text, char *font_name, float font_size, float color[3]);

fz_annot_type pdf_annot_obj_type(pdf_obj *obj);

/*
	pdf_poll_changed_annot: enumerate the changed annotations recoreded
	by a call to pdf_update_page.
*/
pdf_annot *pdf_poll_changed_annot(pdf_document *idoc, pdf_page *page);

#endif
