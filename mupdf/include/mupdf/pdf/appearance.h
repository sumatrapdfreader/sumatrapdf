#ifndef MUPDF_PDF_APPEARANCE_H
#define MUPDF_PDF_APPEARANCE_H

typedef struct pdf_da_info_s
{
	char *font_name;
	int font_size;
	float col[4];
	int col_size;
} pdf_da_info;

void pdf_da_info_fin(fz_context *ctx, pdf_da_info *di);
void pdf_parse_da(fz_context *ctx, char *da, pdf_da_info *di);
void pdf_fzbuf_print_da(fz_context *ctx, fz_buffer *fzbuf, pdf_da_info *di);

void pdf_update_text_appearance(pdf_document *doc, pdf_obj *obj, char *eventValue);
void pdf_update_combobox_appearance(pdf_document *doc, pdf_obj *obj);
void pdf_update_pushbutton_appearance(pdf_document *doc, pdf_obj *obj);
void pdf_update_text_markup_appearance(pdf_document *doc, pdf_annot *annot, fz_annot_type type);
void pdf_update_ink_appearance(pdf_document *doc, pdf_annot *annot);
void pdf_update_text_annot_appearance(pdf_document *doc, pdf_annot *annot);
/*
	pdf_update_free_text_annot_appearance: update the appearance stream for a free text
	annotation, basing it on the annoations rectangle and contents.
*/
void pdf_update_free_text_annot_appearance(pdf_document *doc, pdf_annot *annot);

/*
	pdf_set_annot_appearance: update the appearance of an annotation based
	on a display list.
*/
void pdf_set_annot_appearance(pdf_document *doc, pdf_annot *annot, fz_rect *rect, fz_display_list *disp_list);

/*
	fz_set_markup_appearance: set the appearance stream of a text markup annotations, basing it on
	its QuadPoints array
*/
void pdf_set_markup_appearance(pdf_document *doc, pdf_annot *annot, float color[3], float alpha, float line_thickness, float line_height);

void pdf_set_signature_appearance(pdf_document *doc, pdf_annot *annot, char *name, char *dn, char *date);

#endif
