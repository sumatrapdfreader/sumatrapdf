#ifndef MUPDF_PDF_FIELD_H
#define MUPDF_PDF_FIELD_H

/* Field flags */
enum
{
	Ff_Multiline = 1 << (13-1),
	Ff_Password = 1 << (14-1),
	Ff_NoToggleToOff = 1 << (15-1),
	Ff_Radio = 1 << (16-1),
	Ff_Pushbutton = 1 << (17-1),
	Ff_Combo = 1 << (18-1),
	Ff_FileSelect = 1 << (21-1),
	Ff_MultiSelect = 1 << (22-1),
	Ff_DoNotSpellCheck = 1 << (23-1),
	Ff_DoNotScroll = 1 << (24-1),
	Ff_Comb = 1 << (25-1),
	Ff_RadioInUnison = 1 << (26-1)
};

pdf_obj *pdf_get_inheritable(pdf_document *doc, pdf_obj *obj, char *key);
int pdf_get_field_flags(pdf_document *doc, pdf_obj *obj);
int pdf_field_type(pdf_document *doc, pdf_obj *field);
char *pdf_field_value(pdf_document *doc, pdf_obj *field);
int pdf_field_set_value(pdf_document *doc, pdf_obj *field, char *text);
char *pdf_field_border_style(pdf_document *doc, pdf_obj *field);
void pdf_field_set_border_style(pdf_document *doc, pdf_obj *field, char *text);
void pdf_field_set_button_caption(pdf_document *doc, pdf_obj *field, char *text);
void pdf_field_set_fill_color(pdf_document *doc, pdf_obj *field, pdf_obj *col);
void pdf_field_set_text_color(pdf_document *doc, pdf_obj *field, pdf_obj *col);
int pdf_field_display(pdf_document *doc, pdf_obj *field);
char *pdf_field_name(pdf_document *doc, pdf_obj *field);
void pdf_field_set_display(pdf_document *doc, pdf_obj *field, int d);
pdf_obj *pdf_lookup_field(pdf_obj *form, char *name);
void pdf_field_reset(pdf_document *doc, pdf_obj *field);

#endif
