#ifndef MUPDF_PDF_ANNOT_IMP_H
#define MUPDF_PDF_ANNOT_IMP_H

#include "mupdf/pdf.h"

struct pdf_annot
{
	int refs;

	pdf_page *page;
	pdf_obj *obj;

	int is_hot;
	int is_active;

	int needs_new_ap; /* If set, then a resynthesis of this annotation has been requested. */
	int has_new_ap; /* If set, then the appearance stream has changed since last queried. */
	int ignore_trigger_events;

	pdf_annot *next;
};

void pdf_load_annots(fz_context *ctx, pdf_page *page, pdf_obj *annots);
void pdf_drop_annots(fz_context *ctx, pdf_annot *annot_list);
void pdf_drop_widgets(fz_context *ctx, pdf_widget *widget_list);

void pdf_set_annot_has_changed(fz_context *ctx, pdf_annot *annot);

#endif
